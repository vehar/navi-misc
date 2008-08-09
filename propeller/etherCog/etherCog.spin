{{                                              

etherCog
--------

This is a fast single-cog driver for the ENC28J60, an easy to
use SPI-attached 10baseT chip from Microchip. It implements
the chip's SPI protocol, as well as a fast low-level packet
dispatch system, ARP, and UDP.

This object is an original work. It is not based on Microchip's
sample implementation, only the ENC28J60 data sheet and errata
documents. Inspiration was taken from Harrison Pham's
driver_enc28j60 object, and from fsrw's SPI routines- but all
code here is original, so I've taken the opportunity to release
this object under the MIT license instead of the GPL.
  
┌───────────────────────────────────┐
│ Copyright (c) 2008 Micah Dowty    │               
│ See end of file for terms of use. │
└───────────────────────────────────┘

}}

VAR
  byte cog

PUB start(csPin, sckPin, siPin, soPin) : okay | handshake
  '' Initialize the ethernet driver. This resets and
  '' initializes the ENC28J60 chip, then starts a single
  '' cog for receiving and transmitting packets.
  ''
  '' This function is not re-entrant. If you have multiple
  '' etherCog objects, initialize them one-at-a-time.

  ' Store raw pin numbers (for the initialization only)

  cs_pin   := csPin
  si_pin   := siPin
  so_pin   := soPin
  sck_pin  := sckPin
  
  ' Set up pin masks (for the driver cog)

  cs_mask  := |< csPin
  si_mask  := |< siPin
  so_mask  := |< soPin
  init_dira := cs_mask | si_mask | (|< sckPin)

  ' During initialization, this Spin cog owns the pins.
  ' After the driver cog itself has started, we'll disclaim
  ' ownership over them.

  outa |= cs_mask               ' Order matters. CS high first, then make it an output.
  dira |= init_dira             ' Otherwise CS will glitch low briefly.
  
  ' We use CTRA to generate the SPI clock

  init_ctra := sckPin | constant(%00100 << 26) 

  ' Send the ENC28J60 chip a software reset

  init_spi_begin
  init_spi_Write8(SPI_SRC)
  init_spi_end
  
  ' 1ms delay on reset.
  ' This is for clock stabilization and PHY init.
  ' Also see Rev B5 errata 1.

  waitcnt(cnt + clkfreq / 1000 * 50)

  ' Table-driven initialization for MAC and PHY registers.
  ' To save space in the driver cog we do this before
  ' starting it, using a slow SPI engine written in Spin. 

  init_reg_writeTable(@reg_init_table)

  ' Wire up our LMM overlay base address.
  ' (The Spin compiler isn't smart enough to resolve
  ' hub addresses at compile time.)

  lmm_base := @lmm_base_label
  
  ' Start the cog, and hand off ownership over the SPI bus.
  ' We need to wait until the cog has set initial dira/outa
  ' state before we release ours, or the SPI bus will glitch.
  ' However, the cog also needs to wait until we release CS
  ' or it won't be able to send any SPI commands.
  '
  ' So, 'handshake' starts out set to -1. The cog sets it to '1'
  ' once after outa/dira are set. When we notice this, we release
  ' our ownership over the SPI bus, and set 'handshake' to zero.
  ' When the cog notices that, it continues.

  handshake~~ 
  okay := cog := 1 + cognew(@entry, @handshake)
  repeat while handshake <> 1
  dira &= !init_dira
  outa &= !init_dira
  handshake~
  
PUB stop
  if cog > 0
    cogstop(cog - 1)
    cog~

CON

'==============================================================================
' ENC28J60 Constants
'==============================================================================

' All the constants in this section are defined by the ENC28J60 data sheet.
' They should not be changed.

' SPI Commands

SPI_RCR = %000_00000   ' Read Control Register
SPI_RBM = %001_11010   ' Read Buffer Memory
SPI_WCR = %010_00000   ' Write Control Register
SPI_WBM = %011_11010   ' Write Buffer Memory
SPI_BFS = %100_00000   ' Bit Field Set
SPI_BFC = %101_00000   ' Bit Field Clear
SPI_SRC = %111_11111   ' System Reset Command

' Register names.
'
' Each register's address is encoded: Bits 4:0 are the register's
' position within its bank, bits 6:5 are its bank number, bit
' 7 is cleared if the register is available in all banks, and bit 8
' is set if the register will be preceeded by a dummy byte on read.
' Bit 8 is not necessary for writes. (Our register write tables store
' encoded register names in a single byte.)
'
' These encoded register names can be used with the reg_Read and reg_Write routines.

' All banks
EIE       = (%00_00 << 5) | $1B
EIR       = (%00_00 << 5) | $1C
ESTAT     = (%00_00 << 5) | $1D
ECON2     = (%00_00 << 5) | $1E
ECON1     = (%00_00 << 5) | $1F

' Bank 0      
ERDPTL    = (%01_00 << 5) | $00
ERDPTH    = (%01_00 << 5) | $01
EWRPTL    = (%01_00 << 5) | $02
EWRPTH    = (%01_00 << 5) | $03
ETXSTL    = (%01_00 << 5) | $04
ETXSTH    = (%01_00 << 5) | $05
ETXNDL    = (%01_00 << 5) | $06
ETXNDH    = (%01_00 << 5) | $07
ERXSTL    = (%01_00 << 5) | $08
ERXSTH    = (%01_00 << 5) | $09
ERXNDL    = (%01_00 << 5) | $0A
ERXNDH    = (%01_00 << 5) | $0B
ERXRDPTL  = (%01_00 << 5) | $0C
ERXRDPTH  = (%01_00 << 5) | $0D
ERXWRPTL  = (%01_00 << 5) | $0E
ERXWRPTH  = (%01_00 << 5) | $0F
EDMASTL   = (%01_00 << 5) | $10
EDMASTH   = (%01_00 << 5) | $11
EDMANDL   = (%01_00 << 5) | $12
EDMANDH   = (%01_00 << 5) | $13
EDMADSTL  = (%01_00 << 5) | $14
EDMADSTH  = (%01_00 << 5) | $15
EDMACSL   = (%01_00 << 5) | $16
EDMACSH   = (%01_00 << 5) | $17
              
' Bank 1
EHT0      = (%01_01 << 5) | $00
EHT1      = (%01_01 << 5) | $01
EHT2      = (%01_01 << 5) | $02
EHT3      = (%01_01 << 5) | $03
EHT4      = (%01_01 << 5) | $04
EHT5      = (%01_01 << 5) | $05
EHT6      = (%01_01 << 5) | $06
EHT7      = (%01_01 << 5) | $07
EPMM0     = (%01_01 << 5) | $08
EPMM1     = (%01_01 << 5) | $09
EPMM2     = (%01_01 << 5) | $0A
EPMM3     = (%01_01 << 5) | $0B
EPMM4     = (%01_01 << 5) | $0C
EPMM5     = (%01_01 << 5) | $0D
EPMM6     = (%01_01 << 5) | $0E
EPMM7     = (%01_01 << 5) | $0F
EPMCSL    = (%01_01 << 5) | $10
EPMCSH    = (%01_01 << 5) | $11
EPMOL     = (%01_01 << 5) | $14
EPMOH     = (%01_01 << 5) | $15
ERXFCON   = (%01_01 << 5) | $18
EPKTCNT   = (%01_01 << 5) | $19
              
' Bank 2
MACON1    = (%11_10 << 5) | $00
MACON3    = (%11_10 << 5) | $02
MACON4    = (%11_10 << 5) | $03
MABBIPG   = (%11_10 << 5) | $04
MAIPGL    = (%11_10 << 5) | $06
MAIPGH    = (%11_10 << 5) | $07
MACLCON1  = (%11_10 << 5) | $08
MACLCON2  = (%11_10 << 5) | $09
MAMXFLL   = (%11_10 << 5) | $0A
MAMXFLH   = (%11_10 << 5) | $0B
MICMD     = (%11_10 << 5) | $12
MIREGADR  = (%11_10 << 5) | $14
MIWRL     = (%11_10 << 5) | $16
MIWRH     = (%11_10 << 5) | $17
MIRDL     = (%11_10 << 5) | $18
MIRDH     = (%11_10 << 5) | $19
              
