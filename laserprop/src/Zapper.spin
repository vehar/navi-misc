{{

Zapper
------

This object implements an interface to a (modified) Zapper light gun.
The modified light gun has been equipped with a TSL230R light-to-frequency
converter chip. We sample the waveform from the sensor, and correlate it
with laser position data obtained from the OpticalProximity cogs.

References:
   http://ca.geocities.com/jefftranter@rogers.com/xgs/zapper.html
   http://www.zero-soft.com/HW/USB_ZAPPER/
   http://yikescode.blogspot.com/2007/12/nes-controller-arduino.html

Pin layout:   
                  ┌────┐
   (BLK) 1. Data  │•  •│ 4. Trigger (YEL)
   (ORG) 2. Latch │•  •│ 5. Light Sensor (GRN)
   (RED) 3. Clock │•  •│    +5V (WHT)
   (BLU)    Gnd   │• ┌─┘
                  └──┘

  All pins have a 10K pull-up resistor to 5V.

Zapper mod:

  I originally wrote this module for use with an unmodified Zapper
  controller. It was possible to trigger the Zapper using the laser
  projector, but sensitivity left much to be desired. The light sensor
  in the Zapper insn't particularly sensitive, but more importantly it
  has a band-pass filter on it which makes it even harder to trigger
  using laser light that's pulsed at frequencies we can produce.

  The mod works by installing a TSL230R light-to-frequency converter
  in front of the original light sensor board. The only modifications
  necessary to the original circuit board are to cut one trace and
  add three wires.

  First, prep the TSL230R with a series resistor (to limit output
  slew rate and current) and a decoupling capacitor. I did mine
  "dead bug" style, with the capacitor and resistor soldered
  directly onto the back of the chip after folding the leads in.
  
         +5v
           ┬
           │
           ┣──┳──────────────┐                                                    
           │  │ TSL230R      │               
           │  │ ┌──────────┐ │               
    0.1 µF ┣─┤ S0    S3 ├─┼─┐                      
           │  └─┤ S1    S2 ├─┫ │  100 Ω                    
           │  ┌─┤ /OE  OUT ├─┼─┼────── OUT                  
           │  ┣─┤ GND  Vdd ├─┘ │                     
           │  │ └──────────┘   │                         
           ┣──┻────────────────┘           
           

  Solder three wires onto the TSL230R assembly, for GND, +5v, and
  OUT. I used 30 gauge wire-wrap wire.

  Hot glue the TSL230R just behind the rearmost circular partition
  in the Zapper barrel. The chip in the TSL230R should be centered
  in the hole in this partition.

  Now attach it to the original Zapper circuit board. This is a
  bottom view of the board, with a corresponding portion of the
  schematic:

   ┌────────── ─ ─       +5v
   │        BC            ┬             
   │  •     •••           │          
   │ A•                    100 Ω       ┌─ A 
   │  •   •••  •D      D ┫          C ┫ 
   │  •    ••            10 µF        
   │                   B ┫             │
   └────────── ─ ─                     

  This shows the original Zapper's power supply filter, and its
  open-collector drive transistor for the original light detector
  output. To attach the new TSL230R assembly:

    - Cut the trace between point A and C
    - Connect the TSL230 ground to point D
    - Connect TSL230 power to point D (utilizing the original power filter)
    - Connect the TSL230 output (with 100 Ω resistor) to point A

  That's it! Test the sensor with an oscilloscope, then reassemble. The
  modified Zapper should not damage an NES, but it won't work correctly
  of course. If you want to undo the mod, just unglue (pull off) the TSL230R
  itself, unsolder its wires from the Zapper board, and run a jumper wire
  between points D and A.

┌───────────────────────────────────┐
│ Copyright (c) 2008 Micah Dowty    │               
│ See end of file for terms of use. │                                                                             
└───────────────────────────────────┘

}}

