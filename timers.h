#ifndef TIMERS_H
#define TIMERS_H

/* Programmable Interval Timer, 8253/8254 */
/* ports 0x40 - 0x43 */
struct pit
{   
  unsigned int
    CNTR0,	/* 0x40, time of day clock (usu/ mode 3) */
    CNTR1,	/* 0x41, RAM refresh cntr (usu. mode 2) */
    CNTR2,	/* 0x42, cassette/spkr */
    MCR;	/* 0x43, mode control register */
  unsigned char
    s0,         /* states */
    s1,
    s2,
    sm;
} pit;

unsigned long timer_tick(void),
              set_ticks(unsigned long);

#define BIOS_TICK_ADDR		(void *)0x46c
#define TICK_OVERFLOW_ADDR	(void *)0x470

/* these were 330000 and 250000 in dosemu0.4 */
#define UPDATE	200000		/* waiting time in usec...this is the main
				 * dosemu "system" closk */
#define DELAY	250000		/* sleeping time in usec */

#define FREQ   (1000000 / UPDATE)    /* rough (low) estimate of how
				      * many times a second sigalrm()
				      * is called */

#endif /* TIMERS_H */