' Bank 3
MAADR5    = (%11_11 << 5) | $00
MAADR6    = (%11_11 << 5) | $01
MAADR3    = (%11_11 << 5) | $02
MAADR4    = (%11_11 << 5) | $03
MAADR1    = (%11_11 << 5) | $04
MAADR2    = (%11_11 << 5) | $05
EBSTSD    = (%01_11 << 5) | $06
EBSTCON   = (%01_11 << 5) | $07
EBSTCSL   = (%01_11 << 5) | $08
EBSTCSH   = (%01_11 << 5) | $09
MISTAT    = (%11_11 << 5) | $0A
EREVID    = (%01_11 << 5) | $12
ECOCON    = (%01_11 << 5) | $15
EFLOCON   = (%01_11 << 5) | $17
EPAUSL    = (%01_11 << 5) | $18
EPAUSH    = (%01_11 << 5) | $19

' PHY registers

PHCON1    = $00
PHSTAT1   = $01
PHID1     = $02
PHID2     = $03
PHCON2    = $10
PHSTAT2   = $11
PHIE      = $12
PHIR      = $13
PHLCON    = $14

' Individual register bits

'EIE
RXERIE    = 1 << 0
TXERIE    = 1 << 1
TXIE      = 1 << 3
LINKIE    = 1 << 4
DMAIE     = 1 << 5
PKTIE     = 1 << 6
INTIE     = 1 << 7

'EIR
RXERIF    = 1 << 0
TXERIF    = 1 << 1
TXIF      = 1 << 3
LINKIF    = 1 << 4
DMAIF     = 1 << 5
PKTIF     = 1 << 6

'ESTAT
CLKRDY    = 1 << 0
TXABRT    = 1 << 1
RXBUSY    = 1 << 2
LATECOL   = 1 << 4
BUFER     = 1 << 6
INT       = 1 << 7

'ECON2
VRPS      = 1 << 3
PWRSV     = 1 << 5
PKTDEC    = 1 << 6
AUTOINC   = 1 << 7

'ECON1
BSEL0     = 1 << 0
BSEL1     = 1 << 1
RXEN      = 1 << 2
TXRTS     = 1 << 3
CSUMEN    = 1 << 4
DMAST     = 1 << 5
RXRST     = 1 << 6
TXRST     = 1 << 7

'ERXFCON
BCEN      = 1 << 0
MCEN      = 1 << 1
HTEN      = 1 << 2
MPEN      = 1 << 3
PMEN      = 1 << 4
CRCEN     = 1 << 5
ANDOR     = 1 << 6
UCEN      = 1 << 7

'MACON1
MARXEN    = 1 << 0
PASSALL   = 1 << 1
RXPAUS    = 1 << 2
TXPAUS    = 1 << 3

'MACON3 
FULDPX    = 1 << 0
FRMLNEN   = 1 << 1
HFRMEN    = 1 << 2
PHDREN    = 1 << 3
TXCRCEN   = 1 << 4
PADCFG0   = 1 << 5
PADCFG1   = 1 << 6
PADCFG2   = 1 << 7

'MACON4 
NOBKOFF   = 1 << 4
BPEN      = 1 << 5
DEFER     = 1 << 6

'MICMD
MIIRD     = 1 << 0
MIISCAN   = 1 << 1

'EBSTCON                       
BISTST    = 1 << 0
TME       = 1 << 1
TMSEL0    = 1 << 2
TMSEL1    = 1 << 3
PSEL      = 1 << 4
PSV0      = 1 << 5
PSV1      = 1 << 6
PSV2      = 1 << 7

'MISTAT 
BUSY      = 1 << 0
SCAN      = 1 << 1
NVALID    = 1 << 2

'EFLOCON
FCEN0     = 1 << 0
FCEN1     = 1 << 1
FULDPXS   = 1 << 2

'PHCON1
PDPXMD    = 1 << 8
PPWRSV    = 1 << 11
PLOOPBK   = 1 << 14
PRST      = 1 << 15

'PHSTAT1
JBSTAT    = 1 << 1
LLSTAT    = 1 << 2
PHDPX     = 1 << 11
PFDPX     = 1 << 12

'PHCON2
HDLDIS    = 1 << 8
JABBER    = 1 << 10
TXDIS     = 1 << 13
FRCLNK    = 1 << 14

'PHSTAT2
PLRITY    = 1 << 5
DPXSTAT   = 1 << 9
LSTAT     = 1 << 10
COLSTAT   = 1 << 11
RXSTAT    = 1 << 12
TXSTAT    = 1 << 13

'PHIE
PGEIE     = 1 << 1
PLNKIE    = 1 << 4

'PHIR
PGIF      = 1 << 2
PLNKIF    = 1 << 4

'PHLCON
STRCH     = 1 << 1
LFRQ_0    = 0 << 2
LFRQ_1    = 1 << 2
LFRQ_2    = 2 << 2
LBCFG_BIT = 4
LACFG_BIT = 8

'Nonzero reserved bits in PHLCON
PHLCON_RESERVED = $3000

'LED settings (for LACFG/LBCFG fields in PHLCON)
LED_TX          = 1
LED_RX          = 2
LED_COLLISION   = 3
LED_LINK        = 4
LED_DUPLEX      = 5
LED_TXRX        = 7
LED_ON          = 8
LED_OFF         = 9
LED_BLINK_FAST  = 10
LED_BLINK_SLOW  = 11
LED_LINK_RX     = 12
LED_LINK_TXRX   = 13
LED_DUPLEX_COLL = 14


'==============================================================================
' Protocol constants
'==============================================================================

' Ethernet frame types (EtherType)

ETHERTYPE_ARP   = $0806
ETHERTYPE_IPV4  = $0800

' ARP constants

ARP_HW_ETHERNET = $00010000
ARP_LENGTHS     = $06040000
ARP_REQUEST     = $00000001
ARP_REPLY       = $00000002

' IP constants

IP_VERSION      = 4
IP_HEADER_LEN   = 5    ' In 32-bit words
IP_PROTO_ICMP   = 1
IP_PROTO_TCP    = 6
IP_PROTO_UDP    = 17

' ICMP constants

ICMP_ECHO_REQUEST  = 8
ICMP_ECHO_REPLY    = 0


'==============================================================================
' Implementation-specific constants
'==============================================================================

' These constants represent values that are not intrinsic to the ENC28J60
' chip itself, but are constants specific to this driver implementation.

' Maximum amount of data we have to buffer for one frame:
' 6-byte DA, 6-byte SA, 2-byte type/length, up to 1500 data
' and padding bytes, 4-byte FCS.

MAX_FRAME_LEN   = 1518

' Allocation for the chip's 8K SRAM buffer. We can partition the 8K any way we
' like. For the best performance, we should be able to transmit/receive and
' prepare to transmit/receive simultaneously. This means we need a transmit
' buffer large enough to store at least two full ethernet frames. For simplicity,
' we use a pure double-buffering scheme- so we have exactly two transmit buffers,
' each of which hold a single frame.
'
' It would be more efficient to treat the transmit buffer space as a continuous
' ring buffer, but that would require a lot of extra cog memory for the code and
' data necessary to track multiple frames that are all ready to transmit.
'
' All remaining memory is used for the receive buffer. The receive buffer is
' more important anyway, since it's what lets us avoid dropping packets even if
' they arrive while we were busy doing something else, like preparing to transmit.
' We always want the receive buffer to be larger than the transmit buffer.
'
' Note that transmitted packets include 8 bytes of extra control/status information.
' We add this to MAX_FRAME_LEN to calculate the actual amount of buffer space that's
' required.
'
' The start/end values are inclusive the bytes pointed to by 'start' and 'end'
' are both part of the buffer.
'
' Also note Rev B5 errata 3: We must place the receive buffer at offset zero.
'
' And Rev B5 errata 11: RX_BUF_END must always be odd.

MEM_SIZE        = 8192           
TX_BUF2_END     = MEM_SIZE - 1                            ' One 1528-byte TX buffer
TX_BUF2_START   = MEM_SIZE - (MAX_FRAME_LEN + 8) * 1
TX_BUF1_END     = TX_BUF2_START - 1                       ' Another 1528-byte TX buffer
TX_BUF1_START   = MEM_SIZE - (MAX_FRAME_LEN + 8) * 2
RX_BUF_END      = TX_BUF1_START - 1                       ' 5140-byte RX buffer
RX_BUF_START    = 0

