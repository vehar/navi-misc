{{

 Nokia6100 Driver, ver 0.6
──────────────────────────────────────────────────────────────────

This is a simple 4096-color driver for the cheap and common Nokia 6100
LCD panel, using either the Epson S1D15G00 or Philips/NXP PCF8833 controllers.

This code is based on the interface tutorial document by Jim Lynch:
  http://www.sparkfun.com/tutorial/Nokia%206100%20LCD%20Display%20Driver.pdf

I have only been able to test it with the Epson chip so far, but it's designed
to work with either. If it doesn't work for you, please bang on it until it
does and send me patches :)

This module provides a public drawing interface that's (very) roughly a subset
of the Graphics object. This might make it easier to port software that's
written for a TV, but keep in mind that the coordinate system and color format
are different, and only a subset of the drawing commands are supported.

The default coordinate system of this LCD is rather odd, so this module
tries to normalize it to look more like a standard raster display. (0,0) is
at the top-left corner, +X is right, +Y is down.

The SPI interface uses my own fast (10 MHz) SPI engine, which unrolls its own
loops at load-time in order to minimize RAM footprint. This module also uses
a scaled version of the ROM font, so it doesn't need to include the data
necessary for an additional font.

The latest version of this driver should always be at:
http://svn.navi.cx/misc/trunk/propeller/nokia6100/

 ┌──────────────────────────────────────────────────────────┐
 │ Copyright (c) 2010 Micah Elizabeth Scott <micah@navi.cx> │               
 │ See end of file for terms of use.                        │
 └──────────────────────────────────────────────────────────┘
}}

CON
  ' init() Mode
  EPSON   = 0
  PHILIPS = 1

  ' Dimensions
  WIDTH = 130
  HEIGHT = 130

  ' Colors
  BLACK    = $000
  WHITE    = $FFF
  RED      = $F00
  GREEN    = $0F0
  BLUE     = $00F
  CYAN     = $0FF
  MAGENTA  = $F0F
  YELLOW   = $FF0
  BROWN    = $B22
  ORANGE   = $FA0
  PINK     = $F6A

  BLEND_TABLE_SIZE = 5

  ' Aligns the usable portion of the display
  DISPLAY_RIGHT  = 129
  DISPLAY_BOTTOM = 131                  

VAR
  byte  cog
  long  cog_cmd

  byte  cmd_don, cmd_doff
  word  fg_color, bg_color
  byte  blend_valid


DAT  
'==============================================================================
' Initialization
'==============================================================================

  
PUB start(pin_rst, pin_data, pin_clk, pin_cs, mode) | initList, c
  '' Initialize the LCD. Starts our driver cog.
  ''
  '' 'mode' must be either EPSON or PHILIPS.

  ' Pins that are owned by the driver cog (CS is always 0)
  init_dira := |<pin_clk | |<pin_data | |<pin_cs

  ' We use CTRA to generate the SPI clock
  init_ctra := pin_clk | constant(%00100 << 26)

  ' Cog's output mask
  spi_dat := |<pin_data

  ' Link cog pointers
  pc := @init_entry

  if mode
    ' Philips
    cmd_paset |= PCF_PASET
    cmd_caset |= PCF_CASET
    cmd_ramwr |= PCF_RAMWR
    cmd_don := PCF_DISPON
    cmd_doff := PCF_DISPOFF
    initList := @initPhilips

  else
    ' Epson
    cmd_paset |= S1D_PASET
    cmd_caset |= S1D_CASET
    cmd_ramwr |= S1D_RAMWR
    cmd_don := S1D_DISON
    cmd_doff := S1D_DISOFF
    initList := @initEpson

  ' Start the driver cog, and wait for it to initialize.
  cog_cmd~~
  cog := cognew(@entry, @cog_cmd)
  repeat while cog_cmd

  ' Reset pulse
  outa[pin_rst]~  
  dira[pin_rst]~~
  waitcnt(cnt + clkfreq/50)
  outa[pin_rst]~~
  waitcnt(cnt + clkfreq/50)

  ' Run the initialization list. After this, the controller is ready.
  repeat while c := WORD[initList]
    spiWrite9(c)
    initList += 2

  ' Default LCD contents
  color(WHITE)
  background(BLACK)
  clear
  on

PUB stop
  '' Stop the SPI driver cog.
  off
  cogstop(cog)

PUB on
  '' Turn on the LCD power
  spiWrite9(cmd_don)

