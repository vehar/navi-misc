{                                   

rfid-lf - Minimalist software-only reader for low-frequency RFID tags
─────────────────────────────────────────────────────────────────────

I. Supported Tags

  Tested with the EM4102 compatible RFID tags sold by Parallax, and with
  HID proximity cards typically issued for door security.

  These are two fairly typical ASK and FSK tags, respectively. To support
  other RFID protocols, it should be possible to use one of these two
  decoders as a starting point.

I. Theory of operation

  The Propeller itself generates the 125 kHz carrier wave, which excites
  an LC tank. The inductor in this tank is an antenna coil, which becomes
  electromagnetically coupled to the RFID tag when it comes into range.
  This carrier powers the RFID tag. The RFID sends back data by modulating
  the amplitude of this carrier. We detect this modulation via a peak
  detector and low-pass filter (D1, C2, R1). The detected signal is AC
  coupled via C3, and an adjustable bias is applied via R3 from a PWM signal
  which is filtered by R4 and C4. This signal is fed directly to an input
  pin via R2 (which limits high-frequency noise) where the pin itself acts
  as an analog comparator.

  The waveform which exits the peak detector is a sequence of narrow spikes
  at each oscillation of the carrier wave. Each of these spikes occurs when
  C2 charges quickly via D1, then discharges slowly via R1. We can use this
  to fashion a simple A/D converter by having the Propeller use a counter to
  measure the duty cycle of this waveform. The higher the carrier amplitude,
  the more charge was stored in C2, and the longer the pulse will be.

  At this point, we have a digital representation of the baseband RF signal
  from the RFID tags. This signal is then put through a bank of demodulators,
  filters, and protocol decoders.
  
  We include multiple closed-loop control systems in order to keep the reader
  calibrated. We adjust the Threshold (THR) voltage dynamically in order to
  keep the duty cycle at a reasonable level so we don't over-saturate our
  A/D converter. We also automatically tweak the carrier frequency in order
  to keep the LC tank operating at its resonant peak.

  This object requires two cogs, mostly just because it needs three counter
  units: One to generate the carrier, one to measure incoming pulses, and one
  to generate the threshold voltage.

II. Schematic

                (P5) IN ─┐    (P1) C+ ──┐    
                          │               │    
                       R2                 L1 
               R4         │  C3       D1  │    
   (P3) THR ────┳────┻────┳──┳────┫    
                   │  R3        │  │      │    
                C4        R1  C2  C1 
                                       │    
                                          │    
                               (P0) C- ──┘    
                   
  C1     1.5 nF
  C2     2.2 nF
  C3     1 nF
  C4     2.2 nF
  R1     1 MΩ
  R2     270 Ω
  R4     100 kΩ
  R3     220 kΩ
  D1     Some garden variety sigal diode from my junk drawer.
  L1     About 1 mH. Tune for 125 kHz resonance for C1.
 
  Optional parts:

   - An amplifier for the carrier wave. Ideally it would be a very high
     slew rate op-amp or buffer which could convert the 3.3v signal from
     the Prop into a higher-voltage square wave for exciting the LC tank.
  
     I've tried a MAX233A, but it was unsuccessful due to the low slew rate
     which caused excessive harmonic distortion. The best option is probably
     a high voltage H-bridge, but I haven't tested this yet.

   - An external comparator for the input value. This makes the circuit
     easier to debug, and it helps if you're in a noisy electrical environment.

     Hook the inverting input up to a voltage divider that generates a Vdd/2
     reference, and the non-inverting input up to the junction between C3 and R3.
     I've tested this with a MAX473 op-amp.
        
III. License
       
 ┌───────────────────────────────────┐
 │ Copyright (c) 2008 Micah Dowty    │               
 │ See end of file for terms of use. │
 └───────────────────────────────────┘

}

