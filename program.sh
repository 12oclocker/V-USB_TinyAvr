#!/bin/bash

#####################################
#
# Date:    07-09-2019
# Author:  12oClocker
# License: GNU GPL v2 (see License.txt)
#
#####################################

#/dev/ttyUSB0
#PRT="/dev/ttyAMA0"
#if [ "$1" != "" ]; then
#    echo "Positional parameter 1 contains something"
#else
#    echo "Positional parameter 1 is empty"
#fi

source ./compile_config.sh

#detect architecture
PORT="/dev/ttyAMA0"
ARCH=`dpkg --print-architecture`
if [ "$ARCH" == "armhf" ]; then
echo "Detected ARM Raspberry Pi"
else
echo "Detected x86 or amd64 Linux"
PORT="/dev/ttyUSB0"
fi

#directories
 HEX_DIR="./out_attiny1614"     
   FLASH="$HEX_DIR/main.hex"      
  EEPROM="$HEX_DIR/main.eep"     
   
BTF=$CFG_BOOT_FUSE
F05=$CFG_FUSE_5
LKB=$CFG_LKBITS
OSC=$CFG_OSC

FUSES=" -fuseW  0 0x00 " # 0x00 = watchdog disable
FUSES+="-fuseW  1 0x45 " # <--- 0x45 is BOD lvl 2 datasheet says it good to 10MHz, though we run at 3.3v which is good to almost 13MHz
FUSES+="-fuseW  2 $OSC " # <--- 0x02 = Run at 20MHz, 0x01 = Run at 16MHz
FUSES+="-fuseW  5 $F05 " # <--- 0xF6=Updi_pin_normal, 0xF2=change UPDI pin to GPIO (we can pull to 12v with 1K resistor to re-enable UPDI)
FUSES+="-fuseW  8 $BTF " # 0x00=no bootloader, 0x01=256 bytes, 0x02=512 bytes, 0x03=768, ect
FUSES+="-fuseW 10 $LKB " # 0xC5=Unlock  0xFF=Locked

# $fuses should NOT be quoted
#$TOOL_DIR/updi_pub -pn t16k -p "$PORT" -b 115200 -e -i -flashW $FLASH -eepromW $EEPROM $FUSES
$TOOL_DIR/updi_pub -pn t16k -p "$PORT" -b 115200 -e -i -flashW $FLASH $FUSES

#read -p "Press [Enter] key to continue..."

exit
