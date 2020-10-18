#!/bin/bash

reset

#detect arch... amd64 is Linux system, armhf is raspberry pi

ARCH=`dpkg --print-architecture`

echo "Compiling updi..."
if [ "$ARCH" == "armhf" ]; then
echo "Detected ARM Raspberry Pi"
gcc -O2 -DDEBUG -DLINUX -D_GNU_SOURCE -DHEX_TOOLS_NO_LOG_TRACE "bcm2835.c" "updi_pub.c" -g -std=c99 -Wno-unused-function -Wall -Wshadow -Wno-misleading-indentation -o updi_pub
else
echo "Detected x86 or amd64 Linux"
gcc -O2 -DDEBUG -DLINUX -D_GNU_SOURCE -DHEX_TOOLS_NO_LOG_TRACE "updi_pub.c" -g -std=c99 -Wno-unused-function -Wall -Wshadow -Wno-misleading-indentation -o updi_pub
fi
echo "Compile Finished"

exit
