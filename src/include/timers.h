#ifndef TIMERS_H
#define TIMERS_H

#include "extern.h"
#include "types.h"
#include <sys/time.h>

#define TIMER_DIVISOR   3

extern void timer_tick(void);
extern void set_ticks(unsigned long);
extern Bit8u pit_inp(Bit32u);
extern void pit_outp(Bit32u, Bit8u);
extern int  inport_43();
extern void outport_43(int);

#define BIOS_TICK_ADDR		(void *)0x46c
#define TICK_OVERFLOW_ADDR	(void *)0x470

/* these were 330000 and 250000 in dosemu0.4 */
#define UPDATE  config.update	/* waiting time in usec...this is the main
				 * dosemu "system" clock . 54945*/
#define DELAY	250000		/* sleeping time in usec */

#define FREQ   config.freq	/* rough (low) estimate of how
				      * many times a second sigalrm()
				      * is called */

/* this specifies how many microseconds int 0x2f, ax=0x1680, will usleep().
 * we don't really have a "give up time slice" primitive, but something
 * like this works okay...thanks to Andrew Tridgell.
 * same for int 0x15, ax=0x1000 (TopView/DESQview).
 */
#if 1
#define INT2F_IDLE_USECS	80000
#define INT15_IDLE_USECS	80000
#define INT28_IDLE_USECS	5000
#else
#define INT2F_IDLE_USECS	0
#define INT15_IDLE_USECS	0
#define INT28_IDLE_USECS	0
#endif

extern Bit8u pit_control_inp(Bit32u);
extern void pit_control_outp(Bit32u port, Bit8u val);
extern void initialize_timers(void);
#if 0
typedef struct {
  short          read_state;
  short          write_state;
  int            mode;
  int            read_latch;
  int            write_latch;
  long           cntr;
  struct timeval time;
} pit_latch_struct;
#endif
#endif /* TIMERS_H */
