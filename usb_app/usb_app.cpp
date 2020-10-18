//////////////////////////////////////////////////////////////////
// 
// Author:  12oClocker
// License: GNU GPL v2 (see License.txt)
// Date:    08-01-2020
//
// To Compile
// g++ -std=c++11 -g -Wall -Wshadow -DDEBUG -lusb-1.0 -pthread usb_app.cpp -o usb_app
//
//////////////////////////////////////////////////////////////////
//
// lsusb to list devices
// lsusb -vvv to get all info
//
// -----------USB PERMISSIONS TO WORK WITHOUT SUDO------------
// USB permissions located in... /etc/udev/rules.d/
// create file... 50-usb-permissions.rules
// contents...
// # USB devices (usbfs replacement)
// SUBSYSTEM=="usb", ATTR{idVendor}=="16c0", ATTR{idProduct}=="05dc", MODE="0666"
// KERNEL=="hiddev*", ATTRS{idVendor}=="16c0", MODE="0666"
//
//////////////////////////////////////////////////////////////////

#define USB_VID   0x16c0
#define USB_PID   0x05dc

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>             //for CTRL+C
#include <unistd.h>             //for CTRL+C
#include <atomic>               //std::atomic
#include <stdlib.h>             //srand, rand
//#include <time.h>             //time()
//#include <chrono>               //std::chrono::milliseconds, requires C++11 standard
#include <libusb-1.0/libusb.h>  //requires linker option -lusb-1.0    
//sudo apt-get install libusb-1.0-0-dev   
//sudo apt-get install libusb-1.0-0

//ints
#define  U8 uint8_t
#define U16 uint16_t
#define U32 uint32_t
#define U64 uint64_t
#define  I8 int8_t
#define I16 int16_t
#define I32 int32_t
#define I64 int64_t
#define f32 float_t
#define f64 double_t

//delays
#define delay_us(x)  usleep(x)
#define delay_ms(x)  usleep((x*1000))
#define delay_s(x)   sleep(x)

//printf colors
#define C_RED       "\033[0;31m"  //Red
#define C_RED_BOLD  "\033[1;31m"  //Bold Red
#define C_GRN       "\033[0;32m"  //Green
#define C_GRN_BOLD  "\033[1;32m"  //Bold Green
#define C_YEL       "\033[0;33m"  //Yellow
#define C_YEL_BOLD  "\033[01;33m" //Bold Yellow
#define C_BLU       "\033[0;34m"  //Blue
#define C_BLU_BOLD  "\033[1;34m"  //Bold Blue
#define C_MAG       "\033[0;35m"  //Magenta
#define C_MAG_BOLD  "\033[1;35m"  //Bold Magenta
#define C_CYAN      "\033[0;36m"  //Cyan
#define C_CYAN_BOLD "\033[1;36m"  //Bold Cyan
#define C_WHT       "\033[0;37m"  //White
#define C_WHT_BOLD  "\033[1;37m"  //Bold White
#define C_RESET     "\033[0m"     //Reset

#ifdef DEBUG
#define TRACE(...)     printf(__VA_ARGS__); fflush(stdout) //printf(x...) //windows method
#define ETRACE(...)    printf(C_RED); printf(__VA_ARGS__); printf(C_RESET); fflush(stdout) //printf(x...) //windows method
#define ERRTRACE(...)  printf(C_RED "\nFILE: %s\nLINE: %d\nFUNC: %s\nERROR: ",__FILE__,__LINE__,__func__); printf(__VA_ARGS__); printf("\n" C_RESET); fflush(stdout)
#endif


//global
int m_run = 1;
int m_interface_claimed = 0;
libusb_device_handle* m_devh = 0;

void hid_disconnect()
{
	if(m_devh)
	{
		if(m_interface_claimed)
		{
			libusb_release_interface(m_devh,0);
			m_interface_claimed = 0;
			TRACE("Released usb interface\n");
		}

		libusb_close(m_devh);
		m_devh = 0;
	}
}