PUB off
  '' Turn off the LCD power
  spiWrite9(cmd_doff) 
  
PRI command(ptr, argList)
  ' Issue a command, blocking only until the argument list has been copied.
  cog_cmd := ((ptr - @entry) << 14) | argList
  repeat while cog_cmd

PRI spiWrite9(b)
  ' Send a single (9-bit) byte to the controller
  command(@cmd_spiWrite9, @b)

PUB flush
  '' Make sure any pending asynchronous commands are finished.
  ''
  '' This must be used if you want to reuse a temporary string or image
  '' buffer, for example, so you can be sure the graphics cog is finished
  '' reading the data first.

  ' No-op command
  command(@mainLoop, @mainLoop)


DAT  
'==============================================================================
' Color Utilities
'==============================================================================


PUB color(c)
  '' Set the current drawing color, as a 12-bit packed value.
  fg_color := c
  command(@cmd_color, @c)
  blend_valid~

PUB background(c)
  '' Set the current background color for 'text' and 'clear'
  bg_color := c
  blend_valid~

PUB alphaBlend(c1, c2, alpha) | x1, x2, xO
  '' If alpha=$0, returns c1.
  '' If alpha=$F, returns c2.
  '' Values in-between will blend between c1 and c2.

  ' Unpack the colors into a padded 24-bit format (000R0G0B)
  x1 := ((c1 << 8) & $0F0000) | ((c1 << 4) & $0F00) | (c1 & $0F)
  x2 := ((c2 << 8) & $0F0000) | ((c2 << 4) & $0F00) | (c2 & $0F)

  ' Now we can do the blend with fixed-point multiply,
  ' and the results will be in the format 00R0G0B0.
  xO := (x1 * ($10 - alpha)) + (x2 * (alpha + 1))

  ' Re-pack it.
  return ((xO >> 12) & $F00) | ((xO >> 8) & $0F0) | ((xO >> 4) & $00F)

PRI updateBlendTable | i, value
  ' If the blending table is not valid, re-generates it.
  '  
  ' When downsampling a font, each output pixel is created by averaging
  ' four input pixels. So, we need 5 levels of gradation between
  ' our foreground and background. (0 to 4 pixels)
  '
  ' We generate this blending table only when the colors have changed, and only
  ' when we're actually drawing text.

  if not blend_valid
    blend_valid~~

    repeat i from 0 to constant(BLEND_TABLE_SIZE - 1)
      value := alphaBlend(bg_color, fg_color, i * $F / constant(BLEND_TABLE_SIZE - 1))
      command(@cmd_setBlendTable, @i)

  
DAT  
'==============================================================================
' Drawing Commands
'==============================================================================


PUB box(x, y, w, h)
  '' Draw a filled rectangle

  if w and h
    command(@cmd_box, @x)

PUB plot(x, y)
  '' Draw a single pixel

  ' This will always be slow, no reason to optimize it here...
  box(x, y, 1, 1)

PUB clear | c
  '' Clear the whole screen to the current background color

  c := bg_color
  command(@cmd_color, @c)

  box(0, 0, WIDTH, HEIGHT)

  c := fg_color
  command(@cmd_color, @c)

PUB text(x, y, str)
  '' Draw a nul-terminated string to (x, y), in the current color,
  '' using the ROM font downsampled by 1/2 to 8x16.
  ''
  '' This is asynchronous. The string data must stay valid until the
  '' command is completed.
  

  updateBlendTable
  command(@cmd_text8x16, @x)

PUB image(x, y, w, h, ptr)
  '' Draw a 12-bit color image from hub memory. 'ptr' is a tightly
  '' packed array of 12-bit pixel data, padded to the nearest byte.
  '' The scan direction is right-to-left, bottom-to-top.
  ''
  '' See the included 'Nokia6100_Convert.py' script for converting
  '' these images.
  ''
  '' This is asynchronous. The image data must stay valid until the
  '' command is completed.

  if w and h
    command(@cmd_image, @x)
       