CON
  CARRIER_HZ         = 125_000      ' Initial carrier frequency
  DEFAULT_THRESHOLD  = $80000000    ' Initial comparator frequency
  THRESHOLD_GAIN     = 7            ' Log2 gain for threshold control loop
  CARRIER_RATE       = 7            ' Inverse Log2 rate for carrier frequency control loop
  CARRIER_GAIN       = 300          ' Gain for carrier frequency control loop  

  SHIELD2_PIN        = 6            ' Optional, driven to ground to shield INPUT.
  INPUT_PIN          = 5            ' Input signal
  SHIELD1_PIN        = 4            ' Optional, driven to ground to shield INPUT.
  THRESHOLD_PIN      = 3            ' PWM output for detection threshold
  DEBUG_PIN          = 2            ' Optional, for oscilloscope debugging
  CARRIER_POS_PIN    = 1            ' Carrier wave pin
  CARRIER_NEG_PIN    = 0            ' Carrier wave pin

  ' Code formats.
  ' The low 16 bits indicate length, in longs.
  ' Other bits are used to make each format unique.

  FORMAT_EM4102      = $0001_0002   ' Two longs: 8-bit manufacturer code, 32-bit unique ID.
  FORMAT_HID         = $0002_0002   ' HID 128 KHz prox cards. 45-bit code.
  
VAR
  byte cog1, cog2
  long format_buf
  long shared_pulse_len
  long em_buffer[2]
  long hid_buffer[2]
  
PUB start | period, okay

  ' Fundamental timing parameters: Default carrier wave
  ' drive frequency, and the period of the carrier in clock
  ' cycles.

  init_frqa := fraction(CARRIER_HZ, clkfreq)
  period := clkfreq / CARRIER_HZ

  ' Derived timing parameters
  
  pulse_target := period / 3      ' What is our 'center' pulse width?
  next_hyst := pulse_target
  hyst_constant := period / 100   ' Amount of pulse hysteresis

  ' Output buffers
  em_buf_ptr := @em_buffer
  hid_buf_ptr := @hid_buffer
  format_ptr := @format_buf
  pulse_ptr1 := @shared_pulse_len
  pulse_ptr2 := @shared_pulse_len
  format_buf~
  
  cog1 := cognew(@cog1_entry, 0) + 1
  okay := cog2 := cognew(@cog2_entry, 0) + 1

  
PUB stop
  if cog2
    cogstop(cog1~ - 1)
    cogstop(cog2~ - 1)
  
PUB read(buffer) : format
  '' Read an RFID code, if one is available.
  ''
  '' If a code is available, it is copied into 'buffer', and we return
  '' a FORMAT_* constant that identifies the code's format. If no code
  '' is available, returns zero.
  ''
  '' The format code's low 16 bits indicate the length of the received
  '' RFID code, in longs. The other format bits are used to uniquely
  '' identify each format.
  ''
  '' The buffer must be at least 16 longs, to hold the largest code format.

  format := format_buf

  if format == FORMAT_EM4102
    longmove(buffer, @em_buffer, constant(FORMAT_EM4102 & $FFFF))

  if format == FORMAT_HID
    longmove(buffer, @hid_buffer, constant(FORMAT_HID & $FFFF))

  format_buf~
  
PRI fraction(a, b) : f
  a <<= 1
  repeat 32
    f <<= 1
    if a => b
      a -= b
      f++
    a <<= 1

DAT

'==============================================================================
' Cog 1: Pulse timing
'==============================================================================

                        org

cog1_entry

                        '
                        ' Measure a low pulse.
                        '
                        ' The length of this pulse is proportional to the amplitude of the carrier.
                        ' To measure it robustly, we'll measure the average duty cycle rather than
                        ' looking at actual rising or falling edges.
                        '
                        ' We could easily do this in cog2, but we're out of counters there. Measure
                        ' the pulse length using this cog's CTRA, and send it back to cog2 synchronously.
                        '                        

                        mov     ctra, :ctra_value
                        mov     frqa, #1

:loop                   mov     t0, phsa
                        wrlong  t0, pulse_ptr1
:wait                   rdlong  t0, pulse_ptr1 wz
              if_z      jmp     #:loop
                        jmp     #:wait

