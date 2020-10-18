
#include "usb.h"

//extern PROGMEM const char usbDescriptorHidReport[];
//extern PROGMEM const char usbDescriptorConfiguration[];

//-----------------custom Descriptors begin----------------------------
//#undef USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH
//#define USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH 34 //sizeof(usbDescriptorHidReport)
PROGMEM const char usbDescriptorHidReport[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = { 
	#define USB_CFG_EP1_NUMBER      1    //EP3 number is defined in usbconfig.h, EP1 is 1, and defined here for custom descriptors below
	#define MY_EN1_REPORT_CNT       8    //number of bytes in endpoint1 interrupt transfer if one is used
	#define MY_EN3_REPORT_CNT       8    //number of bytes in endpoint3 interrupt transfer if one is used
	#define USB_CFG_ENDPOINT1_TYPE  0x80 //(default)ReportIn=0x80[PC_RECV]   ReportOut=0x00[PC_XMT]
	#define USB_CFG_ENDPOINT3_TYPE  0x00 //(default)ReportIn=0x80[PC_RECV]   ReportOut=0x00[PC_XMT]	
	//[Endpoint 0] 
	0x06, 0xa0, 0xff, // USAGE_PAGE (Vendor Defined Page 1)
	0x09, 0x01,       // USAGE (Vendor Usage 1)
	0xa1, 0x01,       // COLLECTION (Application)
	//[Endpoint 1] Interrupt-In (used to send data to the host spontaneously)
	// Input Report [PC receives data from AVR]
	0x09, 0x02,       // Usage ID - vendor defined usage2
	0x15, 0x00,       // Logical Minimum (0)
	0x26, 0xFF, 0x00, // Logical Maximum (255)
	0x75, 0x08,       // Report Size (8 bits)
	0x95, MY_EN1_REPORT_CNT,       // Report Count (8 fields)
	#if USB_CFG_ENDPOINT1_TYPE == 0x00 //set report type
	0x91, 0x02,       // Output (Data, Variable, Absolute) //0x81=PC_RECV 0x91=PC_XMT
	#else
	0x81, 0x02,       // Input (Data, Variable, Absolute) //0x81=PC_RECV 0x91=PC_XMT
	#endif
	#if USB_CFG_HAVE_INTRIN_ENDPOINT3 == 1
	//[Endpoint 2] Interrupt-Out
	// Output Report [PC sends data to AVR]
	0x09, 0x03,       // Usage ID - vendor defined usage3
	0x15, 0x00,       // Logical Minimum (0)
	0x26, 0xFF, 0x00, // Logical Maximum (255)
	0x75, 0x08,       // Report Size (8 bits)
	0x95, MY_EN3_REPORT_CNT,  // Report Count (8 fields)
		#if USB_CFG_ENDPOINT3_TYPE == 0x00 //set report type
		0x91, 0x02,       // Output (Data, Variable, Absolute) //0x81=PC_RECV 0x91=PC_XMT
		#else
		0x81, 0x02,       // Input (Data, Variable, Absolute) //0x81=PC_RECV 0x91=PC_XMT
		#endif
	#endif
	0xc0              // END_COLLECTION
};

//#undef USB_CFG_DESCR_PROPS_CONFIGURATION
//#define USB_CFG_DESCR_PROPS_CONFIGURATION   32 //sizeof(usbDescriptorConfiguration)
PROGMEM const char usbDescriptorConfiguration[USB_CFG_DESCR_PROPS_CONFIGURATION] = {  //USB configuration descriptor
	#define USBDESCR_CONFIG     2 //also defined in usbdrv.h, need here for custom descriptor
	#define USBDESCR_INTERFACE  4 //also defined in usbdrv.h, need here for custom descriptor
	#define USBDESCR_ENDPOINT   5 //also defined in usbdrv.h, need here for custom descriptor
    9,                // sizeof(usbDescriptorConfiguration): length of descriptor in bytes 
    USBDESCR_CONFIG,  // descriptor type 
    18 + 7 * USB_CFG_HAVE_INTRIN_ENDPOINT + 7 * USB_CFG_HAVE_INTRIN_ENDPOINT3 +
                (USB_CFG_DESCR_PROPS_HID & 0xff), 0,
                // total length of data returned (including inlined descriptors) 
    1,          // number of interfaces in this configuration 
    1,          // index of this configuration 
    0,          // configuration name string index 
#if USB_CFG_IS_SELF_POWERED
    (1 << 7) | USBATTR_SELFPOWER,       // attributes 
#else
    (1 << 7),                           // attributes 
#endif
    USB_CFG_MAX_BUS_POWER/2,            // max USB current in 2mA units 
// interface descriptor follows inline: 
    9,          // sizeof(usbDescrInterface): length of descriptor in bytes 
    USBDESCR_INTERFACE, // descriptor type 
    0,          // index of this interface 
    0,          // alternate setting for this interface 
    USB_CFG_HAVE_INTRIN_ENDPOINT + USB_CFG_HAVE_INTRIN_ENDPOINT3, // endpoints excl 0: number of endpoint descriptors to follow 
    USB_CFG_INTERFACE_CLASS,
    USB_CFG_INTERFACE_SUBCLASS,
    USB_CFG_INTERFACE_PROTOCOL,
    0,          // string index for interface 
#if (USB_CFG_DESCR_PROPS_HID & 0xff)    // HID descriptor 
    9,          // sizeof(usbDescrHID): length of descriptor in bytes 
    USBDESCR_HID,   // descriptor type: HID 
    0x01, 0x01, // BCD representation of HID version 
    0x00,       // target country code 
    0x01,       // number of HID Report (or other HID class) Descriptor infos to follow 
    0x22,       // descriptor type: report 
    USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH, 0,  // total length of report descriptor 
#endif
#if USB_CFG_HAVE_INTRIN_ENDPOINT    // endpoint descriptor for endpoint 1 
    7,          // sizeof(usbDescrEndpoint) 
    USBDESCR_ENDPOINT,  // descriptor type = endpoint 
    (char)(USB_CFG_ENDPOINT1_TYPE | USB_CFG_EP1_NUMBER), // IN endpoint number 1 was originally 0x81, now dynamically figured out
    0x03,       // attrib: Interrupt endpoint 
    8, 0,       // maximum packet size 
    USB_CFG_INTR_POLL_INTERVAL, // in ms 
#endif
#if USB_CFG_HAVE_INTRIN_ENDPOINT3   // endpoint descriptor for endpoint 3 
    7,          // sizeof(usbDescrEndpoint) 
    USBDESCR_ENDPOINT,  // descriptor type = endpoint 
    (char)(USB_CFG_ENDPOINT3_TYPE | USB_CFG_EP3_NUMBER), // IN endpoint number 3 was originally (0x80 | USB_CFG_EP3_NUMBER)
    0x03,       // attrib: Interrupt endpoint 
    8, 0,       // maximum packet size 
    USB_CFG_INTR_POLL_INTERVAL, // in ms 
#endif
};
//----------------------------------------------------------------------


//----------------------------------------------------------
//----------------------------------------------------------
//                    BEGIN USB FUNCTION CODE
//----------------------------------------------------------
//----------------------------------------------------------

//-----------------GLOBAL VARIABLES-------------------------
#define USB_REPORT_CNT 0x08 //size of 8 bytes is max for low speed usb
volatile U8 g_UsbBuf[USB_REPORT_CNT];// = {1,2,3,4,5,6,7,8};
//----------------------------------------------------------

//do we have data to send?
inline void usbPollSendtoHost()
{
	//we use first byte (command byte) to detect if we have data to send
	if(g_UsbBuf[0] && usbInterruptIsReady())//if we have data to send, and previous data was sent
	{               
		uchar* p = (void*)g_UsbBuf;
	    usbSetInterrupt(p,USB_REPORT_CNT);
		g_UsbBuf[0] = 0; //we copied data to interrupt buffer, clear this byte
	}
}

//this is where we receive data from PC
inline void usbFunctionWriteOut(uchar *data, uchar len)
{
	//first byte is EEPROM address, second byte is the data to write
	U8* pCmd = (void*)data;		//'R'=read (will respond with read byte), 'W'=write (will NOT repond)
	U8* pAdr = (void*)data+1;	//EEPROM address to read or write
	U8* pVal = (void*)data+2;	//value to read or write
	switch( (*pCmd) )
	{
		case 'W': cli(); UpdateEE8( (*pAdr) , (*pVal) ); sei(); break;	    //write EEPROM (no reponse is given after writing), maybe can use ATOMIC_BLOCK(ATOMIC_FORCEON){
		case 'R': g_UsbBuf[2] = ReadEE8( (*pAdr) ); g_UsbBuf[0]='R'; break; //read EEPROM (this will trigger usbPollSendtoHost to send a response) 			
	}
}

//we don't use this feature, so just return 0
inline usbMsgLen_t usbFunctionSetup(uchar *data)
{
	return 0;
}

//calibrate internal oscillator to 16.5MHz or 12.8MHz
static inline void usb_calibrate_osc()
{
	//16.5MHz will be 2356 (we want to arrive at this point)
	//16.0MHz will be 2284 (internal oscillator at 16MHz starts at about this point)
	//12.8MHz will be 1827 (we want to arrive at this point)
	I16 targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5); //10.5e6 is 10500000 //targetValue should be 2356 for 16.5MHz
	U8 tmp = CLKCTRL_OSC20MCALIBA; //oscilator default calibration value
	
	//trim to 16.5MHz or 12.8MHz
	// https://www.silabs.com/community/interface/knowledge-base.entry.html/2004/03/15/usb_clock_tolerance-gVai
	// http://vusb.wikidot.com/examples	
	U8  sav = tmp;
	I16 low = 32767; //lowest saved value
	while(1) //normally solves in about 5 itterations
	{
		I16 cur = usbMeasureFrameLength() - targetValue; //we expect cur to be negative numbers until we overshoot
		
		I16 curabs = cur;
		if(curabs < 0){curabs = -curabs;} //make a positive number, so we know how far away from zero it is
		if(curabs < low)  //current value is lower
		{
			low = curabs; //got a new lower value, save it
			sav = tmp;    //save the OSC CALB value
		}
		else //things just got worse, curabs > low, so saved value before this was the best value
		{
			_PROTECTED_WRITE(CLKCTRL_OSC20MCALIBA,sav);
			return;
		}		
		
		if(cur < 0) //freq too high
		{ tmp++; }  //+1 will increase frequency by about 1%	
		else        //freq too low
		{ tmp--; }  //-1 will increase frequency by about 1%	
		//apply change and check again
		_PROTECTED_WRITE(CLKCTRL_OSC20MCALIBA,tmp);		
	}	
}

//a USB reset occured, disable all internal functions except USB
inline void usbHadReset()
{
	#if F_CPU == 16500000 || F_CPU == 12800000
    //cli();  // usbMeasureFrameLength() counts CPU cycles, so disable interrupts.
    ATOMIC_BLOCK(ATOMIC_FORCEON)
    {
		usb_calibrate_osc();
	}
    //sei();
    #endif
}

inline void usbMyInit()
{
	usbInit();
    usbDeviceDisconnect();  //enforce re-enumeration, do this while interrupts are disabled!
	_delay_ms(250);
    usbDeviceConnect();
}

inline void usbMyPolling()
{
    usbPoll();//check for USB work and incoming messages
	usbPollSendtoHost();//check if we have USB data to send
}
//----------------------------------------------------------
//----------------------------------------------------------
//                        END USB FUNCTION CODE
//----------------------------------------------------------
//----------------------------------------------------------