' Default PHLCON value, without LED definitions.
' Enable pulse stretching, with the medium duration.

PHLCON_TEMPLATE = PHLCON_RESERVED | STRCH | LFRQ_0

' Default LED assignments:
'
'    - LED A shows link and receive activity
'    - LED B shows transmit activity

PHLCON_INIT     = PHLCON_TEMPLATE | (LED_LINK_RX << LACFG_BIT) | (LED_TX << LBCFG_BIT)


DAT
'==============================================================================
' Low-level Initialization Code
'==============================================================================

' All initialization code is optimized for space rather than speed, so it's written
' in Spin using a totally separate (and very slow) SPI engine. Init code runs before
' the main driver cog starts.
'
' This SPI engine was clocked at 24 kBps on an 80 MHz sysetm clock. No speed
' demon for sure, but it's plenty fast enough to run through the initialization
' tables once.
'
' All initialization-only routines start with "init_", and they're separated into
' spi, reg, and phy layers.
'
' Initialization-only variables do not need to be stored per-object, since we only
' allow one etherCog to be initializing at a time. So, we use this DAT block.

cs_pin        byte  0
si_pin        byte  0
so_pin        byte  0
sck_pin       byte  0


PRI init_spi_begin
  ' Begin an SPI command (pull CS low)
  ' This is slow enough that we definitely don't need to worry about setup/hold time.

  outa[cs_pin]~

PRI init_spi_end
  ' End an SPI command (pull CS high)
  ' This is slow enough that we definitely don't need to worry about setup/hold time.

  outa[cs_pin]~~

PRI init_spi_Write8(data)
  ' Write an 8-bit value to the SPI port, MSB first, from the 8 least significant
  ' bits of 'data'. This is a very slow implementation, used only during initialization.

  repeat 8
    outa[si_pin] := (data <<= 1) >> 8
    outa[sck_pin]~~
    outa[sck_pin]~

PRI init_reg_BankSel(reg)
  ' Select the proper bank for a register, given in the 8-bit encoded
  ' format. Since we're optimizing for space rather than speed, this
  ' always writes to ECON1, without checking to see whether we were
  ' already in the right bank. During initialization, we know ECON1
  ' is always zero (other than the bank bits).

  init_spi_begin
  init_spi_Write8(constant(SPI_WCR | (ECON1 & %11111)))
  init_spi_Write8((reg >> 5) & $03)
  init_spi_end

PRI init_reg_Write(reg, data)
  ' Write an 8-bit value to an ETH/MAC register. Automatically switches banks.

  init_reg_BankSel(reg)
  init_spi_begin
  init_spi_Write8(SPI_WCR | (reg & %11111))
  init_spi_Write8(data)
  init_spi_end

PRI init_reg_WriteTable(table) | reg
  ' Perform a list of register writes, listed as 8-bit name/value pairs
  ' in a table in hub memory. The table is terminated by a zero byte.

  repeat
    reg := BYTE[table++]
    if reg
      init_reg_Write(reg, BYTE[table++])
    else
      return

{ (Currently unused)      

PRI init_spi_Read8 : data
  ' Read an 8-bit value from the SPI port, MSB first.
  ' This is a very slow implementation, used only during initialization.

  data~
  repeat 8
    outa[sck_pin]~~
    data := (data << 1) | ina[so_pin]
    outa[sck_pin]~

PRI init_reg_Read(reg) : data
  ' Read an 8-bit value from an ETH/MAC register. Automatically switches banks.
  ' This requires the 9-bit form of the encoded register name.

  init_reg_BankSel(reg)
  init_spi_begin
  init_spi_Write8(SPI_RCR | (reg & %11111))

  if reg & $100
    ' Read dummy byte, needed by MAC/MII registers.
    init_spi_Read8

  data := init_spi_Read8
  init_spi_end

}

    
DAT

'==============================================================================
' Initialization Tables
'==============================================================================

                        '
                        ' reg_init_table --
                        '
                        '   This is a table of register names and their initial values.
                        '   Almost any non-PHY (ETH/MAC) register can be specified here.
                        '   The register names include bank information, so no explicit
                        '   bank switches are necessary.
                        '
                        '   ECON1 cannot be written from this table, it is special
                        '   cased because of how it's used internally for bank switching.
                        '
                        '   During initialization, ECON1 is always zero. After the driver
                        '   cog is fully initialized, it sets RXEN to turn on the receiver.
                        '
reg_init_table
                        ' Automatic buffer pointer increment, no power save mode.

                        byte    ECON2,     AUTOINC

                        ' Disable interrupts, clear interrupt flags.
                        '
                        ' Since we have a dedicated cog on the Propeller,
                        ' there isn't really anything for us to gain by enabling
                        ' interrupts. It will increase code size, and it won't
                        ' decrease receive latency at all. The only real benefit is
                        ' that it would decrease the average latency in detecting
                        ' event sources other than the ENC28J60, such as new transmit
                        ' data from other cogs.

                        byte    EIE,       0
                        byte    EIR,       DMAIF | TXIF | TXERIF | RXERIF
                        
                        ' Receive buffer allocation.
                        '
                        ' Note that we don't have to explicitly allocate the transmit
                        ' buffer now- we set ETXST/ETXND only when we have a specific
                        ' frame ready to transmit.                        

                        byte    ERXSTL,    RX_BUF_START & $FF
                        byte    ERXSTH,    RX_BUF_START >> 8
                        byte    ERXNDL,    RX_BUF_END & $FF
                        byte    ERXNDH,    RX_BUF_END >> 8

                        ' Initial position of the RX read pointer. This is the boundary
                        ' address, indicating how much of the FIFO space is available to
                        ' the MAC. The hardware will write to all bytes up to but not
                        ' including this one.
                        '
                        ' Order matters: we must write high byte last.
                        
                        byte    ERXRDPTL,  RX_BUF_END & $FF 
                        byte    ERXRDPTH,  RX_BUF_END >> 8

                        ' Default SPI read pointer. We initialize this at the beginning
                        ' of the read buffer, and we just let it follow us around the FIFO
                        ' as we receive frames.

                        byte    ERDPTL,    RX_BUF_START & $FF
                        byte    ERDPTH,    RX_BUF_START >> 8
                        
                        ' Receive filters.
                        '
                        ' The ENC28J60 supports several types of packet filtering,
                        ' including an advanced hash table filter and a pattern matching
                        ' filter. Currently we don't use these features. Just program it
                        ' to accept any broadcast packets, and any unicast packets directed
                        ' at this node's MAC address. Also, have the chip verify CRC in
                        ' hardware, and reject any packets with bad CRCs.
                        '
                        ' If you want to run in promiscuous mode, set ERXFCON to 0. Note
                        ' that some of the protocol handling code in etherCog assumes that
                        ' all received packets are addressed to our MAC address, so other
                        ' changes will be necessary.

                        byte    ERXFCON,   UCEN | CRCEN | BCEN
                        
                        ' MAC initialization.
                        '
                        ' Enable the MAC to receive frames, and enable IEEE flow control.
                        ' Ignore control frames. (Don't pass them on to the filter.)

                        byte    MACON1,    TXPAUS | RXPAUS | MARXEN

                        ' Half-duplex mode, padding to 60 bytes, and hardware CRC on
                        ' transmit. Also enable hardware frame length checking. 

                        byte    MACON3,    TXCRCEN | PADCFG0 | FRMLNEN

                        ' For IEEE 802.3 compliance, wait indefinitely for the medium to
                        ' become free before starting a transmission.

                        byte    MACON4,    DEFER

                        ' Back-to-back inter-packet gap setting.
                        ' The minimum gap specified by the IEEE is 9.6us, encoded
                        ' here as $15 for full-duplex and $12 for half-duplex. 
                        
                        byte    MABBIPG,   $12

                        ' The non-back-to-back inter-packet gap. The datasheet
                        ' recommends $0c12 for half-duplex applocations, or $0012
                        ' for full-duplex.

                        byte    MAIPGL,    $12
                        byte    MAIPGH,    $0c

                        ' Maximum permitted frame length, including header and CRC.

                        byte    MAMXFLL,   MAX_FRAME_LEN & $FF
                        byte    MAMXFLH,   MAX_FRAME_LEN >> 8

                        ' Retransmission maximum and collision window, respectively.
                        ' These values are used for half-duplex links.
                        ' The current values here are the hardware defaults,
                        ' included for completeness.

                        byte    MACLCON1,  $0F
                        byte    MACLCON2,  $37

                        ' XXX: MAC address

                        byte    MAADR1,    $10
                        byte    MAADR2,    $00
                        byte    MAADR3,    $00
                        byte    MAADR4,    $00
                        byte    MAADR5,    $00
                        byte    MAADR6,    $01

                        ' Now initialize PHY registers. We have few enough PHY regs
                        ' that doesn't save us space to have a separate table of PHY
                        ' registers. It's cheaper to just encode them manually in this
                        ' table. Some things to note:
                        '
                        '   - We must write the low byte first, high byte last.
                        '     Writing the high byte actually triggers the PHY write.
                        '
                        '   - Normally we'd need to wait for the write to complete,
                        '     but the initialization-only SPI code is so slow that
                        '     by comparison the PHY completes pretty much instantly.
                        '     (PHY writes take ~10us, our Spin SPI code is about 40us
                        '     per bit period!)

                        ' Diable half-duplex loopback (Explicitly turn off the receiver
                        ' when we're transmitting on a half-duplex link.) This bit is ignored
                        ' in full-duplex mode.

                        byte    MIREGADR,   PHCON2
                        byte    MIWRL,      HDLDIS & $FF
                        byte    MIWRH,      HDLDIS >> 8

                        ' PHY in normal operation (no loopback, no power-saving mode)
                        ' and we're forcing half-duplex operation. Without forcing
                        ' half- or full-duplex, it will autodetect based on the polarity
                        ' of an external LED. We want to be sure the PHY and MAC agree
                        ' on duplex configuration.

                        byte    MIREGADR,   PHCON1
                        byte    MIWRL,      0
                        byte    MIWRH,      0

                        ' Disable PHY interrupts
                        
                        byte    MIREGADR,   PHIE
                        byte    MIWRL,      0
                        byte    MIWRH,      0

                        ' LED configuration. See the definition of PHLCON_INIT above.

                        byte    MIREGADR,   PHLCON
                        byte    MIWRL,      PHLCON_INIT & $FF
                        byte    MIWRH,      PHLCON_INIT >> 8

                        ' End of table
                        byte    0