CON
  
  ' Public cog data
  PARAM_TRIGGER_CNT   = 0   'OUT,    Counts trigger presses
  PARAM_PULSE_CNT     = 1   'OUT,    Light sensor pulse counter
  PARAM_PULSE_X       = 2   'OUT,    Last light sensor X position (0 if below min S/N)
  PARAM_PULSE_Y       = 3   'OUT,    Last light sensor Y position (0 if below min S/N)
  PARAM_CAL_MS        = 4   'IN,     Calibrate every CAL_MS milliseconds
  PARAM_MIN_SN        = 5   'IN,     Minimum signal to noise ratio (1.31 fixed point)
  PARAM_THRESH_PULSE  = 6   'IN,     Relative pulse threshold (1.31 fixed point)
  PARAM_THRESH_HYST   = 7   'IN,     Relative hysteresis threshold (1.31 fixed point)
  PARAM_PULSE_LEN     = 8   'OUT,    Last detected pulse length, in clock ticks
  NUM_PARAMS          = 9

  ' Private cog data
  COG_LIGHT_MASK      = 10  'CONST, base pin number
  COG_PROX_X          = 11  'CONST, X OpticalProximity params
  COG_PROX_Y          = 12  'CONST, Y OpticalProximity params
  COG_LIGHT_MIN       = 13  'IN/OUT, Minimum detected light sensor period
  COG_LIGHT_MAX       = 14  'IN/OUT, Maximum detected light sensor period
  COG_THRESH_PULSE    = 15  'IN,     Light sensor pulse threshold
  COG_THRESH_HYST     = 16  'IN,     Light sensor hysteresis threshold
                         
  COG_DATA_SIZE       = 17  '(Must be last)

  ' Constants from OpticalProximity
  PROX_PARAM_OUTPUT   = 0
  PROX_PARAM_FILTER   = 1
  PROX_PARAM_ACCUM    = 2
  PROX_PARAM_COUNT    = 3
  PROX_SCALE_BITS     = 11
                      
VAR
  long cog
  long cogdata[COG_DATA_SIZE]

  long trigger_pin
  long trigger_debounce

  long millisec_cnt
  long calibration_deadline
  
PUB start(lightPin, triggerPin, xProx, yProx) : okay
  '' Initialize the controller port, and start its driver cog.
  '' The controller must be connected to five pins, starting with 'basePin'.
  '' xProx and yProx are the parameter blocks for the X and Y OpticalProximity sensors.

  ' Initialize cog parameters
  longfill(@cogdata, 0, COG_DATA_SIZE)
  cogdata[COG_PROX_X] := xProx
  cogdata[COG_PROX_Y] := yProx

  ' Defaults
  cogdata[PARAM_CAL_MS]       := 170
  cogdata[PARAM_MIN_SN]       := 537_000_000
  cogdata[PARAM_THRESH_PULSE] := 1_954_000_000
  cogdata[PARAM_THRESH_HYST]  := 1_823_000_000
  
  trigger_pin := triggerPin
  trigger_debounce := $FFFFFFFF

  if lightPin => 0
    cogdata[COG_LIGHT_MASK] := |< lightPin

  ' Start calibrating after a short delay
  millisec_cnt := clkfreq / 1000
  calibration_deadline := cnt
    
  okay := (cog := cognew(@entry, @cogdata)) + 1
     
PUB stop
  if cog > 0
    cogstop(cog)
  cog := -1

PUB getParams : addr
  '' Get the address of our parameter block (public PARAM_* values)

  addr := @cogdata

PUB poll
  '' Poll the light gun trigger, and perform low-frequency calibration tasks.
  '' This is called about every 10ms, from the supervisor cog.

  ' Poll the trigger about every 10ms
  pollTrigger

  ' Adjust the sensor calibration CAL_HZ times per second.
  if (cnt - calibration_deadline) > 0
    calibration_deadline += millisec_cnt * cogdata[PARAM_CAL_MS]
    calibrate

    ' If we're too far behind, catch up.
    if (cnt - calibration_deadline) > 0
      calibration_deadline := cnt + millisec_cnt * cogdata[PARAM_CAL_MS]

PRI pollTrigger  
  ' Debounce the trigger using a shift register
  trigger_debounce := (trigger_debounce << 1) | ina[trigger_pin]

  if (trigger_debounce & %11111111) == %11111000
    cogdata[PARAM_TRIGGER_CNT]++

PRI calibrate | minP, maxP, diffP

  ' Sample the min and max light frequency periods
  minP := cogdata[COG_LIGHT_MIN]
  cogdata[COG_LIGHT_MIN] := $7FFFFFFF
  maxP := cogdata[COG_LIGHT_MAX]
  cogdata[COG_LIGHT_MAX]~
  diffP := maxP - minP
  
  ' Did we get any samples?
  if diffP < 0
    return

  ' Our signal to noise ratio is (max - min) / max.
  ' If we're under our minimum S/N setting, set the
  ' thresholds to impossible values in order to disable
  ' pulse detection.

  if diffP < (cogdata[PARAM_MIN_SN] ** (maxP << 1))
    ' Disable pulse detection
    cogdata[COG_THRESH_PULSE]~
    cogdata[COG_THRESH_HYST] := $7FFFFFFF
    cogdata[PARAM_PULSE_X]~
    cogdata[PARAM_PULSE_Y]~
  
  else 
    ' Detect a pulse when we're 30% brighter than MAX,
    ' and end the pulse when we're 20% brighter.
    cogdata[COG_THRESH_PULSE] := maxP - (cogdata[PARAM_THRESH_PULSE] ** (diffP << 1))
    cogdata[COG_THRESH_HYST]  := maxP - (cogdata[PARAM_THRESH_HYST] ** (diffP << 1))

  
