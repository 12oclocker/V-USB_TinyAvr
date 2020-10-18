#ifndef __usbdesc_h_included__
#define __usbdesc_h_included__

#include <avr/pgmspace.h>   //required by usbdrv.h
#include <util/delay.h>		//_delay_ms() or _delay_us()
#include "usbdrv.h"
#include "defines.h"
#include <avr/eeprom.h>
#include <inttypes.h>
#include <avr/io.h>
#include <util/atomic.h>

//custom descriptors are in the C file

//functions
void usbPollSendtoHost();                         //do we have data to send?
void usbFunctionWriteOut(uchar *data, uchar len); //this is where we receive data from PC
usbMsgLen_t usbFunctionSetup(uchar *data);        //we don't use this feature, so just return 0
void usbHadReset();                               //a USB reset occured, disable all internal functions except USB
void usbMyInit();                                 //init USB driver
void usbMyPolling();                              //usb data polling

#endif