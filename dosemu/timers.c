#define TIMERS_C
/* timers.c
 *     for dosemu 0.48+
 *     Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1995/01/14 15:29:17 $
 * $Source: /home/src/dosemu0.60/dosemu/RCS/timers.c,v $
 * $Revision: 2.3 $
 * $State: Exp $
 *
 * $Log: timers.c,v $
 * Revision 2.3  1995/01/14  15:29:17  root
 * New Year checkin.
 *
 * Revision 2.2  1994/09/22  23:51:57  root
 * Prep for pre53_21.
 *
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.6  1994/03/13  01:07:31  root
 * Poor attempts to optimize.
 *
 * Revision 1.5  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.4  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.3  1993/11/29  00:05:32  root
 * *** empty log message ***
 *
 * Revision 1.2  1993/11/23  22:24:53  root
 * *** empty log message ***
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.7  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.6  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.5  1993/03/02  03:06:42  root
 * somewhere between 0.48pl1 and 0.49 (with IPC).  added virtual IOPL
 * and AC support (for 386/486 tests), -3 and -4 flags for choosing.
 * Split dosemu into 2 processes; the child select()s on the keyboard,
 * and signals the parent when a key is received (also sends it on a
 * UNIX domain socket...this might not work well for non-console keyb).
 *
 * Revision 1.4  1993/02/24  11:33:24  root
 * some general cleanups, fixed the key-repeat bug.
 *
 * Revision 1.3  1993/02/18  19:35:58  root
 * just added newline so diff wouldn't barf
 *
 * Revision 1.2  1993/02/16  00:21:29  root
 * DOSEMU 0.48 DISTRIBUTION
 *
 * Revision 1.1  1993/02/13  23:37:20  root
 * Initial revision
 *
 */

#include <linux/time.h>
#include "emu.h"
#include "timers.h"

extern config_t config;
extern int ignore_segv;

void
timer_tick(void)
{
  unsigned long *ticks = BIOS_TICK_ADDR;
  unsigned char *overflow = TICK_OVERFLOW_ADDR;

  /* excuse the inaccuracies, I'll do this better later...this simulates
   * an (very) roughly 20 Hz timer increment rate...cheesy, but it's okay.
   * I used 22 instead of 18 not only for the roundoff error, but
   * because of the latency problem with 1x10^6/UPDATE < 18
   */
  (*ticks) += (22 / FREQ);
  /*  warn("TIMER: updated count to %d\n", *ticks); */
  if (*ticks == 0) {
    h_printf("TIMER: 24 hours overflow!\n");
    (*overflow)++;
  }
}

void
set_ticks(unsigned long new)
{
  unsigned long *ticks = BIOS_TICK_ADDR;
  unsigned char *overflow = TICK_OVERFLOW_ADDR;

  ignore_segv++;
  *ticks = new;
  *overflow = 0;
  /* warn("TIMER: update value of %d\n", (40 / (1000000 / UPDATE))); */
  ignore_segv--;
}

#undef TIMERS_C