DAT    
'==============================================================================
' Driver Cog
'==============================================================================

                        '======================================================
                        ' Cog Initialization
                        '======================================================

                        ' The cog initialization consists of two main steps:
                        '
                        '   - Executing instructions, for initializing I/O and
                        '     handshaking with the cog who is running start().
                        '
                        '   - Writing unrolled SPI loops into cog memory. We
                        '     have several large unrolled loops which are necessary
                        '     for speed, but we'd rather not store them in their
                        '     entirety. They just waste hub memory.
                        '
                        ' We can accomplish both of these tasks using very little
                        ' memory, via a somewhat modified version of the Large
                        ' Memory Model concept. I call this meta-LMM. We fetch words
                        ' from main memory. Some of these words are stored at the
                        ' current write pointer and some of them are executed
                        ' immediately. This means we can interleave code and data
                        ' seamlessly, making it very natural to write code which
                        ' programmatically unrolls loops at initialization time.
                        '
                        ' The rule is that code marked with the "if_never" condition
                        ' code is stored at the current write pointer, which then
                        ' increments. Instructions with any other condition code are
                        ' run as-is. This means that we can't generate code that uses
                        ' condition codes, but this is fine for our simple SPI loops.
                        ' The other main limitation is that our VM uses the zero flag,
                        ' so the initialization code itself can only use the carry flag.
                        '
                        ' To save memory, the code memory used for the execution
                        ' engine is repurposed as temporary space after initialization
                        ' is complete. The length of the variable block here *must*
                        ' not exceed the length of the initialization code.
                        '
                        ' When initialization is done, the initialization code will
                        ' jump to the main loop.

                        org     ' Data which replaces initialization code, no more than 9 longs

spi_reg                 res     1

adr_left                                        ' adr_left == arg0
cmd_arg0                res     1
adr_top                                         ' adr_top == arg0
cmd_arg1                res     1
cmd_arg2                res     1
cmd_arg3                res     1
cmd_arg4                res     1

                        org     ' Initialization code, 9 longs
entry
                        rdlong  :inst, pc
                        add     pc, #4
                        test    :inst, cond_mask wz
              if_z      or      :inst, cond_mask        ' If cond was if_never, make it if_always...
:write        if_z      mov     init_data, :inst        ' ..and write to the initialization data pointer.
              if_z      add     :write, inst_dest1      ' Advance the write pointer
              if_z      jmp     #entry
:inst                   nop                             ' Execute the immediate instruction
                        jmp     #entry
               


                        '======================================================
                        ' Main Loop
                        '======================================================

mainLoop

                        ' Wait for command (Handler address in high word, args ptr in low word)
                        rdlong  r1, par wz
              if_z      jmp     #mainLoop

                        ' Read 5 command arguments.
                        movd    :readArg, #cmd_arg0 
                        mov     r0, #5          
:readArg                rdlong  0, r1
                        add     r1, #4 
                        add     :readArg, inst_dest1
                        djnz    r0, #:readArg

                        ' Command received, tell Spin to stop waiting.
                        ' (We needed it to wait just until we're done with its stack)
                        wrlong  c_0, par
                        
                        ' Jump to command handler
                        shr     r1, #16
                        movs    :cmdJmp, r1
                        nop
:cmdJmp                 jmp     #0


                        '======================================================
                        ' 8x8 Unsigned Multiply
                        '======================================================

mul8x8
                        mov     mul_out, mul_b
                        shl     mul_a, #24              ' Justify mul_a
                        shr     mul_out, #1 wc          ' Load the first bit of mul_b

                        mov     r0, #8
:loop         if_c      add     mul_out, mul_a wc
                        rcr     mul_out, #1 wc
                        djnz    r0, #:loop

                        shr     mul_out, #16

mul8x8_ret              ret 


                        '======================================================
                        ' Addressing Setup
                        '======================================================

                        ' This sets the current addressing box according to
                        ' (adr_left, adr_top, adr_right, adr_bottom).
                        '
                        ' This is also where we convert from our own sensible
                        ' coordinate system to whatever this LCD controller model uses.

addressSetupBox
                        ' Set bottom/right according to (x, y, w, h) args
                        mov     adr_right, adr_left
                        add     adr_right, cmd_arg2
                        sub     adr_right, #1
                        mov     adr_bottom, adr_top
                        add     adr_bottom, cmd_arg3
                        sub     adr_bottom, #1
                        
addressSetup

cmd_paset               mov     spi_reg, #0                         ' Placeholder for PASET
                        call    #spi_Write9

                        mov     spi_reg, #($100 | DISPLAY_BOTTOM)   ' p1 = DISPLAY_BOTTOM - bottom
                        sub     spi_reg, adr_bottom
                        call    #spi_Write9

                        mov     spi_reg, #($100 | DISPLAY_BOTTOM)   ' p2 = DISPLAY_BOTTOM - top
                        sub     spi_reg, adr_top
                        call    #spi_Write9