DAT

'==============================================================================
' Driver Cog
'==============================================================================

                        org

                        '======================================================
                        ' Initialization
                        '======================================================

entry
                        mov     dira, init_dira         ' Take control over the SPI bus
                        mov     ctra, init_ctra
                                                
                        wrlong  r0, par                 ' Tell the Spin cog that we took the bus
                                                        '   (r0 is initialized data, set to 1)
:release                rdlong  r0, par wz              ' Wait for it to release the bus
              if_nz     jmp     #:release

                        call    #reg_UpdateECON1        ' Enable packet reception
                        

                        '======================================================
                        ' Main Loop
                        '======================================================

mainLoop
                        mov     reg_name, #EPKTCNT      ' Poll EPKTCNT (Rev B5 errata 4)
                        call    #reg_Read
                        mov     rx_pktcnt, reg_data wz
              if_nz     call    #rx_frame               ' Receive any pending frames

                        call    #tx_poll                ' Can we transmit?
              
                        jmp     #mainLoop


                        '======================================================
                        ' TCP protocol
                        '======================================================

rx_tcp
                        ' XXX, not yet
                        jmp     #rx_frame_finish


                        '======================================================
                        ' UDP protocol
                        '======================================================

rx_udp
                        ' XXX, not yet
                        jmp     #rx_frame_finish


                        '======================================================
                        ' IPv4 Protocol
                        '======================================================

                        '
                        ' rx_ipv4 --
                        '
                        '   Receive handler for IPv4 frames. Validate and store
                        '   the IP header, then dispatch to a protocol-specific handler.
                        '
rx_ipv4
                        ' First 32 bits of IP header: Version, header length,
                        ' Type of Service, and Total Length. Verify the version
                        ' and IP header length is what we support, and store the
                        ' IP packet length.

                        call    #spi_read32
                        mov     ip_length, spi_reg
                        and     ip_length, hFFFF
                        shr     spi_reg, #24
                        cmp     spi_reg, #IP_HEADER_LEN | (IP_VERSION << 4) wz
              if_nz     jmp     #rx_frame_finish        ' Includes unsupported IP options

                        ' Second 32 bits: Identification word, flags, fragment
                        ' offset. We ignore the identification word, and make
                        ' sure the More Fragments flag and fragment offset are all
                        ' zero. We don't support fragmented IP packets, so this is
                        ' where they get dropped.

                        call    #spi_read32
                        shl     spi_reg, #16            ' Ignore identification
                        rcl     spi_reg, #1 wc          ' Extract More Fragments bit
                        shl     spi_reg, #2 wz          ' Ignore Don't Fragment and Reserved
           if_nz_or_c   jmp     #rx_frame_finish        ' Fragmented packet       

                        ' Third 32 bits: TTL, protocol, header checksum. We don't
                        ' bother checking the checksum, and we aren't routing
                        ' packets, so all we care about is the protocol. Stow it
                        ' temporarily in reg_name.

                        call    #spi_read32
                        mov     reg_name, spi_reg
                        shr     reg_name, #16
                        and     reg_name, #$FF

                        ' Fourth 32 bits: Source address. Store it.

                        call    #spi_read32
                        mov     peer_IP, spi_reg

                        ' Fifth 32 bits: Destination address. Make sure this
                        ' packet is for us. If not, drop it.
                        '
                        ' Note that we don't support broadcast IP yet.
                        ' All broadcast IP packets are dropped. (But this
                        ' is usually a good thing...)

                        call    #spi_read32
                        cmp     spi_reg, local_IP wz
              if_nz     jmp     #rx_frame_finish        ' Packet not for us

                        ' We know there are no option words (we checked the IP
                        ' header length) so the rest of this frame is protocol
                        ' data. Branch to a protocol-specific handler...

                        cmp     reg_name, #IP_PROTO_UDP wz
              if_z      jmp     #rx_udp

                        cmp     reg_name, #IP_PROTO_TCP wz
              if_z      jmp     #rx_tcp

                        cmp     reg_name, #IP_PROTO_ICMP wz
              if_z      mov     pc, #rx_icmp
              if_z      jmp     #lmm_vm                 ' ICMP is in the LMM overlay.

                        ' No handler.. drop the packet
                        jmp     #rx_frame_finish
                        
                        '
                        ' tx_ipv4 --
                        '
                        '   Start transmitting an IPv4 packet. Waits for the
                        '   transmit back-buffer to become available if necessary.
                        '   Assembles Ethernet and IPv4 headers using peer_MAC,
                        '   peer_IP, ip_length, and proto.
                        '
tx_ipv4
                        shl     proto, #16              ' Move IP protocol to upper 16 bits
                        or      proto, c_ethertype_ipv4 ' Ethernet protocol is IPv4
                        call    #tx_begin               ' Write Ethernet frame
                        mov     checksum, #0            ' Reset checksum
                        
                        ' First 32 bits of IP header: Version, header length,
                        ' Type of Service, and Total Length.

                        mov     spi_reg, #IP_HEADER_LEN | (IP_VERSION << 4)
                        shl     spi_reg, #24
                        or      spi_reg, ip_length
                        call    #ip_sum32
                        call    #spi_Write32

                        ' Second 32 bits: Identification word, flags, fragment
                        ' offset.

                        mov     spi_reg, tx_ip_flags
                        call    #ip_sum32
                        call    #spi_Write32
                        
                        ' Third 32 bits: TTL, protocol, header checksum.
                        ' Protocol comes from 'proto', which is now shifted
                        ' into place properly. Use a default TTL.

                        mov     spi_reg, local_IP       ' Add local/peer IPs to checksum
                        call    #ip_sum32
                        mov     spi_reg, peer_IP
                        call    #ip_sum32                        
                        
                        mov     spi_reg, proto
                        or      spi_reg, tx_ip_ttl
                        andn    spi_reg, hFFFF          ' Zero checksum field
                        call    #ip_sum32               ' Add other bits to checksum
                        xor     checksum, hFFFF         ' Finish checksum
                        or      spi_reg, checksum       '   .. and include it

                        call    #spi_Write32

                        ' Fourth 32 bits: Source address. This is us.

                        mov     spi_reg, local_IP
                        call    #spi_Write32

                        ' Fifth 32 bits: Destination address. This is our peer.

                        mov     spi_reg, peer_IP
                        call    #spi_Write32