void hid_shutdown()
{
	hid_disconnect();
	libusb_exit(NULL);	//shutdown library
}

bool hid_init()
{
	//init libusb
	int r = libusb_init(NULL);
	if(r < 0){ TRACE("Failed to initialise usb\n"); exit(0); return false;}
	return true;
}

//will open a device based on vid, pid, manufacturer_string, product_string
libusb_device_handle* hid_open_hiddev(uint16_t vid, uint16_t pid, const char* mfg, const char* prd)
{
	//default return value
	libusb_device_handle* devH = NULL;

	//get all usb devices to devs pointer
	libusb_device** devs;
	ssize_t cnt = libusb_get_device_list(NULL,&devs);
	if(cnt < 0){ return NULL; }//no usb devices found

	//spin through devices
	libusb_device *dev;
	int i=0;
	while((dev = devs[i++]) != NULL)
	{
		//get device descriptor
		libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if(r != 0){continue;}//failed to get device descriptor for this device

		//if vid and pid match, check the strings
		if(vid == desc.idVendor && pid == desc.idProduct && desc.iManufacturer > 0 && desc.iProduct > 0)
		{
			r = libusb_open(dev,&devH);//open device so we can read strings
			if(r == LIBUSB_SUCCESS && devH != NULL)
			{
				//Get the string associated with iManufacturer index.
				const int buflen = 256;
				unsigned char strbuf[buflen];
				r = libusb_get_string_descriptor_ascii(devH,desc.iManufacturer,strbuf,buflen);
				TRACE("MFG: %s\n",strbuf);
				if(!mfg || (r > 0 && strcmp((char*)strbuf,mfg)==0))//mfg string matches
				{
					r = libusb_get_string_descriptor_ascii(devH,desc.iProduct,strbuf,buflen);
					TRACE("PRD: %s\n",strbuf);
					if(!prd || (r > 0 && strcmp((char*)strbuf,prd)==0))//product string matches
					{ goto done; }// leave handle open and return the open handle, we found our device.
				}
			}
			else
			{
				const char* sError = libusb_error_name(r);
				ETRACE(sError);		
			}
			//close device handle
			if(devH != NULL )
			{ libusb_close(devH); devH=NULL; }
		}
	}

	//cleanup and return
	done:
	libusb_free_device_list(devs,1);
	return devH;
}

bool hid_connect(uint16_t vid, uint16_t pid, const char* mfg, const char* prd)
{
	if(m_devh){return false;}

	//open device
	//m_devh = libusb_open_device_with_vid_pid(NULL,USB_VID,USB_PID);
    m_devh = hid_open_hiddev(vid,pid,mfg,prd);
	if(!m_devh){ ETRACE("Failed to open device %04X %04X [%s] [%s]\n",vid,pid,mfg?mfg:"*",prd?prd:"*"); return false; }

	//auto detach kernel driver
	libusb_set_auto_detach_kernel_driver(m_devh,1);

	//claim the usb interface
	int r = libusb_claim_interface(m_devh, 0);
	if (r < 0){ ETRACE("Usb claim interface error %d\n", r); hid_disconnect(); return false; }
	m_interface_claimed = 1;
	TRACE("Claimed usb interface\n");

	return true;
}