cmd_caset               mov     spi_reg, #0                         ' Placeholder for CASET
                        call    #spi_Write9

                        mov     spi_reg, #($100 | DISPLAY_RIGHT)    ' c1 = DISPLAY_RIGHT - right
                        sub     spi_reg, adr_right
                        call    #spi_Write9

                        mov     spi_reg, #($100 | DISPLAY_RIGHT)    ' c2 = DISPLAY_RIGHT - left
                        sub     spi_reg, adr_left
                        call    #spi_Write9

cmd_ramwr               mov     spi_reg, #0                         ' Placeholder for RAMWR
                        call    #spi_Write9

addressSetupBox_ret
addressSetup_ret        ret


                        '======================================================
                        ' Command: SPI Write
                        '======================================================

cmd_spiWrite9
                        mov     spi_reg, cmd_arg0
                        call    #spi_Write9
                        jmp     #mainLoop


                        '======================================================
                        ' Command: Color
                        '======================================================

                        ' The provided color is a 12-bit value:
                        '    $RGB
                        '
                        ' For fills, we want a tiled 32-bit representation.
                        ' The widest SPI write we have is 32 bits. To utilize this
                        ' fully, we'll write a rotating pattern that repeats every 8
                        ' pixels:
                        '
                        '   $RGBRGBRG
                        '   $BRGBRGBR
                        '   $GBRGBRGB
                        ' 
                        ' Extend the color thusly before saving it.

cmd_color
                        and     cmd_arg0, c_FFF

                        ' Right-shifted parts

                        mov     r0, cmd_arg0
                        mov     cur_color2, r0          ' color2 = $00000RGB
                        shr     r0, #4
                        mov     cur_color0, r0          ' color0 = $000000RG
                        shr     r0, #4
                        mov     cur_color1, r0          ' color1 = $0000000R

                        ' Left-shifted parts

                        shl     cmd_arg0, #4
                        or      cur_color1, cmd_arg0    ' color1 = $0000RGBR
                        shl     cmd_arg0, #4
                        or      cur_color0, cmd_arg0    ' color0 = $000RGBRG
                        shl     cmd_arg0, #4
                        or      cur_color2, cmd_arg0    ' color2 = $00RGBRGB
                        shl     cmd_arg0, #4
                        or      cur_color1, cmd_arg0    ' color1 = $0RGBRGBR
                        shl     cmd_arg0, #4
                        or      cur_color0, cmd_arg0    ' color0 = $RGBRGBRG
                        shl     cmd_arg0, #4
                        or      cur_color2, cmd_arg0    ' color2 = $GBRGBRGB
                        shl     cmd_arg0, #4
                        or      cur_color1, cmd_arg0    ' color1 = $BRGBRGBR
                        
                        jmp     #mainLoop


                        '======================================================
                        ' Command: Box
                        '======================================================

                        ' Set up our addressing to cover the extents of this box,
                        ' then just blast uniformly-colored pixels over SPI as fast
                        ' as we bloody can.
                        '
                        ' We use our repeating pattern of 3 LONGs to write 8-pixel
                        ' blocks. We can round up to the nearest block, since the
                        ' extra pixels, if any, will just wrap around.
                        '
                        ' Args: (x, y, width, height)

cmd_box                 call    #addressSetupBox

                        ' Calculate how many blocks to write
                        mov     mul_a, cmd_arg2
                        mov     mul_b, cmd_arg3
                        call    #mul8x8
                        add     mul_out, #7
                        shr     mul_out, #3

:loop                   mov     spi_reg, cur_color0
                        call    #spi_WriteDat32
                        mov     spi_reg, cur_color1
                        call    #spi_WriteDat32
                        mov     spi_reg, cur_color2
                        call    #spi_WriteDat32

                        djnz    mul_out, #:loop
                        jmp     #mainLoop


                        '======================================================
                        ' Command: Set Blend Table
                        '======================================================

                        ' The blending table is calculated by Spin code, and
                        ' copied into cog memory when necessary.
                        '
                        ' Args: (index, color)

cmd_setBlendTable
                        add     cmd_arg0, #blend_table
                        movd    :copy, cmd_arg0
                        nop
:copy                   mov     0, cmd_arg1

                        jmp     #mainLoop


                        '======================================================
                        ' Command: Image
                        '======================================================

                        ' Draw an arbitrary-size image to the screen, from tightly
                        ' packed 12-bit source data. We calculate how big the image
                        ' is, in bytes, and use 32-bit writes to do as much as possible.
                        ' We pick up the remainder using 9-bit writes.
                        '
                        ' Args: (x, y, width, height, ptr)