tx_ipv4_ret             ret

                        '
                        ' ip_sum32 --
                        '
                        '   Update the 'checksum' variable by performing
                        '   one's complement addition (add the carry bit)
                        '   for each of the 16-bit words in spi_reg. Does
                        '   not modify spi_reg.
                        '
                        '   'checksum' always contains the one's complement
                        '   of an IP-style checksum in its lower 16 bits.
                        '
                        '   Note that we must calculate checksums in software,
                        '   even though the ENC28J60's DMA engine can perform
                        '   them in hardware. This is due to Rev B5 errata 15:
                        '   received packets may be dropped if we use the
                        '   hardware checksum module at any time.
                        '
ip_sum32                shl     checksum, #16           ' Move checksum to upper word, lower word 0
                        add     checksum, spi_reg wc    ' Add high word (ignore low word)
              if_c      add     checksum, h10000        ' Carry one's complement
                        mov     r0, spi_reg             ' Justify low word
                        shl     r0, #16
                        add     checksum, r0 wc         ' Add low word
              if_c      add     checksum, h10000        ' Carry one's complement
                        shr     checksum, #16           ' Move checksum back, discard temp bits
ip_sum32_ret            ret                        
                               

                        '======================================================
                        ' Ethernet Frame Layer
                        '======================================================

                        '
                        ' rx_frame --
                        '
                        '   Receive a single Ethernet frame header, and dispatch
                        '   to an appropriate handler for the rest of the frame.
                        '   The handler returns to rx_frame_finish, where we
                        '   acknowledge that we've finished processing the frame.
                        '
                        '   Must only be called when a packet is waiting (EPKTCNT > 0)
                        '                        
rx_frame
                        ' The read pointer (ERDPTR) is already pointed at the beginning
                        ' of the first available packet. Start reading it...

                        call    #spi_ReadBufMem

                        call    #spi_Read8              ' Read 16-bit Next Packet Pointer, low byte first. 
                        mov     rx_next, spi_reg        '   (spi_Read16 would give us the wrong byte order)   
                        call    #spi_Read8
                        shl     spi_reg, #8
                        or      rx_next, spi_reg

                        call    #spi_Read32             ' Ignore 32-bit status vector

                        ' At this point, we've read the ENC28J60's header, and all
                        ' subsequent data is from the actual Ethernet frame:
                        '
                        '    1. 48-bit destination address
                        '    2. 48-bit source address
                        '    3. 16-bit EtherType
                        '
                        ' We ignore the destination address, since we already know
                        ' that (due to the hardware filter) this packet is for us. It
                        ' may be a multicast or a unicast packet. If we need to know
                        ' which it was, we could save the receive status word.
                        '
                        ' We store the source address, since it's usually necessary
                        ' to create reply packets.
                        '
                        ' The EtherType tells us what protocol the rest of the packet
                        ' uses. We only care about IP and ARP. Other EtherTypes are
                        ' ignored.

                        call    #spi_Read32             ' Ignore upper 32 bits of DA
                        call    #spi_Read32             ' Ignore lower 16 bits of DA, store upper 16 bits of SA
                        and     spi_reg, hFFFF
                        mov     peer_MAC_high, spi_reg
                        call    #spi_Read32             ' Store lower 32 bits of SA
                        mov     peer_MAC_low, spi_reg

                        call    #spi_Read16             ' Read EtherType. This tells us what protocol the
                                                        ' rest of the frame is using.

                        ' Branch out to protocol-specific handlers...

                        cmp     spi_reg, c_ethertype_ipv4 wz
              if_z      jmp     #rx_ipv4

                        cmp     spi_reg, c_ethertype_arp wz
              if_z      mov     pc, #rx_arp
              if_z      jmp     #lmm_vm                 ' ARP is in the LMM overlay.

rx_frame_finish                        

                        ' Now we're done with the frame. Either we read the whole thing,
                        ' or we decided to ignore it. Free up this buffer space, and
                        ' prepare to read the next frame.

                        mov     reg_name, #ERDPTL       ' Set read pointer to the next frame
                        mov     reg_data, rx_next
                        call    #reg_Write16

                        mov     reg_data, rx_next       ' Compute ERXRDPT such that it's always
                        sub     reg_data, #1 wc         '    odd. This is Rev B5 errata 11.
              if_c      mov     reg_data, c_rx_buf_end
                        mov     reg_name, #ERXRDPTL     ' Write it
                        call    #reg_Write16
                        
                        mov     reg_name, #ECON2        ' Acknowledge the interrupt
                        mov     reg_data, #(AUTOINC | PKTDEC)
                        call    #reg_Write

                        sub     rx_pktcnt, #1 wz        ' Decrement local packet count                        
              if_nz     jmp     #rx_frame               ' Keep recieving if there are more packets.
                        
rx_frame_ret            ret

                        '
                        ' tx_begin --
                        '
                        '   Begin preparing a new frame for transmission,
                        '   and write the ethernet frame header.
                        '
                        '   Transmission is double-buffered, so this can be used
                        '   even if the hardware is still busy transmitting the
                        '   'front' buffer's frame.
                        '
                        '   This routine will start writing into the transmit
                        '   back buffer. The caller should start writing an ethernet
                        '   frame using the spi_Write functions. When finished, the
                        '   packet is made ready to transmit by adding spi_write_count
                        '   to tx_back_buf_end.
                        '
tx_begin                cmp     tx_back_buf, tx_back_buf_end wz
              if_z      jmp     #:ready
                        call    #tx_poll
                        jmp     #tx_begin

:ready                  mov     reg_name, #EWRPTL       ' Set SPI write pointer
                        mov     reg_data, tx_back_buf
                        call    #reg_Write16

                        call    #spi_WriteBufMem

                        mov     spi_reg, #0             ' Write control byte (zero, no overrides needed)
                        call    #spi_Write8

                        mov     spi_write_count, #0     ' Start counting written bytes. We start after
                                                        '   the control byte, because we actually want
                                                        '   one less than the number of total bytes, so
                                                        '   that we can add this number to the tx_back_buf
                                                        '   pointer to get a pointer to the last byte in
                                                        '   the frame.

                        mov     spi_reg, peer_MAC_high  ' Destination Address
                        call    #spi_Write16
                        mov     spi_reg, peer_MAC_low
                        call    #spi_Write32

                        mov     spi_reg, local_MAC_high ' Source Address
                        call    #spi_Write16
                        mov     spi_reg, local_MAC_low
                        call    #spi_Write32

                        mov     spi_reg, proto          ' EtherType (protocol)
                        call    #spi_Write16

tx_begin_ret            ret

                        '
                        ' tx_poll --
                        '
                        '   Wait for the transmit hardware to become idle. Once it's idle, swap
                        '   the front and back buffers, and start sending a new packet if we need to.
                        '
tx_poll                 mov     reg_name, #ECON1
                        call    #reg_Read
                        test    reg_data, #TXRTS wz 
              if_nz     jmp     #tx_poll_ret            ' Transmitter is busy.

                        ' Swap front/back. We can do this without a temporary, since front==front_end.
                        cmp     tx_front_buf, tx_front_buf_end wz
              if_z      mov     tx_front_buf_end, tx_back_buf_end
              if_z      mov     tx_back_buf_end, tx_front_buf
              if_z      mov     tx_front_buf, tx_back_buf
              if_z      mov     tx_back_buf, tx_back_buf_end

                        cmp     tx_front_buf, tx_front_buf_end wz
              if_z      jmp     #tx_poll_ret            ' No data to transmit

                        or      reg_ECON1, #TXRST       ' Reset transmitter for Rev B5 errata 10  
                        call    #reg_UpdateECON1        ' Toggle TXRST on
                        call    #reg_UpdateECON1        ' Auto-clear TXRST  

                        mov     reg_name, #ETXSTL       ' Set transmit start pointer
                        mov     reg_data, tx_front_buf
                        call    #reg_Write16

                        mov     reg_name, #ETXNDL       ' Write transmit end pointer
                        mov     reg_data, tx_front_buf_end
                        call    #reg_Write16

                        or      reg_ECON1, #TXRTS       ' Start transmitting
                        call    #reg_UpdateECON1

                        ' Next time the transmitter becomes idle, the backbuffer will be free.
                        mov     tx_front_buf_end, tx_front_buf