//proprietary read byte command for t44a firmware
bool hid_read_byte(U8* ret_byte, U8 adr)
{	
	//command setup
    #define CMD_READ    'R'
    #define CMD_WRITE   'W'
    U8 data[8] = {0,0,0,0,0,0,0,0};
	data[0] = CMD_READ;
	data[1] = adr; //address to read
	
	//xmt data
	//EP OUT 0x02 = Endpoint Type 0x00 + Endpoint Number 2
	int xfer = 0;
	int r = libusb_interrupt_transfer(m_devh,0x02,data,sizeof(data),&xfer,250);//timeout in 250ms
	if(r != 0 || xfer != sizeof(data)){ETRACE("USB XMT ERROR %d, SENT %d\n",r,xfer); hid_disconnect(); return false;}

	//wait for response
	//EP IN 0x81 = Endpoint Type 0x80 + Endpoint Number 1
	xfer = 0;
	retry:
	r = libusb_interrupt_transfer(m_devh,0x81,data,sizeof(data),&xfer,250);//retry every 250ms
	if(m_run && r == LIBUSB_ERROR_TIMEOUT && xfer == 0)
	{goto retry;}//if m_run==1 and we have a timeout, try again
	if(r != 0 || xfer != sizeof(data))
	{ETRACE("RCV USB ERROR %d, XFER %d\n",r,xfer); hid_disconnect(); return false;}
	
	//byte0: CMD_READ, anything else error
	//byte1: echo adr
	//byte2: data requested
	if(data[0] != CMD_READ && data[1] != adr)
	{ETRACE("ECHO RESPONSE ERROR RSP=%d ADR=%d\n",data[0],adr); hid_disconnect(); return false;}
	
	//returned byte of data
	*ret_byte = data[2];
	
	return true;
}

//proprietary write byte command for t44a firmware
bool hid_write_byte(U8* data_byte, U8 adr)
{	
	//command setup
    #define CMD_READ    'R'
    #define CMD_WRITE   'W'
    U8 data[8] = {0,0,0,0,0,0,0,0};
	data[0] = CMD_WRITE; //command
	data[1] = adr;       //address to write to
	data[2] = (U8)(*data_byte); //data to write	
	
	//xmt data
	//EP OUT 0x02 = Endpoint Type 0x00 + Endpoint Number 2
	int xfer = 0;
	int r = libusb_interrupt_transfer(m_devh,0x02,data,sizeof(data),&xfer,250);//timeout in 250ms
	if(r != 0 || xfer != sizeof(data)){ETRACE("USB XMT ERROR %d, SENT %d\n",r,xfer); hid_disconnect(); return false;}

	//verify data written
	U8 verify_byte = 0xFF;
	if(!hid_read_byte(&verify_byte,adr)){return false;}
	if(*data_byte != verify_byte){ ETRACE("USB XMT VERIFY ERROR\n"); return false;}
	
	return true;
}

//proprietary read eeprom or compare eeprom
bool read_eeprom(const char* sDump, int toread)
{
	//
	// problem, sometimes dump file first byte is 0xFF,
	// added 250ms delay before reading, maybe that will fix the issue (it did fix the issue)
	//
	
	TRACE("Reading eeprom ");
	delay_ms(250);
	
	//256 bytes of EEPROM in attiny44a
	const int eelen = 256;
	U8 eeprom[eelen];
	memset(eeprom,0xFF,sizeof(eeprom));
	
	//read entire eeprom, or toread length
	TRACE("000%%");
	for(int i=0; i<toread; i++)
	{
		if(!hid_read_byte(&eeprom[i],i))
		{ return false; }
		int iprog = (i+1)*100/toread;
		printf("\b\b\b\b%3d%%",iprog); fflush(stdout);
	}
	TRACE("\n");
	
	//we have a dump file, so we are reading, not comparing
	if(sDump)
	{
		TRACE("Saving: %s\n",sDump);
		FILE* pFile = fopen(sDump,"wb");
		if(!pFile){ ETRACE("Unable to write file: %s\n",sDump); return false; }
		size_t written = fwrite(eeprom,1,toread,pFile);
		fclose(pFile);
		if(written != (size_t)toread){ ETRACE("Only wrote %lu of %d bytes\n",written,eelen); return false; }
	}
	else
	{
		//TODO, compare with file
		//load up all .eep files in current directory, compare with them.
		//check what kind of memory we had
		//dump to file, then compare to make sure it is correct.
		//or we could just have a bash script do this junk		
	}
	
	return true;
}