DAT

'==============================================================================
' Driver Cog
'==============================================================================

                        org

                        '======================================================
                        ' Initialization
                        '======================================================

entry                   mov     t1, par                 ' Read pin masks
                        add     t1, #4*COG_LIGHT_MASK
                        rdlong  light_mask, t1

                        mov     t1, par                 ' Read X/Y OpricalProximity addresses
                        add     t1, #4*COG_PROX_X
                        rdlong  addr_prox_x_accum, t1
                        add     addr_prox_x_accum, #4*PROX_PARAM_ACCUM                                

                        mov     t1, par
                        add     t1, #4*COG_PROX_Y
                        rdlong  addr_prox_y_accum, t1
                        add     addr_prox_y_accum, #4*PROX_PARAM_ACCUM                      


                        '======================================================
                        ' Main polling loop
                        '======================================================

pollLoop
                        ' Wait for the pulse threshold to be reached
                        call    #takeSample
                        mov     t1, par
                        add     t1, #4*COG_THRESH_PULSE
                        rdlong  t2, t1
                        cmp     t2, light_period wc     ' Jump if light_period > thresh_pulse
              if_c      jmp     #pollLoop

                        ' Save the accumulator values from the beginning of
                        ' the previous cycle. This marks the beginning of our pulse.
                        mov     prox_x_pulse_accum, prox_x_last_accum
                        mov     prox_x_pulse_count, prox_x_last_count
                        mov     prox_y_pulse_accum, prox_y_last_accum
                        mov     prox_y_pulse_count, prox_y_last_count

                        ' Time the pulses, so the host can keep track of how good we're doing
                        ' at pulse detection.
                        mov     pulse_begin_cnt, cnt
                        
                        ' Now wait for the pulse to end, with hysteresis.
:hystLoop               call    #takeSample
                        mov     t1, par
                        add     t1, #4*COG_THRESH_HYST
                        rdlong  t2, t1
                        cmp     t2, light_period wc     ' Jump if light_period <= thresh_hyst
              if_nc     jmp     #:hystLoop

                        ' Measure the pulse length, and write that to the PARAMS block.
                        mov     t2, cnt
                        sub     t2, pulse_begin_cnt
                        mov     t1, par
                        add     t1, #4*PARAM_PULSE_LEN
                        wrlong  t2, t1
              
                        ' Now the difference between the 'this' and 'pulse' accumulators
                        ' represent the integral of the laser position over the duration
                        ' of this entire pulse. We can use this to calculate the average.

                        ' First, take the difference: pulse = (this - pulse)

                        mov     t1, prox_x_this_accum
                        sub     t1, prox_x_pulse_accum
                        mov     prox_x_pulse_accum, t1

                        mov     t1, prox_x_this_count
                        sub     t1, prox_x_pulse_count
                        mov     prox_x_pulse_count, t1

                        mov     t1, prox_y_this_accum
                        sub     t1, prox_y_pulse_accum
                        mov     prox_y_pulse_accum, t1

                        mov     t1, prox_y_this_count
                        sub     t1, prox_y_pulse_count
                        mov     prox_y_pulse_count, t1

                        ' For each axis, calculate:
                        '   position = (accum << PROX_SCALE_BITS) / count
                        '
                        ' When shifting left, convert from 32 bits to 64
                        ' bits, then do a 64-bit division. The result is
                        ' stored in hub memory.

                        mov     divA_low, prox_x_pulse_accum
                        mov     divA_high, prox_x_pulse_accum
                        mov     divB_low, prox_x_pulse_count
                        shl     divA_low, #PROX_SCALE_BITS
                        shr     divA_high, #(32 - PROX_SCALE_BITS)
                        mov     divB_high, #0
                        call    #div64
                        mov     t1, par
                        add     t1, #4*PARAM_PULSE_X
                        wrlong  div_out, t1

                        mov     divA_low, prox_y_pulse_accum
                        mov     divA_high, prox_y_pulse_accum
                        mov     divB_low, prox_y_pulse_count
                        shl     divA_low, #PROX_SCALE_BITS
                        shr     divA_high, #(32 - PROX_SCALE_BITS)
                        mov     divB_high, #0
                        call    #div64
                        mov     t1, par
                        add     t1, #4*PARAM_PULSE_Y
                        wrlong  div_out, t1

                        ' Increment pulse counter

                        add     light_cnt, #1
                        mov     t1, par
                        add     t1, #4*PARAM_PULSE_CNT
                        wrlong  light_cnt, t1

                        jmp     #pollLoop


                        '======================================================
                        ' Light sensor sampler
                        '======================================================

                        ' This takes individual light sensor samples (half-periods),
                        ' times them, and synchronously samples the OpticalProximity
                        ' accumulators for the X and Y axes.
                        '
                        ' Writes results to the PARAM block (LIGHT, MIN, MAX),
                        ' and updates the prox_* and light_period variables.

