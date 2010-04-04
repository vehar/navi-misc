{{

 usb-fs-host  ver 0.3
──────────────────────────────────────────────────────────────────

This is a software implementation of a simple full-speed
(12 Mb/s) USB 1.1 host controller for the Parallax Propeller.

This module is a self-contained USB stack, including host
controller driver, host controller, and a bit-banging PHY layer.

Software implementations of low-speed (1.5 Mb/s) USB have become
fairly common, but full-speed pushes the limits of even a fairly
powerful microcontroller like the Propeller. So naturally, I
had to cut some corners. See the sizable list of limitations and
caveats below.

The latest version of this file should always be at:
http://svn.navi.cx/misc/trunk/propeller/usb-host/usb-fs-host.spin

 ┌────────────────────────────────────────────────┐
 │ Copyright (c) 2010 Micah Dowty <micah@navi.cx> │               
 │ See end of file for terms of use.              │
 └────────────────────────────────────────────────┘


Hardware Requirements
─────────────────────

 - 96 MHz (overclocked) Propeller
 - USB D- attached to P0 with a 47Ω series resistor
 - USB D+ attached to P1 with a 47Ω series resistor
 - Pull-down resistors (~47kΩ) from USB D- and D+ to ground

  ┌───────────────┐  USB Host (Type A) Socket
  │ ─┬┬─┬┬─┬┬─┬┬─ │  
  │ 1└┘2└┘3└┘4└┘  │  1: Vbus (+5v)  Red
  ├───────────────┤  2: D-          White
                     3: D+          Green
                     4: GND         Black

                +5v
                  ┌┐
      2x 47Ω     └─┤│ (1) Vbus
  P0 ─────┳─────┤│ (2) D- 
  P1 ─────┼─┳───┤│ (3) D+
             │ │ ┌─┤│ (4) GND
             │ │ │ └┘
     2x 47kΩ   │       
             │ │ │
               
     

Limitations and Caveats
───────────────────────

 - Supports only a single device.
   (One host controller, one port, no hubs.)

 - Pin numbers are currently hardcoded as P0 and P1.
   Clock speed is hardcoded as 96 MHz (overclocked)

 - Requires 3 cogs.

   (We need a peak of 3 cogs during receive, but two
   of them are idle at other times. There is certainly
   room for improvement...)

 - Maximum transmitted packet size is approx. 384 bytes
   Maximum received packet size is approx. 1024 bytes

 - Doesn't even pretend to adhere to the USB spec!
   May not work with all USB devices.

 - Receiver is single-ended and uses a fixed clock rate.
   Does not tolerate line noise well, so use short USB
   cables if you must use a cable at all.

 - Maximum average speed is much less than line rate,
   due to time spent pre-encoding and post-decoding.

 - SOF packets do not have an incrementing frame number
   SOF packets may not be sent on-time due to other traffic

 - We don't detect TX/RX buffer overruns. If it hurts,
   don't do it. (Also, do not use this HC with untrusted
   devices- a babble condition can overwrite cog memory.)

 - We must acknowledge received packets before we have any
   idea if they're valid. So, there's no protocol-level way
   to automatically retry if there is a CRC error.


Theory of Operation
───────────────────

With the Propeller overclocked to 96 MHz, we have 8 clock cycles
(2 instructions) for every bit. That isn't nearly enough! So, we
cheat, and we do as much work as possible either before sending
a packet, after receiving a packet, or concurrently on another cog.

One cog is responsible for the bulk of the host controller
implementation. This is the transmit/controller cog. It accepts
commands from Spin code, handles pre-processing transmitted
data and post-processing received data, and oversees the process
of sending and receiving packets.

The grunt work of actually transmitting and receiving data is
offloaded from this cog. Transmit is handled by the TX cog's video
generator hardware. It's programmed in "VGA" mode to send two-byte
"pixels" to the D+/D- pins every 8 clock cycles. We pre-calculate
a buffer of "video" data representing each transmitted packet.

Receiving is harder- there's no hardware to help. In software, we
can test a port and shift its value into a buffer in 8 clock cycles,
but we don't have time to do anything else. So we can receive bursts
that are limited by cog memory. To receive continuously, this module
uses two receive cogs, and carefully interleaves them. Each cog
receives 16 bits at a time.

The other demanding operation we need to perform is a data IN
transfer. We need to transmit a token, receive a data packet, then
transmit an ACK. All back-to-back, with just a handful of bit
periods between packets. This is handled by some dedicated code
on the TX cog that does nothing but send low-latency ACKs after
the EOP state.


Programming Model
─────────────────

This should look a little familiar to anyone who's written USB
drivers for a PC operating system, or used a user-level USB library
like libusb.

Public functions are provided for each supported transfer type.
(BulkRead, BulkWrite, InterruptRead...) These functions take an
endpoint descriptor, hub memory buffer, and buffer size.

All transfers are synchronous. (So, during a transfer itself,
we're really tying up 5 cogs if you count the one you called from.)
All transfers and most other functions return an error code. See
the E_* constants below.

Since the transfer functions need to know both an endpoint's address
and its maximum packet size, we refer to endpoints by passing around
pointers to the endpoint's descriptor. In fact, this is how we refer
to interfaces too. This object keeps an in-memory copy of the
configuration descriptor we're using, so this data is always handy.
There are high-level functions for iterating over a device's
descriptors.

When a device is attached, call Enumerate to reset and identify it.
After Enumerate, the device will be in the 'addressed' state. It
will not be configured yet, but we'll have a copy of the device
descriptor and the first configuration descriptor in memory. To use
that default configuration, you can immediately call Configure. Now
the device is ready to use.

This host controller is a singleton object which is intended to
be instantiated only once per Propeller. Multiple objects can declare
OBJs for the host controller, but they will all really be sharing the
same instance. This will prevent you from adding multiple USB
controllers to one system, but there are also other reasons that we
don't currently support that. It's convenient, though, because this
means multiple USB device drivers can use separate instances of the
host controller OBJ to speak to the same USB port. Each device driver
can be invoked conditionally based on the device's class(es).

}}

