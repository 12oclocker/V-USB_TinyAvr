///////////////////////////////////////////
//
// UPDI TinyAvr0 TinyAvr1 Programmer
// For use with Raspberry or Linux
//
// Date:    11-21-2018
// Author:  12oClocker
// License: GNU GPL v2 (see License.txt)
//
// Version 10-17-2020
// Pubic Release
//
///////////////////////////////////////////

//if compiling for arm, assum raspberry pi		              
#ifdef __arm__
#define PI_COMPILE
#define VER_TYPE  "Pi"
#else
#define VER_TYPE  "Linux"
#endif	 

#define VERSION_STR  __DATE__ //"12-01-2018"

#define PROG_HEADER   "UPDI Programming Tool\n" \
		              VER_TYPE " Version " VERSION_STR "\n" \
		              "Copyright (C) 2018 12oClocker\n"

#include "hex_tools_pub.h"
#ifdef PI_COMPILE
#include "bcm2835.h"
#endif
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>         //vsnprintf vprintf
#include <stdlib.h>
#include <string.h>
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART
//#include <asm/termbits.h>
////#include <asm/termios.h> //dont use
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>        //to catch Ctrl+X in console
#include <errno.h>         //to display errors
#include <time.h>          //time()

#define I8  int8_t
#define I16 int16_t
#define I32 int32_t
#define I64 int64_t
#define U8  uint8_t
#define U16 uint16_t
#define U32 uint32_t
#define U64 uint64_t

#define sleep_ms(x) usleep(1000*(x))
#define sleep_us(x) usleep(x)
#define sleep_s(x)  sleep(x)
#define W(x)        (x)&0xFF,(x)>>8 //convert word value into 2 bytes

//gpio stuff // TX (GPIO14); RX (GPIO15)	LINE GPIO23
//old design used GPIO23 as way to pull LINE low, polarity was inverted on the pull, we now can do it with UART xmt pin
//uses same circuit design as LINE interface, you CANNOT directly put resistor on uart XMT and RCV, it sometimes causes strange errors
//https://www.dummies.com/computers/raspberry-pi/raspberry-pi-gpio-pin-alternate-functions/
#ifdef PI_COMPILE
#define XMT_GPIO_PIN    14 //TX pin
#define PWR_GPIO_PIN    23 //AMG PWR_ON pin
#define AMG_PWR_ON      bcm2835_gpio_fsel(PWR_GPIO_PIN,BCM2835_GPIO_FSEL_OUTP); bcm2835_gpio_write(PWR_GPIO_PIN,HIGH)
#define AMG_PWR_OFF     bcm2835_gpio_write(PWR_GPIO_PIN,LOW)
#define XMT_SETUP_IO    bcm2835_gpio_fsel(XMT_GPIO_PIN,BCM2835_GPIO_FSEL_OUTP) //make GPIO an output
#define XMT_SETUP_UART  bcm2835_gpio_fsel(XMT_GPIO_PIN,BCM2835_GPIO_FSEL_ALT0) //make GPIO a UART again
#define XMT_LINE_LO     bcm2835_gpio_write(XMT_GPIO_PIN,LOW)
#define XMT_LINE_HI     bcm2835_gpio_write(XMT_GPIO_PIN,HIGH)
#define XMT_LINE_HIZ    bcm2835_gpio_fsel(XMT_GPIO_PIN,BCM2835_GPIO_FSEL_INPT); bcm2835_gpio_set_pud(XMT_GPIO_PIN,BCM2835_GPIO_PUD_OFF) 
#define PI_INIT         if(!bcm2835_init()){printf("bcm2835_init failed\n"); return 0;} while(0)
#define PI_CLEANUP      bcm2835_close()
#else
#define PI_INIT         while(0)
#define PI_CLEANUP      while(0)
#endif  

//defines
#define byte_delay()  sleep_us(100)    //byte delay for sending; 0.0001 sec
#define UART_BAUD     B115200          //default baud, B9600 B115200  ////1200, 2400, 4800, 19200, 38400, 57600, and 115200 (pg520 of attiny1614 datasheet, Recommended UART Baud Rate)
#ifdef PI_COMPILE
	#define UART_NAME     "/dev/ttyAMA0"   //default UART, pi2b
#else
	#define UART_NAME     "/dev/ttyUSB0"   //default UART, pi2b
#endif
//#define UART_NAME   "/dev/ttyS0"     //pi3b
//#define UART_NAME   "/dev/serial0"   //Linux
#define MAX_FILE_READ  1048576         //max file read size is one MiB
#define MAX_HEX_BUF    65535           //max hex buf size of 65536

//UPDI commands and control definitions
#define UPDI_BREAK           0x00
#define UPDI_LDS             0x00
#define UPDI_STS             0x40
#define UPDI_LD              0x20
#define UPDI_ST              0x60
#define UPDI_LDCS            0x80
#define UPDI_STCS            0xC0
#define UPDI_REPEAT          0xA0
#define UPDI_KEY             0xE0
#define UPDI_PTR             0x00
#define UPDI_PTR_INC         0x04
#define UPDI_PTR_ADDRESS     0x08
#define UPDI_ADDRESS_8       0x00
#define UPDI_ADDRESS_16      0x04
#define UPDI_DATA_8          0x00
#define UPDI_DATA_16         0x01
#define UPDI_KEY_SIB         0x04
#define UPDI_KEY_KEY         0x00
#define UPDI_KEY_64          0x00
#define UPDI_KEY_128         0x01
#define UPDI_SIB_8BYTES      UPDI_KEY_64
#define UPDI_SIB_16BYTES     UPDI_KEY_128
#define UPDI_REPEAT_BYTE     0x00
#define UPDI_REPEAT_WORD     0x01
#define UPDI_PHY_SYNC        0x55
#define UPDI_PHY_ACK         0x40
#define UPDI_MAX_REPEAT_SIZE 0xFF
//CS and ASI Register Address map  "Register Summary - UPDI" in datasheet
#define UPDI_CS_STATUSA           0x00
#define UPDI_CS_STATUSB           0x01
#define UPDI_CS_CTRLA             0x02
#define UPDI_CS_CTRLB             0x03
#define UPDI_ASI_KEY_STATUS       0x07
#define UPDI_ASI_RESET_REQ        0x08
#define UPDI_ASI_CTRLA            0x09
#define UPDI_ASI_SYS_CTRLA        0x0A
#define UPDI_ASI_SYS_STATUS       0x0B
#define UPDI_ASI_CRC_STATUS       0x0C
#define UPDI_CTRLA_IBDLY_BIT      7
#define UPDI_CTRLB_CCDETDIS_BIT   3
#define UPDI_CTRLB_UPDIDIS_BIT    2
#define UPDI_KEY_NVM              "NVMProg " //0x4E564D50726F6720
#define UPDI_KEY_CHIPERASE        "NVMErase" //0x4E564D4572617365
#define UPDI_KEY_UROWWRITE        "NVMUs&te" //0x4E564D5573267465  pg573 is the write sequence
#define UPDI_ASI_STATUSA_REVID    4
#define UPDI_ASI_STATUSB_PESIG    0
#define UPDI_ASI_KEY_STATUS_CHIPERASE  3
#define UPDI_ASI_KEY_STATUS_NVMPROG    4
#define UPDI_ASI_KEY_STATUS_UROWWRITE  5
#define UPDI_ASI_SYS_STATUS_RSTSYS     5
#define UPDI_ASI_SYS_STATUS_INSLEEP    4
#define UPDI_ASI_SYS_STATUS_NVMPROG    3
#define UPDI_ASI_SYS_STATUS_UROWPROG   2  //"user row not affected by chip erase" in datasheet
#define UPDI_ASI_SYS_STATUS_LOCKSTATUS 0
#define UPDI_RESET_REQ_VALUE           0x59
#define UPDI_UROWFINAL_VALUE           0x02 //bit 1 is set
#define UPDI_UROWWRITE_VALUE           0x20 //bit 5 is set
//FLASH CONTROLLER
#define UPDI_NVMCTRL_CTRLA     0x00
#define UPDI_NVMCTRL_CTRLB     0x01
#define UPDI_NVMCTRL_STATUS    0x02
#define UPDI_NVMCTRL_INTCTRL   0x03
#define UPDI_NVMCTRL_INTFLAGS  0x04
#define UPDI_NVMCTRL_DATAL     0x06
#define UPDI_NVMCTRL_DATAH     0x07
#define UPDI_NVMCTRL_ADDRL     0x08
#define UPDI_NVMCTRL_ADDRH     0x09
//CTRLA
#define UPDI_NVMCTRL_CTRLA_NOP              0x00
#define UPDI_NVMCTRL_CTRLA_WRITE_PAGE       0x01
#define UPDI_NVMCTRL_CTRLA_ERASE_PAGE       0x02
#define UPDI_NVMCTRL_CTRLA_ERASE_WRITE_PAGE 0x03
#define UPDI_NVMCTRL_CTRLA_PAGE_BUFFER_CLR  0x04
#define UPDI_NVMCTRL_CTRLA_CHIP_ERASE       0x05
#define UPDI_NVMCTRL_CTRLA_ERASE_EEPROM     0x06
#define UPDI_NVMCTRL_CTRLA_WRITE_FUSE       0x07
#define UPDI_NVM_STATUS_WRITE_ERROR         2
#define UPDI_NVM_STATUS_EEPROM_BUSY         1
#define UPDI_NVM_STATUS_FLASH_BUSY          0

//user for EEPROM and FLASH info, see "Physical Properties of EEPROM" in datasheet
#define TYPE_FLASH   0
#define TYPE_EEPROM  1
typedef struct { 
	U16 start;    //start address
	U16 size;     //size in bytes
	U8  pagesize; //page size in bytes	
	U8  type;     //memory type (TYPE_FLASH or TYPE_EEPROM)
} nvm_info;