cmd_image               call    #addressSetupBox

                        ' Calculate how many bytes to write (pixels * 1.5)
                        ' If we had an extra half-byte, round it up.

                        mov     mul_a, cmd_arg2
                        mov     mul_b, cmd_arg3
                        call    #mul8x8         ' mul_out = total numnber of pixels
                        mov     r0, mul_out
                        add     r0, #1
                        shr     r0, #1
                        add     r0, mul_out     ' r0 = total bytes

                        '                        
                        ' Draw the image! We can write 32 bits at a time as long as
                        '
                        '   - The source pointer is aligned
                        '   - There are at least 32 bits remaining
                        '
                        ' Otherwise, we write 9-bit bytes.
:copyLoop
                        and     cmd_arg4, #%11 nr,wz    ' Z = Is aligned?
                        sub     r0, #4 nr,wc            ' !C = At least 4 bytes left
        if_nz_or_c      jmp     #:slowPath

                        ' Copying 4 bytes at a time

                        rdlong  spi_reg, cmd_arg4       ' Load one long (little endian)
                        add     cmd_arg4, #4
                        sub     r0, #4

                        mov     r1, spi_reg             ' Chip Gracey's elegant 6-instruction byte swap
                        rol     spi_reg, #8
                        ror     r1, #8
                        and     spi_reg, c_00FF00FF
                        andn    r1, c_00FF00FF
                        or      spi_reg, r1
                        
                        call    #spi_WriteDat32         ' Write it out MSB-first
                        jmp     #:copyLoop

                        ' Copying one byte at a time

:slowPath               sub     r0, #1 wc               ' Any bytes left?
              if_c      jmp     #:done

                        rdbyte  spi_reg, cmd_arg4       ' Load one byte
                        add     cmd_arg4, #1
                        or      spi_reg, #$100
                        call    #spi_Write9

                        jmp     #:copyLoop              ' May need to choose fast path

:done

                        ' If there were an odd number of pixels in the image, we rounded
                        ' up, and wrote an extra 4 bits. We need to discard these bits
                        ' somehow, or they'll corrupt all future drawing.
                        '
                        ' The NOP method doesn't seem to work on the Epson controller,
                        ' nor does simply ignoring the bits and changing the write address.
                        ' To clear them, well need to explicitly send one extra dummy byte,
                        ' but we need to set the window first so that we dump the resulting
                        ' pixel somewhere off-screen.

                        test    mul_out, #1 wz          ' Odd number of pixels?
              if_z      jmp     #mainLoop               ' Nope. We're done.

                        neg     adr_left, #2            ' Past right edge of LCD panel
                        mov     adr_top, #0  
                        mov     adr_right, adr_left
                        mov     adr_bottom, adr_top
                        call    #addressSetup
                        mov     spi_reg, #$1FF
                        call    #spi_Write9

                        jmp     #mainLoop


                        '======================================================
                        ' Command: Text 8x16
                        '======================================================

                        ' This is a rather fancy text rendering function
                        ' which draws the built-in ROM font, downsampled by 1/2
                        ' to 8x16 pixels.
                        '
                        ' We downsample using a precalculated color table with
                        ' 5 different gradations from background to foreground
                        ' color. These correspond to zero through four '1' bits
                        ' in the 4 pixels we sample for each output pixel.
                        '
                        ' Args: (x, y, str)