CON
  NUM_COGS = 3

  ' Transmit / Receive Size limits.
  '
  ' Transmit size limit is based on free Cog RAM. It can be increased if we save space
  ' in the cog by optimizing the code or removing other data. Receive size is limited only
  ' by available hub ram.

  TX_BUFFER_WORDS = 200
  RX_BUFFER_WORDS = 256

  ' Maximum stored configuration descriptor size, in bytes. If the configuration
  ' descriptor is longer than this, we'll truncate it.

  CFGDESC_BUFFER_LEN = 256
  
            
  ' USB data pins. We make a couple assumptions about these, so if you want to change these
  ' it isn't as straightforward as just changing these constants:
  '
  '   - Both DMINUS and DPLUS must be <= 8, since we use pin masks in instruction literals.
  '   - The vcfg and palette values we use in the transmitter hardcode these values,
  '     but they could be changed without too much trouble.
  
  DMINUS = 0
  DPLUS = 1

  ' DEBUG: If nonzero, this is a mask for a pin to pulse when we're sending an ACK packet.
  '        We either drive this low or float it, so use a pull-up resistor!

  DEBUG_ACK_MASK = 0
                     
  ' Output bus states
  BUS_MASK  = (|< DPLUS) | (|< DMINUS)
  STATE_J   = |< DPLUS
  STATE_K   = |< DMINUS
  STATE_SE0 = 0
  STATE_SE1 = BUS_MASK
  
  ' Maximum number of times to retry a single command list. We try to space the retries
  ' out in intervals of one frame, so this is roughly the number of milliseconds we'll
  ' spend on a single portion of a transfer.
  MAX_CL_RETRIES   = 100

  ' Offsets in EndpointTable
  EPTABLE_SHIFT      = 2        ' log2 of entry size
  EPTABLE_TOKEN      = 0        ' word
  EPTABLE_TOGGLE_IN  = 2        ' byte
  EPTABLE_TOGGLE_OUT = 3        ' byte
  
  ' Port connection status codes
  PORTC_NO_DEVICE  = STATE_SE0     ' No device (pull-down resistors in host)
  PORTC_FULL_SPEED = STATE_J       ' Full speed: pull-up on D+
  PORTC_LOW_SPEED  = STATE_K       ' Low speed: pull-up on D-
  PORTC_INVALID    = STATE_SE1     ' Buggy device? Electrical transient?
  PORTC_NOT_READY  = $FF           ' Haven't checked port status yet

  ' Command opcodes for the controller cog.

  OP_RESET         = 1 << 24           ' Send a USB Reset signal   ' 
  OP_TX_BEGIN      = 2 << 24           ' Start a TX packet. Includes 8-bit PID
  OP_TX_END        = 3 << 24           ' End a TX packet
  OP_TX_FLUSH      = 4 << 24           ' Transmit a batch of TX packets (no receive)
  OP_TXRX          = OP_TX_FLUSH | %01 ' Transmit a batch and immediately start receiving                                
  OP_TXRX_ACK      = OP_TX_FLUSH | %11 ' Transmit, receive, and immediately ACK
  OP_TX_DATA_16    = 5 << 24           ' Encode and store a 16-bit word
  OP_TX_DATA_PTR   = 6 << 24           ' Encode data from hub memory.
                                       '   [23:16] = # of bytes, [15:0] = pointer
  OP_TX_CRC16      = 7 << 24           ' Encode  a 16-bit CRC of all data since the PID   
  OP_RX_PID        = 8 << 24           ' Decode and return a 16-bit PID word, reset CRC-16
  OP_RX_DATA_PTR   = 9 << 24           ' Decode data to hub memory.
                                       '   [23:16] = max # of bytes, [15:0] = pointer
                                       '   Return value is actual # of bytes (including CRC-16)
  OP_RX_CRC16      = 10 << 24          ' Decode and check CRC. Returns (actual XOR expected)
  OP_SOF_WAIT      = 11 << 24          ' Wait for one SOF to be sent
  
  OP_EOL           = $8000_0000        ' End-of-list. Last command in a CommandList().
  
  ' USB PID values / commands

  PID_OUT    = %1110_0001
  PID_IN     = %0110_1001
  PID_SOF    = %1010_0101
  PID_SETUP  = %0010_1101
  PID_DATA0  = %1100_0011
  PID_DATA1  = %0100_1011
  PID_ACK    = %1101_0010
  PID_NAK    = %0101_1010
  PID_STALL  = %0001_1110
  PID_PRE    = %0011_1100

  ' NRZI-decoded representation of a SYNC field, and PIDs which include the SYNC.
  ' These are the form of values returned by OP_RX_PID.

  SYNC_FIELD      = %10000000
  SYNC_PID_ACK    = SYNC_FIELD | (PID_ACK << 8)
  SYNC_PID_NAK    = SYNC_FIELD | (PID_NAK << 8)
  SYNC_PID_STALL  = SYNC_FIELD | (PID_STALL << 8)
  SYNC_PID_DATA0  = SYNC_FIELD | (PID_DATA0 << 8)
  SYNC_PID_DATA1  = SYNC_FIELD | (PID_DATA1 << 8)    

  ' USB Tokens (Device ID + Endpoint) with pre-calculated CRC5 values.
  ' Since we only support a single USB device, we only need tokens for
  ' device 0 (the default address) and device 1 (our arbitrary device ID).
  ' For device 0, we only need endpoint zero. For device 1, we include
  ' tokens for every possible endpoint.
  '
  '                  CRC5  EP#  DEV#
  TOKEN_DEV0_EP0  = %00010_0000_0000000
  TOKEN_DEV1_EP0  = %11101_0000_0000001
  TOKEN_DEV1_EP1  = %01011_0001_0000001
  TOKEN_DEV1_EP2  = %11000_0010_0000001
  TOKEN_DEV1_EP3  = %01110_0011_0000001
  TOKEN_DEV1_EP4  = %10111_0100_0000001
  TOKEN_DEV1_EP5  = %00001_0101_0000001
  TOKEN_DEV1_EP6  = %10010_0110_0000001
  TOKEN_DEV1_EP7  = %00100_0111_0000001
  TOKEN_DEV1_EP8  = %01001_1000_0000001
  TOKEN_DEV1_EP9  = %11111_1001_0000001
  TOKEN_DEV1_EP10 = %01100_1010_0000001
  TOKEN_DEV1_EP11 = %11010_1011_0000001
  TOKEN_DEV1_EP12 = %00011_1100_0000001
  TOKEN_DEV1_EP13 = %10101_1101_0000001
  TOKEN_DEV1_EP14 = %00110_1110_0000001
  TOKEN_DEV1_EP15 = %10000_1111_0000001

  ' Standard device requests.
  '
  ' This encodes the first two bytes of the SETUP packet into
  ' one word-sized command. The low byte is bmRequestType,
  ' the high byte is bRequest.

  REQ_CLEAR_DEVICE_FEATURE     = $0100
  REQ_CLEAR_INTERFACE_FEATURE  = $0101
  REQ_CLEAR_ENDPOINT_FEATURE   = $0102
  REQ_GET_CONFIGURATION        = $0880
  REQ_GET_DESCRIPTOR           = $0680
  REQ_GET_INTERFACE            = $0a81
  REQ_GET_DEVICE_STATUS        = $0000
  REQ_GET_INTERFACE_STATUS     = $0001
  REQ_GET_ENDPOINT_STATUS      = $0002
  REQ_SET_ADDRESS              = $0500
  REQ_SET_CONFIGURATION        = $0900
  REQ_SET_DESCRIPTOR           = $0700
  REQ_SET_DEVICE_FEATURE       = $0300
  REQ_SET_INTERFACE_FEATURE    = $0301
  REQ_SET_ENDPOINT_FEATURE     = $0302
  REQ_SET_INTERFACE            = $0b01
  REQ_SYNCH_FRAME              = $0c82

  ' Standard descriptor types.
  '
  ' These identify a descriptor in REQ_GET_DESCRIPTOR,
  ' via the high byte of wValue. (wIndex is the language ID.)
  '
  ' The 'DESCHDR' variants are the full descriptor header,
  ' including type and length. This matches the first two bytes
  ' of any such static-length descriptor.
  
  DESC_DEVICE           = $0100
  DESC_CONFIGURATION    = $0200
  DESC_STRING           = $0300
  DESC_INTERFACE        = $0400
  DESC_ENDPOINT         = $0500

  DESCHDR_DEVICE        = $01_12
  DESCHDR_CONFIGURATION = $02_09
  DESCHDR_INTERFACE     = $04_09
  DESCHDR_ENDPOINT      = $05_07
  
  ' Descriptor Formats

  DEVDESC_bLength             = 0
  DEVDESC_bDescriptorType     = 1
  DEVDESC_bcdUSB              = 2
  DEVDESC_bDeviceClass        = 4
  DEVDESC_bDeviceSubClass     = 5
  DEVDESC_bDeviceProtocol     = 6
  DEVDESC_bMaxPacketSize0     = 7
  DEVDESC_idVendor            = 8
  DEVDESC_idProduct           = 10
  DEVDESC_bcdDevice           = 12
  DEVDESC_iManufacturer       = 14
  DEVDESC_iProduct            = 15
  DEVDESC_iSerialNumber       = 16
  DEVDESC_bNumConfigurations  = 17
  DEVDESC_LEN                 = 18

  CFGDESC_bLength             = 0
  CFGDESC_bDescriptorType     = 1
  CFGDESC_wTotalLength        = 2
  CFGDESC_bNumInterfaces      = 4
  CFGDESC_bConfigurationValue = 5
  CFGDESC_iConfiguration      = 6
  CFGDESC_bmAttributes        = 7
  CFGDESC_MaxPower            = 8

  IFDESC_bLength              = 0
  IFDESC_bDescriptorType      = 1
  IFDESC_bInterfaceNumber     = 2
  IFDESC_bAlternateSetting    = 3
  IFDESC_bNumEndpoints        = 4
  IFDESC_bInterfaceClass      = 5
  IFDESC_bInterfaceSubClass   = 6
  IFDESC_bInterfaceProtocol   = 7
  IFDESC_iInterface           = 8

  EPDESC_bLength              = 0
  EPDESC_bDescriptorType      = 1
  EPDESC_bEndpointAddress     = 2
  EPDESC_bmAttributes         = 3
  EPDESC_wMaxPacketSize       = 4
  EPDESC_bInterval            = 6
                
  ' Negative error codes for functions that return them.
  '
  ' So that multiple levels of the software stack can share
  ' error codes, we define a few reserved ranges:
  '
  '   -1 to -99    : Application
  '   -100 to -150 : Device or device class driver
  '   -150 to -199 : USB host controller
  '
  ' Within the USB host controller range:
  '
  '   -150 to -159 : Device connectivity errors
  '   -160 to -179 : Low-level transfer errors
  '   -180 to -199 : High-level parsing errors
  '
  ' When adding new errors, please keep existing errors constant
  ' to avoid breaking other modules who may depend on these values.
  ' (But if you're writing another module that uses these values,
  ' please use the constants from this object rather than hardcoding
  ' them!)

  E_SUCCESS       = 0

  E_NO_DEVICE     = -150        ' No device is attached
  E_LOW_SPEED     = -151        ' Low-speed devices are not supported

  E_TIMEOUT       = -160        ' Timed out waiting for a response
  E_TRANSFER      = -161        ' Generic low-level transfer error
  E_CRC           = -162        ' CRC-16 mismatch
  E_TOGGLE        = -163        ' DATA0/1 toggle error
  E_PID           = -164        ' Invalid or malformed PID
  E_STALL         = -165        ' USB STALL response (pipe error)
  
  E_ADDR_FAIL     = -180        ' Failed to put device in Addressed state
  E_DESC_PARSE    = -181        ' Can't parse a USB descriptor

  
DAT
  ' This is a singleton object, so we use DAT for all variables.
  ' Note that, unlike VARs, these won't be sorted automatically.
  ' Keep variables of the same type together.

rx_buffer     long      0[RX_BUFFER_WORDS]
  
txc_command   long      -1                      ' Command buffer for transmit/control cog
txc_result    long      0
rx1_time      long      0                       ' Trigger time for RX1 cog
rx2_time      long      0                       ' Trigger time for RX2 cog

isRunning     byte      0
portc         byte      PORTC_NOT_READY         ' Port connection status
rxdone        byte      $FF

devdesc       byte      0[DEVDESC_LEN]
cfgdesc       byte      0[CFGDESC_BUFFER_LEN]

