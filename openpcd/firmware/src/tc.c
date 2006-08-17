/* OpenPC TC (Timer / Clock) support code
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * This idea of this code is to feed the 13.56MHz carrier clock of RC632
 * into TCLK1, which is routed to XC1.  Then configure TC0 to divide this
 * clock by a configurable divider.
 *
 * PICC Simulator Side:
 * In order to support responding to synchronous frames (REQA/WUPA/ANTICOL),
 * we need a second Timer/Counter (TC1).  This unit is reset by an external
 * event (rising edge of modulation pause PCD->PICC) connected to TIOB2, and
 * counts up to a configurable number of carrier clock cycles (RA). Once the
 * RA value is reached, TIOA2 will see a rising edge.  This rising edge will
 * be interconnected to TF (Tx Frame) of the SSC to start transmitting our
 * synchronous response.
 *
 */

#include <lib_AT91SAM7.h>
#include "openpcd.h"
#include "dbgu.h"
#include "tc.h"

static AT91PS_TCB tcb = AT91C_BASE_TCB;

/* set carrier divider to a specific */
void tc_cdiv_set_divider(u_int16_t div)
{
	tcb->TCB_TC0.TC_RC = div;

	/* set to 50% duty cycle */
	tcb->TCB_TC0.TC_RA = 1;
	tcb->TCB_TC0.TC_RB = 1 + (div >> 1);
}

void tc_cdiv_phase_add(int16_t inc)
{
	tcb->TCB_TC0.TC_RA = (tcb->TCB_TC0.TC_RA + inc) % tcb->TCB_TC0.TC_RC;
	tcb->TCB_TC0.TC_RB = (tcb->TCB_TC0.TC_RB + inc) % tcb->TCB_TC0.TC_RC;

	/* FIXME: can this be done more elegantly? */
	if (tcb->TCB_TC0.TC_RA == 0) {
		tcb->TCB_TC0.TC_RA += 1;
		tcb->TCB_TC0.TC_RB += 1;
	}
}

#ifdef CONFIG_PICCSIM
void tc_fdt_set(u_int16_t count)
{
	tcb->TC_TC2.TC_RA = count;
}
#endif

void tc_cdiv_init(void)
{
	/* Cfg PA28(TCLK1), PA0(TIOA0), PA1(TIOB0), PA20(TCLK2) as Periph B */
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, 
			    OPENPCD_PIO_CARRIER_IN |
			    OPENPCD_PIO_CARRIER_DIV_OUT |
			    OPENPCD_PIO_CDIV_HELP_OUT |
			    OPENPCD_PIO_CDIV_HELP_IN);

	AT91F_PMC_EnablePeriphClock(AT91C_BASE_PMC, 
				    ((unsigned int) 1 << AT91C_ID_TC0));

	/* Enable Clock for TC0 */
	tcb->TCB_TC0.TC_CCR = AT91C_TC_CLKEN;

	/* Connect TCLK1 to XC1, TCLK2 to XC2 */
	tcb->TCB_BMR &= ~(AT91C_TCB_TC1XC1S | AT91C_TCB_TC2XC2S);
	tcb->TCB_BMR |=  (AT91C_TCB_TC1XC1S_TCLK1 | AT91C_TCB_TC2XC2S_TCLK2);

	/* Clock XC1, Wave mode, Reset on RC comp
	 * TIOA0 on RA comp = set, * TIOA0 on RC comp = clear,
	 * TIOB0 on EEVT = set, TIOB0 on RB comp = clear,
	 * EEVT = XC2 (TIOA0) */
	tcb->TCB_TC0.TC_CMR = AT91C_TC_CLKS_XC1 | AT91C_TC_WAVE |
			      AT91C_TC_WAVESEL_UP_AUTO | 
			      AT91C_TC_ACPA_SET | AT91C_TC_ACPC_CLEAR |
			      AT91C_TC_BEEVT_SET | AT91C_TC_BCPB_CLEAR |
			      AT91C_TC_EEVT_XC2 | AT91C_TC_ETRGEDG_RISING;

	tc_cdiv_set_divider(128);

#ifdef CONFIG_PICCSIM
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, AT91C_PA15_TF,
			    AT91C_PA26_TIOA2, AT91C_PA17_TIOB2);
	AT91F_PMC_EnablePeriphClock(AT91C_BASE_PMC,
				    ((unsigned int) 1 << AT91C_ID_TC2));
	/* Clock XC1, Wave Mode, No automatic reset on RC comp
	 * TIOA2 in RA comp = set, TIOA2 on RC comp = clear,
	 * TIOB2 as input, EEVT = TIOB2, Reset/Trigger on EEVT */
	tcb->TCB_TC2.TC_CMR = AT91C_TC_CLKS_XC1 | AT91C_TC_WAVE |
			      AT91C_TC_WAVESEL_UP |
			      AT91C_TC_ACPA_SET | AT91C_ACPC_CLEAR |
			      AT91C_TC_BEEVT_NONE | AT91C_TC_BCPB_NONE |
			      AT91C_TC_EEVT_TIOB | AT91C_TC_ETRGEDG_RISING |
			      AT91C_TC_ENETRG ;
#endif

	/* Reset to start timers */
	tcb->TCB_BCR = 1;
}

void tc_cdiv_print(void)
{
	DEBUGP("TCB_BMR=0x%08x ", tcb->TCB_BMR);
	DEBUGP("TC0_CV=0x%08x ", tcb->TCB_TC0.TC_CV);
	DEBUGP("TC0_CMR=0x%08x ", tcb->TCB_TC0.TC_CMR);
	DEBUGPCR("TC0_SR=0x%08x", tcb->TCB_TC0.TC_SR);

	DEBUGPCR("TC0_RA=0x%04x, TC0_RB=0x%04x, TC0_RC=0x%04x",
		 tcb->TCB_TC0.TC_RA, tcb->TCB_TC0.TC_RB, tcb->TCB_TC0.TC_RC);
}

void tc_cdiv_fini(void)
{
	tcb->TCB_TC0.TC_CCR = AT91C_TC_CLKDIS;
	AT91F_PMC_DisablePeriphClock(AT91C_BASE_PMC,
				     ((unsigned int) 1 << AT91C_ID_TC0));
}