takeSample
                        ' Rotate OpticalProximity samples..
                        mov     prox_x_last_accum, prox_x_this_accum
                        mov     prox_x_last_count, prox_x_this_count
                        mov     prox_y_last_accum, prox_y_this_accum
                        mov     prox_y_last_count, prox_y_this_count

                        ' Wait for the next edge, alternating
                        ' positive and negative edges.
                        waitpeq lightMask_toggle, light_mask                        
                        xor     lightMask_toggle, light_mask
                        mov     last_cnt, this_cnt
                        mov     this_cnt, cnt

                        ' Sample X and Y axis OpticalProximity accumulators
                        rdlong  prox_x_this_accum, addr_prox_x_accum
                        rdlong  prox_y_this_accum, addr_prox_y_accum
                        mov     t1, addr_prox_x_accum
                        add     t1, #4*(PROX_PARAM_COUNT - PROX_PARAM_ACCUM)
                        rdlong  prox_x_this_count, t1
                        mov     t1, addr_prox_y_accum
                        add     t1, #4*(PROX_PARAM_COUNT - PROX_PARAM_ACCUM)
                        rdlong  prox_y_this_count, t1
                        
                        ' Calculate light sensor period
                        mov     light_period, this_cnt
                        sub     light_period, last_cnt

                        ' Update max.
                        mov     t1, par
                        add     t1, #4*COG_LIGHT_MAX
                        rdlong  t2, t1
                        min     t2, light_period
                        wrlong  t2, t1  

                        ' Update min.
                        mov     t1, par
                        add     t1, #4*COG_LIGHT_MIN
                        rdlong  t2, t1
                        max     t2, light_period
                        wrlong  t2, t1  

takeSample_ret          ret


                        '======================================================
                        ' Math routines
                        '======================================================

                        ' 64-bit by 64-bit division, with a 32-bit result.
                        '
                        ' div_out = divA_high:divA_low / divB_high:divB_low

div64                   mov     div_out, #0
                        mov     divT_low, #0
                        mov     divT_high, #0
                        mov     t1, #64                        ' Loop over 64 bits
                        
:div_loop               shl     divA_low, #1 wc                ' Rotate a bit from divA into divT
                        rcl     divA_high, #1 wc
                        rcl     divT_low, #1 wc
                        rcl     divT_high, #1 wc

                        shl     div_out, #1

                        cmp     divT_high, divB_high wc,wz     ' 64-bit comparison                 
              if_z      cmp     divT_low, divB_low wc
              if_c      jmp     #:next_bit                     ' Skip if divB > divA

                        add     div_out, #1                    ' Increment output if divB fits in divA
                        sub     divT_low, divB_low wc          ' 64-bit subtract
                        subx    divT_high, divB_high

:next_bit               djnz    t1, #:div_loop
div64_ret               ret


'------------------------------------------------------------------------------
' Initialized Data
'------------------------------------------------------------------------------

lightMask_toggle        long    0
light_mask              long    1
light_cnt               long    0
                   
'------------------------------------------------------------------------------
' Uninitialized Data
'------------------------------------------------------------------------------

t1                      res     1
t2                      res     1

divA_high               res     1
divA_low                res     1
divB_high               res     1
divB_low                res     1
divT_high               res     1
divT_low                res     1
div_out                 res     1

this_cnt                res     1
last_cnt                res     1
light_period            res     1
pulse_begin_cnt         res     1

addr_prox_x_accum       res     1
addr_prox_y_accum       res     1

prox_x_this_accum       res     1
prox_x_this_count       res     1
prox_x_last_accum       res     1
prox_x_last_count       res     1
prox_x_pulse_accum      res     1
prox_x_pulse_count      res     1
prox_y_this_accum       res     1
prox_y_this_count       res     1
prox_y_last_accum       res     1
prox_y_last_count       res     1
prox_y_pulse_accum      res     1
prox_y_pulse_count      res     1

                        fit

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