#include "config.h"

#ifdef USE_THREADS

#include <stdio.h>
#include "emu.h"

#include "lt-threads.h"

#define THREAD_STACK_SIZE	0x10000	/* 64 K granulation */
extern void background_ioctl_thread(void *);

static thread_function_type *list_of_threads[] = {
	background_ioctl_thread,
	0
};

void Exit(int status)
{
	exit_all(status);
}

static struct tcb *thread0 = 0;

void treads_init(void)
{
	thread_function_type **tf = list_of_threads;

#if 1
	if (!*tf) return;
#endif
#if 0   /* NOTE: we realy don't need the below, 8Meg of stack are enough */
	make_stack_unlimited(0); /* 0 = do not drop privs totally */
#endif
	thread0 = init_zero_thread(THREAD_STACK_SIZE);
#if 0
	fprintf(stdout, "THREADS: thread0 = %p\n", thread0); fflush(stdout);
#endif

	while (*tf) {
		/* call the function to init stuff that must be set up
		 * _before_ the thread gets started.
		 * Note: this runs under scope of thread0.
		 */
		(*tf)((void *)0);
		tf++;
	}
	tf = list_of_threads;
	while (*tf) {
		/* now create the thread, the function will be
		 * started by the scheduler, and that may not
		 * imediatly.
		 */
		create_thread(*tf, (void *)1);
		tf++;
	}
}

#endif /* USE_THREADS */