tx_poll_ret             ret     

                        
                        '======================================================
                        ' Register Access Layer
                        '======================================================
                        
                        ' This layer implements low-level control over the
                        ' ENC28J60's main register file and PHY registers.
                        ' It's responsible for memory banking, controlling the
                        ' CS line, and performing PHY read/write sequences.

                        '
                        ' reg_Write --
                        '
                        '   Write any 8-bit register other than ECON1, named by reg_name.
                        '   Uses data from the low 8 bits of reg_data.
                        '
                        '   Guaranteed not to modify reg_data.
                        '
reg_Write               call    #reg_BankSel
                        call    #spi_Begin
                        or      spi_reg, #SPI_WCR
                        call    #spi_Write8
                        mov     spi_reg, reg_data
                        call    #spi_Write8
reg_Write_ret           ret

                        '
                        ' reg_UpdateECON1 --
                        '
                        '   This is a multi-function routine for updating bits in ECON1.
                        '   It's actually pretty simple, but we reuse it in various ways
                        '   in order to save code space.
                        '
                        '    - Bank bits are unconditionally set from reg_ECON1, and
                        '      reg_ECON1 is not modified. We send a BFC command to clear
                        '      the hardware's bank bits, then we OR in the correct
                        '      bits in a BFS.
                        '
                        '    - Other bits are OR'ed with the bits in reg_ECON1, and
                        '      those bits in reg_ECON1 are cleared afterwards. This
                        '      lets us queue up 'enable' bits (transmit/receive, reset)
                        '      in reg_ECON1, and this routine transfers those bits to
                        '      the real hardware. We never clear non-bank bits in the
                        '      real ECON1, and we'll never set an ECON1 bit more than
                        '      once unless it was set in reg_ECON1 again.
                        '
                        '    - Reset bits are automatically cleared, along with the bank
                        '      bits. This makes it possible to momentarily reset the
                        '      transmit/receive logic by writing the reset bit to reg_ECON1
                        '      and calling this routine twice.
                        '
reg_UpdateECON1         call    #spi_Begin              ' Clear bank/reset bits
                        mov     spi_reg, #SPI_BFC | (%11111 & ECON1)
                        call    #spi_Write8
                        mov     spi_reg, #BSEL0 | BSEL1 | TXRST | RXRST
                        call    #spi_Write8

                        call    #spi_Begin              ' Set all bits
                        mov     spi_reg, #SPI_BFS | (%11111 & ECON1)
                        call    #spi_Write8
                        mov     spi_reg, reg_ECON1
                        call    #spi_Write8

                        and     reg_ECON1, #$03         ' Clear all local non-bank bits
reg_UpdateECON1_ret     ret

                        '
                        ' reg_Write16 --
                        '
                        '   Write a 16-bit value to a pair of 8-bit registers, named
                        '   by reg_name (low) and reg_name+1 (high). Uses the low
                        '   16-bits of reg_data, without modifying reg_data.
                        '
                        '   Always writes the high byte last, as is required by
                        '   registers which are latched in hardware after a write.
                        '
reg_Write16             call    #reg_Write
                        add     reg_name, #1
                        shr     reg_data, #8
                        call    #reg_Write
reg_Write16_ret         ret

                        '
                        ' reg_Read --
                        '
                        '   Read an 8-bit register, named by reg_name.
                        '   Returns the value via reg_data.
                        '
reg_Read                call    #reg_BankSel
                        call    #spi_Begin
                        or      spi_reg, #SPI_RCR
                        call    #spi_Write8
                        test    reg_name, #$100 wz      ' Read dummy byte?
              if_nz     call    #spi_Read8              
                        call    #spi_Read8
                        mov     reg_data, spi_reg
reg_Read_ret            ret

                        '
                        ' reg_BankSel --
                        '
                        '   Select the proper bank for the register in
                        '   reg_name, and load the lower 5 bits of the
                        '   register name into spi_data.
                        '
                        '   Must not modify reg_data.
                        '
reg_BankSel             test    reg_name, #$80 wz       ' Is the register unbanked?
              if_nz     mov     r0, reg_name            ' Compute difference between...
              if_nz     shr     r0, #5                  '   reg_name[6:5]
              if_nz     xor     r0, reg_ECON1           '   and reg_ECON[1:0]
              if_nz     and     r0, #%11 wz             ' Already in the right bank?
              if_nz     xor     reg_ECON1, r0           ' Set new bank
              if_nz     call    #reg_UpdateECON1                        
                        mov     spi_reg, reg_name       ' Save lower 5 bits
                        and     spi_reg, #%11111
reg_BankSel_ret         ret

                        { (Currently unused)

                        '
                        ' phy_Write --
                        ' 
                        '   Write a 16-bit PHY register, named by phy_name,
                        '   using a value from phy_data.
                        '
phy_Write               mov     reg_name, #MIREGADR     ' Set PHY address
                        mov     reg_data, phy_name
                        call    #reg_Write
                        mov     reg_name, #MIWRL        ' Write low byte
                        mov     reg_data, phy_data
                        call    #reg_Write
                        mov     reg_name, #MIWRH        ' Write high byte and begin write
                        shr     reg_data, #8
                        call    #reg_Write
                        call    #phy_Wait               ' Wait for PHY to finish (~10uS)
phy_Write_ret           ret

                        '
                        ' phy_Read --
                        ' 
                        '   Read a 16-bit PHY register, named by phy_name,
                        '   and store the value in phy_data.
                        '
phy_Read                mov     reg_name, #MIREGADR     ' Set PHY address
                        mov     reg_data, phy_name
                        call    #reg_Write
                        mov     reg_name, #MICMD        ' Begin read
                        mov     reg_data, #MIIRD
                        call    #reg_Write
                        call    #phy_Wait               ' Wait for PHY to finish (~10uS)
                        mov     reg_name, #MICMD        ' End read
                        mov     reg_data, #0
                        call    #reg_Write
                        mov     reg_name, #MIRDL        ' Read low byte
                        call    #reg_Read
                        mov     phy_data, reg_data
                        mov     reg_name, #MIRDH        ' Read high byte
                        call    #reg_Read
                        shl     reg_data, #8
                        or      phy_data, reg_data
phy_Read_ret            ret                  

                        '
                        ' phy_Wait --
                        '
                        '   Wait for the PHY to become non-busy. 
                        '
                        '   XXX: Might want a timeout here...
                        '
phy_Wait                mov     reg_name, #MISTAT
                        call    #reg_Read
                        test    reg_data, #BUSY wz
              if_nz     jmp     #phy_Wait
phy_Wait_ret            ret

                        }
                        
                        '======================================================
                        ' Fast SPI Engine
                        '======================================================
                        
                        ' This SPI engine uses a very carefully timed CTRA to
                        ' generate the clock pulses while an unrolled loop reads
                        ' the bus at two instructions per bit. This gives an SPI
                        ' clock rate 1/8 the system clock. At an 80Mhz system clock,
                        ' that's an SPI data rate of 10 Mbit/sec!
                        '
                        ' Only touch this code if you have an oscilloscope at hand :)

                        '
                        ' spi_WriteBufMem --
                        '
                        '   Begin writing to the ENC28J60's buffer memory, at the
                        '   current write location. Does not affect spi_write_count.
                        '
spi_WriteBufMem         sub     spi_write_count, #1     ' Undo effects of Write8 below
                        call    #spi_Begin
                        mov     spi_reg, #SPI_WBM
                        call    #spi_Write8
