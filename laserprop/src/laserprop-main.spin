 {{

laserprop-main
--------------

Main program for the Propeller-based laser projector.
     
┌───────────────────────────────────┐
│ Copyright (c) 2008 Micah Dowty    │               
│ See end of file for terms of use. │
└───────────────────────────────────┘

}}

OBJ
  prox[2]  : "OpticalProximity"
  vcm[2]   : "VoiceCoilServo"
  ow       : "SpinOneWire"
  vm       : "VectorMachine"
  bt       : "BluetoothConduit"
  nes      : "NES"
  zapper   : "Zapper"
  
CON
  _clkmode = xtal1 + pll16x
  _xinfreq = 5_000_000

  VECTOR_MEM_SIZE = 1024 * 5

  X_ONEWIRE_PIN = 1                                     ' Pins for axis actuators
  X_FREQ_PIN = 4
  X_LED_PIN = 5
  X_DIR_PIN = 6
  X_PWM_PIN = 8

  Y_ONEWIRE_PIN = 15
  Y_FREQ_PIN = 16
  Y_LED_PIN = 17
  Y_DIR_PIN = 2
  Y_PWM_PIN = 0

  LASER_PIN = 9                                         ' Laser modulation
  
  BT_CTS_I_PIN = 23                                     ' Spark Fun bluetooth module
  BT_TX_O_PIN = 22
  BT_RX_I_PIN = 21
  BT_RTS_O_PIN = 20

  NES_DATA_PIN    = 10                                  ' NES controller pins
  NES_LATCH_PIN   = 11
  NES_CLOCK_PIN   = 12
  NES_TRIGGER_PIN = 13
  NES_LIGHT_PIN   = 14
  
  PANIC_BLUETOOTH_ERROR = 1                             ' Panic beep codes
  PANIC_CALIBRATION_X_ERROR = 2
  PANIC_CALIBRATION_Y_ERROR = 3
  PANIC_THERM_SENSOR_ERROR = 4
  PANIC_THERM_OVERHEAT = 5
  PANIC_PROX_LIMITS = 6
  PANIC_PROX_OSCILLATE = 7
  PANIC_SERVO_ACCUMULATOR = 8

  CAL_X_MIN = 0                                         ' Calibration data offsets
  CAL_Y_MIN = 1
  CAL_X_MAX = 2
  CAL_Y_MAX = 3
  CAL_DATA_SIZE = 4

  CAL_PARAM_P = 15_000_000                              ' Parameters that affect calibration
  CAL_TARGET_MAX = 1_000_000
  CAL_WAIT_MS = 600
  CAL_MINIMUM_DIFF = 100_000

  DEFAULT_SCALE_SHIFT = 16                              ' Servo parameter defaults
  UNSTICK_P_SCALE = 400_000_000
  DEFAULT_P_SCALE = 100_000_000
  INIT_RAMP_MS = 300                                    ' How quickly to ramp up defaults

  PROX_LIMITS_PERCENT = 5                               ' How far outside of calibration limits can we be?
  PROX_OSCILLATE_PERCENT = 85                           ' How much of the prox range can we occupy during one sample?
  PROX_LIMIT_ERROR_MAX = 2                              ' Max number of errors to ignore before panic'ing.
  PROX_OSCILLATE_ERROR_MAX = 2
  
  THERM_POLL_MS = 10                                    ' Temperature sensor parameters
  THERM_POLL_COUNT = 75
  THERM_LIMIT_FAHRENHEIT = 110                          ' Thermal shutdown limit, in °F
  THERM_LIMIT = (THERM_LIMIT_FAHRENHEIT - 32) * 9

VAR
  long  vector_mem[VECTOR_MEM_SIZE]
  long  therm_data[2]
  long  cal_data[CAL_DATA_SIZE]
  byte  limit_err_counter[2]
  byte  oscillate_err_counter[2]
  long  nes_buttons
  
DAT

vector_desc   long      VECTOR_MEM_SIZE
              byte      "vector_mem", 0      

vm_desc       long      vm#NUM_PARAMS
              byte      "vm", 0

prox_x_desc   long      prox#NUM_PARAMS
              byte      "prox_x", 0

prox_y_desc   long      prox#NUM_PARAMS
              byte      "prox_y", 0

vcm_x_desc    long      vcm#NUM_PARAMS
              byte      "vcm_x", 0                 

vcm_y_desc    long      vcm#NUM_PARAMS
              byte      "vcm_y", 0                 
              
therm_desc    long      2
              byte      "therm", 0

cal_desc      long      CAL_DATA_SIZE
              byte      "cal", 0

nes_desc      long      1
              byte      "nes", 0

zapper_desc   long      zapper#NUM_PARAMS
              byte      "zapper", 0
              
              