:ctra_value             long    (%01100 << 26) | INPUT_PIN
pulse_ptr1              long    0
t0                      res     1
                        
                        fit


DAT

'==============================================================================
' Cog 2: Control loops and protocol decoding
'==============================================================================

                        '======================================================
                        ' Initialization
                        '======================================================

                        org

cog2_entry              mov     dira, init_dira
                        mov     ctra, init_ctra         ' CTRA generates the carrier wave
                        mov     frqa, init_frqa
                        mov     ctrb, init_ctrb         ' CTRB generates a pulse threshold bias
                        mov     frqb, init_frqb


                        '======================================================
                        ' Main A/D loop
                        '======================================================

mainLoop
                        '
                        ' Synchronize each loop iteration with a rising edge on
                        ' the carrier wave. To avoid races when reading pulse_ptr,
                        ' we should ideally synchronize at a point 180 degrees
                        ' out of phase with the negative pulse from our analog peak
                        ' detector.
                        '

                        mov     prev_pulse, cur_pulse   ' Remember previous pulse info 

:high                   test    h80000000, phsa wz
              if_z      jmp     #:high
:low                    test    h80000000, phsa wz      
              if_nz     jmp     #:low

                        rdlong  cur_pulse, pulse_ptr2   ' Fetch pulse count from cog1
                        wrlong  zero, pulse_ptr2
                        
                        mov     pulse_len, cur_pulse    ' Measure length of pulse
                        sub     pulse_len, prev_pulse
                        
                        add     pulse_count, #1         ' Global pulse counter, used below

                        '
                        ' Adjust the comparator threshold in order to achieve our pulse_target,
                        ' using a linear proportional control loop.
                        '

                        mov     r0, pulse_target
                        sub     r0, pulse_len
                        shl     r0, #THRESHOLD_GAIN
                        sub     frqb, r0

                        '
                        ' We also want to dynamically tweak the carrier frequency, in order
                        ' to hit the resonance of our LC tank as closely as possible. The
                        ' value of frqb is actually a filtered representation of our overall
                        ' inverse carrier amplitude, so we want to adjust frqa in order to
                        ' minimize frqb.
                        '
                        ' Since we can't adjust frqa drastically while the RFID reader is
                        ' operating, we'll make one small adjustment at a time, and decide
                        ' whether or not it was an improvement. This process eventually converges
                        ' on the correct resonant frequency, so it should be enough to keep our
                        ' circuit tuned as the analog components fluctuate slightly in value
                        ' due to temperature variations.
                        '
                        ' This algorithm is divided into four phases, sequenced using two
                        ' bits from the pulse_count counter:
                        '
                        '   0. Store a reference frqb value, and increase frqa
                        '   1. Test the frqb value. If it wasn't an improvement, decrease frqa
                        '   2. Store a reference frqb value, and decrease frqa
                        '   3. Test the frqb value. If it wasn't an improvement, increase frqa
                        '

                        test    pulse_count, carrier_mask wz
        if_nz           jmp     #:skip_frqa
                        test    pulse_count, carrier_bit0 wz
                        test    pulse_count, carrier_bit1 wc
                        negc    r0, #CARRIER_GAIN
        if_nz           mov     prev_frqb, frqb
        if_nz           add     frqa, r0
        if_z            cmp     prev_frqb, frqb wc
        if_z_and_c      sub     frqa, r0        