DAT
''
''
''==============================================================================
'' Host Controller Setup
''==============================================================================

  
PUB Start

  '' Starts the software USB host controller, if it isn't already running.
  '' Requires 3 free cogs.                             

  if isRunning
    return

  ' Runtime address patching
  Setup_DataPtr |= @Setup_Request

  ' Set up pre-cognew parameters
  sof_deadline := cnt
  txp_portc    := @portc
  txp_result   := @txc_result
  txp_rx1_time := @rx1_time
  txp_rx2_time := @rx2_time

  txp_rxdone   := rx1p_done   := rx2p_done   := @rxdone
  txp_rxbuffer := rx1p_buffer := rx2p_buffer := @rx_buffer
  
  cognew(@controller_cog, @txc_command)
  cognew(@rx_cog_1, @rx1_time)
  cognew(@rx_cog_2, @rx2_time)

  isRunning~~
  

DAT
''
''==============================================================================
'' High-level Device Framework
''==============================================================================

PUB GetPortConnection
  '' Is a device connected? If so, what speed? Returns a PORTC_* constant.

  repeat while portc == PORTC_NOT_READY
  return portc

PUB Enumerate : error
  '' Initialize the attached USB device, and get information about it.  
  ''
  ''   1. Reset the device
  ''   2. Assign it an address
  ''   3. Read the device descriptor
  ''   4. Read the first configuration descriptor

  case GetPortConnection
    PORTC_NO_DEVICE, PORTC_INVALID:
      return E_NO_DEVICE
    PORTC_LOW_SPEED:
      return E_LOW_SPEED          

  DeviceReset

  ' Before we can do any transfers longer than 8 bytes, we need to know the maximum
  ' packet size on EP0. Otherwise we won't be able to determine when a transfer has
  ' ended. So, we'll use a temporary maximum packet size of 8 in order to address the
  ' device and to receive the first 8 bytes of the device descriptor. This should
  ' always be possible using transfers of no more than one packet in length.
  BYTE[@devdesc + DEVDESC_bMaxPacketSize0] := 8    

  if DeviceAddress < 0
    return E_ADDR_FAIL

  ' Read the real max packet length (Must request exactly 8 bytes)
  error := ControlRead(REQ_GET_DESCRIPTOR, DESC_DEVICE, 0, @devdesc, 8)
  if error < 0
    return

  ' Validate device descriptor header
  if UWORD(@devdesc) <> DESCHDR_DEVICE
    return E_DESC_PARSE

  ' Read the whole descriptor
  error := ControlRead(REQ_GET_DESCRIPTOR, DESC_DEVICE, 0, @devdesc, DEVDESC_LEN)
  if error < 0
    return

  return ReadConfiguration(0)

PUB Configure : error
  '' Switch device configurations. This (re)configures the device according to
  '' the currently loaded configuration descriptor. To use a non-default configuration,
  '' call ReadConfiguration() to load a different descriptor first.

  ResetEndpointToggle
  return Control(REQ_SET_CONFIGURATION, BYTE[@cfgdesc + CFGDESC_bConfigurationValue], 0)

PUB UnConfigure : error
  '' Place the device back in its un-configured state.
  '' In the unconfigured state, only the default control endpoint may be used.

  return Control(REQ_SET_CONFIGURATION, 0, 0)
  
PUB ReadConfiguration(index) : error
  '' Read in a configuration descriptor from the device. Most devices have only one
  '' configuration, and we load it automatically in Enumerate. So you usually don't
  '' need to call this function. But if the device has multiple configurations, you
  '' can use this to get information about them all.
  ''
  '' This does not actually switch configurations. If this newly read configuration
  '' is indeed the one you want to use, call Configure.

  error := ControlRead(REQ_GET_DESCRIPTOR, DESC_CONFIGURATION | index, 0, @cfgdesc, CFGDESC_BUFFER_LEN)
  if error < 0
    return

  if UWORD(@cfgdesc) <> DESCHDR_CONFIGURATION
    return E_DESC_PARSE

  return 0
  
PUB DeviceDescriptor : ptr
  '' Get a pointer to the enumerated device's Device Descriptor
  return @devdesc

PUB ConfigDescriptor : ptr
  '' Get a pointer to the last config descriptor read with ReadConfiguration().
  '' If the configuration was longer than CFGDESC_BUFFER_LEN, it will be truncated.
  return @cfgdesc

PUB VendorID : devID
  '' Get the enumerated device's 16-bit Vendor ID
  return UWORD(@devdesc + DEVDESC_idVendor)
  
PUB ProductID : devID
  '' Get the enumerated device's 16-bit Product ID
  return UWORD(@devdesc + DEVDESC_idProduct)

DAT
''
''==============================================================================
'' Configuration Descriptor Parsing
''==============================================================================

