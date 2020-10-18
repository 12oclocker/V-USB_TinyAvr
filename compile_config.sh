#!/bin/bash

# Change the directory below to your Linux tool chain absolute path
# PACKS should be located in a PACKS directly inside toolchain directory,
# example... TC_DIR/PACKS/Atmel.ATtiny_DFP.1.3.172
       TC_DIR="/media/$USER/Storage/dev/lib/avr/linux/avr8-gnu-toolchain-3.6.2.1759-linux_x86_64"
     TOOL_DIR="/media/$USER/Storage/dev/avr/VUSB/TinyAvrSeries_t1614/try07_internal_osc_PUBLIC/tools"
       
#config
      CFG_MCU=attiny1614  # MCU we are compiling for
      CFG_OSC=0x01        # 0x02 = Run at 20MHz, 0x01 = Run at 16MHz
   CFG_FUSE_5=0xF6        # <--- 0xF6=Updi_pin_normal, 0xF2=change UPDI pin to GPIO
CFG_BOOT_FUSE=0x00        # 0x01=256 bytes, 0x02=512 bytes, 0x03=768, ect
   CFG_LKBITS=0xFF        # 0xFF=Locked, 0xC5=UnLocked
  CFG_APP_ENT=0x0000      # entry point for app, if bootloader is 0x02, this would be 0x0200


#do NOT exit
#exit