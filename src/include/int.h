#ifndef INT_H
#define INT_H

#include "extern.h"

/*
   Queue to hold all pending hard-interrupts. When an interrupt is
   placed into this queue, it can include a function to be run
   prior to the actuall interrupt being placed onto the DOS stack,
   as well as include a function to be called after the interrupt
   finishes.
*/
EXTERN void *interrupt_function[0x100];

EXTERN int           int_queue_start INIT(0);
EXTERN int           int_queue_end INIT(0);
EXTERN unsigned int  check_date INIT(0);
EXTERN time_t        start_time;
EXTERN unsigned long last_ticks;

#define IQUEUE_LEN 1000
struct int_queue_struct {
  int interrupt;
  int (*callstart) ();
  int (*callend) ();
};
EXTERN struct int_queue_struct int_queue[IQUEUE_LEN];

/*
   This is here to allow multiple hard_int's to be running concurrently.
   Needed for programs that steal INT9 away from DOSEMU.
*/
#define NUM_INT_QUEUE 64
struct int_queue_list_struct {
  struct int_queue_struct int_queue_ptr;
  int int_queue_return_addr;
  u_char in_use;
#ifdef __NetBSD__
  struct sigcontext saved_regs;
#endif
#ifdef __linux__
  struct vm86_regs saved_regs;
#endif
};
EXTERN struct int_queue_list_struct int_queue_head[NUM_INT_QUEUE];

EXTERN int int_queue_running INIT(0);
EXTERN u_char in_sigsegv INIT(0);
EXTERN u_char in_sighandler INIT(0);	/* so I know to not use non-reentrant
					 * syscalls like ioctl() :-( */
EXTERN u_char ignore_segv INIT(0);      /* ignore sigsegv's */

void do_int(int);
void setup_interrupts(void);
void version_init(void);
void int_queue_run(void);
extern void queue_hard_int(int i, void (*callstart), void (*callend));
extern int can_revector(int i);

#endif