PUB NextDescriptor(ptrIn) : ptrOut | endPtr
  '' Advance to the next descriptor within the configuration descriptor.
  '' If there is another descriptor, returns a pointer to it. If we're at
  '' the end of the descriptor or the buffer, returns 0.

  ptrOut := ptrIn + BYTE[ptrIn]
  endPtr := @cfgdesc + (UWORD(@cfgDesc + CFGDESC_wTotalLength) <# CFGDESC_BUFFER_LEN)

  if ptrOut => endPtr
    ptrOut~

PUB NextHeaderMatch(ptrIn, header) : ptrOut
  '' Advance to the next descriptor which matches the specified header.

  repeat while ptrIn := NextDescriptor(ptrIn)
    if UWORD(ptrIn) == header
      return ptrIn
  return 0

PUB FirstInterface : firstIf
  '' Return a pointer to the first interface in the current config
  '' descriptor. If there were no valid interfaces, returns 0.

  return NextInterface(@cfgdesc)

PUB NextInterface(curIf) : nextIf
  '' Advance to the next interface after 'curIf' in the current
  '' configuration descriptor. If there are no more interfaces, returns 0.

  return NextHeaderMatch(curIf, DESCHDR_INTERFACE)

PUB NextEndpoint(curIf) : nextIf
  '' Advance to the next endpoint after 'curIf' in the current
  '' configuration descriptor. To get the first endpoint in an interface,
  '' pass in a pointer to the interface descriptor.
  ''
  '' If there are no more endpoints in this interface, returns 0.

  repeat while curIf := NextDescriptor(curIf)
    case UWORD(curIf)
      DESCHDR_ENDPOINT:
        return curIf
      DESCHDR_INTERFACE:
        return 0

  return 0

PUB FindInterface(class) : foundIf
  '' Look for the first interface which has the specified class.
  '' If no such interface exists on the current configuration, returns 0.

  foundIf := FirstInterface
  repeat while foundIf 
    if BYTE[foundIf + IFDESC_bInterfaceClass] == class
      return foundIf
    foundIf := NextInterface(foundIf)

PUB UWORD(addr) : value
  '' Like WORD[addr], but works on unaligned addresses too.
  '' You must use this rather than WORD[] when reading 16-bit values
  '' from descriptors, since descriptors have no alignment guarantees.
 
  return BYTE[addr] | (BYTE[addr + 1] << 8)
     

DAT
''
''==============================================================================
'' Device Setup
''==============================================================================

PUB DeviceReset
  '' Asynchronously send a USB bus reset signal.

  Command(OP_RESET) 
  ResetEndpointToggle

PUB DeviceAddress : error | buf

  '' Send a SET_ADDRESS(1) to device 0.
  ''
  '' This should be sent after DeviceReset to transition the
  '' device from the Default state to the Addressed state. All
  '' other transfers here assume the device address is 1.

  Setup_Request := REQ_SET_ADDRESS
  Setup_Value := 1
  Setup_Index~
  Setup_Length~

  return ControlRaw(TOKEN_DEV0_EP0, @buf, 4)


DAT
''
''==============================================================================
'' Control Transfers
''==============================================================================

PUB Control(req, value, index) : error | buf

  '' Issue a no-data control transfer to an addressed device.

  Setup_Request := req
  Setup_Value := value
  Setup_Index := index
  Setup_Length~

  return ControlRaw(TOKEN_DEV1_EP0, @buf, 4)

  
PUB ControlRead(req, value, index, bufferPtr, length) : error

  '' Issue a control IN transfer to an addressed device.
  ''
  '' Returns the number of bytes read. Zero is a successful zero-length
  '' transfer, negative numbers indicate errors.

  Setup_Request := req
  Setup_Value := value
  Setup_Index := index
  Setup_Length := length

  return ControlRaw(TOKEN_DEV1_EP0, bufferPtr, length)


PRI ControlRaw(token, buffer, length) : error | toggle

  ' Common low-level implementation of a control transfer. Supports arbitrary tokens,
  ' and assumes the setup packet has already been filled out.

  SendToken(PID_SETUP, token)
  if SCLRetry(@Setup_Commands, SYNC_PID_ACK)
    return E_TRANSFER

  toggle := PID_DATA1
  return DataIN(token, buffer, length, BYTE[@devdesc + DEVDESC_bMaxPacketSize0], @toggle, MAX_CL_RETRIES)


DAT
''
''==============================================================================
'' Interrupt Transfers
''==============================================================================

PUB InterruptRead(epd, buffer, length) : actual | epTable

  '' Try to read one packet, up to 'length' bytes, from an Interrupt IN endpoint.
  '' Returns the actual amount of data read.
  '' If no data is available, returns E_TIMEOUT without waiting.
  ''
  '' 'epd' is a pointer to this endpoint's Endpoint Descriptor.

  ' This is different from Bulk in two main ways:
  '
  '   - We give DataIN an artificially large maxPacketSize, since we
  '     never want it to receive more than one packet at a time here.
  '   - We give it a retry of 0, since we don't want to retry on NAK.

  epTable := EndpointTableAddr(epd) 
  return DataIN(WORD[epTable], buffer, length, $1000, epTable + EPTABLE_TOGGLE_IN, 0)


DAT
''
''==============================================================================
'' Bulk Transfers
''==============================================================================

PUB BulkWrite(epd, buffer, length) : error | packetSize, epTable, maxPacketSize

  '' Write 'length' bytes of data to a Bulk OUT endpoint.
  ''
  '' Always writes at least one packet. If 'length' is zero,
  '' we send a zero-length packet. If 'length' is any other
  '' even multiple of maxPacketLen, we send only maximally-long
  '' packets and no zero-length packet.
  ''
  '' 'epd' is a pointer to this endpoint's Endpoint Descriptor.
  
  epTable := EndpointTableAddr(epd)
  maxPacketSize := EndpointMaxPacketSize(epd)
  
  repeat
    packetSize := length <# maxPacketSize
    
    error := DataOUT(WORD[epTable], buffer, packetSize, epTable + EPTABLE_TOGGLE_OUT, MAX_CL_RETRIES)
    if error
      return

    buffer += packetSize
    if (length -= packetSize) =< 0
      return
    
PUB BulkRead(epd, buffer, length) : actual | epTable

  '' Read up to 'length' bytes from a Bulk IN endpoint.
  '' Returns the actual amount of data read.
  ''
  '' 'epd' is a pointer to this endpoint's Endpoint Descriptor.

  epTable := EndpointTableAddr(epd)
  return DataIN(WORD[epTable], buffer, length, EndpointMaxPacketSize(epd), epTable + EPTABLE_TOGGLE_IN, MAX_CL_RETRIES)


DAT

'==============================================================================
' Low-level Transfer Utilities
'==============================================================================

PRI EndpointTableAddr(epd) : addr
  ' Given an endpoint descriptor, return the address of our EndpointTable entry.

  return @EndpointTable + ((BYTE[epd + EPDESC_bEndpointAddress] & $F) << EPTABLE_SHIFT)

PRI EndpointMaxPacketSize(epd) : maxPacketSize
  ' Parse the max packet size out of an endpoint descriptor

  return UWORD(epd + EPDESC_wMaxPacketSize)
  
PRI ResetEndpointToggle | ep
  ' Reset all endpoints to the default DATA0 toggle

  ep := @EndpointTable
  repeat 16
    BYTE[ep + EPTABLE_TOGGLE_IN] := BYTE[ep + EPTABLE_TOGGLE_OUT] := PID_DATA0
    ep += constant(|< EPTABLE_SHIFT)

PRI DataIN(token, buffer, length, maxPacketLen, togglePtr, retries) : actual | retval

  ' Issue IN tokens and read the resulting data packets until
  ' a packet smaller than maxPacketLen arrives. On success,
  ' returns the actual number of bytes read. On failure, returns
  ' a negative error code.
  '
  ' 'togglePtr' is a pointer to a byte with either PID_DATA0 or
  ' PID_DATA1, depending on which DATA PID we expect next. Every
  ' time we receive a packet, we toggle this byte from DATA0 to
  ' DATA1 or vice versa.
  '
  ' Each packet will have up to 'retries' additional attempts
  ' if the device responds with a NAK.

  actual~

  ' As long as there's buffer space, send IN tokens. Each IN token
  ' allows the device to send us back up to maxPacketLen bytes of data.
  ' If the device sends a short packet (including zero-byte packets)
  ' it terminates the transfer.
  
  repeat     
    retval := RequestDataIN(token, togglePtr, retries)
    if retval
      return retval
  
    ' Now the packet is sitting in the receive buffer, but it hasn't
    ' been decoded. Decode it, copying the data into 'buffer' and looking
    ' ahead for an EOP.

    retval := DecodeData(buffer, length)
    actual += retval
    buffer += retval
    length -= retval

    if SyncCommand(OP_RX_CRC16)
      return E_CRC

    if length == 0
      ' Received exactly the right amount of data.
      ' We should end now- some (buggy?) devices will NAK or otherwise
      ' do the wrong thing instead of giving us a zero-length packet to
      ' terminate the transmission.
      return
    
    if retval < maxPacketLen
      ' Short packet or zero-length packet. Transfer is done!
      return

PRI DataOUT(token, buffer, length, togglePtr, retries) : error

  ' Transmit a single packet to an endpoint, as an OUT followed by DATA.
  '
  ' 'togglePtr' is a pointer to a byte with either PID_DATA0 or
  ' PID_DATA1, depending on which DATA PID we expect next. Every
  ' time we receive a packet, we toggle this byte from DATA0 to
  ' DATA1 or vice versa.
  '
  ' Each packet will have up to 'retries' additional attempts
  ' if the device responds with a NAK.

  repeat
    SendToken(PID_OUT, token)                  ' OUT
    Command(OP_TX_BEGIN | BYTE[togglePtr])     ' DATA0/1
    EncodeData(buffer, length)                 ' Payload
    case SyncCommandList(@DATA_OUT_Commands)   ' CRC, Flush, receive PID

      SYNC_PID_NAK:
        ' Busy. Wait a frame and try again.
        if --retries =< 0
          return E_TIMEOUT
        Command(OP_SOF_WAIT)    

      SYNC_PID_STALL:
        return E_STALL

      SYNC_PID_ACK:
        BYTE[togglePtr] ^= constant(PID_DATA0 ^ PID_DATA1) 
        return E_SUCCESS

      other:
        return E_PID
  
PRI RequestDataIN(token, togglePtr, retries) : error | pid

  ' Low-level data IN request. Handles data toggle and retry.
  ' This is part of the implementation of DataIN().
                                               
  repeat
    SendToken(PID_IN, token)  
    case pid := SyncCommandList(@TXRX_ACK_PID_Commands)

      SYNC_PID_NAK:
        ' Busy. Wait a frame and try again.
        if --retries =< 0
          return E_TIMEOUT
        Command(OP_SOF_WAIT)

      SYNC_PID_STALL:
        return E_STALL

      SYNC_PID_DATA0, SYNC_PID_DATA1:
        if (pid >> 8) <> BYTE[togglePtr]
          return E_TOGGLE
        BYTE[togglePtr] ^= constant(PID_DATA0 ^ PID_DATA1)  
        return 0
    
      other:
        return E_PID
      
PRI DecodeData(buffer, length) : actual | chunkLen, chunkActual

  ' Decode data from the RX buffer into a provided hub-memory buffer.
  ' Decodes up to 'length' bytes, but stops early if we hit a pseudo-EOP.
  ' Does not decode or check the CRC.
  '
  ' This is typically just a single call to OP_RX_DATA_PTR, but we may need
  ' to loop since that command only decodes up to 255 bytes at a time.

  actual~
  
  repeat while length > 0
    chunkLen := length <# $FF
    chunkActual := SyncCommand(OP_RX_DATA_PTR | (chunkLen << 16) | buffer)

    actual += chunkActual
    buffer += chunkActual
    length -= chunkActual

    if chunkActual < chunkLen
      return

PRI EncodeData(buffer, length) | chunk

  ' Encode data into the RX buffer from hub memory. Always writes
  ' the full buffer. Does not automatically append a data CRC.
  '
  ' This is typically just a single call to OP_TX_DATA_PTR, but we may need
  ' to loop since that command only decodes up to 255 bytes at a time.

  repeat while length > 0
    chunk := length <# $FF
    Command(OP_TX_DATA_PTR | (chunk << 16) | buffer)
    buffer += chunk
    length -= chunk

PRI SendToken(pid, token)
  ' Enqueue a token in the TX buffer

  Command(OP_TX_BEGIN | pid)
  Command(OP_TX_DATA_16 | token)
  Command(OP_TX_END)


DAT

'==============================================================================
' Low-level Command Interface
'==============================================================================

PRI Command(c)
  ' Asynchronously execute a low-level driver cog command
  
  repeat while txc_command
  txc_command := c

PRI SyncCommand(c) : r
  ' Synchronously execute a low-level driver cog command, and return the result

  Command(c)
  Command(0)
  return txc_result
  
PRI CommandList(ptr) | c
  ' Execute several commands from a list in hub memory.
  ' The last command has the OP_EOL bit set.

  repeat
    c := LONG[ptr]
    Command(c & $7FFF_FFFF)
    ptr += 4
    if c & OP_EOL
      return

PRI SyncCommandList(ptr) : r
  ' Execute a command list synchronously, and return the result of the
  ' last command in the list which wrote to the result buffer.

  CommandList(ptr)
  Command(0)
  return txc_result

PRI SCLRetry(ptr, expected) : error

  ' Run a command list synchronously, retrying it if it returns
  ' a value other than 'expected'. If it doesn't work even after
  ' the maximum number of retries, returns nonzero.

  repeat MAX_CL_RETRIES
    if SyncCommandList(ptr) == expected
      return 0

    ' Wait a frame between retries
    Command(OP_SOF_WAIT)

  return E_TRANSFER


DAT

'==============================================================================
' Endpoint State Table
'==============================================================================

' For each endpoint number, we have:
'
' Offset    Size    Description
' ----------------------------------------------------
'  0        Word    Token (device, endpoint, crc5)
'  2        Byte    Toggle for IN endpoint (PID_DATA0 / PID_DATA1)
'  3        Byte    Toggle for OUT endpoint

EndpointTable           word    TOKEN_DEV1_EP0, 0
                        word    TOKEN_DEV1_EP1, 0
                        word    TOKEN_DEV1_EP2, 0
                        word    TOKEN_DEV1_EP3, 0
                        word    TOKEN_DEV1_EP4, 0
                        word    TOKEN_DEV1_EP5, 0
                        word    TOKEN_DEV1_EP6, 0
                        word    TOKEN_DEV1_EP7, 0
                        word    TOKEN_DEV1_EP8, 0
                        word    TOKEN_DEV1_EP9, 0
                        word    TOKEN_DEV1_EP10, 0
                        word    TOKEN_DEV1_EP11, 0
                        word    TOKEN_DEV1_EP12, 0
                        word    TOKEN_DEV1_EP13, 0
                        word    TOKEN_DEV1_EP14, 0
                        word    TOKEN_DEV1_EP15, 0

'==============================================================================
' Command List Templates
'==============================================================================

        ' Shared buffer for SETUP packets on any control transfer.

Setup_Request           word  0
Setup_Value             word  0
Setup_Index             word  0
Setup_Length            word  0

        ' Setup Data out
        '
        '   Host:  DATA0 [data]         
        ' Device:               [ACK]
        '
        ' Return value: ACK pid
        
Setup_Commands          long  OP_TX_BEGIN | PID_DATA0
Setup_DataPtr           long  OP_TX_DATA_PTR | (8 << 16)
                        ' * Fall Through *

        ' Normal data OUT

DATA_OUT_Commands       long  OP_TX_CRC16
                        long  OP_TX_END
                        long  OP_TXRX
                        long  OP_RX_PID | OP_EOL

        ' Flush TX buffer, receive a packet, automatic ACK, return the PID

TXRX_ACK_PID_Commands   long  OP_TXRX_ACK
                        long  OP_RX_PID | OP_EOL


DAT
            
'==============================================================================
' Controller / Transmitter Cog
'==============================================================================

' This is the "main" cog in the host controller. It processes commands that arrive
' from Spin code. These commands can build encoded USB packets in a local buffer,
' and transmit them. Multiple packets can be buffered back-to-back, to reduce the
' gap between packets to an acceptable level.
'
' This cog also handles triggering our two receiver cogs. Two receiver cogs are
' interleaved, so we can receive packets larger than what will fit in a single
' cog's unrolled loop.
'
' The receiver cogs are also responsible for managing the bus ownership, and the
' handoff between a driven idle state and an undriven idle. We calculate timestamps
' at which the receiver cogs will perform this handoff.

              org
controller_cog

              '======================================================
              ' Cog Initialization
              '======================================================

              ' Initialize the PLL and video generator for 12 MB/s output.
              ' This sets up CTRA as a divide-by-8, with no PLL multiplication.
              ' Use 2bpp "VGA" mode, so we can insert SE0 states easily. Every
              ' two bits we send to waitvid will be two literal bits on D- and D+.
              
              ' To start with, we leave the pin mask in vcfg set to all zeroes.
              ' At the moment we're actually ready to transmit, we set the mask.
              '
              ' We also re-use this initialization code space for temporary variables.
              
tx_count      mov       ctra, ctra_value
t1            mov       frqa, frqa_value
l_cmd         mov       vcfg, vcfg_value
codec_buf     mov       vscl, vscl_value

codec_cnt     call      #enc_reset

              '======================================================
              ' Command Processing
              '======================================================

              ' Wait until there's a command available or it's time to send a SOF.
              ' SOF is more important than a command, but we have no way of ensuring
              ' that a SOF won't need to occur during a command- so the SOF might be
              ' late.

cmdret        wrlong    c_zero, par
command_loop
              cmp       tx_count, #0 wz         ' Skip SOF if the buffer is in use
              mov       t1, cnt                 ' cnt - sof_deadline, store sign bit
              sub       t1, sof_deadline
              rcl       t1, #1 wc               ' C = deadline is in the future
  if_z_and_nc jmp       #tx_sof                 ' Send the SOF.

              rdlong    l_cmd, par wz           ' Look for an incoming command
        if_z  jmp       #command_loop

              mov       t1, l_cmd
              shr       t1, #24
              add       t1, #:cmdjmp
              movs      :cmdjmp, t1
              nop                               ' Instruction fetch delay

              ' Command jump table

:cmdjmp       jmp       #0
              jmp       #cmd_reset
              jmp       #cmd_tx_begin
              jmp       #cmd_tx_end
              jmp       #cmd_tx_flush
              jmp       #cmd_tx_data_16
              jmp       #cmd_tx_data_ptr
              jmp       #cmd_tx_crc16
              jmp       #cmd_rx_pid
              jmp       #cmd_rx_data_ptr
              jmp       #cmd_rx_crc16
              jmp       #cmd_sof_wait

              '======================================================
              ' SOF Packets / PORTC Sampling
              '======================================================

              ' If we're due for a SOF and we're between packets,
              ' this routine is called to transmit the SOF packet.
              '
              ' We're allowed to use the transmit buffer, but we must
              ' not return via 'cmdret', since we don't want to clear
              ' our command buffer- if another cog wrote a command
              ' while we're processing the SOF, we would miss it.
              ' So we need to use the lower-level encoder routines
              ' instead of calling other command implementations.
              '
              ' This happens to also be a good time to sample the port
              ' connection status. When the bus is idle, its state tells
              ' us whether a device is connected, and what speed it is.
              ' We need to sample this when the bus is idle, and this
              ' is a convenient time to do so. We can also skip sending
              ' the SOF if the bus isn't in a supported state.
 
tx_sof
              xor       cmd_sof_wait, c_condition     ' Let an SOF wait through.
                                                      ' (Swap from if_always to if_never)

              mov       t1, ina
              and       t1, #BUS_MASK
              wrbyte    t1, txp_portc                 ' Save idle bus state as PORTC
              cmp       t1, #PORTC_FULL_SPEED wz
        if_nz jmp       #:skip                        ' Only send SOF to full-speed devices
                          
              call      #encode_sync                  ' SYNC field

              mov       codec_buf, sof_frame          ' PID and Token
              mov       codec_cnt, #24
              call      #encode

              call      #encode_eop                   ' End of packet and inter-packet delay
                                                
              mov       l_cmd, #0                     ' TX only, no receive
              call      #txrx

:skip
              add       sof_deadline, sof_period
              
              jmp       #command_loop 
              
              '======================================================
              ' OP_TX_BEGIN
              '======================================================

              ' When we begin a packet, we'll always end up generating
              ' 16 bits (8 sync, 8 pid) which will fill up the first long
              ' of the transmit buffer. So it's legal to use tx_count!=0
              ' to detect whether we're using the transmit buffer.
              
cmd_tx_begin
              call      #encode_sync

              ' Now NRZI-encode the PID field

              mov       codec_buf, l_cmd
              mov       codec_cnt, #8
              call      #encode

              ' Reset the CRC-16, it should cover only data from after the PID.

              mov       enc_crc16, crc16_mask

              jmp       #cmdret

              '======================================================
              ' OP_TX_END
              '======================================================

cmd_tx_end
              call      #encode_eop

              jmp       #cmdret

              '======================================================
              ' OP_TX_DATA_16
              '======================================================

cmd_tx_data_16
              mov       codec_buf, l_cmd
              mov       codec_cnt, #16
              call      #encode

              jmp       #cmdret

              '======================================================
              ' OP_TX_DATA_PTR
              '======================================================

              ' Byte count in l_cmd[23:16], hub pointer in [15:0].
              '
              ' This would be faster if we processed in 32-bit
              ' chunks when possible (at least 4 bytes left, pointer is
              ' long-aligned) but right now we're optimizing for simplicity. 
              
cmd_tx_data_ptr
              test      l_cmd, c_00FF0000 wz    ' At least 1 byte to send?
        if_z  jmp       #cmdret

              rdbyte    codec_buf, l_cmd
              mov       codec_cnt, #8
              add       l_cmd, c_FFFF0001       ' Count - 1, Pointer + 1
              call      #encode

              jmp       #cmd_tx_data_ptr

              '======================================================
              ' OP_TX_CRC16
              '======================================================

cmd_tx_crc16
              mov       codec_buf, enc_crc16
              xor       codec_buf, crc16_mask
              mov       codec_cnt, #16
              call      #encode

              jmp       #cmdret
                          
              '======================================================
              ' OP_TX_FLUSH, OP_TXRX, OP_TXRX_ACK
              '======================================================

cmd_tx_flush
              call      #txrx
              jmp       #cmdret

              '======================================================
              ' OP_RESET
              '======================================================
              
cmd_reset

              mov       outa, #0                ' Start driving SE0
              mov       dira, #BUS_MASK

              mov       t1, cnt
              add       t1, reset_period
              waitcnt   t1, #0
                                                    
              mov       dira, #0                ' Stop driving
              mov       sof_deadline, cnt       ' Ignore SOFs that should have occurred
              
              jmp       #cmdret

              '======================================================
              ' OP_RX_PID
              '======================================================

              ' Receive a 16-bit word, and reset the CRC-16.
              ' For use in receiving and validating a packet's SYNC/PID header.

cmd_rx_pid
              mov       codec_cnt, #16
              call      #decode
              shr       codec_buf, #16
              wrlong    codec_buf, txp_result

              mov       dec_crc16, crc16_mask   ' Reset the CRC-16

              jmp       #cmdret
              
              '======================================================
              ' OP_RX_DATA_PTR
              '======================================================

              ' Max byte count in l_cmd[23:16], hub pointer in [15:0].
              ' Returns the actual number of bytes received.
              '
              ' This would be faster if we processed in 32-bit
              ' chunks when possible (at least 4 bytes left, pointer is
              ' long-aligned) but right now we're optimizing for simplicity. 
              '
              ' This determines actual length by looking for a pseudo-EOP.
              ' After every byte, we scan ahead in the raw buffer and look
              ' for when the bus goes idle for at least 7 bit periods.
              ' This can't happen during a packet due to bit stuffing. Once
              ' we find this, we can search backward to the nearest byte
              ' boundary, then back two bytes (for the CRC-16).
              '
              ' By keeping a look-ahead buffer in the decoder, this test
              ' can be made very efficient. See dec_nrzi and eop_mask below.
              
cmd_rx_data_ptr
              mov       t1, #0                  ' Count received bytes

:loop8        test      l_cmd, c_00FF0000 wz    ' At least 1 byte to decode?
        if_nz test      dec_nrzi, eop_mask_1 wz ' Test for pseudo-EOP
        if_nz test      dec_nrzi, eop_mask_2 wz
        if_nz test      dec_nrzi, eop_mask_3 wz
        if_nz test      dec_nrzi, eop_mask_4 wz
        if_z  jmp       #:done

              mov       codec_cnt, #8
              call      #decode
              shr       codec_buf, #24          ' Right-justify result
              wrbyte    codec_buf, l_cmd
              add       l_cmd, c_FFFF0001       ' Count - 1, Pointer + 1
              add       t1, #1                  ' Result += 1

              jmp       #:loop8
:done
              wrlong    t1, txp_result
              jmp       #cmdret
    
              '======================================================
              ' OP_RX_CRC16
              '======================================================
             
cmd_rx_crc16
              xor       dec_crc16, crc16_mask   ' Save CRC of payload
              mov       t1, dec_crc16

              mov       codec_cnt, #16
              call      #decode

              shr       codec_buf, #16          ' Justify received CRC
              xor       t1, codec_buf           ' Compare
              wrlong    t1, txp_result          ' and return
              jmp       #cmdret
              
              '======================================================
              ' OP_SOF_WAIT
              '======================================================

              ' Normally this jumps back to the command loop without
              ' completing the command. In tx_sof, this code is modified
              ' to return exactly once.
              '
              ' (The modification works by patching the condition code on the
              ' first instruction in this routine.)

cmd_sof_wait  jmp       #command_loop
              xor       cmd_sof_wait, c_condition       ' Swap from if_never to if_always
              jmp       #cmdret

        
              '======================================================
              ' Transmit / Receive Front-end
              '======================================================

txrx
              ' Save the raw transmit length, not including padding,
              ' then pad our buffer to a multiple of 16 (one video word)

              mov       tx_count_raw, tx_count
:pad          test      tx_count, #%1111 wz
        if_z  jmp       #:pad_done
              call      #encode_idle
              jmp       #:pad
:pad_done

              ' Reset the receiver state (regardless of whether we're using it)

              wrbyte    v_idle, txp_rxdone      ' Arbitrary nonzero byte

              ' Save bit 0 (receiver enable) in C

              rcr       l_cmd, #1 nr,wc
              
              ' Transmitter startup: We need to synchronize with the video PLL,
              ' and transition from an undriven idle state to a driven idle state.
              ' To do this, we need to fill up the video generator register with
              ' idle states before setting DIRA and VCFG.
              '
              ' Since we own the bus at this point, we don't have to hurry.

              mov       vscl, vscl_value                ' Back to normal video speed
              waitvid   v_palette, v_idle
              waitvid   v_palette, v_idle
              movs      vcfg, #BUS_MASK
              mov       dira, #BUS_MASK

              ' Give the receiver cogs a synchronized timestamp to wake up at.
              ' We use the same timestamp below to figure out when we should
              ' stop driving the bus.
            
              shl       tx_count_raw, #3                ' 8 cycles per bit
              add       tx_count_raw, #$60              ' Constant offset
              add       tx_count_raw, cnt

        if_c  wrlong    tx_count_raw, txp_rx1_time
        if_c  wrlong    tx_count_raw, txp_rx2_time
    
              ' Transmit our NRZI-encoded packet

              movs      :tx_loop, #tx_buffer
              shr       tx_count, #4                    ' Bits -> words