spi_WriteBufMem_ret     ret

                        '
                        ' spi_ReadBufMem --
                        '
                        '   Begin reading from the ENC28J60's buffer memory, at the
                        '   current read location. Does not affect spi_write_count.
                        '
spi_ReadBufMem          sub     spi_write_count, #1
                        call    #spi_Begin
                        mov     spi_reg, #SPI_RBM
                        call    #spi_Write8
spi_ReadBufMem_ret      ret
                        
                        '
                        ' spi_Begin --
                        '
                        '   End the previous SPI command, and begin a new one.
                        '   We don't have an explicit spi_End, to save code space.
                        '   The etherCog always begins a new command shortly after
                        '   ending any command, so there is really nothing to gain
                        '   by explicitly ending our commands.
                        '
                        '   The ENC28J60's SPI interface has a setup time and a
                        '   disable time of 50ns each, or one instruction at 80 MHz.
                        '   The hold time is 210ns for MAC and MII register accesses.
                        '   At worst, this requires us to waste 16 extra clock cycles.
                        '   This would be four no-ops, or the equivalent but smaller
                        '   two TJZ/TJNZ instructions which fail to branch.
                        '
spi_Begin               tjz     hFFFF, #0
                        tjz     hFFFF, #0
                        or      outa, cs_mask
                        tjz     hFFFF, #0
                        andn    outa, cs_mask
spi_Begin_ret           ret

                        '
                        ' spi_Write8 --
                        '
                        '   Write the upper 8 bits of spi_reg to the SPI port.
                        '   Increments spi_write_count by 1.
                        '
                        ' spi_ShiftOut8 --
                        '
                        '   A variant of Write8 which doesn't justify spi_reg first.
                        ' 
spi_Write8              shl     spi_reg, #24            ' Left justify MSB
spi_ShiftOut8           rcl     spi_reg, #1 wc          ' Shift bit 7
                        mov     phsa, #0                ' Rising edge at center of each bit
                        muxc    outa, si_mask           ' Output bit 7
                        mov     frqa, frq_8clk          ' First clock edge, period of 8 clock cycles
                        rcl     spi_reg, #1 wc          ' Bit 6
                        muxc    outa, si_mask         
                        rcl     spi_reg, #1 wc          ' Bit 5
                        muxc    outa, si_mask         
                        rcl     spi_reg, #1 wc          ' Bit 4
                        muxc    outa, si_mask         
                        rcl     spi_reg, #1 wc          ' Bit 3
                        muxc    outa, si_mask         
                        rcl     spi_reg, #1 wc          ' Bit 2
                        muxc    outa, si_mask         
                        rcl     spi_reg, #1 wc          ' Bit 1
                        muxc    outa, si_mask         
                        rcl     spi_reg, #1 wc          ' Bit 0
                        muxc    outa, si_mask
                        nop                             ' Finish last clock cycle
                        mov     frqa, #0                ' Turn off clock
                        andn    outa, si_mask           ' Turn off SI
                        add     spi_write_count, #1     ' Update spi_write_count
spi_ShiftOut8_ret
spi_Write8_ret          ret

                        '
                        ' spi_Write16 --
                        '
                        '   Write 16 bits to the SPI port, in big endian byte order.
                        '   Increments spi_write_count by 2.
                        '
spi_Write16             shl     spi_reg, #16
                        call    #spi_ShiftOut8
                        call    #spi_ShiftOut8
spi_Write16_ret         ret   

                        '
                        ' spi_Write32 --
                        '
                        '   Write entire 32-bit contents of spi_reg to the SPI port.
                        '   Increments spi_write_count by 4.
                        ' 
spi_Write32             rcl     spi_reg, #1 wc          ' Shift bit 31
                        mov     phsa, #0                ' Rising edge at center of each bit
                        muxc    outa, si_mask           ' Output bit 31
                        mov     frqa, frq_8clk          ' First clock edge, period of 8 clock cycles
                        rcl     spi_reg, #1 wc          ' Bit 30
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 29
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 28
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 27
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 26
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 25
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 24
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 23
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 22
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 21
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 20
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 19
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 18
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 17
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 16
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 15
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 14
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 13
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 12
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 11
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 10
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 9
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 8
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 7
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 6
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 5
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 4
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 3
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 2
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 1
                        muxc    outa, si_mask
                        rcl     spi_reg, #1 wc          ' Bit 0
                        muxc    outa, si_mask
                        nop                             ' Finish last clock cycle
                        mov     frqa, #0                ' Turn off clock
                        andn    outa, si_mask           ' Turn off SI
                        add     spi_write_count, #4
spi_Write32_ret         ret

                        '
                        ' spi_Read8 --
                        '
                        '   Read 8 bits from the SPI port, and return them in the lower 8
                        '   bits of spi_reg.
                        '
                        ' spi_ShiftIn8 --
                        '
                        '   A variant of Read8 that doesn't clear the spi_reg, but shifts it
                        '   left by 8 bits as we read into the 8 LSBs.
                        '
spi_Read8               mov     spi_reg, #0             ' Clear unused bits
spi_ShiftIn8            mov     phsa, #0                ' Rising edge at center of each bit
                        mov     frqa, frq_8clk          ' First clock edge, period of 8 clock cycles
                        test    so_mask, ina wc         ' Bit 7
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 6
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 5
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 4
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 3
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 2
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 1
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Sample bit 0
                        mov     frqa, #0                ' Turn off clock
                        rcl     spi_reg, #1             ' Store bit 0
spi_ShiftIn8_ret
spi_Read8_ret           ret

                        '
                        ' spi_Read16 --
                        '
                        '   Read 16 bits from the SPI port, and return them in the
                        '   entirety of spi_reg. (Big endian byte order)
                        '
spi_Read16              call    #spi_Read8
                        call    #spi_ShiftIn8
spi_Read16_ret          ret                        

                        '
                        ' spi_Read32 --
                        '
                        '   Read 32 bits from the SPI port, and return them in the
                        '   entirety of spi_reg. (Big endian byte order)
                        ' 
spi_Read32              mov     phsa, #0                ' Rising edge at center of each bit
                        mov     frqa, frq_8clk          ' First clock edge, period of 8 clock cycles
                        test    so_mask, ina wc         ' Bit 31
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 30
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 29
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 28
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 27
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 26
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 25
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 24
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 23
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 22
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 21
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 20
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 19
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 18
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 17
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 16
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 15
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 14
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 13
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 12
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 11
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 10
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 9
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 8
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 7
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 6
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 5
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 4
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 3
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 2
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Bit 1
                        rcl     spi_reg, #1
                        test    so_mask, ina wc         ' Sample bit 0
                        mov     frqa, #0                ' Turn off clock
                        rcl     spi_reg, #1             ' Store bit 0
spi_Read32_ret          ret


                        '======================================================
                        ' Large Memory Model VM
                        '======================================================

                        '
                        ' lmm_vm --
                        '
                        '   This is the main loop for a "Large memory model"
                        '   VM, which executes code from hub memory location 'pc'.
                        '   We use LMM for larger and less speed-critical parts
                        '   of the driver cog.
                        '
                        '   LMM code can freely call and jump into non-LMM code,
                        '   but jumping *into* LMM code requires setting the pc
                        '   manually and jumping into lmm_vm.
                        '
                        '   On entry to lmm_vm, pc is automatically translated
                        '   from an overlay-relative address in longs, to a
                        '   hub-memory-relative address in bytes.
                        '              
lmm_vm                  shl     pc, #2
                        add     pc, lmm_base
:loop                   rdlong  :inst1, pc
                        add     pc, #4
:inst1                  nop
                        rdlong  :inst2, pc              ' Unrolled a little...
                        add     pc, #4
:inst2                  nop
                        jmp     #:loop


'------------------------------------------------------------------------------
' Initialized Data
'------------------------------------------------------------------------------

lmm_base                long    0               ' LMM overlay address

hFFFF                   long    $FFFF
h10000                  long    $10000
frq_8clk                long    1 << (32 - 3)   ' 1/8th clock rate

c_rx_buf_end            long    RX_BUF_END
c_ethertype_arp         long    ETHERTYPE_ARP
c_ethertype_ipv4        long    ETHERTYPE_IPV4
c_arp_proto             long    ARP_HW_ETHERNET | ETHERTYPE_IPV4
c_arp_request           long    ARP_LENGTHS | ARP_REQUEST
c_arp_reply             long    ARP_LENGTHS | ARP_REPLY