cmd_text8x16
                        rdbyte  cmd_arg3, cmd_arg2 wz   ' Read character
              if_z      jmp     #mainLoop
                        add     cmd_arg2, #1

                        ' Character cell
                        mov     adr_right, adr_left
                        add     adr_right, #7
                        mov     adr_bottom, adr_top
                        add     adr_bottom, #15

                        call    #addressSetup

                        ' Advance x/y
                        add     adr_left, #8
                        cmp     adr_left, #(WIDTH - 8) wc
              if_nc     mov     adr_left, #0
              if_nc     add     adr_top, #16

                        ' The ROM font is 16x32. Each row is one LONG (using every other bit,
                        ' depending on whether it's an even or odd character). To downsample
                        ' it, for each output pixel we need to count the number of 1 bits
                        ' in a 2x2 pixel block.
                        '
                        ' An 8-pixel-wide span divides evenly into three LONGs, so we'll process
                        ' an entire row at a time. The pattern again is:
                        '
                        '   $RGBRGBRG
                        '   $BRGBRGBR
                        '   $GBRGBRGB
                        '
                        ' The font row we're processing, with sampling areas marked, is:
                        '
                        '          24      16       8       0
                        '    +---+---+---+---+---+---+---+---+
                        '   %|0_0|0_0|0_0|0_0|0_0|0_0|0_0|0_0|
                        '   %|0_0|0_0|0_0|0_0|0_0|0_0|0_0|0_0|
                        '    +---+---+---+---+---+---+---+---+
                        '      ^                   ^   ^   ^
                        '      |         .  .  .   |   |   | pixel 0
                        '      |                   |   | pixel 1
                        '      |                   | pixel 2
                        '      | pixel 7
                        '

                        mov     fontPtr, cmd_arg3       ' Character index
                        shr     fontPtr, #1             ' Discard even/odd bit
                        add     fontPtr, #$101          ' Font base is $8000, start at the end of the character
                        shl     fontPtr, #7             ' Character size is $80

                        mov     r1, #16                 ' Row count                         
:row
                        sub     fontPtr, #4
                        rdlong  fontRow1, fontPtr       ' Read two full rows of the ROM font
                        sub     fontPtr, #4
                        rdlong  fontRow2, fontPtr
                        
                        test    cmd_arg3, #1 wz         ' Even/odd character?
              if_nz     shr     fontRow1, #1
              if_nz     shr     fontRow2, #1

                        ' Sum the bits in each sampling square
              
                        and     fontRow1, c_55555555    ' Discard unused odd bits
                        and     fontRow2, c_55555555    ' Discard unused odd bits
                        add     fontRow1, fontRow2      ' Sum the two rows. Now each quat is a bit count from 0 to 2.
                        mov     fontRow2, fontRow1      ' Duplicate the summed row
                        and     fontRow1, c_33333333    ' Low quat in each sampling square
                        andn    fontRow2, c_33333333    ' High quat in each sampling square
                        shr     fontRow2, #2            ' Align quats
                        add     fontRow1, fontRow2      ' Sum. Now each nybble is a bit count from 0 to 5.

                        ' Use blendFn to convert each pixel to a 12-bit color, then pack those into LONGs.

                        call    #blendFn                ' Pixel 0
                        shl     r0, #20
                        mov     spi_reg, r0

                        call    #blendFn                ' Pixel 1
                        shl     r0, #8
                        or      spi_reg, r0

                        call    #blendFn                ' Pixel 2 (RG)
                        mov     r2, r0
                        shr     r0, #4
                        or      spi_reg, r0

                        call    #spi_WriteDat32         ' Long 0

                        shl     r2, #28                 ' Pixel 2 (B)
                        mov     spi_reg, r2

                        call    #blendFn                ' Pixel 3
                        shl     r0, #16
                        or      spi_reg, r0

                        call    #blendFn                ' Pixel 4
                        shl     r0, #4
                        or      spi_reg, r0

                        call    #blendFn                ' Pixel 5 (R)
                        mov     r2, r0
                        shr     r0, #8
                        or      spi_reg, r0

                        call    #spi_WriteDat32         ' Long 1

                        shl     r2, #24                 ' Pixel 5 (GB)
                        mov     spi_reg, r2

                        call    #blendFn                ' Pixel 6
                        shl     r0, #12
                        or      spi_reg, r0

                        call    #blendFn                ' Pixel 7
                        or      spi_reg, r0
                        
                        call    #spi_WriteDat32         ' Long 2

                        djnz    r1, #:row               ' Next row
                        jmp     #cmd_text8x16           ' Next character

                        ' Blending routine: Shift a sample out of fontRow1, returns a color in r0
                        ' Note: This display likes its pixels right-to-left. Reverse the pixel order.
                        
blendFn                 mov     r0, fontRow1
                        shr     r0, #28                 ' Examine most significant nybble
                        movs    :blendRead, #blend_table
                        add     :blendRead, r0
                        shl     fontRow1, #4            ' Next highest nybble
:blendRead              mov     r0, 0
blendFn_ret             ret
                                

'------------------------------------------------------------------------------
' Initialized Data
'------------------------------------------------------------------------------

c_0                     long    0
c_FFF                   long    $FFF
c_8000                  long    $8000
c_55555555              long    $55555555
c_33333333              long    $33333333
c_00FF00FF              long    $00FF00FF
frq_8clk                long    1 << (32 - 3)   ' 1/8th clock rate
inst_dest1              long    1 << 9          ' Value of '1' in the instruction dest field
cond_mask               long    %1111 << 18     ' Condition code mask