:tx_loop      waitvid   v_palette, 0
              add       :tx_loop, #1
              djnz      tx_count, #:tx_loop

              ' Stop driving the bus at the same time our RX cogs wake up.
              ' This should be as soon as possible after the idle state has been
              ' driven for one bit period. (See the constant time offset above)
              '
              ' We also need to trigger the ACK cog as soon as possible after
              ' TX is done, if ACKs are enabled.
              
              waitcnt   tx_count_raw, #0
              mov       dira, #0

              ' As soon as we're done transmitting, switch to a 'turbo' vscl value,
              ' so that after the current video word expires we switch to a faster
              ' clock. This will help us synchronize to the video generator faster
              ' when sending ACKs, decreasing the maximum ACK latency.

              mov       vscl, vscl_turbo

              '======================================
              ' Receiver Controller
              '======================================

        if_nc jmp       #:rx_done                       ' Receiver disabled
              test      l_cmd, #2 wz
        if_z  jmp       #:ack_done                      ' ACK packet disabled

              '==================
              ' ACK Transmitter
              '==================

              ' If we're sending an ACK, it must be transmitted after the received
              ' packet EOP, and at just the right time. Too soon and it will interfere
              ' with the pseudo-EOP detection. Too far, and the device will time out
              ' while waiting for it.
              '
              ' So, to get predictable latency while also keeping code size down, we
              ' use the video generator in a somewhat odd way. Prior to this code,
              ' we set the video generator to run very quickly, so the variation in
              ' waitvid duration is fairly small. After re-synchronizing to the video
              ' generator, we slow it back down and emit a pre-constructed ACK packet.
              '
              ' To trigger the ACK, we look for a real EOP by sampling the bus and
              ' waiting for a SE0 state. This is the only fast way of detecting EOPs.
              ' (Elsewhere we use a single ended pseudo-EOP detector, which is very
              ' high latency.)
              '
              ' In case the device never replies, we need to time out. We have a separate
              ' timeout for this EOP wait (there is another RX timeout below).
  
              mov       t1, eopwait_iters
