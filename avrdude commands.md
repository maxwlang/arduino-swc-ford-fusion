## Using arduino uno ISP, list supported devices
avrdude -c avrisp -P /dev/cu.usbmodem11101

## Using arduino uno ISP, program
avrdude -c avrisp -P /dev/cu.usbmodem11101 -p m328 -b 19200 -U flash:w:arduino-swc-ford-fusion.hex:i