pc                      long    0               ' Initialized with @init_entry
init_dira               long    0
init_ctra               long    0
spi_dat                 long    0


'------------------------------------------------------------------------------
' Uninitialized Data
'------------------------------------------------------------------------------

r0                      res     1
r1                      res     1
r2                      res     1

cur_color0              res     1
cur_color1              res     1
cur_color2              res     1

mul_a                   res     1
mul_b                   res     1
mul_out                 res     1

fontPtr                 res     1
fontRow1                res     1
fontRow2                res     1

blend_table             res     BLEND_TABLE_SIZE

adr_right               res     1
adr_bottom              res     1

'
' This is a template for the SPI routines which we unroll
' at initialization time. This must be kept in sync with
' the actual loop unrolling code in the initialization overlay.
'

init_data

spi_Write9              res     5 + 8*2 + 2
spi_Write9_ret          res     1

spi_WriteDat32          res     3 + 3*(8*2 + 2) + 8*2 + 2
spi_WriteDat32_ret      res     1

                        fit

                        
'==============================================================================
' Initialization Overlay
'==============================================================================

                        org
init_entry

                        mov     dira, init_dira         ' Take control over the SPI bus
                        mov     ctra, init_ctra

                        ' This SPI engine uses a very carefully timed CTRA to
                        ' generate the clock pulses while an unrolled loop reads
                        ' the bus at two instructions per bit. This gives an SPI
                        ' clock rate 1/8 the system clock. At an 80Mhz system clock,
                        ' that's an SPI data rate of 10 Mbit/sec!
                        '
                        ' Only touch this code if you have an oscilloscope at hand :)

                        '
                        ' spi_Write9 --
                        '
                        '   Write the lower 9 bits of spi_reg to the SPI port, MSB first.
                        '

        if_never        shl     spi_reg, #23            ' Left justify MSB
        if_never        rcl     spi_reg, #1 wc          ' Shift bit 8
        if_never        mov     phsa, #0                ' Rising edge at center of each bit
        if_never        muxc    outa, spi_dat           ' Output bit 7
        if_never        mov     frqa, frq_8clk          ' First clock edge, period of 8 clock cycles

:spiw9_loop             mov     r0, #7                  ' Repeat 8 times, for bits 7..0

        if_never        rcl     spi_reg, #1 wc
        if_never        muxc    outa, spi_dat         

                        sub     r0, #1 wc
:spiw9_loop_rel   if_nc sub     pc, #4*(:spiw9_loop_rel - :spiw9_loop)

        if_never        or      r0, #0                  ' NOP, Finish last clock cycle
        if_never        mov     frqa, #0                ' Turn off clock
        if_never        ret

                        '
                        ' spi_WriteDat32 --
                        '
                        '   Write 32 bits worth of data to the SPI port, from spi_reg.
                        '   This automatically inserts the Command bits, so we actually
                        '   end up writing a total of 36 bits.
                        ' 

        if_never        mov     phsa, #0                ' Rising edge at center of each bit
        if_never        or      outa, spi_dat           ' Output first command bit
        if_never        mov     frqa, frq_8clk          ' First clock edge, period of 8 clock cycles

                        ' Bytes 0, 1, 2
              
:spiw32_loop1           mov     r0, #2                  ' Repeat 3 times
       
:spiw32_loop2           mov     r1, #7                  ' Output one byte; Repeat 8 times
        if_never        rcl     spi_reg, #1 wc
        if_never        muxc    outa, spi_dat
                        sub     r1, #1 wc
:spiw32_loop2_rel if_nc sub     pc, #4*(:spiw32_loop2_rel - :spiw32_loop2)

        if_never        or      r0, #0                  ' Command bit between bytes
        if_never        or      outa, spi_dat

                        sub     r0, #1 wc
:spiw32_loop1_rel if_nc sub     pc, #4*(:spiw32_loop1_rel - :spiw32_loop1)

                        ' Byte 3
       
:spiw32_loop3           mov     r0, #7                  ' Repeat 8 times
        if_never        rcl     spi_reg, #1 wc
        if_never        muxc    outa, spi_dat
                        sub     r0, #1 wc
