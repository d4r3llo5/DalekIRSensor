#include "wireless.h"              // Wireless setup & function defs
#include "msp430x22x4.h"           // chip-specific macros & defs
#include "stdint.h"                // MSP430 data type definitions

/*
 * Packet format is given with 4b: Address 4b: signal
 * 	signal is given as either 5 (0x0101) for active or A for off (0x1010)
 * 	We'll send data out every 1ms?
 */
// content: pktData[4] = {Pyld leng, base 	addr, 	data}
static uint8_t pktData[3] = {0x02,   0xFF,	0xA5};
uint8_t pktLen = 3;                 // Packet size
const uint8_t channel = 0;

#pragma vector=TIMERA1_VECTOR
__interrupt void IsrTimerTACC1(void)
{
	switch ( __even_in_range( TAIV, 10 ) ) {
	case TAIV_TAIFG:
		if ( (P2IN & 0x01) == 0x01 ) {		// P2.4 is high
			pktData[2] = 0xA5;
		} else {
			pktData[2] = 0xAA;
		}
		RFSendPacket(pktData, pktLen);      // Activate TX mode & send packet
		TI_CC_SPIStrobe(TI_CCxxx0_SIDLE);   // Set cc2500 to IDLE mode.
											// TX mode re-activates in RFSendPacket
											// AutoCal @ IDLE to TX Transitions
		P1OUT ^= 0x03;                      // Start of TX => toggle LEDs
		break;
	}
	TACTL &= ~(0x0001);                    // Clear TAIFG
}

//---------------------------------------------------------------------------
// Func:  Setup MSP430 Ports & Clocks and reset & config cc2500
// Args:  none
// Retn:  none
//---------------------------------------------------------------------------
void SetupWireless(void)
{
	volatile uint16_t delay;

	for(delay=0; delay<650; delay++){};   // Empirical: cc2500 Pwr up settle

	// set up clock system
	BCSCTL1 = CALBC1_8MHZ;                // set DCO freq. from cal. data
	BCSCTL2 |= DIVS_3;                    // SMCLK = MCLK/8 = 1MHz
	DCOCTL  = CALDCO_8MHZ;                // set MCLK to 8MHz

	// Port config
	P1DIR |=  0x03;                       // Set LED pins to output
	P1OUT &= ~0x03;                       // Clear LEDs

	// Wireless Initialization
	TI_CC_SPISetup();                     // Init. SPI port for cc2500
	P2SEL = 0;                            // Set P2.6 & P2.7 as GPIO
	TI_CC_PowerupResetCCxxxx();           // Reset cc2500
	writeRFSettings();                    // Put settings to cc2500 config regs

	TI_CC_SPIWriteReg(TI_CCxxx0_CHANNR,  channel);  // Set Your Own Channel Number (1,2,3,4)
	// AFTER writeRFSettings (???)

	for(delay=0; delay<650; delay++){};   // Empirical: Let cc2500 finish setup

	P1OUT = 0x02;                         // Setup done => Turn on green LED
}

/*
 * main.c
 */
int main(void) {
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

	/* Wifi set up */
	SetupWireless();

	/** IR Sensor **/
	P2DIR = 0x00;
	P2OUT = 0x00;

	/** Set up the clock for a 1ms interrupt **/
	TACTL   = TASSEL_2 | ID_0 | MC_1 | TAIE;
	TACCR0 = 0xDF3E;

	__bis_SR_register(CPUOFF + GIE);   // sleep until next timer A IRQ

	return 0;
}
