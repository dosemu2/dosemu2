#define TIMERS_C
/* timers.c
 *     for dosemu 0.48+
 *     Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1993/02/16 00:21:29 $
 * $Source: /usr/src/dos/RCS/timers.c,v $
 * $Revision: 1.2 $
 * $State: Exp $
 *
 * $Log: timers.c,v $
 * Revision 1.2  1993/02/16  00:21:29  root
 * DOSEMU 0.48 DISTRIBUTION
 *
 * Revision 1.1  1993/02/13  23:37:20  root
 * Initial revision
 *
 */

#include "emu.h"
#include "timers.h"

unsigned long timer_tick(void)
{
  unsigned long *ticks=BIOS_TICK_ADDR;
  unsigned char *overflow=TICK_OVERFLOW_ADDR;

  /* excuse the inaccuracies, I'll do this better later...this simulates
   * an (very) roughly 20 Hz timer increment rate...cheesy, but it's okay.
   * I used 22 instead of 18 not only for the roundoff error, but
   * because of the latency problem with 1x10^6/UPDATE < 18
   */
   
  *ticks += (22 / (1000000 / UPDATE));
  if (*ticks == 0)
    {
      h_printf("TIMER: 24 hours overflow!\n");
      *overflow=1;
    }
}

unsigned long set_ticks(unsigned long new)
{
  unsigned long *ticks=BIOS_TICK_ADDR;
  unsigned char *overflow=TICK_OVERFLOW_ADDR;

  warn("TIMER: setting ticks to: 0x%08x  from: 0x%08x\n", new, *ticks);
  *ticks=new;
  *overflow=0;
  warn("TIMER: update value of %d\n", (40 / (1000000 / UPDATE)));
}
#undef TIMERS_C