:wait_eop     test      c_bus_mask, ina wz
        if_nz djnz      t1, #:wait_eop

              waitvid   v_palette, v_idle                ' Sync to vid gen. at turbo speed
              mov       vscl, vscl_value                 ' Back to normal speed at the next waitvid
              waitvid   v_palette, v_ack1                ' Start ACK after a couple idle cycles
              mov       dira, #DEBUG_ACK_MASK | BUS_MASK ' Take bus ownership during the idle
              waitvid   v_palette, v_ack2                ' Second half of the ACK + EOP
              waitvid   v_palette, v_idle                ' Wait for the ack to completely finish
              mov       dira, #0                         ' Release bus

:ack_done               

              '==================
              ' RX Babysitter
              '==================

              ' Now we're just waiting for the RX cog to finish. Poll RX_DONE.
              ' For simplicity, we just have a single conservative timeout.
              ' If that one timeout expires, wake up the RX cog manuallly.

              mov       t1, eopwait_hub
:rx_wait      rdbyte    t3, txp_rxdone wz
        if_nz djnz      t1, #:rx_wait

              ' If the timeout expired and our RX cogs still aren't done,
              ' we'll manually wake them up by driving a SE1 onto the bus
              ' for a few cycles.

        if_nz mov       outa, #BUS_MASK
        if_nz mov       dira, #BUS_MASK
        if_nz jmp       #$+1
        if_nz jmp       #$+1
        if_nz mov       dira, #0
        if_nz mov       outa, #0

              ' Initialize the decoder, point it at the top of the RX buffer.
              ' The decoder will load the first long on our first invocation.
              
              mov       dec_rxbuffer, txp_rxbuffer
              mov       dec_nrzi_cnt, #1        ' Mod-32 counter
              mov       dec_nrzi_st, #0
              mov       dec_1cnt, #0
              rdlong    dec_nrzi, dec_rxbuffer
              add       dec_rxbuffer, #4
              
:rx_done
              '======================================
              ' End of Receiver Controller
              '======================================
                                                                                
              call      #enc_reset              ' Reset the encoder too
              movs      vcfg, #0                ' Disconnect vid gen. from outputs

txrx_ret      ret


              '======================================================
              ' NRZI Encoding and Bit Stuffing
              '======================================================

              ' Encode (NRZI, bit stuffing, and store) up to 32 bits.
              '
              ' The data to be encoded comes from codec_buf, and codec_cnt
              ' specifies how many bits we shift out from the LSB side.
              '
              ' For both space and time efficiency, this routine is also
              ' responsible for updating a running CRC-16. This is only
              ' used for data packets- at all other times it's quietly
              ' ignored.