typedef struct { 
	nvm_info flash;
	nvm_info eeprom;
	U16 syscfg_address;
	U16 nvmctrl_address;
	U16 sigrow_address;
	U16 fuses_address;
	U16 userrow_address;
} device;

device DEVICES_MEGA_48K = {
	.flash.start     = 0x4000,
	.flash.size      = (48 * 1024),
	.flash.pagesize  = 128,
	.flash.type      = TYPE_FLASH,
	.eeprom.start    = 0x1400,
	.eeprom.size     = 256,
	.eeprom.pagesize = 64,
	.eeprom.type     = TYPE_EEPROM,	
	.syscfg_address  = 0x0F00,
	.nvmctrl_address = 0x1000,
	.sigrow_address  = 0x1100,
	.fuses_address   = 0x1280,
	.userrow_address = 0x1300
};
device DEVICES_MEGA_32K = {
	.flash.start     = 0x4000,
	.flash.size      = (32 * 1024),
	.flash.pagesize  = 128,
	.flash.type      = TYPE_FLASH,
	.eeprom.start    = 0x1400,
	.eeprom.size     = 256,
	.eeprom.pagesize = 64,
	.eeprom.type     = TYPE_EEPROM,
	.syscfg_address  = 0x0F00,
	.nvmctrl_address = 0x1000,
	.sigrow_address  = 0x1100,
	.fuses_address   = 0x1280,
	.userrow_address = 0x1300
};
device DEVICES_TINY_32K = {
	.flash.start     = 0x8000,
	.flash.size      = (32 * 1024),
	.flash.pagesize  = 128,
	.flash.type      = TYPE_FLASH,
	.eeprom.start    = 0x1400,
	.eeprom.size     = 256,
	.eeprom.pagesize = 64,
	.eeprom.type     = TYPE_EEPROM,
	.syscfg_address  = 0x0F00,
	.nvmctrl_address = 0x1000,
	.sigrow_address  = 0x1100,
	.fuses_address   = 0x1280,
	.userrow_address = 0x1300
};
device DEVICES_TINY_16K = {
	.flash.start     = 0x8000,
	.flash.size      = (16 * 1024),
	.flash.pagesize  = 64,
	.flash.type      = TYPE_FLASH,
	.eeprom.start    = 0x1400,
	.eeprom.size     = 256,
	.eeprom.pagesize = 32,
	.eeprom.type     = TYPE_EEPROM,
	.syscfg_address  = 0x0F00,
	.nvmctrl_address = 0x1000,
	.sigrow_address  = 0x1100,
	.fuses_address   = 0x1280,
	.userrow_address = 0x1300
}; 
device DEVICES_TINY_8K = {           
	.flash.start     = 0x8000,
	.flash.size      = (8 * 1024),
	.flash.pagesize  = 64,
	.flash.type      = TYPE_FLASH,
	.eeprom.start    = 0x1400,
	.eeprom.size     = 128,
	.eeprom.pagesize = 32,	
	.eeprom.type     = TYPE_EEPROM,
	.syscfg_address  = 0x0F00,
	.nvmctrl_address = 0x1000,
	.sigrow_address  = 0x1100,
	.fuses_address   = 0x1280,
	.userrow_address = 0x1300
};       
device DEVICES_TINY_4K = {        
	.flash.start     = 0x8000,
	.flash.size      = (4 * 1024),
	.flash.pagesize  = 64,
	.flash.type      = TYPE_FLASH,
	.eeprom.start    = 0x1400,
	.eeprom.size     = 128,
	.eeprom.pagesize = 32,
	.eeprom.type     = TYPE_EEPROM,
	.syscfg_address  = 0x0F00,
	.nvmctrl_address = 0x1000,
	.sigrow_address  = 0x1100,
	.fuses_address   = 0x1280,
	.userrow_address = 0x1300
};
device DEVICES_TINY_2K = {    
	.flash.start     = 0x8000,
	.flash.size      = (2 * 1024),
	.flash.pagesize  = 64,
	.flash.type      = TYPE_FLASH,
	.eeprom.start    = 0x1400,
	.eeprom.size     = 64,
	.eeprom.pagesize = 32,
	.eeprom.type     = TYPE_EEPROM,
	.syscfg_address  = 0x0F00,
	.nvmctrl_address = 0x1000,
	.sigrow_address  = 0x1100,
	.fuses_address   = 0x1280,
	.userrow_address = 0x1300
};

//globals
#define VERBOSE_MINIMAL  0
#define VERBOSE_NORMAL   1
#define VERBOSE_DEBUG    2
U8    g_ChipEraseOK    =  0;  //if we erased the whole chip ok, we dont have to do pages erases before writing!
U8    g_Verbose        =  1;  //enable verbose log info, 0=no_progress_bar, 1=normal, 2=debug_info
device* g_pDevice      =  0;  //specs to device we are programming
int   g_run            =  1;  //allow program to run
int   g_handle         = -1;  //uart handle
char  g_DefaultUart[]  = UART_NAME;
char* g_UartName       = g_DefaultUart;
int   g_Baud           = UART_BAUD;

void err_msg(const char* format, ...)
{
	va_list args;
	va_start(args,format);
	printf(RED);
	if(g_Verbose == VERBOSE_MINIMAL){ printf("%s ",g_UartName); } //prepend port info for ALL error messages
	vprintf(format,args);
	va_end(args);
	printf(CEND);
}

void log_msg(U8 verbose_lvl, const char* format, ...)
{
	va_list args;
	va_start(args,format);
	if(g_Verbose == VERBOSE_MINIMAL && verbose_lvl == VERBOSE_MINIMAL){ printf("%s ",g_UartName); } //prepend port info, for THIS message
	if(g_Verbose >= verbose_lvl){vprintf(format,args);}
	va_end(args);
}

void Cleanup()
{
	if(g_handle >= 0){close(g_handle); g_handle=-1;}
	PI_CLEANUP; //if running on pi, do pi cleanup
}

void signal_handler(int sig)
{
	//https://stackoverflow.com/questions/18935446/program-received-signal-sigpipe-broken-pipe
	//if sending and the socket closes, don't exit app
	//if(sig == SIGPIPE)
	//{return;}
	g_run=0;
	Cleanup();
    printf("\nexiting now...\n");
    exit(0);
}

//sec in X1 res, 11 = 1.1sec
void SetTimeout(int val, int bytes)
{
	//min bytes
	//https://www.gnu.org/software/libc/manual/html_node/Noncanonical-Input.html
	//set timeout 
	//https://stackoverflow.com/questions/2917881/how-to-implement-a-timeout-in-read-function-call
	struct termios options;
	tcgetattr(g_handle, &options);
	options.c_lflag &= ~ICANON; //Set non-canonical mode
	options.c_cc[VTIME] = val;  //Set timeout of 0.1 seconds
	options.c_cc[VMIN] = bytes; //minimum number of bytes we must receive before we return
	tcsetattr(g_handle, TCSANOW, &options);		
}

//open uart
#define STOP_BITS_ONE  0
#define STOP_BITS_TWO  CSTOPB
void OpenUart(int baud, int stop_bits) //1200, 2400, 4800, 19200, 38400, 57600, and 115200
{
	if(g_handle >= 0){close(g_handle);}
	//g_handle = open(UART_NAME, O_RDWR | O_NOCTTY | O_NDELAY );  //Open in non blocking read/write mode O_NDELAY
	g_handle = open(g_UartName, O_RDWR | O_NOCTTY );               //Open in blocking read/write mode O_NDELAY
	if(g_handle == -1)
	{
		err_msg("Error - Unable to open UART.  Ensure it is not in use by another application\n");
		if(g_handle >= 0){close(g_handle);}
		exit(0);
		return;
	}	
	//depending on the programmer dongle we are using, we may have to set DTR and RTS to idle high
	int DTR_flag = TIOCM_DTR;
	int RTS_flag = TIOCM_RTS;	
	#define DTR_LO  ioctl(g_handle,TIOCMBIS,&DTR_flag) //Set DTR pin (output will be LOW on CP2102)
	#define RTS_LO  ioctl(g_handle,TIOCMBIS,&RTS_flag) //Set RTS pin (output will be LOW on CP2102)
	#define DTR_HI  ioctl(g_handle,TIOCMBIC,&DTR_flag) //Clear DTR pin (output will be HIGH on CP2102)
	#define RTS_HI  ioctl(g_handle,TIOCMBIC,&RTS_flag) //Clear RTS pin (output will be HIGH on CP2102)	
	DTR_HI; //set line high
	RTS_HI;	//set line high		
	//normal setup
	struct termios options;
	tcgetattr(g_handle, &options);
	options.c_cflag = CS8 | CLOCAL | CREAD | PARENB | baud | stop_bits; //Set baud rate B9600 or B300 or whatever, 2 stop bits is CSTOPB
	options.c_iflag = IGNPAR;
	options.c_oflag = 0;
	options.c_lflag = 0;
	tcflush(g_handle, TCIOFLUSH);//TCIFLUSH
	tcsetattr(g_handle, TCSANOW, &options);	
	
	//set timeout 
	SetTimeout(1,0); //Set timeout of 0.1 seconds	
}

//read data from uart
U8 recv(U8* recv_data, int len)
{
	U8 timeout = 30;//increments of SetTimeout value (30 is 3.0sec) if SetTimeout(1)
	int r = 0;
	again:
	while(0);
	r += read(g_handle, recv_data+r, len-r);//read bytes ( timesout in SetTimeout(X) )
	if(r != len)
	{
		if(r > 0 && timeout){timeout--; goto again;}//we got some bytes but not all, try for some more
		log_msg(VERBOSE_DEBUG,"\treceived only %d of %d requested\n",r,len);
	}
	log_msg(VERBOSE_DEBUG,"\trecv "); for(int i=0; i<len; i++){ log_msg(VERBOSE_DEBUG,"0x%02X ",recv_data[i]); } log_msg(VERBOSE_DEBUG,"\n");
	return r;	
	
	/*
	int r = read(g_handle, recv_data, len);//read bytes
	if(r != len)
	{
		printf("\treceived only %d of %d requested\n",r,len);	
	}
	printf("\trecv "); for(int i=0; i<len; i++){ printf("0x%02X ",recv_data[i]); } printf("\n");
	return r;
	*/
}

