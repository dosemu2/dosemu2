#ifndef TIMERS_H
#define TIMERS_H

/* Programmable Interval Timer, 8253/8254 */
/* ports 0x40 - 0x43 */
struct pit {
  unsigned short
   CNTR0,			/* 0x40, time of day clock (usu/ mode 3) */
   CNTR1,			/* 0x41, RAM refresh cntr (usu. mode 2) */
   CNTR2,			/* 0x42, cassette/spkr */
   MCR;				/* 0x43, mode control register */
  unsigned char
   s0,				/* states */
   s1, s2, sm;
  unsigned long update, freq;
};

extern struct pit pit;

void timer_tick(void), set_ticks(unsigned long);
extern inline int int28(void);

#define BIOS_TICK_ADDR		(void *)0x46c
#define TICK_OVERFLOW_ADDR	(void *)0x470

/* thes were 330000 and 250000 in dosemu0.4 */
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
#define INT2F_IDLE_USECS	80000
#define INT15_IDLE_USECS	80000
#define INT28_IDLE_USECS	5000

#endif /* TIMERS_H */