//proprietary read eeprom or compare eeprom
bool write_eeprom(const char* sWrite, int towrite)
{
	//
	// problem, sometimes dump file first byte is 0xFF,
	// added 250ms delay before reading, maybe that will fix the issue (it did fix the issue)
	//
	
	TRACE("Writing eeprom ");
	delay_ms(250);
	
	//256 bytes of EEPROM in attiny44a
	const int eelen = 256;
	U8 eeprom[eelen];
	memset(eeprom,0xFF,sizeof(eeprom));
	
	//read file into eeprom memory
	FILE* pFile = fopen(sWrite,"rb");
	if(!pFile){ ETRACE("Unable to read file: %s\n",sWrite); return false; }

	//obtain file size:
	//fseek(pFile,0,SEEK_END);
	//long lSize = ftell(pFile);
	//rewind(pFile);
	
	//copy the file into the buffer:
	size_t result = fread(eeprom,1,eelen,pFile);
	fclose(pFile); //close file
	if((int)result < towrite){ ETRACE("Only read %lu bytes from file, wanted %d\n",result,towrite); return false; }		
	
	//write entire eeprom, or towrite length
	TRACE("000%%");
	for(int i=0; i<towrite; i++)
	{
		if(!hid_write_byte(&eeprom[i],i))
		{ return false; }
		int iprog = (i+1)*100/towrite;
		printf("\b\b\b\b%3d%%",iprog); fflush(stdout);
	}
	TRACE("\n");
	
	return true;
}

void signal_handler(int sig)
{
   printf("Caught signal %d\n",sig);
   hid_shutdown();
   exit(1); 
}

int main(int argc, char **argv)
{
	//handle CTRL+C
	signal(SIGINT,signal_handler);//allow Ctrl+C to interrupt appliation
	printf("TinyAvr HID USB Test App [press CTRL+C to exit]\n");
	
	//check arguments
	const char* sDump = 0;
	const char* sWrite = 0;
	int mylimit = 256; //default is read entire eeprom
	
	//no args?
	if(argc == 1)
	{
		help:
		printf("Usage...\n");
		printf("-read  <file>    #create a eeprom dump file\n");
		printf("-write <file>    #write eeprom dump file to device\n");
		printf("-limit <bytes>   #number of eeprom bytes to read 1 to 256\n");
		exit(0);		
	}
	
	//spin through args
	for(int i=1; i<argc; i++) 
	{
		if(strcmp("-help",argv[i])==0 || strcmp("-h",argv[i])==0)//help
		{
			goto help;
		}
		if(strcmp("-read",argv[i])==0 && (i+1)<argc)//dump eeprom to file
		{
			//get next argument
			i++; sDump = argv[i];			
		}
		if(strcmp("-limit",argv[i])==0 && (i+1)<argc)//dump eeprom to file
		{
			//get next argument
			i++; mylimit = atoi(argv[i]);
			if(mylimit < 1){mylimit=1;}
			if(mylimit > 256){mylimit=256;}			
		}
		if(strcmp("-write",argv[i])==0 && (i+1)<argc)//dump eeprom to file
		{
			//get next argument
			i++; sWrite = argv[i];		
		}		
	}
	
	//init library
	if(!hid_init())
	{
		ETRACE("Unable to open device\n");
		exit(0);
	}
	
	//connect to device...
	if(!hid_connect(USB_VID,USB_PID,0,0))
	{
		ETRACE("Unable to open device\n");
		goto done;
	}
	
	//communicate
	if(sDump)
	{
		if(!read_eeprom(sDump,mylimit))
		{
			ETRACE("Unable to communicate with device\n");
			goto done;		
		}
	}
	
	//write to device
	if(sWrite)
	{
		if(!write_eeprom(sWrite,mylimit))
		{
			ETRACE("Unable to communicate with device\n");
			goto done;		
		}		
	}
	
	//shutdown & disconnect
	done:
	hid_shutdown();
	
	return 0;
}