//send some data over UART, with a delay between each byte, and read our echo byte back each time
void send(U8* data, int len)
{
	log_msg(VERBOSE_DEBUG,"\tsend "); for(int i=0; i<len; i++){ log_msg(VERBOSE_DEBUG,"0x%02X ",data[i]); } log_msg(VERBOSE_DEBUG,"\n");
	
	for(int i=0; i<len; i++)//send each byte, with delay between each byte
	{
		//send byte
		int xmt = write(g_handle, &data[i], 1);
		if(xmt != 1)
		{
			err_msg("\txmt error %d of %d sent\n",i,len);
			Cleanup();
			exit(0);//exit program
		}
		//read back echo byte
		U8 tmp = 0;
		int r = read(g_handle, &tmp, 1);//read echo byte	
		if(r != 1)
		{
			err_msg("\trcv error %d of %d read\n",r,len);
			Cleanup();
			exit(0);//exit program		
		}		
		//byte delay
		byte_delay();//100us delay	
	}
}

//Store a value to Control/Status space
void stcs(U8 address, U8 value)
{
	log_msg(VERBOSE_DEBUG,"STCS to 0x%02X\n",address);
	U8 data[] = {UPDI_PHY_SYNC, UPDI_STCS | (address & 0x0F), value};
	send(data,3);
}

//Load data from Control/Status space
U8 ldcs(U8 address)
{
	log_msg(VERBOSE_DEBUG,"LDCS from 0x%02X\n",address);
	U8 data[] = {UPDI_PHY_SYNC, UPDI_LDCS | (address & 0x0F)};
	send(data,2);
	//recv response
	U8 tmp = 0;
	recv(&tmp,1);
	return tmp;//return the data byte
}

//Load a single byte direct from a 16-bit address
U8 ld(U16 address)
{
	log_msg(VERBOSE_DEBUG,"LD from 0x%04X\n",address);
	U8 data[] = { UPDI_PHY_SYNC, UPDI_LDS | UPDI_ADDRESS_16 | UPDI_DATA_8, address & 0xFF, (address >> 8) & 0xFF };
	send(data,4);
	//recv response
	U8 tmp = 0;
	recv(&tmp,1);
	return tmp;//return the data byte
}

//Load a 16-bit word directly from a 16-bit address
U16 ld16(U16 address)
{
	log_msg(VERBOSE_DEBUG,"LD16 from 0x%04X\n",address);
	U8 data[] = { UPDI_PHY_SYNC, UPDI_LDS | UPDI_ADDRESS_16 | UPDI_DATA_16, address & 0xFF, (address >> 8) & 0xFF };
	send(data,4);
	//recv response
    union {U8 bytes[2]; U16 word;}  tmp;
    recv(tmp.bytes,2);
    return tmp.word;
}

void recv_ack(const char* errmsg)
{
	U8 tmp = 0;
	recv(&tmp,1);
	if(tmp != UPDI_PHY_ACK)
	{
		const char* s = (errmsg)? errmsg : "ACK error\n";
		err_msg(s); 
		Cleanup();
		exit(0);//exit program		
	}
}

//Store a single byte value directly to a 16-bit address
void st(U16 address, U8 value)
{
	log_msg(VERBOSE_DEBUG,"ST to 0x%04X\n",address);
	
	U8 data[] = { UPDI_PHY_SYNC, UPDI_STS | UPDI_ADDRESS_16 | UPDI_DATA_8, address & 0xFF, (address >> 8) & 0xFF };
	send(data,4);
	//recv ack response
	recv_ack("Error with ST (1st ACK)\n");
	
	U8 data2[] = { value & 0xFF };
	send(data2,1);
	//recv ack response
	recv_ack("Error with ST (2nd ACK)\n");	
}

//Store a 16-bit word value directly to a 16-bit address
void st16(U16 address, U16 value)
{
	log_msg(VERBOSE_DEBUG,"ST16 to 0x%04X\n",address);

	U8 data[] = { UPDI_PHY_SYNC, UPDI_STS | UPDI_ADDRESS_16 | UPDI_DATA_16, address & 0xFF, (address >> 8) & 0xFF };
	send(data,4);
	//recv ack response
	recv_ack("Error with ST16 (1st ACK)\n");	
	
	U8 data2[] = { value & 0xFF, (value >> 8) & 0xFF };
	send(data2,2);
	//recv ack response
	recv_ack("Error with ST16 (2nd ACK)\n");
}

//Loads a number of bytes from the pointer location with pointer post-increment
void ld_ptr_inc(U8* rcv_data, int len)
{
	if(len > UPDI_MAX_REPEAT_SIZE + 1)
	{
		err_msg("ld_ptr_inc cannot read more than 256 bytes at a time\n"); 
		Cleanup();
		exit(0);//exit program			
	}
	
    log_msg(VERBOSE_DEBUG,"LD8 from ptr++\n");
    U8 data[] = { UPDI_PHY_SYNC, UPDI_LD | UPDI_PTR_INC | UPDI_DATA_8 };
    send(data,2);
    //recv
    recv(rcv_data,len);
}

//Loads a number of bytes from the pointer location with pointer post-increment
void ld_ptr_inc16(U8* rcv_data, int words)
{
    log_msg(VERBOSE_DEBUG,"LD16 from ptr++\n");
    U8 data[] = { UPDI_PHY_SYNC, UPDI_LD | UPDI_PTR_INC | UPDI_DATA_16 };
    send(data,2);    
    //recv
    int len = (words << 1); //the <<1 is the same at *2 
    //SetTimeout(100,len);
    recv(rcv_data,len); 
    //SetTimeout(1,0);
}

//Set the pointer location
void st_ptr(U16 address)
{
	log_msg(VERBOSE_DEBUG,"ST to ptr\n");
	U8 data[] = {UPDI_PHY_SYNC, UPDI_ST | UPDI_PTR_ADDRESS | UPDI_DATA_16, address & 0xFF, (address >> 8) & 0xFF};
	send(data,4);
	//recv ack response
	recv_ack("Error with st_ptr ACK\n");	
}

//Store data to the pointer location with pointer post-increment
void st_ptr_inc(U8* data, int len)
{
	log_msg(VERBOSE_DEBUG,"ST8 to *ptr++\n");
	U8 tdata[] = {UPDI_PHY_SYNC, UPDI_ST | UPDI_PTR_INC | UPDI_DATA_8, data[0]};
	send(tdata,3);
	//recv ack response
	recv_ack("Error with st_ptr (1st ACK)\n");

	//xmt remaining data
	for(int i=1; i<len; i++)
	{
		send(&data[i],1);
		//recv ack response
		recv_ack("Error with st_ptr (x ACK)\n");		
	}	
}

//Store a 16-bit word value to the pointer location with pointer post-increment
void st_ptr_inc16(U8* data, int words)
{	
	log_msg(VERBOSE_DEBUG,"ST8 to *ptr++\n");
	U8 tdata[] = { UPDI_PHY_SYNC, UPDI_ST | UPDI_PTR_INC | UPDI_DATA_16, data[0], data[1] };
	send(tdata,4);	
	//recv ack response
	recv_ack("Error with st_ptr_inc16 (1st ACK)\n");
	
	//xmt remaining data
	int len = words << 1; //same as *2
	for(int i=2; i<len; i+=2)
	{
		U8 xmt_data[] = { data[i], data[i+1] };
		send(xmt_data,2);
		//recv ack response
		recv_ack("Error with st_ptr_inc16 (x ACK)\n");		
	}		
}

/*
//Store a value to the repeat counter
//pg532 of attiny1614 does not show support for UPDI_REPEAT_WORD command!
void repeat(int repeats) //pg532 (says max repeat is 255 size of a byte)
{
	printf("Repeat %d\n",repeats);
	repeats -= 1;
	U8 data[] = { UPDI_PHY_SYNC, UPDI_REPEAT | UPDI_REPEAT_WORD, repeats & 0xFF, (repeats >> 8) & 0xFF };
	send(data,4);	
}  
*/ 

//Store a value to the repeat counter
void repeat(int repeats) //pg532 (says max repeat is 255 size of a byte)
{
	if(repeats > (UPDI_MAX_REPEAT_SIZE + 1))
	{
		err_msg("Repeat value cannot be greater than 256\n");
		Cleanup();
		exit(0);//exit program			
	}
	log_msg(VERBOSE_DEBUG,"Repeat %d\n",repeats);
	repeats -= 1;
	U8 data[] = { UPDI_PHY_SYNC, UPDI_REPEAT | UPDI_REPEAT_BYTE, repeats & 0xFF };
	send(data,3);	
	//U8 data[] = { UPDI_PHY_SYNC, UPDI_REPEAT | UPDI_REPEAT_WORD, repeats & 0xFF, (repeats >> 8) & 0xFF };
	//send(data,4);		
}                                                  

