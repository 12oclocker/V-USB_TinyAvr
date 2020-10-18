#!/bin/bash

reset
echo "Compiling hex_tools..."
#suspect -march=native causes quad core pi compiled code to not work on single core pi's
#nope it's not march=native, but we CANNOT compile code on quadcore pi, must compile on single core pi
gcc -O2 -DDEBUG -DLINUX -D_GNU_SOURCE "hex_tools_pub.c" -g -std=c99 -Wall -Wshadow -Wno-misleading-indentation -o hex_tools_pub
echo "Compile Finished"

exit
