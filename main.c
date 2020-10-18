/////////////////////////////////////////////////////////////////////
//
// Author:  12oClocker
// License: GNU GPL (see License.txt)
// Date:    08-01-2020
//
/////////////////////////////////////////////////////////////////////
//
// The "beginners guide" to Xmega for those coming from tiny/mega is basically:
// DDRx  -> PORTx.DIR
// PINx  -> PORTx.IN
// PORTx -> PORTx.OUT
// xxSET, xxxCLR, xxxTGL options will prevent disturbing other bits
//
/////////////////////////////////////////////////////////////////////

//system includes
#include <avr/pgmspace.h>   //for PROGMEM
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdlib.h>			//for itoa()
#include <string.h>			//for strlen()
#include <util/delay.h>		//_delay_ms() or _delay_us()
#include <util/atomic.h>
#include <avr/wdt.h>		//watchdog timer
#include <stdio.h>			//snprintf (snprintf uses about 1560 bytes of flash)
#include "defines.h"
#include "usb.h"

//operating freq pg550 (+0.18v for every 1Mhz)
// 2.7v  10.0Mhz  Operation ensured down to BOD triggering level, V BOD with BODLEVEL2
// 2.88  11.0Mhz 
// 3.06  12.0Mhz  
// 3.24  13.0Mhz 
// 3.42  14.0Mhz 
// 3.60  15.0Mhz 
// 3.78  16.0Mhz 
// 3.87  16.5MHz
// 3.96  17.0Mhz 
// 4.14  18.0Mhz 
// 4.32  19.0Mhz 
// 4.50  20.0Mhz  Operation ensured down to BOD triggering level, V BOD with BODLEVEL7	
	
static inline void test_freq_out()
{
	DDR_SET(B,1); //configure PB1 as an output
	//we will just generate a freq, so we can measure it, and determine osillator freq.
	//setting 0 for prescaler and for CMP will output MAX freq, which is main_clk div2
	TCA0_SINGLE_CNT   = 0;            //set initial count value
	TCA0_SINGLE_CMP0  = 0;            //set freq gen OVF value 65535
	TCA0_SINGLE_CMP1  = 0;            //freq gen1 output will trigger at this CMP value
	TCA0_SINGLE_CMP2  = 32767;        //freq gen1 output will trigger at this CMP value
	TCA0_SINGLE_CTRLB = TCA_SINGLE_WGMODE_FRQ_gc | TCA_SINGLE_CMP1EN_bm; //freq gen mode (output toggle is NOT enabled yet)
	TCA0_SINGLE_CTRLA = TCA_SINGLE_ENABLE_bm;     //start timer TCA0 with no prescaler
	while(1);	
}

static inline void use_ext_clk()
{
	//select external clk
	_PROTECTED_WRITE(CLKCTRL_MCLKCTRLA,0x03); //pg85 select external lock
	_PROTECTED_WRITE(CLKCTRL_MCLKCTRLB,0x00); //pg86 no prescaler
	while(CLKCTRL_MCLKSTATUS & CLKCTRL_SOSC_bm); //pg88 wait for clock switch to complete		
}

static inline void use_16MHz_osc()
{
	//setup main clk freq to 16MHz, we trim to 16.5MHz later
	//programming fuses will set BOD, and 16MHz or 20MHz
	//datasheet pg550 BODLEVEL2 only guaranteed to 10MHz
	//use 0x02 (OSCCFG) value must be 0x02, for 16MHz fuse must be 0x01
	_PROTECTED_WRITE(CLKCTRL_MCLKCTRLB,0x00);            //no prescaler = 0x00, prescaler enable = CLKCTRL_PEN_bm (default prescaler is div2)
	while((CLKCTRL_MCLKSTATUS & CLKCTRL_OSC20MS_bm)==0); //pg88 wait for OSC20MS to become stable	
	
	// VUSB will trim to 16.5MHz	
}

static inline void use_16p5_Mhz_clk()
{
	//setup main clk freq to 16MHz, we trim to 16.5MHz later
	//programming fuses will set BOD, and 16MHz or 20MHz
	//datasheet pg550 BODLEVEL2 only guaranteed to 10MHz
	//use 0x02 (OSCCFG) value must be 0x02, for 16MHz fuse must be 0x01
	_PROTECTED_WRITE(CLKCTRL_MCLKCTRLB,0x00);            //no prescaler = 0x00, prescaler enable = CLKCTRL_PEN_bm (default prescaler is div2)
	while((CLKCTRL_MCLKSTATUS & CLKCTRL_OSC20MS_bm)==0); //pg88 wait for OSC20MS to become stable	
	
	//trim to 16.5MHz
	// https://www.silabs.com/community/interface/knowledge-base.entry.html/2004/03/15/usb_clock_tolerance-gVai
	// http://vusb.wikidot.com/examples
	//test calibration trimming, to run at 16.5MHz...
	//this test worked, we are running at 16.448Mhz, which is 0.316% of target freq.
	//so we should be able to use this method for V-USB driver
	U8 tmp = CLKCTRL.OSC20MCALIBA;
	tmp += 3; //+3 will increase frequency by about 3%
	_PROTECTED_WRITE(CLKCTRL_OSC20MCALIBA,tmp);	
}

void main(void) __attribute__((noreturn));  void main(void)
{
	#if F_CPU == 12000000 || F_CPU == 16000000 || F_CPU == 18000000 || F_CPU == 20000000
	use_ext_clk();
	#elif F_CPU == 12800000 || F_CPU == 16500000
	use_16MHz_osc();  // V-USB will trim to 16.5MHz or 12.8MHz (if you use VUSB in a bootloader, trim CLKCTRL_OSC20MCALIBA in your main app too!)
	#else
	error "F_CPU is not valid for main.c"
	#endif
	
	//just for testing...
	//use_16p5_Mhz_clk();
	//test_freq_out();
	
	//handy code for init tracing ports
	//VPORTB_DIR = PIN2_bm|PIN1_bm;
	//VPORTB_OUT = PIN2_bm|PIN1_bm;
	//asm(" sbi 0x05, 1 \n"); //SET PB1 HI
	//asm(" cbi 0x05, 1 \n"); //SET PB1 LO
	//;sbi 0x05, 2   ;SET PB2 HI so we can trace ISR duration start

	//usb init...
    usbMyInit();
	sei();//enable global interrupts (must be done AFTER usbMyInit)
	
	for(;;)
	{
		//OUT_TGL(B,1);
        usbMyPolling();	
	}	

}














