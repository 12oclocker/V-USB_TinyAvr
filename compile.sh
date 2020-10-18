#!/bin/bash

#####################################
# Compile Script 
#
# Date:    07-09-2019
# Author:  12oClocker
# License: GNU GPL (see License.txt)
#
#####################################

#  Check compile size WITH and WITHOUT -mcall-prologues to ensure size is decreased and not increased
#  -flto is new and a kind of linker time optimization, replaces the old -fwhole-program option
#  -ffreestanding will allow you to reduce size by having a non-returning main "void main(void)__attribute__((noreturn));" then on next line "void main(void){}"
#  remove unused sections with "-ffunction-sections -fdata-sections and linker option -gc-sections" to prevent optimizing out of eeprom... U8 EEdata[] EEMEM __attribute__((used)) = {0xFF};
#  options to inline functions... -finline-limit=3  -fno-inline-small-functions
#  Linker optimizations... --relax,--gc-sections,
#  remove volatile register warning -Wno-volatile-register-var

#source ../compile_config.sh
source ./compile_config.sh

clear #reset

#mcu we are compiling for
MCU=$CFG_MCU

#variables
PRE=out_
OUT=$PRE$MCU
USR=$USER
ENT=$CFG_APP_ENT   #0=no_bootloader, 0x0100=256_byte_bootloader, 0x0200=512_byte_bootloader
BFU=$CFG_BOOT_FUSE #0x02      #boot fuse 
LKB=$CFG_LKBITS    #0xFF      #lockbits 


TOOLCHAIN=$TC_DIR #"/blahblahblah/avr8-gnu-toolchain-3.6.2.1759-linux_x86_64"
GCC="$TOOLCHAIN/bin"
PAC="$TOOLCHAIN/PACKS/Atmel.ATtiny_DFP.1.3.172"
PACB="$PAC/gcc/dev/$MCU/"
PACI="$PAC/include/"
PACa="-I $PACI -B $PACB"
PACb="-B $PACB" 
CUR=$(pwd)
EZS="/home/$USR/dev/lib_avr/ezstack/ezstack"


#options
# -Wall -gdwarf-2 -std=gnu99                   -DF_CPU=16000000UL -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
OPT='  -DF_CPU=12800000UL '
OPT+=" -DBOOT_FUSE_VAR=$BFU "
OPT+=" -DLKBITS_VAR=$LKB "
OPT+=' -Os '
OPT+=' -gdwarf-2 '                 #-g2 same as -gdwarf-2 I think
OPT+=' -DDEBUG '
OPT+=' -std=gnu99 '
OPT+=' -Wall '    
#OPT+=' -Wshadow '                  #usbdrv has a shadowed variable
OPT+=' -funsigned-char '           
OPT+=' -funsigned-bitfields '
OPT+=' -fpack-struct '  
OPT+=' -fshort-enums ' 
OPT+=' -ffunction-sections '       #remove unused functions
OPT+=' -fdata-sections '
OPT+=' -ffreestanding '
OPT+=' -finline-limit=3 '
OPT+=' -fno-inline-small-functions '
#OPT+=' -mcall-prologues ' #sometimes causes compiled code to be larger
OPT+=' -flto ' #can cause "error: global register variable follows a function definition"


#files to compile #https://stackabuse.com/array-loops-in-bash/
INCS=" -I $CUR/  -I $CUR/usbdrv/ "                        # custom include directories, but -I before each one
ASMS=( usbdrv/usbdrvasm )                       # .S asm files
FILES=( main usb usbdrv/usbdrv  usbdrv/oddebug   ) # .c files
#INCS="   "                        # custom include directories, but -I before each one
#ASMS=(   )                       # .S asm files
#FILES=( main div fgen eeprom      ) # .c files

#compile
echo "__________________________________________________________"
echo "STARTING COMPILE                       EntryPoint = $ENT"
if [ -e "$OUT" ]; #make output directory
then
    echo "found existing bin directory"
else
	echo "creating output directory"
	mkdir "$OUT"
fi
cd "$OUT"
#=========================================================== -std=gnu99

#reset object linking
OL=""

#compile to object file (if not compiling correctly, echo the command below and make sure all variables are set)
FM=${FILES[0]}
for FL in "${FILES[@]}"  
do
echo "  C Compiling... $CUR/$FL.c"
$GCC/avr-gcc -g -x c                              $INCS $PACa $OPT  -mmcu=$MCU -c -MD -MP -MT "$CUR/$FL.o" -MF "$CUR/$FL.d" -o "$CUR/$FL.o" -c "$CUR/$FL.c"
OL+="$CUR/$FL.o " #prep the object linking
done

#compile ASM files
for FL in "${ASMS[@]}"  
do
echo "  ASM Compiling... $CUR/$FL.S"
$GCC/avr-gcc -g -x assembler-with-cpp -Wa,-gdwarf2 $INCS $PACa $OPT -mmcu=$MCU     -MD -MP -MT "$CUR/$FL.o" -MF "$CUR/$FL.d" -o "$CUR/$FL.o" -c "$CUR/$FL.S"
OL+="$CUR/$FL.o " #prep the object linking
done

#compile objects to elf file
#$GCC/avr-gcc -g -o $FM.elf  $OL   -Wl,-Map="$FM.map" -Wl,--start-group -Wl,-lm  -Wl,--end-group -Wl,--gc-sections                                  -mmcu=$MCU $PACb  
$GCC/avr-gcc -g -gdwarf-2 -o $FM.elf  $OL   -Wl,-Map="$FM.map" -Wl,--start-group -Wl,-lm  -Wl,--end-group -Wl,--gc-sections -Wl,--section-start=.text="$ENT" -mmcu=$MCU $PACb

#create hex from elf (must remove any extra data when dumping with -R) cannot use -j because it removes custom section flash data
$GCC/avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures  "$FM.elf" "$FM.hex"
#$GCC/avr-objcopy -j .text -j .data -O ihex "$FM.elf" "$FM.hex" #extract only .text and .data sections

#create srec from elf
$GCC/avr-objcopy -O srec -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures "$FM.elf" "$FM.srec"
#$GCC/avr-objcopy -j .text -j .data -O srec "$FM.elf" "$FM.srec" #extract only .text and .data sections

#create eeprom file from elf
$GCC/avr-objcopy -j .eeprom  --set-section-flags=.eeprom=alloc,load --set-start=0 --change-section-lma .eeprom=0  --no-change-warnings -O ihex "$FM.elf" "$FM.eep"

#dump elf to assembly (must have elf that was generated with -g)
$GCC/avr-objdump -h -S -C -l -F -d "$FM.elf" > "$FM.lss"

#===========================================================
echo "COMPILE FINISHED"
echo "__________________________________________________________"
#display GCC version
$GCC/avr-gcc --version
#check stack
echo "__________________________________________________________"
if [ -z "$1" ] 
then
echo "STACK USAGE not checked, use any switch to check it"
else
echo "STACK USAGE"
$EZS -wrap0 $FM.elf
echo "__________________________________________________________"
fi
#display code size
#echo "HEX SIZE USAGE"
#https://www.avrfreaks.net/forum/where-does-avr-size-get-data-supported-devices
#$GCC/avr-size -C --mcu=$MCU_SIZE $FM.elf  #old method
$GCC/avr-objdump -Pmem-usage $FM.elf
$GCC/avr-size $FM.elf
echo "__________________________________________________________"
cd ..
mv *.o "./$OUT"
mv *.d "./$OUT"
#===============================================================
bash cycle_cnt_lss.sh "$OUT"