//Read the SIB
#define read_sib sib
U8* sib(int* ret_len)
{
	U8 data[] = { UPDI_PHY_SYNC, (UPDI_KEY | UPDI_KEY_SIB | UPDI_SIB_16BYTES) };
	send(data,2);
	//rcv
	static U8 sib[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	//SetTimeout(100,16);
	U8 r = recv(sib,16);
	//SetTimeout(1,0);
	if(ret_len){ *ret_len = r;}
	return sib; //return static value
}

//Write a key
void key(U8 key_size, const char* key)
{
	log_msg(VERBOSE_DEBUG,"Writing key\n");
	U8 len = strlen(key);
	if(len != 8)
	{
		err_msg("Invalid KEY length\n");
		Cleanup();
		exit(0);//exit program			
	}
	//send key size
	U8 data[] = { UPDI_PHY_SYNC, (UPDI_KEY | UPDI_KEY_KEY | key_size) };
	send(data,2);
	//send key in reverse
	while(len)
	{
		len--;
		U8 c = (U8)key[len];
		send(&c,1);
	}	
}

//Applies or releases an UPDI reset condition
void reset(U8 apply_reset)
{
	if(apply_reset)
	{
		log_msg(VERBOSE_DEBUG,"Apply reset\n");
		stcs(UPDI_ASI_RESET_REQ, UPDI_RESET_REQ_VALUE);
	}
	else
	{
		log_msg(VERBOSE_DEBUG,"Release reset\n");
		stcs(UPDI_ASI_RESET_REQ, 0x00);
	}
}

//Waits for the device to be unlocked. All devices boot up as locked until proven otherwise
U8 wait_unlocked(int x1_sec_wait)//50 is 5.0sec
{
	int trys = x1_sec_wait;
	again:
	//check if device is unlocked
	if( !(ldcs(UPDI_ASI_SYS_STATUS) & (1 << UPDI_ASI_SYS_STATUS_LOCKSTATUS)) )
	{return 1;}	
	//retry for 100ms to see if device becomes unlocked
	trys--;
	if(trys)
	{
		sleep_ms(100);
		goto again;
	}
	
	//err_msg("Device is locked\n");
	return 0;        	
}

//Unlock and erase
U8 unlock_erase()
{
	log_msg(VERBOSE_NORMAL,"\n---unlock_erase---\n");
	
	//enter erase key
	key(UPDI_KEY_64, UPDI_KEY_CHIPERASE);

	//Check key status
	U8 key_status = ldcs(UPDI_ASI_KEY_STATUS);
	log_msg(VERBOSE_DEBUG,"ChipErase Key status = 0x%02X\n",key_status);
	if( !(key_status & (1 << UPDI_ASI_KEY_STATUS_CHIPERASE)) )
	{
		err_msg("ChipErase Key not accepted\n");
		Cleanup();
		exit(0);//exit program			
	}

	//Toggle reset
	reset(1);
	reset(0);

	//And wait for unlock
	if(!wait_unlocked(50)) //wait up to 5sec
	{
		err_msg("unlock_erase Failed using key\n");
		Cleanup();
		exit(0);//exit program			
	}
	else
	{
		log_msg(VERBOSE_MINIMAL,"unlock_erase completed OK\n");
		return 1;
	}
	return 0;
}

//Checks whether the NVM PROG flag is up
U8 in_prog_mode()
{
	if( ldcs(UPDI_ASI_SYS_STATUS) & (1 << UPDI_ASI_SYS_STATUS_NVMPROG) )
	{return 1;}
	return 0;	
}

//Enters into NVM programming mode
U8 enter_progmode()
{
	log_msg(VERBOSE_NORMAL,"\n---Enter Programming Mode---\n");
	//First check if NVM is already enabled
	if(in_prog_mode())
	{log_msg(VERBOSE_NORMAL,"Already in NVM programming mode\n"); return 1;}	
	log_msg(VERBOSE_NORMAL,"Entering NVM programming mode\n");
	
	//Put in the key
	key(UPDI_KEY_64, UPDI_KEY_NVM);
	
	//Check key status
	U8 key_status = ldcs(UPDI_ASI_KEY_STATUS);
	log_msg(VERBOSE_DEBUG,"NvmProg Key status = 0x%02X\n",key_status);
	if( !(key_status & (1 << UPDI_ASI_KEY_STATUS_NVMPROG)) )
	{
		err_msg("NvmProg Key not accepted\n");
		return 0;		
	}
	
	//Toggle reset
	reset(1);
	reset(0);

	//And wait for unlock
	if(!wait_unlocked(5))//wait up to 500ms
	{
		err_msg("Device is locked, cannot enter programming mode (erase device first)\n");
		return 0;		
	}	
	
	//Check for NVMPROG flag
	if(!in_prog_mode())
	{
		err_msg("Failed to enter NVM programming mode\n");
		return 0;				
	}
	log_msg(VERBOSE_MINIMAL,"Now in NVM programming mode\n");
	return 1;			
}

//Disables UPDI which releases any keys enabled
void leave_progmode()
{
	log_msg(VERBOSE_NORMAL,"\nLeaving NVM programming mode\n");
	
	//Toggle reset
	reset(1);
	reset(0);	
	
	stcs(UPDI_CS_CTRLB, (1 << UPDI_CTRLB_UPDIDIS_BIT) | (1 << UPDI_CTRLB_CCDETDIS_BIT));
}

//Waits for the NVM controller to be ready
U8 wait_nvm_ready()
{
	log_msg(VERBOSE_DEBUG,"wait_nvm_ready\n");
	U8 trys = 100; //wait up to 10sec
	
	again:
	while(0);
	U8 status = ld(g_pDevice->nvmctrl_address + UPDI_NVMCTRL_STATUS);
	if(status & (1 << UPDI_NVM_STATUS_WRITE_ERROR))
	{
		err_msg("wait_nvm_ready error\n");
		Cleanup();
		exit(0);//exit program			
	}
	
	if(! (status & ((1 << UPDI_NVM_STATUS_EEPROM_BUSY) | (1 << UPDI_NVM_STATUS_FLASH_BUSY))) )
	{return 1;}
	
	trys--;
	if(trys)
	{		
		sleep_ms(100);
		goto again;		
	}	
	
	err_msg("wait_nvm_ready timed out\n");
	Cleanup();
	exit(0);//exit program		
	return 0;	
}

//Executes an NVM COMMAND on the NVM CTRL
void execute_nvm_command(U8 cmd)
{
	log_msg(VERBOSE_DEBUG,"NVMCMD 0x%02X executing\n",cmd);
	st(g_pDevice->nvmctrl_address + UPDI_NVMCTRL_CTRLA, cmd);
}

//Does a chip erase using the NVM controller, Note that on locked devices this it not possible and the ERASE KEY has to be used instead
void chip_erase()
{
	//wait for NVM to be ready
	if(!wait_nvm_ready())
	{
		err_msg("Timeout waiting for flash ready before erase\n");
		Cleanup();
		exit(0);//exit program			
	}
	
	//erase
	execute_nvm_command(UPDI_NVMCTRL_CTRLA_CHIP_ERASE);
	
	//wait for NVM to be ready
	if(!wait_nvm_ready())
	{
		err_msg("Timeout waiting for flash ready after erase\n");
		Cleanup();
		exit(0);//exit program			
	}	
}   

//Writes a number of words to memory
void write_data_words(U16 address, U8* data, int words)
{
	//special case of 1 word
	if(words == 1)
	{
		U16 value = data[0] + (data[1] << 8);
		st16(address, value);
		return;	
	}
	
	//Store the address
	st_ptr(address);
	
	//set how many times to execute next command
	if(words > 1)
	{repeat(words);}
	
	//write the words
	st_ptr_inc16(data,words);			
} 

//Writes a number of bytes to memory
void write_data(U16 address, U8* data, int len)
{
	//special case of 1 byte
	if(len == 1)
	{
		st(address,data[0]);
		return;	
	}
	if(len == 2)
	{
		st(address,data[0]);
		st(address,data[1]);
		return;	
	}	
	
	//Store the address
	st_ptr(address);
	
	//set how many times to execute next command
	if(len > 1)
	{repeat(len);}
	
	//write the words
	st_ptr_inc(data,len);	
}

void read_data_words(U16 address, U8* recv_data, int words)
{
	//Store the address
	st_ptr(address);
		
	//set how many times to execute next command
	if(words > 1)
	{repeat(words);}
	
	//read the number of words to pRcvData
	ld_ptr_inc16(recv_data,words);	
}

//Reads a number of bytes of data from UPDI
void read_data(U16 address, U8* recv_data, int len)
{
	log_msg(VERBOSE_DEBUG,"Reading %d bytes from 0x%04X",len,address);
	if(len > (UPDI_MAX_REPEAT_SIZE + 1))
	{
		err_msg("Cant read %d bytes, is more than %d max\n",len,(UPDI_MAX_REPEAT_SIZE + 1));
		Cleanup();
		exit(0);//exit program	
	}
	
	//set the address to read from
	st_ptr(address);
	
	//set the read repeat count
	if(len > 1)
	{repeat(len);}
	
	//read the bytes
	ld_ptr_inc(recv_data,len); 
}

//returns size data if page contains some data, returns 0 if it has no data (contains only 0xFF)
int HasPageData(U8* data, int len)
{
	while(len)
	{
		len--;
		if(data[len] != 0xFF){return len+1;}
	}

	return 0;//no data found
}

//get number of pages with data, its possible some pages in the middle may not have data, this is just used to calculate write progress
//so if 5 pages had data, this will return 5, those pages with data could be at the beining or end or both, of the data memory to be written
int GetPageCntWithData(U8* buf, nvm_info* pNvm)
{
	//figure out number of pages to read
	U16 page_cnt = pNvm->size / pNvm->pagesize;
	if(page_cnt < 1){page_cnt = 1;}//prevent underflow if we have less than 1 page of data
	//buf variable to increment
	U8* pData = buf;
	//number of pages with data
	int pages_with_data = 0;
	//loop and check
	for(U16 i=0; i<page_cnt; i++)
	{
		if(HasPageData(pData,pNvm->pagesize)){pages_with_data++;}
		pData += pNvm->pagesize;
	}
	return pages_with_data;
}

//for showing page write or read or verify progress
void ShowProgress(int i, int PageCnt, int PagesWithData, int CurPageHasData)
{
	if(g_Verbose < 1){return;}
	//make progress look like this.... (20 prgress section marks)
	//
	// Pages = %03d, Processing Page %03d
	// pg001 [####################] 100%   //33 char long
	//
	// normally i * 100 / PageCnt would give us percentage, but not all pages have data
	// so what could happen is the progress slowly ticks to 50%, then sporadically jumps to 100%, we dont want that
	// instead we increment progress only when a page has data, and we use PagesWithData as our max value for progress calc
	static int x = 0;
	if(i==0)//first itter, setup the layout
	{
		x = 0;
		printf("Total Pages = %3d\n",PageCnt);
		printf("pg%03d[                    ]   0%%",PageCnt); 
	}
	else
	{
		char b[] = "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";//backup 33 characters
		if(CurPageHasData){x++;}//this page has data, increment progress counter
		int prog = ((x+1) * 100 / PagesWithData);
		int mrks = prog / 5; //how many marks do we need for this prog value
		//now print stuff...
		printf(b);//backup to print new values
		printf("pg%03d[",i);//print current page we are processing
		for(int j=1; j<=20;j++)//print the progress marks
		{
			if(j<=mrks){printf("#");}else{printf(" ");}
		}
		printf("] %3d%%",prog);//print current page we are processing
	}
	fflush(stdout);
}

//read NVM memory to buf (buf size is expected to be pNVM->size)
//use_bytes is for byte chunk reading (slower than word chunk reading)
void nvm_read(U8* buf, nvm_info* pNvm)
{
	//start reading at this address
	U16 address = pNvm->start;
	
	//figure out number of pages to read
	U16 page_cnt = pNvm->size / pNvm->pagesize;
	if(page_cnt < 1){page_cnt = 1;}//prevent underflow if we have less than 1 page of data
	
	//words per page to read... (should always be less than 0xFF) because mega48K is ((48 * 1024) / 128) which is 384 and div by 2 is 192
	U16 words = pNvm->pagesize >> 1; //words per page
	
	//if EEPROM type, we must use byte read/write instead of word read/write
	//the only hint to this was on datasheet pg65 of t1614 which said "The EEPROM programming is similar, but only the bytes updated in the page buffer will be written or erased in the EEPROM."
	U8 use_bytes = (pNvm->type == TYPE_EEPROM)? 1 : 0;	
	if(use_bytes){log_msg(VERBOSE_NORMAL,"\n---nvm_eeprom_read---\n");}else{log_msg(VERBOSE_NORMAL,"\n---nvm_flash_read---\n");}
	
	//double check we do not exceed limit
	U16 word_limit = ((UPDI_MAX_REPEAT_SIZE >> 1) + 1);
	U16 byte_limit = UPDI_MAX_REPEAT_SIZE+1;
	if(words > word_limit || (use_bytes && (pNvm->pagesize > byte_limit)) )
	{
		err_msg("Cant read that many words at a time, %d exceeds %d",words,word_limit);
		Cleanup();
		exit(0);//exit program		
	}	
	
	//setup the variable to save the bytes we read
	U8* pRcvData = buf;
	
	//read the pages
	log_msg(VERBOSE_MINIMAL,"Reading data...\n");
	//if(!g_LogEnable){ printf("Pages = %03d, Reading Page %03d",page_cnt,1); }
	for(U16 i=0; i<page_cnt; i++)
	{
		//show progress
		ShowProgress(i,page_cnt,page_cnt,1);
		//if(!g_LogEnable){ printf("\b\b\b%03d",i+1); fflush(stdout); }
		//read the number of words or bytes to pRcvData
		if(use_bytes)
		{read_data(address,pRcvData,pNvm->pagesize);}
		else
		{read_data_words(address,pRcvData,words);}
        //increment
        address  += pNvm->pagesize;
        pRcvData += pNvm->pagesize;
	}
	if(g_Verbose>0){ log_msg(VERBOSE_NORMAL,"\n"); }	
	
	
	//for progress, count number of pages with data
	//make progress look like this.... [####################] 99%
	int PagesWithData = GetPageCntWithData(buf,pNvm);	
	
	//printf("NVM Read Done\n");
	log_msg(VERBOSE_MINIMAL,"NVM Read Done, %d of %d pages had data\n",PagesWithData,page_cnt);
}

//verify nvm with buf (buf size is expected to be pNVM->size)
//we only verify pages that have data
void nvm_verify(U8* buf, nvm_info* pNvm)
{
	//start reading at this address
	U16 address = pNvm->start;
	
	//figure out number of pages to read
	U16 page_cnt = pNvm->size / pNvm->pagesize;
	if(page_cnt < 1){page_cnt = 1;}//prevent underflow if we have less than 1 page of data
	
	//words per page to read... (should always be less than 0xFF) because mega48K is ((48 * 1024) / 128) which is 384 and div by 2 is 192
	U16 words = pNvm->pagesize >> 1; //words per page
	
	//if EEPROM type, we must use byte read/write instead of word read/write
	U8 use_bytes = (pNvm->type == TYPE_EEPROM)? 1 : 0;	
	if(use_bytes){log_msg(VERBOSE_NORMAL,"\n---nvm_eeprom_verify---\n");}else{log_msg(VERBOSE_NORMAL,"\n---nvm_flash_verify---\n");}
	
	//double check we do not exceed limit
	U16 word_limit = ((UPDI_MAX_REPEAT_SIZE >> 1) + 1);
	U16 byte_limit = UPDI_MAX_REPEAT_SIZE+1;
	if(words > word_limit || (use_bytes && (pNvm->pagesize > byte_limit)) )
	{
		err_msg("Cant read that many words at a time, %d exceeds %d",words,word_limit);
		Cleanup();
		exit(0);//exit program		
	}
	
	//setup the variable to save the bytes we read
	U8  tmp_buf[pNvm->pagesize];
	U8* pData = buf;
		
	//for progress, count number of pages with data
	//make progress look like this.... [####################] 99%
	int PagesWithData = GetPageCntWithData(buf,pNvm);
	if(PagesWithData < 1){goto done;}
	
	//verify the pages
	log_msg(VERBOSE_MINIMAL,"Verifying data...\n");
	for(U16 i=0; i<page_cnt; i++)
	{
		//check if this page is empty, if it is, we skip writing this page
		int HasData = HasPageData(pData,pNvm->pagesize);
		//show progress
		ShowProgress(i,page_cnt,PagesWithData,HasData);
		//if their is some data in this page, then we will verify it
		if(HasData)
		{		
			//read the number of words or bytes to pRcvData
			if(use_bytes)
			{read_data(address,tmp_buf,pNvm->pagesize);}
			else
			{read_data_words(address,tmp_buf,words);}
			//do the compare
			if(memcmp(tmp_buf,pData,pNvm->pagesize) != 0)
			{
				err_msg("\nNVM Verify FAILED! Page %d (index %d) does not match",i+1,i);
				break;			
			}
		}
        //increment
        address  += pNvm->pagesize;
        pData    += pNvm->pagesize;
	}
	if(g_Verbose>0){ log_msg(VERBOSE_NORMAL,"\n"); }	
	
	done:
	log_msg(VERBOSE_MINIMAL,"NVM Verify Done, %d of %d pages had data to verify\n",PagesWithData,page_cnt);	
}

//write NVM memory from buf (you should do a chip erase before this operation!) (buf size is expected to be pNVM->size)
void nvm_write(U8* buf, nvm_info* pNvm)
{
	//start reading at this address
	U16 address = pNvm->start;
	
	//figure out number of pages to write
	U16 page_cnt = pNvm->size / pNvm->pagesize;
	if(page_cnt < 1){page_cnt = 1;}//prevent underflow, we have a partial page to write
	
	//words per page to write... (should always be less than 0xFF) because mega48K is ((48 * 1024) / 128) which is 384 and div by 2 is 192
	U16 words = pNvm->pagesize >> 1; //words per page
	
	//if EEPROM type, we must use byte read/write instead of word read/write
	U8 use_bytes = (pNvm->type == TYPE_EEPROM)? 1 : 0;
	if(use_bytes){log_msg(VERBOSE_NORMAL,"\n---nvm_eeprom_write---\n");}else{log_msg(VERBOSE_NORMAL,"\n---nvm_flash_write---\n");}
	
	//double check we do not exceed limit
	U16 word_limit = ((UPDI_MAX_REPEAT_SIZE >> 1) + 1);
	U16 byte_limit = UPDI_MAX_REPEAT_SIZE+1;
	if(words > word_limit || (use_bytes && (pNvm->pagesize > byte_limit)) )
	{
		err_msg("Cant write that many words at a time, %d exceeds %d",words,word_limit);
		Cleanup();
		exit(0);//exit program		
	}
	
	//setup pointer for increment
	U8* pData = buf;
	
	//NVM write command select, if we already erased the chip, we can skip page erases!
	U8 nvcmd;
	if(g_ChipEraseOK)
	{
		nvcmd = UPDI_NVMCTRL_CTRLA_WRITE_PAGE;//fast
		log_msg(VERBOSE_MINIMAL,"Detected chip already erased, skipping page erase cycles\n");
	}
	else
	{
		nvcmd = UPDI_NVMCTRL_CTRLA_ERASE_WRITE_PAGE;//slow
		log_msg(VERBOSE_MINIMAL,"Detected chip NOT erased, will erase pages before writing\n");		
	}
	
	//for progress, count number of pages with data
	//make progress look like this.... [####################] 99%
	int PagesWithData = GetPageCntWithData(buf,pNvm);
	if(PagesWithData < 1){goto done;}
	
	//write the pages
	log_msg(VERBOSE_MINIMAL,"Writing data...\n");
	for(U16 i=0; i<page_cnt; i++)
	{
		//check if this page is empty, if it is, we skip writing this page
		int HasData = HasPageData(pData,pNvm->pagesize);
		//show progress
		ShowProgress(i,page_cnt,PagesWithData,HasData);
		//if their is some data in this page, then we will write it
		if(HasData)
		{
			//wait for flash to become ready
			wait_nvm_ready();		
			//clear page buffer
			execute_nvm_command(UPDI_NVMCTRL_CTRLA_PAGE_BUFFER_CLR);	
			//wait for flash to become ready
			wait_nvm_ready();	
			//write words or bytes
			if(use_bytes)
			{write_data(address,pData,pNvm->pagesize);}
			else
			{write_data_words(address,pData,words);}
			//commit page (should we call UPDI_NVMCTRL_CTRLA_ERASE_PAGE first, probably YES if we dont call chip erase first)
			//erasing something that was already erased does NOT count as reducing the cell life
			//https://www.avrfreaks.net/forum/avr-eeprom-life-endurance
			execute_nvm_command(nvcmd);	//or could call UPDI_NVMCTRL_CTRLA_ERASE_WRITE_PAGE if we did not clear chip first or call UPDI_NVMCTRL_CTRLA_WRITE_PAGE if we did
		}
        //increment
        address += pNvm->pagesize;
        pData   += pNvm->pagesize;				
	}
	if(g_Verbose>0){ log_msg(VERBOSE_NORMAL,"\n"); }	
	
	done:
	log_msg(VERBOSE_MINIMAL,"NVM Write Done, %d of %d pages had data to write\n",PagesWithData,page_cnt);	
}

char* disp_binary(U8 val)
{
	static char s[] = "00000000";
	for(U8 i=0; i<8; i++)
	{
		U8 b = 7 - i;
		char c = (val & (1 << b))? '1' : '0';
		s[i] = c;
	}
	return s;
}

//Reads one fuse value
U8 fuse_read(U8 fuse_num)
{
	if(!in_prog_mode())
	{
		err_msg("Cannot read fuse %d, not in progmode\n",fuse_num);
		return 0;
	}
	
	//read the fuse value
	U16 address = g_pDevice->fuses_address + fuse_num;
	U8 data = ld(address);
	log_msg(VERBOSE_NORMAL,"Fuse %02d Value = 0x%02X  %s\n",fuse_num,data, disp_binary(data) );
	return data;	
}

U8 fuse_write(U8 fuse_num, U8 fuse_data)
{
	log_msg(VERBOSE_NORMAL,"\n---write_fuse---\n");
	
	if(!in_prog_mode())
	{
		err_msg("Cannot read fuse %d, not in progmode\n",fuse_num);
		return 0;
	}
	
	//wait for flash to become ready
	wait_nvm_ready();	
	
	//read old fuse value, in case we are changing it
	U16 address = g_pDevice->fuses_address + fuse_num;
	U8 old_fuse_val = ld(address);	
	
	//if fuse already equals same value, dont write it again
	if(old_fuse_val == fuse_data)
	{
		log_msg(VERBOSE_MINIMAL,"Fuse %02d value is already 0x%02X %s\n",fuse_num, fuse_data, disp_binary(fuse_data));
		return 1;
	}
	
	//write fuse value
	U16 fuse_address = g_pDevice->fuses_address + fuse_num;
	U8  data;
	
	//inform lower part of fuse address
	address = g_pDevice->nvmctrl_address + UPDI_NVMCTRL_ADDRL;
	data    = (fuse_address & 0xFF);
	write_data(address, &data, 1);
	
	//inform upper part of fuse address
	address = g_pDevice->nvmctrl_address + UPDI_NVMCTRL_ADDRH;
	data    = (fuse_address >> 8);
	write_data(address, &data, 1);
	
	//inform the fuse data
	address = g_pDevice->nvmctrl_address + UPDI_NVMCTRL_DATAL;
	data    = fuse_data;
	write_data(address, &data, 1);
	
	//inform to write the fuse now
	address = g_pDevice->nvmctrl_address + UPDI_NVMCTRL_CTRLA;
	data    = UPDI_NVMCTRL_CTRLA_WRITE_FUSE;
	write_data(address, &data, 1);
	
	//read fuse to verify
	address = g_pDevice->fuses_address + fuse_num;
	data = ld(address);	
	
	//verify
	if(data != fuse_data)
	{
		err_msg("Fuse %d write failed; Write Value 0x%02X != 0x%02X Read Value\n",fuse_num,fuse_data,data);
		return 0;
	}
	else
	{
		log_msg(VERBOSE_NORMAL, "Fuse %02d Old Value = 0x%02X %s\n",fuse_num, old_fuse_val, disp_binary(old_fuse_val) );
		log_msg(VERBOSE_NORMAL, "Fuse %02d New Value = 0x%02X %s\n",fuse_num, fuse_data, disp_binary(fuse_data) );
		log_msg(VERBOSE_MINIMAL,"Fuse %02d writen OK (value = 0x%02X)\n", fuse_num, fuse_data);
	}
	return 1;
}


//Waits for the device to be unlocked. All devices boot up as locked until proven otherwise
U8 wait_userrow_state(U8 state, int x1_sec_wait)//50 is 5.0sec
{
	int trys = x1_sec_wait;//wait up to 500ms for status
	again:
	while(0);
	//check if urowprog is ready
	U8 cur_state = (ldcs(UPDI_ASI_SYS_STATUS) & (1 << UPDI_ASI_SYS_STATUS_UROWPROG))? 1 : 0;
	if(cur_state == state)
	{return 1;}
	
	trys--;
	if(trys){ sleep_ms(100); goto again; }
	
	err_msg("wait_userrow_ready never became ready\n");
	return 0;		        	
}

//userrow read function
#define userrow_read(pUserRowData)  read_data(g_pDevice->userrow_address, pUserRowData, g_pDevice->eeprom.pagesize)

//write user row data (which is the size of 1 eeprom page)
U8 userrow_write(U8* pUserRowData)
{
	//Peripheral Module Address Map
	//pg20 In Locked mode, the USERROW can be written blindly using the fuse Write command, but the current USERROW values cannot be read out
	//pg18 USERROW supports single byte read and write as the normal EEPROM.
	//     The CPU can write and read this memory as normal EEPROM and the UPDI can write and read it as a normal EEPROM memory if the part is unlocked. 
	//     The User Row can be written by the UPDI when the part is locked. USERROW is not affected by a chip erase.
	//pg573 "User Row Programming"
	
	//user row data size is the same as eeprom page size
	
	log_msg(VERBOSE_NORMAL,"\n---userrow_write---\n");

	//Put in the key
	key(UPDI_KEY_64, UPDI_KEY_UROWWRITE);
	
	//Check key status
	U8 key_status = ldcs(UPDI_ASI_KEY_STATUS);
	log_msg(VERBOSE_DEBUG,"UserRowWrite Key status = 0x%02X\n",key_status);
	if( !(key_status & (1 << UPDI_ASI_KEY_STATUS_UROWWRITE)) )
	{
		err_msg("UserRowWrite Key not accepted\n");
		return 0;		
	}
	
	//Toggle reset
	reset(1);
	reset(0);
	
	//wait for URowProgBit status
	wait_userrow_state(1,5);//wait up to 500ms for status
	
	//write the data
	write_data(g_pDevice->userrow_address,pUserRowData,g_pDevice->eeprom.pagesize);
	
	//write the userrowfinal bit
	stcs(UPDI_ASI_SYS_CTRLA, UPDI_UROWFINAL_VALUE);//pg547
	
	//wait for URowProgBit status
	wait_userrow_state(0,5);//wait up to 500ms for status	
	
	//Write the UROWWRITE bit in UPDI.ASI_KEY_STATUS
	stcs(UPDI_ASI_KEY_STATUS, UPDI_UROWWRITE_VALUE);//pg547
	
	//Toggle reset
	reset(1);
	reset(0);	
	
	log_msg(VERBOSE_MINIMAL,"userrow_write Done, %d bytes written\n",g_pDevice->eeprom.pagesize);

	return 1;		
}

//print some hex data
void print_hex_data(U8* data, int len, int col_width)
{
	int x=0;
	for(int i=0; i<len; i++)
	{
		printf("%02X ",data[i]);
		x++; if(x==col_width || i==(len-1)){x=0; printf("\n");}
	}
}

//print some char data
void print_chr_data(U8* data, int len, int col_width)
{
	int x=0;
	for(int i=0; i<len; i++)
	{
		U8 c = data[i];
		if(c < ' ' || c > '~'){c = ' ';}//deal with invalid characters
		printf(" %c ",c);
		x++; if(x==col_width || i==(len-1)){x=0; printf("\n");}
	}
}

//Reads out device information from various sources
void device_info()
{
	printf("\n---UPDI device info---\n"); //datasheet pg535 for SIB
	int ret_len = 0;
	U8* d = read_sib(&ret_len);
	printf("Ret Bytes     = %03d\n",ret_len);
	printf("SIB           = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15]);
	printf("Family ID     = %c%c%c%c%c%c%c\n",d[0],d[1],d[2],d[3],d[4],d[5],d[6]);
	printf("Reserved      = 0x%02X\n",d[7]);
	printf("NVM revision  = %c%c%c\n",d[8],d[9],d[10]);
	printf("OCD revision  = %c%c%c\n",d[11],d[12],d[13]);
	printf("Reserved      = 0x%02X\n",d[14]);
	printf("PDI OSC       = %c MHz\n",d[15]);
	printf("PDI revision  = %03d\n",ldcs(UPDI_CS_STATUSA) >> 4);
	//printf("Family ID     = %02X %02X %02X %02X %02X %02X %02X %02X\n",d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7]);
	//printf("NVM revision  = %03d\n",d[10]);
	//printf("OCD revision  = %03d\n",d[13]);
	//printf("PDI OSC       = %03d MHz\n",d[15]);
	//printf("PDI revision  = %03d\n",ldcs(UPDI_CS_STATUSA) >> 4);
	if(in_prog_mode() && g_pDevice)
	{
		//device ID info
		U8 devid[3] = {0,0,0};
		U8 devrev = 0;
		read_data(g_pDevice->sigrow_address  , devid, 3);
		read_data(g_pDevice->syscfg_address+1,&devrev,1);
		printf("Device ID     = %02X %02X %02X rev %c\n",devid[0],devid[1],devid[2],('A'+devrev) );
		//display sigrow data...
		#define SIGROWEND  0x25
		#define SIGROWSIZE (SIGROWEND+1)
		U8 s[SIGROWSIZE];
		read_data(g_pDevice->sigrow_address,s,SIGROWSIZE);
		printf("Serial Number = %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",s[3],s[4],s[5],s[6],s[7],s[8],s[9],s[10],s[11],s[12]);
		printf("SigRow        = \n"); 
		print_hex_data(s,SIGROWSIZE,16); 
		print_chr_data(s,SIGROWSIZE,16);
		//fuses
		//for(int i=0; i<=10; i++)//all devices seem to have 10 fuses
		//{ fuse_read(i); }
		printf("WDTCFG   "); fuse_read(0x00); 
		printf("BODCFG   "); fuse_read(0x01); 
		printf("OSCCFG   "); fuse_read(0x02); 
		printf("Reserved "); fuse_read(0x03); 
		printf("TCD0CFG  "); fuse_read(0x04); 
		printf("SYSCFG0  "); fuse_read(0x05); 
		printf("SYSCFG1  "); fuse_read(0x06);
		printf("APPEND   "); fuse_read(0x07); 
		printf("BOOTEND  "); fuse_read(0x08); 
		printf("Reserved "); fuse_read(0x09); 
		printf("LOCKBIT  "); fuse_read(0x0A);
		//user row data
		U8 urdata[g_pDevice->eeprom.pagesize];
		read_data(g_pDevice->userrow_address, urdata, g_pDevice->eeprom.pagesize);	
		printf("UserRowData   = \n"); 
		print_hex_data(urdata,g_pDevice->eeprom.pagesize,16); 				
	}
} 

U8 init()
{
	log_msg(VERBOSE_NORMAL,"\n---UPDI init---\n");
	//open uart
	log_msg(VERBOSE_DEBUG,"Opening uart\n");
	OpenUart(g_Baud,STOP_BITS_TWO);
	
	//send break handshake
	U8 brk = UPDI_BREAK;
	send(&brk,1);
	
    //Set the inter-byte delay bit and disable collision detection
	stcs(UPDI_CS_CTRLB, (1 << UPDI_CTRLB_CCDETDIS_BIT) );
	stcs(UPDI_CS_CTRLA, (1 << UPDI_CTRLA_IBDLY_BIT) );	
		
	//check status
	if(ldcs(UPDI_CS_STATUSA) != 0)
	{
		log_msg(VERBOSE_MINIMAL,"UPDI init OK\n");
		return 1;		
	}
	else//UPDI is still not INIT'ED
	{
		log_msg(VERBOSE_DEBUG,"UPDI needs initialized...\n");
		log_msg(VERBOSE_DEBUG,"sending double break\n");
		//Sends a double break to reset the UPDI port
		//BREAK is actually just a slower zero frame
		//A double break is guaranteed to push the UPDI state
		//machine into a known state, albeit rather brutally
		OpenUart(B300,STOP_BITS_ONE);
		
		U8 dbrk[] = {UPDI_BREAK, UPDI_BREAK};
		write(g_handle, dbrk, 2);
		
		U8 tmp[2] = {0,0};
		read(g_handle, tmp, 2);//read echo byte	
	}
		
	//try to open again
	log_msg(VERBOSE_DEBUG,"Opening uart\n");
	OpenUart(g_Baud,STOP_BITS_TWO);	
	
    //Set the inter-byte delay bit and disable collision detection
	stcs(UPDI_CS_CTRLB, (1 << UPDI_CTRLB_CCDETDIS_BIT) );
	stcs(UPDI_CS_CTRLA, (1 << UPDI_CTRLA_IBDLY_BIT) );
		
	//Check UPDI by loading CS STATUSA
	if(ldcs(UPDI_CS_STATUSA) != 0)
	{
		log_msg(VERBOSE_MINIMAL,"UPDI init OK\n");
		return 1;
	}
	
	err_msg("UPDI unable to init\n");
	Cleanup();
	exit(0);//exit program
	return 0;		
}

U8 validate_device()
{
	if(!g_pDevice){err_msg("No part specified! must use -pn to specify a part type\n"); return 0;}
	return 1;
}

int main(int argc, char **argv)
{	
	PI_INIT; //if running on pi, do pi init

	//handle CTRL+C
	signal(SIGINT,signal_handler);//allow Ctrl+C to interrupt application	
	
	//verbose screen logging disabled by default
	g_Verbose = 1;
	
	//no arguments
	if(argc == 1)
	{
		help:
		while(0);
		char msg[] = "\n"
		PROG_HEADER "\n"
		"usage... (actions will be performed in order of entered arguments)\n"
		"-p           <port_name>          For example: \"/dev/ttyAMA0\" or \"/dev/ttyUSB0\" or \"/dev/ttyS0\"\n"
		"-b           <baud_rate>          Set baud rate, default is 115200\n"
		"-pn          <partnum>            Required. Specifies AVR device; t2k, t4k, t8k, t16k, t32k, m32k, m48k\n"
		"-v                                Verbose Level, 0=no_progress_bars 1=normal 2=debug_info\n"
		"-e                                Unlock and Erase part, will clear lockbit fuse to unlock state\n"
		"-i                                Gather as much info from device as possible\n"
		"-fuseR       <fuse_num>           Read specific fuse\n"
		"-fuseW       <fuse_num>  <value>  Write a specific fuse value. example \"-fuseW 1 0xFF\" or \"-fuseW 1 255\"\n"
		"-flashR      <file_name>          Read Device Flash to intel hex file\n"
		"-flashW      <file_name>          Write Device Flash from intel hex file (do a -e first! does not erase pages before write)\n"
		"-eepromR     <file_name>          Read EEPROM to intel hex file\n"
		"-eepromW     <file_name>          Write EEPROM from intel hex file (do a -e first! does not erase pages before write)\n"
		"-userrowW    <file_name>          Write UserRow data from intel hex file (can write even if chip is locked)\n"
		"-userrowR    <file_name>          Read UserRow data to intel hex file (chip must be unlocked to read)\n"
		"-txt2hex     <in> <out>           text file to hex, such as \"00, FF, AB\" or \"0x00, 0xFF, 0xAB\" to intel hex file\n"	
		"-hex2c       <in> <out>           Convert intel hex file into a C variable output file\n"
		"-merge       <in> <in> <out>      Merge bootloader hex and application hex into single intel hex file output\n"
		"\n"
		"If you want to speed up Flashing your chip, erase it first with \"-e\" (example below).\n"
		"Erase argument will erase chips and skip the page erase during the flash write, speeding up the flashing.\n"
		"FLASH and EEPROM writes will be verified after writing. (only pages written will be verified)\n"
		"\n"
		"Example reading device to hex file...      updi -pn t16k -p /dev/ttyAMA0 -flashR FlashDump.hex\n"
		"Example erase/write hex file to device...  updi -pn t16k -p /dev/ttyAMA0 -e -flashW MyFlash.hex\n"
		"Example of unlock,erase,write; Flash & EEPROM & locking the chip again...\n"
		"updi -p /dev/ttyAMA0 -pn t16k -e -flashW MyFlash.hex -eepromW MyEEPROM.hex -fuseW 10 0xFF\n"
		"\n"
		"Supported Devices...\n" //pg10 of datasheet for t1614
		" t2k ... tiny202, tiny204, tiny212, tiny214\n"
		" t4k ... tiny402, tiny404, tiny406, tiny412, tiny414, tiny416, tiny417\n"
		" t8k ... tiny814, tiny816, tiny817\n"
		"t16k ... tiny1614, tiny1616, tiny1617\n"
		"t32k ... tiny3216, tiny3217\n"
		"m32k ... mega3208, mega3209\n"
		"m48k ... mega4808, mega4809\n\n"
		"To setup Pi3 and PiZeroW you must add \"dtoverlay=pi3-miniuart-bt\" to the /boot/config.txt\n"
		"https://www.raspberrypi.org/documentation/configuration/uart.md\n"
		"This is because ttyS0 does NOT have parity support, and we NEED parity support\n"
		"after editing then you will use /dev/ttyAMA0\n\n";
		printf(msg);
		return 0;
	}
	
	//ensure PI UART is active, and not a GPIO
	#ifdef PI_COMPILE
	XMT_SETUP_UART;
	#endif 	

	//parse arguments (that must be processed 1st) ... https://www.thegeekstuff.com/2013/01/c-argc-argv/
	for(int i=1; i<argc; i++) 
	{
		//help
		if(strcmp("-help",argv[i])==0)
		{
			goto help;
		}
		if(strcmp("-v",argv[i])==0)
		{ 
			i++; char* verb_lvl = argv[i];
			g_Verbose = (U8)(int)strtol(verb_lvl,NULL,0);//base 0 will detect "0x" in the hex string, otherwise it assumes base 10
		}
		if(strcmp("-p",argv[i])==0 && (i+1)<argc)
		{
			i++; char* port_name = argv[i];
			if(strlen(port_name) < 1){err_msg("-port is not valid, name too long or too short\n"); return 0;}
			g_UartName = port_name;
		}
		if(strcmp("-b",argv[i])==0 && (i+1)<argc)	
		{
			i++; int baud = atoi(argv[i]);
			switch(baud)
			{
				case     50: g_Baud=B50;     break;
				case     75: g_Baud=B75;     break;
				case    110: g_Baud=B110;    break;
				case    134: g_Baud=B134;    break;
				case    150: g_Baud=B150;    break;
				case    200: g_Baud=B200;    break;
				case    300: g_Baud=B300;    break;
				case    600: g_Baud=B600;    break;
				case   1200: g_Baud=B1200;   break;
				case   1800: g_Baud=B1800;   break;
				case   2400: g_Baud=B2400;   break;
				case   4800: g_Baud=B4800;   break;
				case   9600: g_Baud=B9600;   break;
				case  19200: g_Baud=B19200;  break;
				case  38400: g_Baud=B38400;  break;
				case  57600: g_Baud=B57600;  break;
				case 115200: g_Baud=B115200; break;
				case 230400: g_Baud=B230400; break;
				default: 
				err_msg("-baud %d argument not valid argument, valid baud rates are...\n",baud); 
				err_msg("50,75,110,134,150,200,300,600,1200,1800,2400,\n",baud); 
				err_msg("4800,9600,19200,38400,57600,115200,230400\n",baud); 
				return 0;		
			}
		}	
		if(strcmp("-txt2hex",argv[i])==0 && (i+1)<argc && (i+2)<argc)//convert txt file to intel hex file
		{
			printf("\n---TXT to HEX file conversion---\n");
			//get argument
			i++; char* sIn  = argv[i];
			i++; char* sOut = argv[i];
			ihf_txt2hex(sIn,sOut);
		}	
		if(strcmp("-hex2c",argv[i])==0 && (i+1)<argc && (i+2)<argc)//convert intel hex file to c variable text file
		{
			printf("\n---HEX to C Variable file conversion---\n");
			//get argument
			i++; char* sIn  = argv[i];
			i++; char* sOut = argv[i];
			ihf_hex2c(sIn,sOut);
		}
		if(strcmp("-merge",argv[i])==0 && (i+1)<argc && (i+2)<argc && (i+3)<argc)//merge bootloader and application into single hex file
		{
			printf("\n---Merge Intel Hex Files---\n");
			//get argument
			i++; char* sIn1 = argv[i];
			i++; char* sIn2 = argv[i];
			i++; char* sOut = argv[i];
			ihf_merge(sIn1,sIn2,sOut);
		}		
	}	
	
	log_msg(VERBOSE_NORMAL,PROG_HEADER);
	log_msg(VERBOSE_MINIMAL,"Application started, press CTRL+C to exit...\n");	
	//printf("args %d\n",argc);	
	
	//parse arguments (that must be processed 2nd) ... https://www.thegeekstuff.com/2013/01/c-argc-argv/
	for(int i=1; i<argc; i++) 
	{
		//part type
		if(strcmp("-pn",argv[i])==0 && (i+1)<argc)
		{
			i++; char* prt = argv[i];
			     if(strcmp(prt, "t2k")==0){g_pDevice = &DEVICES_TINY_2K;}
			else if(strcmp(prt, "t4k")==0){g_pDevice = &DEVICES_TINY_4K;}
			else if(strcmp(prt, "t8k")==0){g_pDevice = &DEVICES_TINY_8K;}
			else if(strcmp(prt,"t16k")==0){g_pDevice = &DEVICES_TINY_16K;}
			else if(strcmp(prt,"t32k")==0){g_pDevice = &DEVICES_TINY_32K;}
			else if(strcmp(prt,"m32k")==0){g_pDevice = &DEVICES_MEGA_32K;}
			else if(strcmp(prt,"m48k")==0){g_pDevice = &DEVICES_MEGA_48K;}
			else{ err_msg("-pn argument of [%s] is not valid\n",prt); return 0; }
			init();            //init updi
			enter_progmode();  //enter into programming mode			 
		}		
	}
	
	//parse arguments (that must be processed 3rd)... if NOT in programming mode, the only thing we can do it erase chip, or read device info
	for(int i=1; i<argc; i++) 
	{
		if(strcmp("-i",argv[i])==0)//read device info
		{
			if(!validate_device()){goto done;}
			device_info();			
		}	
		if(strcmp("-e",argv[i])==0)//unlock and erase part
		{
			if(!validate_device()){goto done;}
			g_ChipEraseOK = unlock_erase();
			//testing shows we are already in programming mode after erase cycle.
			//just to be safe, do below again anyway, doing it once more adds almost zero time to process anyway
			init();            //re-inint updi
			enter_progmode();  //try to enter into programming mode				 	
		}			
	}
	
	//if we are NOT in programming mode by now, exit
	if(!in_prog_mode())
	{printf("\nNot in programming mode, exiting now.\n");}		

	//parse arguments again... (everything below, you must be in programming mode) https://www.thegeekstuff.com/2013/01/c-argc-argv/
	for(int i=1; i<argc; i++) 
	{
		//printf("processing arg: %s\n",argv[i]);
		if(strcmp("-fuseR",argv[i])==0 && (i+1)<argc)//read specific fuse
		{
			if(!validate_device()){goto done;}
			i++; fuse_read( atoi(argv[i]) );		
		}
		if(strcmp("-fuseW",argv[i])==0 && (i+1)<argc  && (i+2)<argc)//write a specific fuse value. example -wf 1 0xFF
		{
			if(!validate_device()){goto done;}
			//https://stackoverflow.com/questions/10156409/convert-hex-string-char-to-int
			i++; int fuse_num = (int)strtol(argv[i],NULL,0);//base 0 will detect "0x" in the hex string, otherwise it assumes base 10
			i++; int fuse_val = (int)strtol(argv[i],NULL,0);//base 0 will detect "0x" in the hex string, otherwise it assumes base 10	
			fuse_write(fuse_num,fuse_val);	
		}			
		if(strcmp("-flashR",argv[i])==0 && (i+1)<argc)//read flash to hex file
		{
			if(!validate_device()){goto done;}
			//get argument
			i++; char* sPath = argv[i];
			//create virtual flash memory to work with
			U8 FlashData[g_pDevice->flash.size];
			memset(FlashData,0xFF,sizeof(FlashData));
			//read flash data
			nvm_read(FlashData,&g_pDevice->flash);
			//write to intel hex file
			ihf_write(sPath,FlashData,g_pDevice->flash.size);
			
		}	
		if(strcmp("-flashW",argv[i])==0 && (i+1)<argc)//Write flash to hex file
		{
			if(!validate_device()){goto done;}
			//get argument
			i++; char* sPath = argv[i];
			//create virtual flash memory to work with
			U8 FlashData[g_pDevice->flash.size];
			memset(FlashData,0xFF,sizeof(FlashData));			
			//read intel hex file into memory variable
			if(!ihf_read(sPath,FlashData,g_pDevice->flash.size))
			{err_msg("flashW: intel hex file empty\n"); }	
			else
			{	
				//flash the chip
				nvm_write(FlashData,&g_pDevice->flash);	
				//verify
				nvm_verify(FlashData,&g_pDevice->flash);	
			}		
		}
		if(strcmp("-eepromR",argv[i])==0 && (i+1)<argc)//read eeprom to hex file
		{
			if(!validate_device()){goto done;}
			//get argument
			i++; char* sPath = argv[i];
			//create virtual flash memory to work with
			U8 EepromData[g_pDevice->eeprom.size];
			memset(EepromData,0xFF,sizeof(EepromData));
			//read flash data
			nvm_read(EepromData,&g_pDevice->eeprom);
			//write to intel hex file
			ihf_write(sPath,EepromData,g_pDevice->eeprom.size);
			
		}			
		if(strcmp("-eepromW",argv[i])==0 && (i+1)<argc)//write eeprom to hex file
		{
			if(!validate_device()){goto done;}
			//get argument
			i++; char* sPath = argv[i];
			//create virtual eeprom memory to work with
			U8 EepromData[g_pDevice->eeprom.size];
			memset(EepromData,0xFF,sizeof(EepromData));			
			//read intel hex file into memory variable
			if(!ihf_read(sPath,EepromData,g_pDevice->eeprom.size))
			{err_msg("eepromW: intel hex file empty\n"); }
			else
			{		
				//flash the chip
				nvm_write(EepromData,&g_pDevice->eeprom);
				//verify
				nvm_verify(EepromData,&g_pDevice->eeprom);	
			}						
		}
		if(strcmp("-userrowR",argv[i])==0 && (i+1)<argc)//read user row to hex file
		{
			if(!validate_device()){goto done;}
			//get argument
			i++; char* sPath = argv[i];
			//create virtual memory to work with
			U8 urdata[g_pDevice->eeprom.pagesize];
			//read user row data			
			read_data(g_pDevice->userrow_address, urdata, g_pDevice->eeprom.pagesize);	
			//write intel hex file					
			ihf_write(sPath,urdata,g_pDevice->eeprom.pagesize);
		}			
		if(strcmp("-userrowW",argv[i])==0 && (i+1)<argc)//write user row from hex file
		{
			if(!validate_device()){goto done;}
			//get argument
			i++; char* sPath = argv[i];
			//create virtual flash memory to work with
			U8 UserRowData[g_pDevice->eeprom.pagesize];
			memset(UserRowData,0xFF,sizeof(UserRowData));			
			//read intel hex file into memory variable
			if(!ihf_read(sPath,UserRowData,g_pDevice->eeprom.pagesize))
			{err_msg("eepromW: intel hex file empty\n"); }		
			else
			{
				//userrow write
				userrow_write(UserRowData);			
			}			
		}	
	}
	
	done:
	//automatically exit programming mode when done
	if(in_prog_mode())
	{leave_progmode(); log_msg(VERBOSE_MINIMAL,"Exited programming mode OK\n");}		
		
	log_msg(VERBOSE_NORMAL,"\nFinished with requested actions\n");
	Cleanup();
	return 0;	
	
	//read fuses
	//fuse 10 is lockbit
	//pg30 is FUSE summary
	//pg41 ... 0xC5=DeviceUnlocked,  AnythingElse=DeviceLocked
	//chip_erase will clear lockbits, only CS-space operations can be performed when chip is locked	

	return 0;
}