:spiw32_loop3_rel if_nc sub     pc, #4*(:spiw32_loop3_rel - :spiw32_loop3)

        if_never        or      r0, #0                  ' NOP, Finish last clock cycle
        if_never        mov     frqa, #0                ' Turn off clock
        if_never        ret

                        '
                        ' Initialization is done.
                        ' Tell the spin code that we're done, then break out of LMM and enter the main loop.
                        '

                        wrlong  c_0, par  
                        jmp     #mainLoop


DAT  
'==============================================================================
' LCD Model-specific Data
'==============================================================================


CON
  ' Philips PCF8833 Command Codes
  PCF_NOP      = $00
  PCF_SWRESET  = $01
  PCF_BSTROFF  = $02
  PCF_BSTRON   = $03
  PCF_SLEEPIN  = $10
  PCF_SLEEPOUT = $11
  PCF_PTLON    = $12
  PCF_NORON    = $13
  PCF_INVOFF   = $20
  PCF_INVON    = $21
  PCF_DAL0     = $22
  PCF_DAL      = $23
  PCF_SETCON   = $25
  PCF_DISPOFF  = $28
  PCF_DISPON   = $29
  PCF_CASET    = $2A
  PCF_PASET    = $2B
  PCF_RAMWR    = $2C
  PCF_RGBSET   = $2D
  PCF_PTLAR    = $30
  PCF_VSCRDEF  = $33
  PCF_TEOFF    = $34
  PCF_TEON     = $35
  PCF_MADCTL   = $36
  PCF_SEP      = $37
  PCF_IDMOFF   = $38
  PCF_IDMON    = $39
  PCF_COLMOD   = $3A
  PCF_SETVOP   = $B0
  PCF_BRS      = $B4
  PCF_TRS      = $B6
  PCF_DISCTR   = $B9
  PCF_DOR      = $BA
  PCF_TCDFE    = $BD
  PCF_TCVOPE   = $BF
  PCF_EC       = $C0
  PCF_SETMUL   = $C2
  PCF_TCVOPAB  = $C3
  PCF_TCVOPCD  = $C4
  PCF_TCDF     = $C5
  PCF_DF8COLOR = $C6
  PCF_SETBS    = $C7
  PCF_NLI      = $C9

  ' Epson S1D15G00 Command Codes
  S1D_DISON    = $AF
  S1D_DISOFF   = $AE
  S1D_DISNOR   = $A6
  S1D_DISINV   = $A7
  S1D_COMSCN   = $BB
  S1D_DISCTL   = $CA
  S1D_SLPIN    = $95
  S1D_SLPOUT   = $94
  S1D_PASET    = $75
  S1D_CASET    = $15
  S1D_DATCTL   = $BC
  S1D_RGBSET8  = $CE
  S1D_RAMWR    = $5C
  S1D_PTLIN    = $A8
  S1D_PTLOUT   = $A9
  S1D_RMWIN    = $E0
  S1D_RMWOUT   = $EE
  S1D_ASCSET   = $AA
  S1D_SCSTART  = $AB
  S1D_OSCON    = $D1
  S1D_OSCOFF   = $D2
  S1D_PWRCTR   = $20
  S1D_VOLCTR   = $81
  S1D_VOLUP    = $D6
  S1D_VOLDOWN  = $D7
  S1D_TMPGRD   = $82
  S1D_NOP      = $25

  ' Adjust to taste
  EPSON_CONTRAST = $28
  
DAT

' LCD Initialization Tables

initPhilips   word  PCF_SLEEPOUT        ' Out of sleep mode
              word  PCF_INVON           ' Inversion ON (seems to be required?)
              word  PCF_COLMOD, $103    ' Color mode = 12 bpp
              word  PCF_MADCTL, $1C8    ' Mirror X/Y, reverse RGB
              word  PCF_SETCON, $130    ' Set contrast
              word  0

initEpson     word  S1D_DISCTL, $100, $120, $100
              word  S1D_COMSCN, $101
              word  S1D_OSCON
              word  S1D_SLPOUT
              word  S1D_PWRCTR, $10f
              word  S1D_DISINV
              word  S1D_DATCTL, $100, $100, $102
              word  S1D_VOLCTR, $100 | EPSON_CONTRAST, $103  
              word  0

DAT
{{

┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                                   TERMS OF USE: MIT License                                                  │                                                            
├──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
│Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation    │ 
│files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,    │
│modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software│
│is furnished to do so, subject to the following conditions:                                                                   │
│                                                                                                                              │
│The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.│
│                                                                                                                              │
│THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE          │
│WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR         │
│COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,   │
│ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                         │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
}}