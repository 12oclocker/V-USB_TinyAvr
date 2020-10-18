# V-USB_TinyAvr
V-USB variant with added compatability for TinyAvr0 TinyAvr1 series

Date:    11-21-2018
Author:  12oClocker
License: GNU GPL (see License.txt)

In the various assembly code drivers, you will see opcodes ending in "_tas"
those opcodes are only active when TinyAvr Series is enabled; they are not active for Attiny, Atmega series.

All tools written for x86 Linux and/or ARM RaspberryPi

Step 1) Set tools folder "TOOL_DIR" absolute path and toolchain "TC_DIR" absolute path in ./compile_config.sh
        Set your TinyAvr variable "CFG_MCU" in ./compile_config.sh default is attiny1614 (set program.sh HEX_DIR to match)
        Set the F_CPU in ./compile.sh look for... OPT='  -DF_CPU=12800000UL '
        NOTE: If you set a F_CPU of 12.8MHz, or 16.5MHz main.c automatically uses internal oscillator, otherwise it uses external clk.
Step 2) compile updi programmer ./tools/compile_updi.sh
        compile usb_app program ./usb_app/compile.sh
Step 3) use usbconfig.h to setup which of your TinyAvr pins are connected to USB D+ and D-
        compile main.c (the V-USB TinyAvr HID test) using ./compile.sh
Step 4) You are done, you can test your V-USB device using the usb_app        

-------------------------------------------

./tools             UPDI programming tools (use with any CP2102, CP21XX UsbSerial device, see schematic below)
./usb_app           USB App for testing USB communication with TinyAvr
compile_config.sh   compile config options (set absolute paths here)
compile.sh          you can set your clk freq here, look for... OPT='  -DF_CPU=12800000UL '
cycle_cnt_lss.sh    just a helpful script to look at lss and see opcode cycle counts
program.sh          program your TinyAvr using this script

-------------------------------------------

Works with these AVR series chips...
Attiny, Atmega, TinyAvr0, TinyAvr1

Tested and confirmed works...
12.0 Mhz
12.8 MHz (using internal 16MHz oscillator)
16.0 MHz
16.5 MHz (using internal 16MHz oscillator)
20.0 MHz

Non tested for TinyAvr0, TinyAvr1, but ported, and assumed will work fine...
15.0 MHz
18.0 MHz CRC  

-------------------------------------------

Simplest interface schematic for UPDI
Assuming AVR is 3.3v, and CP2102 variant RX internal pullup is 3.3v
Also works with Raspberry PI

             | /|
UPDI ---+----|< |------- CP2102_RX or RaspberryPi_Uart_RX
        |    | \|
        |
        |
        /
        \ 0ohm to 150ohm (use 150ohm if raspberry pi gives trouble with 0ohm)
        /
        |    
        |
        |    |\ |
        +----| >|------- CP2102_TX or RaspberryPi_Uart_TX
             |/ |