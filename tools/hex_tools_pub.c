////////////////////////////////////////////////////////////
//
// hex_tools
// Functions for manipulating AVR Intel Hex Files.
//
// Date:    11-21-2018
// Author:  12oClocker
// License: GNU GPL (see License.txt)
//
////////////////////////////////////////////////////////////

#include "hex_tools_pub.h"
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>         //vsnprintf vprintf
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>

#define VERSION_STR    __DATE__ //"12-01-2018"
#define PROG_HEADER    "Hex Tools\n" \
		               "Version " VERSION_STR "\n" \
		               "Copyright(C) 12oClocker\n"

void signal_handler(int sig)
{
	//https://stackoverflow.com/questions/18935446/program-received-signal-sigpipe-broken-pipe
	//if sending and the socket closes, don't exit app
	//if(sig == SIGPIPE)
    printf("exiting now...\n");
    exit(0);
}

int main(int argc, char **argv)
{
	printf("\n");
	
	//handle CTRL+C
	signal(SIGINT,signal_handler);//allow Ctrl+C to interrupt application	
	
	//no arguments
	if(argc == 1)
	{
		char msg[] = PROG_HEADER "\n"
		"usage... (actions will be performed in order of entered arguments)\n"
		"-merge    <in> <in> <out>              Merge bootloader and application intel hex files into one output hex file.\n"
		"-txt2hex  <in> <out>                   Convert text file such as \"00, FF, AB\" or \"0x00, 0xFF, 0xAB\" to intel hex file.\n"	
		"-hex2c    <in> <out>                   Convert intel hex file into a C variable output file.\n"
		"-hex2bin  <in> <out>                   Convert intel hex file into a raw binary hex file.\n"
		"\n";
		printf(msg);
		return 0;
	}	
	
	//parse arguments ... https://www.thegeekstuff.com/2013/01/c-argc-argv/
	for(int i=1; i<argc; i++) 
	{
		//part type
		if(strcmp("-merge",argv[i])==0 && (i+1)<argc && (i+2)<argc && (i+3)<argc)//convert txt file to intel hex file
		{
			printf("\n---Merge Intel Hex Files---\n");
			//get argument
			i++; char* sIn1  = argv[i];
			i++; char* sIn2 = argv[i];
			i++; char* sOut = argv[i];
			ihf_merge(sIn1,sIn2,sOut);		
		}	
		if(strcmp("-txt2hex",argv[i])==0 && (i+1)<argc && (i+2)<argc)//convert txt file to intel hex file
		{
			printf("\n---TXT to Intel Hex file conversion---\n");
			//get argument
			i++; char* sIn  = argv[i];
			i++; char* sOut = argv[i];
			ihf_txt2hex(sIn,sOut);		
		}	
		if(strcmp("-hex2c",argv[i])==0 && (i+1)<argc && (i+2)<argc)//convert intel hex file to c variable text file
		{
			printf("\n---Intel Hex to C Variable file conversion---\n");
			//get argument
			i++; char* sIn  = argv[i];
			i++; char* sOut = argv[i];
			ihf_hex2c(sIn,sOut);		
		}
		if(strcmp("-hex2bin",argv[i])==0 && (i+1)<argc && (i+2)<argc)//convert intel hex file to raw hex binary
		{
			printf("\n---Intel Hex to C Variable file conversion---\n");
			//get argument
			i++; char* sIn  = argv[i];
			i++; char* sOut = argv[i];
			ihf_hex2bin(sIn,sOut);		
		}		
	}	
	
	return 0;
}
