:skip_frqa

                        '
                        ' That takes care of all our automatic calibration tasks.. now to
                        ' receive some actual bits. Since our pulse length is proportional
                        ' to the amount of carrier attenuation, our demodulated bits (or
                        ' baseband FSK signal) are determined by the amount of pulse width
                        ' excursion from our center position.
                        '
                        ' We don't need to measure the center, since we're actively balancing
                        ' our pulses around pulse_target. A simple bit detector would just
                        ' compare pulse_len to pulse_target. We go one step further, and
                        ' include a little hysteresis.
                        '

                        cmp     next_hyst, pulse_len wc    
                        muxc    outa, #|<DEBUG_PIN         ' Output demodulated bit to debug pin

                        mov     next_hyst, pulse_target    ' Update hysteresis for the next bit
                        sumc    next_hyst, hyst_constant

              if_nc     add     baseband_s32, #1
              if_nc     add     baseband_s256, #1
                        rcl     baseband_reg+0, #1 wc      ' Store in our baseband shift register
              if_nc     sub     baseband_s32, #1           '   ... and keep a running total of the bits.              
                        rcl     baseband_reg+1, #1 wc
                        rcl     baseband_reg+2, #1 wc
                        rcl     baseband_reg+3, #1 wc
                        rcl     baseband_reg+4, #1 wc
                        rcl     baseband_reg+5, #1 wc
                        rcl     baseband_reg+6, #1 wc
                        rcl     baseband_reg+7, #1 wc
              if_nc     sub     baseband_s256, #1              

                        '
                        ' Our work here is done. Give each card-specific protocol decoder
                        ' a chance to find actual ones and zeroes.
                        '
                        
                        call    #rx_hid
                        call    #rx_em4102

                        jmp     #mainLoop


                        '======================================================
                        ' EM4102 Decoder 
                        '======================================================

                        ' The EM4102 chip actually supports multiple clock rates
                        ' and multiple encoding schemes: Manchester, Biphase, and PSK.
                        ' Their "standard" scheme, and the one Parallax uses, is
                        ' ASK with Manchester encoding at 32 clocks per code (64
                        ' clocks per bit). Our support for this format is hardcoded.
                        '
                        ' The EM4102's data packet consists of 40 payload bits (an 8-bit
                        ' manufacturer ID and 32-bit serial number), 9 header bits, 1 stop
                        ' bit, and 14 parity bits. This is a total of 64 bits. These bits
                        ' are manchester encoded into 128 baseband bits. 
                        '
                        ' We could decode this the traditional way- do clock/data recovery
                        ' on the Manchester signal using a DPLL, look for the header, do
                        ' manchester decoding on the rest of the packet, etc. But this is
                        ' software, and we can throw memory and CPU at the problem in order
                        ' to get a more noise-resistant decoding.
                        '
                        ' A packet in its entirety is 4096 clocks. This is 128 manchester
                        ' codes by 32 clocks per code. We can treat this as 32 possible phases
                        ' and 128 possible code alignments. In fact, it's a more convenient
                        ' to treat it as 64 possible phases and 64 possible bits. We get the
                        ' same result, and it's less data to move around.
                        '
                        ' To save memory, we'll decimate the signal and examine it only every
                        ' other carrier cycle. This gives us only 32 possible phases.
                        '
                        ' Every time a code arrives, we shift it into a 64-bit shift register.
                        ' We have 32 total shift registers, which we cycle through. Every time
                        ' we shift in a new bit, we examine the entire shift register and test
                        ' whether it's a valid packet.
                        
rx_em4102
                        test    pulse_count, #1 wz      ' Decimate x2 (Opposite phase from the HID decoder)
              if_nz     jmp     #rx_em4102_ret

                        ' Low pass filter with automatic gain control. Look at an average of the last
                        ' 32 bits, and correct our duty cycle to 50% by picking a threshold based on
                        ' the average of the last 256 bits.

                        mov     r0, baseband_s256
                        shr     r0, #3
                        cmp     baseband_s32, r0 wc

:shift1                 rcl     em_bits+1, #1 wc        ' Shift in the new filtered bit
:shift2                 rcl     em_bits+0, #1
:load1                  mov     em_shift+0, em_bits+0   ' And save a copy in a static location
:load2                  mov     em_shift+1, em_bits+1

                        add     :shift1, dest_2         ' Increment em_bits pointers
                        add     :shift2, dest_2
                        add     :load1, #2
                        add     :load2, #2
                        cmp     :shift1, em_shift1_end wz
              if_z      sub     :shift1, dest_64        ' Wrap around     
              if_z      sub     :shift2, dest_64
              if_z      sub     :load1, #64     
              if_z      sub     :load2, #64    
                        
                        rdlong  r0, format_ptr wz       ' Make sure the output buffer is available
              if_nz     jmp     #rx_em4102_ret
              
                        '
                        ' At this point, the encoded packet should have the following format:
                        ' (Even bits in the manchester code are not shown.)
                        '
                        '   bits+0:  11111111_1ddddPdd_ddPddddP_ddddPddd                        
                        '   bits+1:  dPddddPd_dddPdddd_PddddPdd_ddPPPPP0
                        '
                        ' Where 'd' is a data bit and 'P' is a parity bit.
                        '

                        mov     r0, em_shift+0          ' Look for the header of nine "1" bits
                        shr     r0, #32-9
                        cmp     r0, #$1FF wz
              if_nz     jmp     #rx_em4102_ret

                        rcr     em_shift+1, #1 nr,wc    ' Look for a footer of one "0"
              if_c      jmp     #rx_em4102_ret

                        ' Looking good so far. Now loop over the 10 data rows...

                        mov     em_decoded, #0
                        mov     em_decoded+1, #0
                        mov     em_parity, #0
                        mov     r0, #10
:row
                        mov     r2, em_shift+0          ' Extract the next row's 5 bits
                        shr     r2, #18
                        and     r2, #%11111 wc          ' Check row parity
              if_c      jmp     #rx_em4102_ret

                        mov     r1, em_decoded+1        ' 64-bit left shift by 4
                        shl     em_decoded+1, #4
                        shl     em_decoded+0, #4
                        shr     r1, #32-4
                        or      em_decoded+0, r1 
                        
                        shr     r2, #1                  ' Drop row parity bit
                        xor     em_parity, r2           ' Update column parity
                        or      em_decoded+1, r2        ' Store 4 decoded bits

                        mov     r1, em_shift+1          ' 64-bit left shift by 5
                        shl     em_shift+1, #5
                        shl     em_shift+0, #5
                        shr     r1, #32-5
                        or      em_shift+0, r1 

                        djnz    r0, #:row

                        mov     r2, em_shift+0          ' Extract the next 4 bits
                        shr     r2, #19
                        and     r2, #%1111
                        xor     em_parity, r2 wc        ' Test column parity
              if_c      jmp     #rx_em4102_ret

                        mov     r0, em_buf_ptr          ' Write the result
                        wrlong  em_decoded+0, em_buf_ptr
                        add     r0, #4
                        wrlong  em_decoded+1, r0
                        wrlong  c_format_em, format_ptr
                        
rx_em4102_ret           ret

em_shift1_end           rcl     em_bits+1+64, #1 wc     ' This is ":shift1" above, after we've
                                                        ' passed the end of the em_bits array.


                        '======================================================
                        ' HID Decoder
                        '======================================================

                        ' This is a data decoder for HID's 125 kHz access control cards.
                        ' I don't have any actual documentation from HID- this is all
                        ' gleaned from public documentation of other RFID systems, and
                        ' from my own reverse engineering.
                        '
                        ' These cards use a FSK scheme, which appears to be identical
                        ' to the one used by the Microchip MCRF200. Zeroes and ones are
                        ' nominally encoded by attenuation pulses of 8 and 10 cycles,
                        ' respectively. Bits appear to last 50 RF cycles, which is one
                        ' of the configurable bit lengths for the MCRF200.
                        '
                        ' See Figure 2-2 of the Microchip microID 125 kHz RFID System Design Guide:
                        '   http://ww1.microchip.com/downloads/en/devicedoc/51115F.pdf
                        '
                        ' So, to decode this signal we use a similar low-pass filter and shift
                        ' register technique as the one we used above in the EM4102 decoder,
                        ' but only after doing an FSK detection stage.
                        '
                        ' After FSK detection and low-pass filtering, the signal appears to
                        ' be a repeating pattern of 96 of these 50-cycle bits. Each repetition
                        ' begins with the special sequence '000111'. The packet data is manchester
                        ' encoded, so runs of three zeroes or ones cannot occur during the packet body.
                        ' This means there should be 45 bits of actual packet data after manchester
                        ' decoding.
                        '
                        ' So, what does this 45-bit packet mean? HID claims that these cards
                        ' have either a 26-bit or 34-bit code. There does not seem to be any extra
                        ' layer of encoding present, since I can match the number printed on these
                        ' cards with some of the bits in this 45-bit packet.
                        '
                        ' On my cards, model number HID 0004H, there are two dot-matrix printed
                        ' numbers on the back of the card: a 6-digit code followed by a hyphenated
                        ' 8-2 digit code. The latter code is the same on cards I've examined. It may
                        ' be some kind of site code or batch number. The 6-digit code is the card's
                        ' unique ID.
                        '
                        ' In the 45-bit packet, the least significant bit seems to always be zero. The
                        ' next 20 bits contain the card's 6-digit unique ID. The other bits are constant
                        ' on the cards I've examined. They're probably a site code.
                        '
                        ' To extract the printed 6-digit ID from a HID card:
                        '    (buffer[1] >> 1) & $FFFFF
                        '

rx_hid
                        ' This is the FSK decoder. We shift zeroes into hid_fsk_reg on every
                        ' carrier wave cycle. When we detect a stable rising edge on the baseband,
                        ' the whole hid_fsk_reg is inverted. At this point, the most recent FSK
                        ' cycle will be all ones, and the previous cycle will be all zeroes.
                        '
                        ' On each stable rising edge, we can detect a 1 or a 0 by tapping the
                        ' hid_fsk_reg. The location of the tap determines our threshold frequency.
                        ' This detected bit is latched until the next rising edge.

                        shl     hid_fsk_reg, #1         ' Advance the FSK detection shifter

                        mov     r0, baseband_reg        ' Detect rising edges
                        and     r0, #%1111
                        cmp     r0, #%0001 wz           ' If Z=1, it's an edge.

              if_z      xor     hid_fsk_reg, hFFFFFFFF  ' Invert on every edge

              if_nz     rcr     hid_lp_reg+0, #1 nr,wc  ' Repeat the last bit, or tap a new bit.
              if_z      test    hid_fsk_reg, hid_fsk_mask wc

                        ' Now the detected FSK bit is in the carry flag. Feed it into
                        ' a 32-bit-wide low pass filter, which is close enough to our
                        ' bit length of 50 cycles. We correct the 32-bit filter's DC bias
                        ' using a 256-bit filter. This helps us maintain an even duty cycle
                        ' in the received manchester bits.

              if_nc     add     hid_lp_s32, #1
              if_nc     add     hid_lp_s256, #1
                        rcl     hid_lp_reg+0, #1 wc
              if_nc     sub     hid_lp_s32, #1              
                        rcl     hid_lp_reg+1, #1 wc
                        rcl     hid_lp_reg+2, #1 wc
                        rcl     hid_lp_reg+3, #1 wc
                        rcl     hid_lp_reg+4, #1 wc
                        rcl     hid_lp_reg+5, #1 wc
                        rcl     hid_lp_reg+6, #1 wc
                        rcl     hid_lp_reg+7, #1 wc
              if_nc     sub     hid_lp_s256, #1              

                        mov     r0, hid_lp_s256
                        shr     r0, #3
                        cmp     hid_lp_s32, r0 wc
                        
                        ' Now the carry flag holds a filtered version of the post-FSK-detection
                        ' signal. These are manchester-encoded bits. We need to detect the header,
                        ' validate the manchester encoding, and extract the actual data bits
                        ' from the received signal. This is a typical data/clock recovery problem.
                        ' We could use a PLL, but when doing this in software it's easier and less
                        ' error-prone to just brute-force it, and try to detect a signal at all
                        ' possible phases.
                        '
                        ' At this point there are 100 possible phases, and we need enough room to
                        ' store 51 bits. (45 data bits + 3 bits each from 2 headers) We'll use a
                        ' 64-bit shift register for each phase.
                        '
                        ' To cut down on the amount of memory required, we'll decimate the signal
                        ' by processing on only every other carrier cycle. This means we only have
                        ' 50 possible phases.

                        test    pulse_count, #1 wz      ' Decimate x2 (Opposite phase from the EM4102 decoder)
              if_z      jmp     #rx_hid_ret

:shift1                 rcl     hid_bits+1, #1 wc       ' Shift in the new filtered bit
:shift2                 rcl     hid_bits+0, #1
:load1                  mov     hid_shift+0, hid_bits+0 ' And save a copy in a static location
:load2                  mov     hid_shift+1, hid_bits+1

                        add     :shift1, dest_2         ' Increment hid_bits pointers
                        add     :shift2, dest_2
                        add     :load1, #2
                        add     :load2, #2
                        cmp     :shift1, hid_shift1_end wz
              if_z      sub     :shift1, dest_100       ' Wrap around     
              if_z      sub     :shift2, dest_100
              if_z      sub     :load1, #100     
              if_z      sub     :load2, #100     

                        ' Also save a 180-degree phase shifted version of hid_shift,
                        ' for doing manchester validation and header detection.
                        ' The bits in hid_shiftp are delayed by 1/2 bit relative to
                        ' hid_shift.                        

:load3                  mov     hid_shiftp+0, hid_bits+0+50
:load4                  mov     hid_shiftp+1, hid_bits+1+50
                        add     :load3, #2
                        add     :load4, #2
                        cmp     :load3, hid_load3_end wz
              if_z      sub     :load3, #100     
              if_z      sub     :load4, #100

                        or      dira, #|<7
                        test    hid_shift+1, #1 wz
                        muxz    outa, #|<7

                        rdlong  r0, format_ptr wz       ' Make sure the output buffer is available
              if_nz     jmp     #rx_hid_ret

                        '
                        ' Ok. At this point, we can validate the contents of hid_shift and
                        ' hid_shiftp, and extract the packet data if possible. hid_shiftp
                        ' has our actual packet data, and em_shift has the complement of that
                        ' data. The header sequence of "000111" turns into "001" in hid_shift
                        ' and "011" in hid_shiftp.
                        '
                        ' To provide further validation, we look for both the current packet's
                        ' header and the next packet's header. So, the data bits are actually
                        ' in bits 3 through 47.
                        '

                        mov     r0, hid_shiftp+0        ' Check 1st header at -180°
                        and     r0, hid_s_mask
                        cmp     r0, hid_sp_check wz
              if_z      mov     r0, hid_shift+0         ' Check 1st header at 0°
              if_z      and     r0, hid_s_mask
              if_z      cmp     r0, hid_s_check wz

              if_z      mov     r0, hid_shiftp+1        ' Check 2nd header at -180°
              if_z      and     r0, #%111
              if_z      cmp     r0, #%001 wz
              if_z      mov     r0, hid_shift+1         ' Check 2nd header at 0°
              if_z      and     r0, #%111
              if_z      cmp     r0, #%011 wz

              if_z      mov     r0, hid_shiftp+0        ' Check Manchester encoding
              if_z      xor     r0, hid_shift+0         '   (high long)
              if_z      xor     r0, hFFFFFFFF
              if_z      and     r0, hid_data_mask wz              
              if_z      mov     r0, hid_shiftp+1        '   (low long)
              if_z      xor     r0, hid_shift+1
              if_z      xor     r0, hFFFFFFFF
              if_z      andn    r0, #%111 wz

              if_nz     jmp     #rx_hid_ret             ' Exit on any decoding error

                        ' Hooray, we have a correct-looking packet. Justify and
                        ' trim the 45 bits of data, and store it.
                        

                        mov     r0, hid_shiftp+0                         
                        mov     r1, hid_shiftp+1

                        and     r0, hid_data_mask       ' Cut off 1st header and unused bits   

                        shr     r0, #1 wc               ' Cut off 2nd header
                        rcr     r1, #1 wc                         
                        shr     r0, #1 wc
                        rcr     r1, #1 wc                         
                        shr     r0, #1 wc
                        rcr     r1, #1 wc

                        mov     r2, hid_buf_ptr         ' Write the result
                        wrlong  r0, r2
                        add     r2, #4
                        wrlong  r1, r2
                        wrlong  c_format_hid, format_ptr
                        
rx_hid_ret              ret

hid_shift1_end          rcl     hid_bits+1+100, #1 wc   ' This is ":shift1" above, after we've
                                                        ' passed the end of the hid_bits array.
                        
hid_load3_end           mov     hid_shiftP+0, hid_bits+0+100


'------------------------------------------------------------------------------
' Initialized Data
'------------------------------------------------------------------------------

hFFFFFFFF     long      $FFFFFFFF
h80000000     long      $80000000
h7FFF0000     long      $7FFF0000
hFFFF         long      $FFFF
zero          long      0
c_format_em   long      FORMAT_EM4102
c_format_hid  long      FORMAT_HID

dest_2        long      2 << 9
dest_100      long      100 << 9
dest_64       long      64 << 9
input_mask2   long      |<INPUT_PIN

init_dira     long      |<CARRIER_POS_PIN | |<CARRIER_NEG_PIN | |<THRESHOLD_PIN | |<DEBUG_PIN | |<SHIELD1_PIN | |<SHIELD2_PIN
init_frqa     long      0
init_frqb     long      DEFAULT_THRESHOLD
init_ctra     long      (%00101 << 26) | (CARRIER_POS_PIN << 9) | CARRIER_NEG_PIN 
init_ctrb     long      (%00110 << 26) | THRESHOLD_PIN

carrier_mask  long      |<CARRIER_RATE - 1
carrier_bit0  long      |<(CARRIER_RATE + 0)
carrier_bit1  long      |<(CARRIER_RATE + 1)

pulse_target  long      0
next_hyst     long      0
hyst_constant long      0

baseband_reg  long      0,0,0,0,0,0,0,0
baseband_s32  long      32      ' Number of zeroes in last 32 baseband bits
baseband_s256 long      256     ' Number of zeroes in last 256 baseband bits

em_buf_ptr    long      0
hid_buf_ptr   long      0
format_ptr    long      0
pulse_ptr2    long      0

hid_header    long      |<16 - 1
hid_max_bits  long      512
hid_fsk_mask  long      |<9

hid_lp_reg    long      0,0,0,0,0,0,0,0
hid_lp_s32    long      32      ' Number of zeroes in last 32 FSK bits
hid_lp_s256   long      256     ' Number of zeroes in last 256 FSK bits

hid_s_mask    long      %111 << (3 + 45 - 32)
hid_s_check   long      %011 << (3 + 45 - 32)
hid_sp_check  long      %001 << (3 + 45 - 32)
hid_data_mask long      (1 << (3 + 45 - 32)) - 1


'------------------------------------------------------------------------------
' Uninitialized Data
'------------------------------------------------------------------------------

r0            res       1
r1            res       1
r2            res       1

cur_pulse     res       1
prev_pulse    res       1
pulse_len     res       1

pulse_count   res       1
prev_frqb     res       1

hid_bit_len   res       1
hid_bit_count res       1
hid_fsk_reg   res       1
hid_reg       res       1

em_bits       res       64      ' 64-bit shift register for each of the 32 phases
em_shift      res       2       ' Just the current shift register
em_decoded    res       2       ' Decoded 40-bit EM4102 packet       
em_parity     res       1       ' Column parity accumulator

hid_bits      res       100     ' 64-bit shift register for each of the 50 phases
hid_shift     res       2       ' Just the current shift register
hid_shiftp    res       2       ' 180 degrees out of phase with hid_shift
hid_decoded   res       2       ' Decoded 45-bit HID packet

                        fit