encode
              rcr       codec_buf, #1 wc

              ' Update the CRC16.
              '
              ' This is equivalent to:
              '
              '   condition = (input_bit ^ (enc_crc16 & 1))
              '   enc_crc16 >>= 1
              '   if condition:
              '     enc_crc16 ^= crc16_poly

              test      enc_crc16, #1 wz
              shr       enc_crc16, #1
    if_z_eq_c xor       enc_crc16, crc16_poly        
      
              ' NRZI-encode one bit.
              '
              ' For every incoming bit, we generate two outgoing bits;
              ' one for D- and one for D+. We can do all of this in three
              ' instructions with SAR and XOR. For example:
              '
              '   Original value of tx_reg:        10 10 10 10
              '   After SAR by 2 bits:          11 10 10 10 10
              '     To invert D-/D+, flip MSB:  01 10 10 10 10
              '    (or)
              '     Avoid inverting by flipping
              '     the next highest bit:       10 10 10 10 10
              '
              ' These two operations correspond
              ' to NRZI encoding 0 and 1, respectively.
  
              sar       enc_nrzi, #2
        if_nc xor       enc_nrzi, c_80000000     ' NRZI 0
        if_c  xor       enc_nrzi, c_40000000     ' NRZI 1


              ' Bit stuffing: After every six consecutive 1 bits, insert a 0.
              ' If we detect that bit stuffing is necessary, we do the branch
              ' after storing the original bit below, then we come back here to
              ' store the stuffed bit.

        if_nc mov       enc_1cnt, #6 wz
        if_c  sub       enc_1cnt, #1 wz
enc_bitstuff_ret

              ' Every time we fill up enc_nrzi, append it to tx_buffer.
              ' We use another shift register as a modulo-32 counter.
     
              ror       enc_nrzi_cnt, #1 wc
              add       tx_count, #1
encode_ptr
        if_c  mov       0, enc_nrzi
        if_c  add       encode_ptr, c_dest_1

              ' Insert the stuffed bit if necessary
              
        if_z  jmp       #enc_bitstuff

              djnz      codec_cnt, #encode
encode_ret    ret

              ' Handle the relatively uncommon case of inserting a zero bit,
              ' for bit stuffing. This duplicates some of the code from above
              ' for NRZI-encoding the extra bit. This bit is *not* included
              ' in the CRC16.

enc_bitstuff  sar       enc_nrzi, #2
              xor       enc_nrzi, c_80000000
              mov       enc_1cnt, #6 wz
              jmp       #enc_bitstuff_ret       ' Count and store this bit            

                          
              '======================================================
              ' Encoder / Transmitter Reset
              '======================================================
        
              ' (Re)initialize the encoder and transmitter registers.
              ' The transmit buffer will now be empty.

enc_reset     mov       enc_nrzi, v_idle
              mov       enc_nrzi_cnt, enc_ncnt_init
              mov       enc_1cnt, #0
              mov       tx_count, #0
              movd      encode_ptr, #tx_buffer
enc_reset_ret ret


              '======================================================
              ' Low-level Encoder
              '======================================================

              ' The main 'encode' function above is the normal case.
              ' But we need to be able to encode special bus states too,
              ' so these functions are slower but more flexible encoding
              ' entry points.
              '
              
              ' Check whether we need to store the contents of enc_nrzi
              ' after encoding another bit-period worth of data from it.
              ' This is a modified version of the tail end of 'encode' above.
              
encode_store
              mov       :ptr, encode_ptr
              ror       enc_nrzi_cnt, #1 wc
              add       tx_count, #1
:ptr    if_c  mov       0, enc_nrzi
        if_c  add       encode_ptr, c_dest_1
encode_store_ret ret

              ' Raw NRZI zeroes and ones, with no bit stuffing

encode_raw0
              sar       enc_nrzi, #2
              xor       enc_nrzi, c_80000000
              mov       enc_1cnt, #0
              call      #encode_store
encode_raw0_ret ret

encode_raw1
              sar       enc_nrzi, #2
              xor       enc_nrzi, c_40000000
              call      #encode_store
encode_raw1_ret ret

              ' One cycle of single-ended zero.

encode_se0
              shr       enc_nrzi, #2
              call      #encode_store
encode_se0_ret ret

              ' One cycle of idle bus (J state).
              
encode_idle
              shr       enc_nrzi, #2
              xor       enc_nrzi, c_80000000
              call      #encode_store
encode_idle_ret ret

              ' Append a raw SYNC field
encode_sync
              mov       t1, #7
:loop         call      #encode_raw0
              djnz      t1, #:loop
              call      #encode_raw1
encode_sync_ret ret

              ' Append a raw EOP
encode_eop    
              call      #encode_se0
              call      #encode_se0
              mov       t1, #4
:loop         call      #encode_idle
              djnz      t1, #:loop
encode_eop_ret ret


              '======================================================
              ' NRZI Decoder / Bit un-stuffer
              '======================================================

              ' Decode (retrieve, NRZI, bit un-stuff) up to 32 bits.
              '
              ' The data to decode comes from the RX buffer in hub memory.
              ' We decode 'codec_cnt' bits into the *MSBs* of 'codec_buf'.
              '
              ' As with encoding, we also run a CRC-16 here, since it's
              ' a convenient place to do so.

decode
              ' Extract the next bit from the receive buffer.
              '
              ' Our buffering scheme is a bit strange, since we need to have
              ' a 32-bit look ahead buffer at all times, for pseudo-EOP detection.
              '
              ' So, we treat 'dec_nrzi' as a 32-bit shift register which always
              ' contains valid bytes from the RX buffer. It is pre-loaded with the
              ' first word of the receive buffer.
              '
              ' Once every 32 decoded bits (starting with the first bit) we load a
              ' new long from dec_rxbuffer into dec_rxlatch. When we shift bits out
              ' of dec_nrzi, bits from dec_rxlatch replace them. 
                                                        
              ror       dec_nrzi_cnt, #1 wc
        if_c  rdlong    dec_rxlatch, dec_rxbuffer
        if_c  add       dec_rxbuffer, #4
              rcr       dec_rxlatch, #1 wc
              rcr       dec_nrzi, #1 wc
        
              ' We use a small auxiliary shift register to XOR the current bit
              ' with the last one, even across word boundaries where we might have
              ' to reload the main shift register. This auxiliary shift register
              ' ends up tracking the state ("what was the last bit?") for NRZI decoding.
              
              rcl       dec_nrzi_st, #1

              cmp       dec_1cnt, #6 wz         ' Skip stuffed bits
        if_z  mov       dec_1cnt, #0
        if_z  jmp       #decode
              
              test      dec_nrzi_st, #%10 wz    ' Previous bit
              shr       codec_buf, #1
    if_c_ne_z or        codec_buf, c_80000000   ' codec_buf <= !(prev XOR current)     
              test      codec_buf, c_80000000 wz ' Move decoded bit to Z

    if_nz     add       dec_1cnt, #1            ' Count consecutive '1' bits       
    if_z      mov       dec_1cnt, #0
    
              ' Update our CRC-16. This performs the same function as the logic
              ' in the encoder above, but it's optimized for our flag usage.

              shr       dec_crc16, #1 wc          ' Shift out CRC LSB into C
    if_z_eq_c xor       dec_crc16, crc16_poly        
   
              djnz      codec_cnt, #decode
decode_ret    ret
             

              '======================================================
              ' Data
              '======================================================

' Parameters that are set up by Spin code prior to cognew()

sof_deadline  long      0
txp_portc     long      0
txp_result    long      0
txp_rxdone    long      0
txp_rx1_time  long      0
txp_rx2_time  long      0
txp_rxbuffer  long      0

' Constants
              
c_zero        long      0
c_40000000    long      $40000000
c_80000000    long      $80000000
c_00FF0000    long      $00FF0000
c_FFFF0001    long      $FFFF0001
c_dest_1      long      1 << 9
c_condition   long      %000000_0000_1111_000000000_000000000
c_bus_mask    long      BUS_MASK

reset_period  long      96_000_000 / 100

frqa_value    long      $10000000                       ' 1/8
ctra_value    long      (%00001 << 26) | (%111 << 23)   ' PLL 1:1
vcfg_value    long      (%011 << 28)                    ' Unpack 2-bit -> 8-bit     
vscl_value    long      (8 << 12) | (8 * 16)            ' Normal 8 clocks per pixel
vscl_turbo    long      (1 << 12) | (1 * 16)            ' 1 clock per pixel
v_palette     long      $03_02_01_00
v_idle        long      %%2222_2222_2222_2222

' Pre-encoded ACK sequence:
'
'    SYNC     ACK      EOP
'    00000001 01001011
'    KJKJKJKK JJKJJKKK 00JJ
'
'    waitvid: %%2200_1112_2122_1121_2121
'
' This encoded sequence requires 40 bits.
' We encode this in two logs, but we don't start
' it immediately. Two reasons:
'
'   - We enable the output drivers immediately
'     after sync'ing to the video generator, So
'     there is about a 1/2 bit delay between
'     starting to send this buffer and when we
'     actually take control over the bus.
'
'   - We need to ensure a minimum delay between
'     EOP and ACK of 8 cycles in order for the
'     pseudo-EOP detector to work correctly.
'
'     There's a small but variable delay between
'     the actual EOP and when we start clocking
'     out this buffer, due to setup time, EOP
'     detection latency, and waitvid latency.
'     So we don't need to wait a full 8 cycles here.
'
'     (Currently we wait 6 bit periods.
'      This may need to be tweaked)
                            
v_ack1        long      %%2211212121222222
v_ack2        long      %%2222222200111221

enc_ncnt_init long      $8000_8000                      ' Shift reg as mod-16 counter

crc16_poly    long      $a001                           ' USB CRC-16 polynomial
crc16_mask    long      $ffff                           ' Init/final mask

' Mask for detecting pseudo-EOPs at a byte boundary. We have a 32-bit lookahead
' buffer for detecting them in the raw NRZI stream. We want to detect them just
' prior to the CRC-16. Due to bit stuffing, the CRC can take anywhere between
' 16 and 20 bits. The EOP itself is detected as at least 8 consecutive zeroes. 
'
' Every '1' bit in this mask is a position that must be zero for the pseudo-EOP
' test to pass, signalling that the packet is about to end.
'
' Due to the variance in CRC length, we have several masks to test, any one of
' which can indicate a pseudo-EOP match.

