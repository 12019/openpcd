#include <errno.h>
#include <string.h>
#include <include/lib_AT91SAM7.h>
#include "dbgu.h"
#include "led.h"
#include "openpcd.h"

int main(void)
{
	/* initialize LED and debug unit */
	led_init();
	AT91F_DBGU_Init();

	/* call application specific init function */
	_init_func();

	// Enable User Reset and set its minimal assertion to 960 us
	AT91C_BASE_RSTC->RSTC_RMR =
	    AT91C_RSTC_URSTEN | (0x4 << 8) | (unsigned int)(0xA5 << 24);

#ifdef DEBUG_CLOCK_PA6
	AT91F_PMC_EnablePCK(AT91C_BASE_PMC, 0, AT91C_PMC_CSS_PLL_CLK);
	AT91F_PIO_CfgPeriph(AT91C_BASE_PIOA, 0, AT91C_PA6_PCK0);
#endif

	/* switch on first led */
	led_switch(1, 1);

	DEBUGPCRF("entering main (idle) loop");
	while (1) {
		/* Call application specific main idle function */
		_main_func();
	}
}