PUB main | okay, syncTime

  okay := bt.start(BT_RTS_O_PIN, BT_RX_I_PIN, BT_TX_O_PIN, BT_CTS_I_PIN, string("Test Device"))
  if not okay
    panic(PANIC_BLUETOOTH_ERROR)

  bt.defineRegion(@vector_desc, @vector_mem)
  bt.defineRegion(@vm_desc, vm.getParams)
  bt.defineRegion(@prox_x_desc, prox[0].getParams)
  bt.defineRegion(@prox_y_desc, prox[1].getParams)
  bt.defineRegion(@vcm_x_desc, vcm[0].getParams)
  bt.defineRegion(@vcm_y_desc, vcm[1].getParams)
  bt.defineRegion(@therm_desc, @therm_data)
  bt.defineRegion(@cal_desc, @cal_data)
  bt.defineRegion(@nes_desc, @nes_buttons)
  bt.defineRegion(@zapper_desc, zapper.getParams)
  
  ' Start the proximity sensors. Do this first, since
  ' they will wait to receive their first reading.

  prox[0].start(X_FREQ_PIN, X_LED_PIN)
  prox[1].start(Y_FREQ_PIN, Y_LED_PIN)

  ' Start the NES controller interface. The light gun and controller
  ' parts of the interface are implemented by separate objects:
  ' controller polling happens periodically from the supervisor cog,
  ' light gun polling gets a dedicated cog.

  nes.Init(NES_LATCH_PIN, NES_DATA_PIN, NES_CLOCK_PIN)
  zapper.start(NES_LIGHT_PIN, NES_TRIGGER_PIN, prox[0].getParams, prox[1].getParams)
  
  ' Synchronize the startup of both servo axes and the VectorMachine.
  syncTime := cnt + clkfreq / 100

  vm.start(syncTime, vcm#LOOP_HZ, LASER_PIN, @vector_mem, VECTOR_MEM_SIZE)
  vcm[0].start(X_DIR_PIN, X_PWM_PIN, syncTime, prox[0].getParams, vm.getOutputAddress(0))
  vcm[1].start(Y_DIR_PIN, Y_PWM_PIN, syncTime, prox[1].getParams, vm.getOutputAddress(1))

  vcmCalibrate
  vcmSetDefaults
  supervisorLoop


PRI delayMS(millisec)
  waitcnt(clkfreq / 1000 * millisec + cnt)


PRI readTemperature(pin) : temp
  '' Read the temperature from a DS18B20 sensor.
  '' Blocks while the conversion is taking place. Returns
  '' a floating point value, in Celsius.
  ''
  '' Assumes there is only one device on the bus.
  ''
  '' In the event of a timeout, panics.
  ''
  '' Other side-effects: Polls the NES controller port.

  ow.start(pin)

  ow.reset
  ow.writeByte(ow#SKIP_ROM)
  ow.writeByte(ow#CONVERT_T)

  ' Wait for the conversion
  repeat THERM_POLL_COUNT

    ' Take this opportunity to poll the NES controller
    nes_buttons := nes.buttons
    zapper.poll
  
    delayMS(THERM_POLL_MS)
    if ow.readBits(1)
      ' Have a reading!

      ow.reset
      ow.writeByte(ow#SKIP_ROM)
      ow.writeByte(ow#READ_SCRATCHPAD)
      temp := ow.readByte + ow.readByte << 8
      if temp == 0
        quit

      ' Success.
      return

  ' Timed out, or got a bad reading.
  panic(PANIC_THERM_SENSOR_ERROR)


PUB supervisorLoop
  '' Perform continuous supervisory / safety tasks:
  ''  - Poll both temperature sensors
  ''  - Panic if we're running too hot
  ''  - Run the servo supervisor between each temperature
  ''    reading (at least once a second)
  ''  - Poll the NES controller (while waiting for the
  ''    temperature sensors)
  ''
  '' Never returns.

  prox[0].resetMinMax
  prox[1].resetMinMax
  
  repeat
    therm_data[0] := readTemperature(X_ONEWIRE_PIN)
    if therm_data[0] > THERM_LIMIT
      panic(PANIC_THERM_OVERHEAT) 

    superviseServos
      
    therm_data[1] := readTemperature(Y_ONEWIRE_PIN)
    if therm_data[1] > THERM_LIMIT
      panic(PANIC_THERM_OVERHEAT) 

    superviseServos


PUB superviseServos | axis, minReading, maxReading, calMin, calMax, calRange, padding
  '' Supervise both servo axes.
  ''
  ''  - Check for out-of-range prox readings
  ''  - Check for servo oscillation
  ''  - Check for very long accumulated servo pulses
  ''
  '' Panics on error.

  repeat axis from 0 to 1
    ' Check the servo accumulator
    if vcm[axis].getAccumulatorMagnitude => clkfreq
      panic(PANIC_SERVO_ACCUMULATOR)
  
    ' Sample the proximity sensor's max/min limits for this period
    minReading := prox[axis].readMin
    maxReading := prox[axis].readMax
    prox[axis].resetMinMax

    ' Come up with reasonable limits based on the calibration data
    calMin := cal_data[CAL_X_MIN + axis] 
    calMax := cal_data[CAL_X_MAX + axis]
    calRange := calMax - calMin

    ' Outside of limits?
    padding := calRange * PROX_LIMITS_PERCENT / 100
    if minReading < (calMin - padding) or maxReading > (calMax + padding)
      limit_err_counter[axis]++
      if limit_err_counter[axis] > PROX_LIMIT_ERROR_MAX
        panic(PANIC_PROX_LIMITS)
    else
      limit_err_counter[axis]~

    ' Oscillating too much?
    if (maxReading - minReading) > (calRange * PROX_OSCILLATE_PERCENT / 100) 
      oscillate_err_counter[axis]++
      if oscillate_err_counter[axis] > PROX_OSCILLATE_ERROR_MAX
        panic(PANIC_PROX_OSCILLATE)
    else
      oscillate_err_counter[axis]~


PUB vcmCalibrate | axis                 
  '' Perform power-on calibration for both VCM axes.
  '' We gently sweep the axes through their whole range of
  '' motion, and record the minimum and maximum OpticalProximity readings.

  repeat axis from 0 to 1
    ' Initialize
    prox[axis].resetMinMax
    vcm[axis].zeroParams

    ' Ramp up gain as we aim for the minimum value
    vcm[axis].rampParam(vcm#PARAM_P, CAL_PARAM_P, CAL_WAIT_MS)

    ' Now climb up to the max value
    vcm[axis].rampParam(vcm#PARAM_CENTER, CAL_TARGET_MAX, CAL_WAIT_MS)

    ' Shut down
    vcm[axis].zeroParams

    ' Collect the results...
    cal_data[CAL_X_MIN + axis] := prox[axis].readMin
    cal_data[CAL_X_MAX + axis] := prox[axis].readMax

  ' Sanity check. This will detect many mechanical and electrical
  ' failures, and report a per-axis panic code.

  if (cal_data[CAL_X_MAX] - cal_data[CAL_X_MIN]) < CAL_MINIMUM_DIFF
    panic(PANIC_CALIBRATION_X_ERROR)
  if (cal_data[CAL_Y_MAX] - cal_data[CAL_Y_MIN]) < CAL_MINIMUM_DIFF
    panic(PANIC_CALIBRATION_Y_ERROR)


PUB vcmSetDefaults | axis, center, range, scale 
  '' Using data from power-on calibration, set reasonable
  '' defaults for VCM servo parameters.    

  repeat axis from 0 to 1
    ' Center it right in the middle of the calibration range
    center := (cal_data[CAL_X_MIN + axis] >> 1) + (cal_data[CAL_X_MAX + axis] >> 1)

    ' Scale is proprortional to the size of the calibrated range
    range := cal_data[CAL_X_MAX + axis] - cal_data[CAL_X_MIN + axis]
    scale := range >> DEFAULT_SCALE_SHIFT                      

    vcm[axis].setParam(vcm#PARAM_CENTER, center)
    vcm[axis].setParam(vcm#PARAM_SCALE, scale)

    ' "Unstick" the VCM- it takes extra force to get it away from the edges.
    ' Note that all servo parameters are *inversely* proportional to the
    ' calibrated range. If the same mechanical motion produces larger changes
    ' in prox reading, we need to compensate by using smaller gains.

    vcm[axis].rampParam(vcm#PARAM_P, UNSTICK_P_SCALE / scale, INIT_RAMP_MS)

    ' Back down to a lower gain
    vcm[axis].rampParam(vcm#PARAM_P, DEFAULT_P_SCALE / scale, INIT_RAMP_MS)
  

PUB panic(beeps) | timer, period
  '' Report a fatal error, with the specified number of beeps.
  '' Stop both servos and the VM (turn off the laser), and start beeping.
  ''
  '' Never returns.

  ' Shut off the laser and servos, but leave Bluetooth and the sensors
  ' running. This means we can calibrate the optical sensors even in
  ' panic mode.
  vm.stop
  vcm[0].stop
  vcm[1].stop

  ' Negative direction- vibrate the VCMs up against their end stop.  
  outa[X_DIR_PIN]~
  outa[Y_DIR_PIN]~

  dira[X_DIR_PIN]~~
  dira[Y_DIR_PIN]~~
  dira[X_PWM_PIN]~~
  dira[Y_PWM_PIN]~~

  timer := cnt
  period := clkfreq / 440

  repeat

    ' Sequence of beeps
    repeat beeps
  
      ' Short beep
      repeat 50
        ' Pulse the motor ever so briefly...
        outa[X_PWM_PIN]~~
        outa[Y_PWM_PIN]~~
        outa[X_PWM_PIN]~~
        outa[Y_PWM_PIN]~~
        outa[X_PWM_PIN]~
        outa[Y_PWM_PIN]~
        waitcnt(timer += period)

      ' Short delay between beeps
      waitcnt(timer += clkfreq / 6)
        
    ' A few seconds between sequences
    waitcnt(timer += clkfreq * 3)

    
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