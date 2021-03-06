#!/usr/bin/env python
#
# Set the ADC calibration values from the command line,
# and display the raw ADC values after accumulation.
#

from fcntl import ioctl
import struct, time, sys

class Device:
    def __init__(self, file="/dev/usb/uvswitch0"):
        self.dev = open(file, "r")

    def setCalibration(self,
                       prechargeReads,
                       integrationReads,
                       interval,
                       integrationPackets,
                       threshold
                       ):
        ioctl(self.dev, 0x3901, struct.pack("iiiii",
                                            prechargeReads,
                                            integrationReads,
                                            interval,
                                            integrationPackets,
                                            threshold))

    def adcReadRaw(self):
        return struct.unpack("i"*8, ioctl(self.dev, 0x3902, struct.pack("i",0)*8))

d = Device()

if len(sys.argv) > 1:
        d.setCalibration(*map(int, sys.argv[1:]))

while True:
    print d.adcReadRaw()
    time.sleep(0.05)