tx_ip_flags             long    $4000           ' Don't fragment
tx_ip_ttl               long    64 << 24        ' Default IP Time To Live

r0                      long    1               ' Temp, used by 'reg_' layer.

init_dira               long    0
init_ctra               long    0

reg_ECON1               long    RXEN            ' Turn on the receiver at our first UpdateECON1

cs_mask                 long    0
si_mask                 long    0
so_mask                 long    0

tx_front_buf            long    TX_BUF1_START   ' The buffer we're currently transmitting
tx_front_buf_end        long    TX_BUF1_START
tx_back_buf             long    TX_BUF2_START   ' Buffer we're preparing to transmit
tx_back_buf_end         long    TX_BUF2_START

' XXX: configurable address info

local_MAC_high          long    $1000           ' Private_00:00:01
local_MAC_low           long    $00000001
local_IP                long    $c0a80120       ' 192.168.1.32       


'------------------------------------------------------------------------------
' Uninitialized Data
'------------------------------------------------------------------------------

pc                      res     1               ' Large Memory Model program counter

reg_name                res     1
reg_data                res     1
spi_reg                 res     1
spi_write_count         res     1

rx_pktcnt               res     1               ' Number of pending packets (8-bit)
rx_next                 res     1               ' Next Packet Pointer (16-bit)

peer_MAC_high           res     1               ' High 16 bits of peer MAC
peer_MAC_low            res     1               ' Low 32 bits of peer MAC
peer_IP                 res     1               ' IP address of peer

ip_length               res     1               ' Total IP packet length
ip_temp                 res     1               ' 32-bit buffer for IP layer

proto                   res     1               ' Ethernet/IP protocol temp
checksum                res     1               ' Current checksum

                        fit

'==============================================================================
' Large Memory Model overlay
'==============================================================================

' This is where we put large non-speed-critical portions of the
' driver cog: protocol handling, mostly. This code follows
' normal LMM rules. You can jump to cog addresses directly,
' but to jump to a LMM address you must use LMM primitives.
'
' Since we can't create const references to addresses in hub
' memory, we must calculate LMM addresses relative to lmm_base.

                        org
lmm_base_label

                        '======================================================
                        ' ARP Protocol
                        '======================================================

                        '
                        ' rx_arp --
                        '
                        '   Handle an incoming ARP packet. If it's an ARP request
                        '   for our IP address, send a reply. Currently we ignore
                        '   all other ARP packets.
                        '                        
rx_arp
                        ' First 32 bits: Hardware type and protocol type.
                        ' Verify that these are Ethernet and IPv4, respectively.
                        ' If not, ignore the packet.

                        call    #spi_read32
                        cmp     spi_reg, c_arp_proto wz
              if_nz     jmp     #rx_frame_finish

                        ' The next 32 bits: Upper half is constant (address lengths),
                        ' lower half indicates whether this is a request or reply.
                        ' For now, we drop any packet that isn't a request. 

                        call    #spi_read32
                        cmp     spi_reg, c_arp_request wz
              if_nz     jmp     #rx_frame_finish

                        ' Next 48 bits: Sender Hardware Address (SHA).
                        ' Verify that this matches the peer MAC address. If not, someone
                        ' is lying and we should drop the packet.

                        call    #spi_read16
                        cmp     spi_reg, peer_MAC_high wz
              if_nz     jmp     #rx_frame_finish 
                        call    #spi_read32
                        cmp     spi_reg, peer_MAC_low wz
              if_nz     jmp     #rx_frame_finish

                        ' Next 32 bits: Sender Protocol Address (SPA).
                        ' This is the IP address of the sender.

                        call    #spi_read32
                        mov     peer_IP, spi_reg

                        ' Next 48 bits: Target Hardware Address (THA).
                        ' This field is ignored in ARP requests.

                        call    #spi_read16
                        call    #spi_read32

                        ' Next 32 bits: Target Protocol Address (TPA).
                        ' If this is our IP address, we're the target!
                        ' If not, ignore the packet.
                        
                        call    #spi_read32
                        cmp     spi_reg, local_IP wz
              if_nz     jmp     #rx_frame_finish

                        ' Prepare an ARP reply frame. This is written into
                        ' the transmit back-buffer, which we can start transmitting
                        ' at the next tx_poll. If the buffer is busy, we wait for it
                        ' to become available.

                        mov     proto, c_ethertype_arp  ' Start an ARP reply
                        call    #tx_begin                                     

                        mov     spi_reg, c_arp_proto    ' Hardware type and protocol type
                        call    #spi_Write32

                        mov     spi_reg, c_arp_reply    ' Lengths, reply opcode
                        call    #spi_Write32

                        mov     spi_reg, local_MAC_high ' Sender Hardware Address: local MAC
                        call    #spi_Write16
                        mov     spi_reg, local_MAC_low
                        call    #spi_Write32

                        mov     spi_reg, local_IP       ' Sender Protocol Address: Local IP
                        call    #spi_Write32

                        mov     spi_reg, peer_MAC_high  ' Target Hardware Address: SA
                        call    #spi_Write16
                        mov     spi_reg, peer_MAC_low
                        call    #spi_Write32

                        mov     spi_reg, peer_IP        ' Target Protocol Address: Sender's IP
                        call    #spi_Write32

                        ' Finish the reply packet
                        add     tx_back_buf_end, spi_write_count
                        jmp     #rx_frame_finish


                        '======================================================
                        ' ICMP protocol
                        '======================================================

                        ' We may care about other ICMP message types in the future,
                        ' but for now this just responds to Echo Request messages (pings).
                        '
                        ' We copy the ping data directly from the receive FIFO to the
                        ' transmit back-buffer, so we can handle any non-fragmented ping
                        ' size (up to the maximum ethernet frame size). This is slow,
                        ' but it doesn't require any hub memory.
                        '
                        ' We also take a shortcut on calculating the ICMP checksum.
                        ' Instead of calculating our own checksum (which requires examining
                        ' the entire packet data), we just modify the sender's checksum
                        ' to account for our header changes. If the sender's checksum was
                        ' right, our reply will be right.
rx_icmp
                        call    #spi_Read32             ' Receive header word
                        mov     ip_temp, spi_reg
                        
                        ror     ip_temp, #24            ' Select type byte
                        sub     ip_temp, #ICMP_ECHO_REQUEST
                        test    ip_temp, #$FF wz        ' Is type byte Echo Request?
              if_nz     jmp     #rx_frame_finish        ' Not an echo request, ignore.
                        rol     ip_temp, #24
              
                        ' Now we need to replace the type with an echo reply.
                        ' Actually, since Echo Reply is zero, we already did that
                        ' as a side-effect. Now we just have to patch the checksum.

                        mov     checksum, ip_temp       ' Separate out the original checksum
                        andn    ip_temp, hFFFF
                        and     checksum, hFFFF
                        xor     checksum, hFFFF         ' Undo one's complement
                        mov     spi_reg, #ICMP_ECHO_REQUEST
                        shl     spi_reg, #8
                        sub     checksum, spi_reg wc    ' One's complement subtraction
              if_c      sub     checksum, #1
                        and     checksum, hFFFF
                        xor     checksum, hFFFF         ' Put it back in
                        or      ip_temp, checksum

                        mov     proto, #IP_PROTO_ICMP   ' Start transmitting an ICMP packet
                        call    #tx_ipv4

                        mov     spi_reg, ip_temp        ' Send the ICMP header
                        call    #spi_Write32

                        sub     ip_length, #20          ' Subtract IP header length
:loop                   sub     ip_length, #4 wz,wc     ' Round up to a 32-bit boundary
          if_z_or_c     add     tx_back_buf_end, spi_write_count
          if_z_or_c     jmp     #rx_frame_finish

                        call    #spi_ReadBufMem         ' Copy 32 bits from RX to TX
                        call    #spi_Read32
                        mov     ip_temp, spi_reg
                        call    #spi_WriteBufMem
                        mov     spi_reg, ip_temp
                        call    #spi_Write32

:loop_rel               sub     pc, #4*(:loop_rel + 1 - :loop)


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