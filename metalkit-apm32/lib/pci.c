/* -*- Mode: C; c-basic-offset: 3 -*-
 *
 * pci.c - Simple PCI configuration interface. This implementation
 *         only supports type 1 accesses to configuration space,
 *         and it ignores the PCI BIOS.
 *
 * This file is part of Metalkit, a simple collection of modules for
 * writing software that runs on the bare metal. Get the latest code
 * at http://svn.navi.cx/misc/trunk/metalkit/
 *
 * Copyright (c) 2008 Micah Dowty
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "pci.h"
#include "io.h"

/*
 * There can be up to 256 PCI busses, but it takes a noticeable
 * amount of time to scan that whole space. Limit the number of
 * supported busses to something more reasonable...
 */
#define PCI_MAX_BUSSES          0x20

#define PCI_REG_CONFIG_ADDRESS  0xCF8
#define PCI_REG_CONFIG_DATA     0xCFC


/*
 * PCIConfigPackAddress --
 *
 *    Pack a 32-bit CONFIG_ADDRESS value.
 */

static fastcall uint32
PCIConfigPackAddress(const PCIAddress *addr, uint16 offset)
{
   const uint32 enableBit = 0x80000000UL;

   return (((uint32)addr->bus << 16) |
           ((uint32)addr->device << 11) |
           ((uint32)addr->function << 8) |
           offset | enableBit);
}


/*
 * PCI_ConfigRead32 --
 * PCI_ConfigRead16 --
 * PCI_ConfigRead8 --
 * PCI_ConfigWrite32 --
 * PCI_ConfigWrite16 --
 * PCI_ConfigWrite8 --
 *
 *    Access a device's PCI configuration space, using configuration
 *    mechanism #1. All new machines should use method #1, method #2
 *    is for legacy compatibility.
 *
 *    See http://www.osdev.org/wiki/PCI
 */

fastcall uint32
PCI_ConfigRead32(const PCIAddress *addr, uint16 offset)
{
   IO_Out32(PCI_REG_CONFIG_ADDRESS, PCIConfigPackAddress(addr, offset));
   return IO_In32(PCI_REG_CONFIG_DATA);
}

fastcall uint16
PCI_ConfigRead16(const PCIAddress *addr, uint16 offset)
{
   IO_Out32(PCI_REG_CONFIG_ADDRESS, PCIConfigPackAddress(addr, offset));
   return IO_In16(PCI_REG_CONFIG_DATA);
}

fastcall uint8
PCI_ConfigRead8(const PCIAddress *addr, uint16 offset)
{
   IO_Out32(PCI_REG_CONFIG_ADDRESS, PCIConfigPackAddress(addr, offset));
   return IO_In8(PCI_REG_CONFIG_DATA);
}

fastcall void
PCI_ConfigWrite32(const PCIAddress *addr, uint16 offset, uint32 data)
{
   IO_Out32(PCI_REG_CONFIG_ADDRESS, PCIConfigPackAddress(addr, offset));
   IO_Out32(PCI_REG_CONFIG_DATA, data);
}

fastcall void
PCI_ConfigWrite16(const PCIAddress *addr, uint16 offset, uint16 data)
{
   IO_Out32(PCI_REG_CONFIG_ADDRESS, PCIConfigPackAddress(addr, offset));
   IO_Out16(PCI_REG_CONFIG_DATA, data);
}

fastcall void
PCI_ConfigWrite8(const PCIAddress *addr, uint16 offset, uint8 data)
{
   IO_Out32(PCI_REG_CONFIG_ADDRESS, PCIConfigPackAddress(addr, offset));
   IO_Out8(PCI_REG_CONFIG_DATA, data);
}


/*
 * PCI_ScanBus --
 *
 *    Scan the PCI bus for devices. Before starting a scan,
 *    the caller should zero the PCIScanState structure.
 *    Every time this function is called, it returns the next
 *    device in sequence.
 *
 *    Returns TRUE if a device was found, leaving that device's
 *    vendorId, productId, and address in 'state'.
 *
 *    Returns FALSE if there are no more devices.
 */

fastcall Bool
PCI_ScanBus(PCIScanState *state)
{
   PCIConfigSpace config;

   for (;;) {
      config.words[0] = PCI_ConfigRead32(&state->nextAddr, 0);

      state->addr = state->nextAddr;

      if (++state->nextAddr.function == 0x8) {
         state->nextAddr.function = 0;
         if (++state->nextAddr.device == 0x20) {
            state->nextAddr.device = 0;
            if (++state->nextAddr.bus == PCI_MAX_BUSSES) {
               return FALSE;
            }
         }
      }

      if (config.words[0] != 0xFFFFFFFFUL) {
         state->vendorId = config.vendorId;
         state->deviceId = config.deviceId;
         return TRUE;
      }
   }
}


/*
 * PCI_FindDevice --
 *
 *    Scan the PCI bus for a device with a specific vendor and device ID.
 *
 *    On success, returns TRUE and puts the device address into 'addrOut'.
 *    If the device was not found, returns FALSE.
 */

fastcall Bool
PCI_FindDevice(uint16 vendorId, uint16 deviceId, PCIAddress *addrOut)
{
   PCIScanState busScan = {};

   while (PCI_ScanBus(&busScan)) {
      if (busScan.vendorId == vendorId && busScan.deviceId == deviceId) {
         *addrOut = busScan.addr;
         return TRUE;
      }
   }

   return FALSE;
}


/*
 * PCI_SetBAR --
 *
 *    Set one of a device's Base Address Registers to the provided value.
 */

fastcall void
PCI_SetBAR(const PCIAddress *addr, int index, uint32 value)
{
   PCI_ConfigWrite32(addr, offsetof(PCIConfigSpace, BAR[index]), value);
}


/*
 * PCI_GetBARAddr --
 *
 *    Get the current address set in one of the device's Base Address Registers.
 *    (We mask off the lower 2 bits, which hold memory type flags.)
 */

fastcall uint32
PCI_GetBARAddr(const PCIAddress *addr, int index)
{
   return PCI_ConfigRead32(addr, offsetof(PCIConfigSpace, BAR[index])) & ~3;
}


/*
 * PCI_SetMemEnable --
 *
 *    Enable or disable a device's memory and IO space. This must be
 *    called to enable a device's resources after setting all
 *    applicable BARs. Also enables/disables bus mastering.
 */

fastcall void
PCI_SetMemEnable(const PCIAddress *addr, Bool enable)
{
   uint16 command = PCI_ConfigRead16(addr, offsetof(PCIConfigSpace, command));

   /* Mem space enable, IO space enable, bus mastering. */
   const uint16 flags = 0x0007;

   if (enable) {
      command |= flags;
   } else {
      command &= ~flags;
   }

   PCI_ConfigWrite16(addr, offsetof(PCIConfigSpace, command), command);
}