eop_mask_1    long      %00000000_11111111_00000000_00000000
eop_mask_2    long      %00000001_11111110_00000000_00000000
eop_mask_3    long      %00000011_11111100_00000000_00000000
eop_mask_4    long      %00000111_11111000_00000000_00000000

' The 'real' low-latency EOP detector that we use for sending ACKs on the TX
' cog needs to eventually time out, in case the device never responded with an
' EOP. We don't have time to do anything complicated, so we just have a simple
' fixed timeout which has to be long enough for the maximum-length received packet.
'
' This is a two-instruction (test / djnz) loop which takes 8 cycles. So each
' iteration is one bit period, and this is really a count of the maximum number
' of bit periods that could exist between the end of the transmitted packet
' and the EOP on the received packet. So it must account for the max size of the
' receive buffer, plus an estimate of the max inter-packet delay.
'
' There is another variant of this timeout that's used when the TX cog is waiting
' on the RX2 cog to signal that it's done. The duration should be the same, but
' the units are different, since we're waiting on a loop with a rdlong in it. We
' synchronize the loop with our hub access windows, so it takes 16 cycles.

eopwait_iters long      ((RX_BUFFER_WORDS * 32) + 128)
eopwait_hub   long      ((RX_BUFFER_WORDS * 32) + 128) / 16

' We try to send SOFs every millisecond, but other traffic can preempt them.
' Since we're not even trying to support very timing-sensitive devices, we
' also send a fake (non-incrementing) frame number.

sof_frame     long      %00010_00000000000_1010_0101    ' SOF PID, Frame 0, valid CRC6
sof_period    long      96_000                          ' 96 MHz, 1ms          

' Encoder only
enc_nrzi      res       1                               ' Encoded NRZI shift register
enc_1cnt      res       1
enc_nrzi_cnt  res       1                               ' Cyclic bit counter
enc_crc16     res       1

' Decoder only
dec_nrzi      res       1                               ' Encoded NRZI shift register
dec_nrzi_cnt  res       1                               ' Cyclic bit counter
dec_nrzi_st   res       1                               ' State of NRZI decoder
dec_1cnt      res       1
dec_rxbuffer  res       1
dec_rxlatch   res       1
dec_crc16     res       1

tx_count_raw  res       1
t3            res       1

tx_buffer     res       TX_BUFFER_WORDS


              fit

DAT

'==============================================================================
' Receiver Cog 1
'==============================================================================

' This receiver cog stores the first 16-bit half of every 32-bit word.

              org
rx_cog_1
              mov       rx1_buffer, rx1p_buffer
              mov       rx1_iters, #RX_BUFFER_WORDS
              
:wait         rdlong    t2, par wz              ' Read trigger timestamp
        if_z  jmp       #:wait
              wrlong    rx1_zero, par           ' One-shot, zero it.
                                                  
              waitcnt   t2, #0                  ' Wait for trigger time          

              ' Now synchronize to the beginning of the next packet.
              ' We sample only D- in the receiver. If we time out,
              ' the controller cog will artificially send a SE1
              ' to bring us out of sleep. (We'd rather not send a SE0,
              ' since we may inadvertently reset the device.)

              waitpne   rx1_zero, rx1_pin
              
:sample_loop
              test      rx1_pin, ina wc         '  0
              rcr       t2, #1
              test      rx1_pin, ina wc         '  1
              rcr       t2, #1
              test      rx1_pin, ina wc         '  2
              rcr       t2, #1
              test      rx1_pin, ina wc         '  3
              rcr       t2, #1
              test      rx1_pin, ina wc         '  4
              rcr       t2, #1
              test      rx1_pin, ina wc         '  5
              rcr       t2, #1
              test      rx1_pin, ina wc         '  6
              rcr       t2, #1
              test      rx1_pin, ina wc         '  7
              rcr       t2, #1
              test      rx1_pin, ina wc         '  8
              rcr       t2, #1
              test      rx1_pin, ina wc         '  9
              rcr       t2, #1
              test      rx1_pin, ina wc         ' 10
              rcr       t2, #1
              test      rx1_pin, ina wc         ' 11
              rcr       t2, #1
              test      rx1_pin, ina wc         ' 12
              rcr       t2, #1
              test      rx1_pin, ina wc         ' 13
              rcr       t2, #1
              test      rx1_pin, ina wc         ' 14
              rcr       t2, #1
              test      rx1_pin, ina wc         ' 15
              rcr       t2, #1

              ' At this exact moment, the RX2 cog takes over sampling
              ' for us. We can store these 16 bits to hub memory, but we
              ' need to use a waitcnt to resynchronize to USB after
              ' waiting on the hub.
              '
              ' This constant must be carefully adjusted so that the period
              ' of this loop is exactly 32*8 cycles. For reference, we can
              ' compare the RX1 and RX2 periods to make sure they're equal.

              mov       rx1_cnt, cnt
              add       rx1_cnt, #(16*8 - 9)

              shr       t2, #16
              wrword    t2, rx1_buffer
              add       rx1_buffer, #4

              ' Stop either when we fill up the buffer, or when RX2 signals
              ' that it's detected a pseudo-EOP and set RX_DONE.
              
              sub       rx1_iters, #1 wz
   if_nz      rdbyte    t2, rx1p_done wz
   if_z       jmp       #rx_cog_1

              waitcnt   rx1_cnt, #0       
              jmp       #:sample_loop

rx1_pin       long      |< DMINUS   
rx1_zero      long      0

' Parameters that are set up by Spin code prior to cognew()
rx1p_done     long      0
rx1p_buffer   long      0

rx1_buffer    res       1                  
rx1_cnt       res       1
rx1_iters     res       1
t2            res       1

              fit

DAT

'==============================================================================
' Receiver Cog 2
'==============================================================================

' This receiver cog stores the second 16-bit half of every 32-bit word.
'
' Since this is the last receiver cog to run, we update the RX_LONGS counter
' and detect when we're "done". We don't actually detect EOP conditions (since
' we are only sampling D-) but we decide to finish receiving when an entire word
' (16 bit perods) of the bus looks idle. Due to bit stuffing, this condition never
' occurs while a packet is in progress.
'
' When we detect this pseudo-EOP condition, we'll set the "done" bit (bit 31) in
' RX_LONGS. This tells both the RX1 cog and the controller that we're finished.

              org
rx_cog_2
              mov       rx2_buffer, rx2p_buffer
              mov       rx2_iters, #RX_BUFFER_WORDS
                            
:wait         rdlong    t4, par wz              ' Read trigger timestamp
        if_z  jmp       #:wait
              wrlong    rx2_zero, par           ' One-shot, zero it.
                                                  
              waitcnt   t4, #0                  ' Wait for trigger time
              waitpne   rx2_zero, rx2_pin       ' Sync to SOP

              ' Calculate a sample time that's 180 degrees out of phase
              ' from the RX1 cog's sampling burst. We want to sample every
              ' 8 clock cycles with no gaps.

              mov       rx2_cnt, cnt            
              add       rx2_cnt, #(16*8 - 4)

              jmp       #:first_sample
        
:sample_loop

              ' Justify the received word. Also detect our pseudo-EOP condition,
              ' when we've been idle (0) for 16 bits.
              shr       t4, #16 wz
              
              add       rx2_buffer, #2
              wrword    t4, rx2_buffer
              add       rx2_buffer, #2
              
              ' Update RX_DONE only after writing to the buffer.
              ' We're done if rx2_iters runs out, or if we're idle.

        if_nz sub       rx2_iters, #1 wz
        if_z  wrbyte    rx2_zero, rx2p_done
        if_z  jmp       #rx_cog_2

:first_sample waitcnt   rx2_cnt, #(32*8)

              test      rx2_pin, ina wc         '  0
              rcr       t4, #1
              test      rx2_pin, ina wc         '  1
              rcr       t4, #1
              test      rx2_pin, ina wc         '  2
              rcr       t4, #1
              test      rx2_pin, ina wc         '  3
              rcr       t4, #1
              test      rx2_pin, ina wc         '  4
              rcr       t4, #1
              test      rx2_pin, ina wc         '  5
              rcr       t4, #1
              test      rx2_pin, ina wc         '  6
              rcr       t4, #1
              test      rx2_pin, ina wc         '  7
              rcr       t4, #1
              test      rx2_pin, ina wc         '  8
              rcr       t4, #1
              test      rx2_pin, ina wc         '  9
              rcr       t4, #1
              test      rx2_pin, ina wc         ' 10
              rcr       t4, #1
              test      rx2_pin, ina wc         ' 11
              rcr       t4, #1
              test      rx2_pin, ina wc         ' 12
              rcr       t4, #1
              test      rx2_pin, ina wc         ' 13
              rcr       t4, #1
              test      rx2_pin, ina wc         ' 14
              rcr       t4, #1
              test      rx2_pin, ina wc         ' 15
              rcr       t4, #1

              jmp       #:sample_loop

rx2_pin       long      |< DMINUS
rx2_zero      long      0

' Parameters that are set up by Spin code prior to cognew()
rx2p_done     long      0
rx2p_buffer   long      0

rx2_done_p    res       1
rx2_time_p    res       1
rx2_buffer    res       1
rx2_iters     res       1                  
rx2_cnt       res       1
t4            res       1

              fit
                                     
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