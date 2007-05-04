/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
                     
#include "config.h"
#include "emu.h"
#include "memory.h"
#include "doshelpers.h"

int my_plugin_conf = 0;
int my_plugin_fd = -1;

void demo_plugin_init(void)
{
	fprintf(stderr, "PLUGIN: demo_plugin_init called, conf=%d\n", my_plugin_conf);
	if (!my_plugin_conf) return;
	my_plugin_fd = open("/tmp/plugin_test_pipe", O_RDWR | O_NONBLOCK);
	if (my_plugin_fd != -1) {
		add_to_io_select(my_plugin_fd, 0, my_plugin_ioselect);
		fprintf(stderr, "PLUGIN: test_pipe listening\n");
	}
}


void demo_plugin_close(void)
{
	if (my_plugin_fd != -1) close(my_plugin_fd);
	fprintf(stderr, "PLUGIN: demo_plugin_close called\n");
}


static Bit32u codefarptr = 0;
static char *bufptr = 0;
static int sizebuf = 0;
static Bit16u cback_ds = 0;

void my_plugin_ioselect(void)
{
	Bit16u nbytes;

	if (!my_plugin_conf) return;
	if (my_plugin_fd == -1) return;
	if (!codefarptr) {
		/* discart all we get in */
		char buf[1024];
		nbytes = read(my_plugin_fd, buf, sizeof(buf));
		fprintf(stderr, "PLUGIN: discart %d bytes\n", nbytes);
	}
	else {
		/* NOTE: we may be totally out of sync to the normal
		 *       instruction sequence of the DOS application,
		 *       hence we must treat this as if beeing
		 *       on 'interrupt' level (saving all registers)
		 */
		static struct vm86_regs save_regs;
		unsigned char * ssp;
		unsigned long sp;

		nbytes = read(my_plugin_fd, bufptr, sizebuf);
		fprintf(stderr, "PLUGIN: read %d bytes\n", nbytes);
		if (nbytes <= 0) return;
		save_regs = REGS;	/* save all registers */
		ssp = (unsigned char *)(LWORD(ss)<<4);
		sp = (unsigned long) LWORD(esp);
		pushw(ssp, sp, nbytes);	/* push nbyte as arg on stack */
		LWORD(esp) -= 2;
		LWORD(ds) = cback_ds;	/* set the DOSapp DS value */
		do_call_back(codefarptr);
		REGS = save_regs;	/* restore all registers */
	}
}


int demo_plugin_inte6(void)
{
	if (!my_plugin_conf) return;
	fprintf(stderr, "PLUGIN: demo_plugin_inte6 called AX=%04x\n", LWORD(eax));
	switch (HI(ax)) {

	    case 0x12: {
		/* simple test for call back, we expect a pointer
		   to the far routine in es:di */
		Bit32u codefarptr = ((Bit32u)LWORD(es) << 16) + LWORD(edi);
		do_call_back(codefarptr);
		return 1;	/* we return the AX the callback routine
				   has set */
	    }
	    case 0x23: {
		/* register a callback, we should deliver input
		   from /tmp/my_plugin_fd to. es:di are pointing to
		   'struct call_back_params': */
		struct call_back_params {
			Bit32u codefarptr;
			Bit32u buffarptr;
			Bit16u sizebuf;
			Bit16u cback_ds;
		} __attribute__((packed));
		struct call_back_params *cbpar;
		cbpar = (void *)SEGOFF2LINEAR(LWORD(es), LWORD(edi));
		codefarptr = cbpar->codefarptr;
		cback_ds = cbpar->cback_ds;
		bufptr = (void *)SEGOFF2LINEAR(
				cbpar->buffarptr >> 16,
				cbpar->buffarptr & 0xffff);
		sizebuf = cbpar->sizebuf;
		break;
	    }
	    case 0x34: {
		/* unregister callback, the DOS should do this at exit */
		codefarptr = 0;
		break;
	    }
	}
	LWORD(eax) = 0;
	return 1;
}

int my_plugin_need_poll = 3;

void my_plugin_poll(int vm86ret)
{
	if (!my_plugin_conf) return;
	if (VM86_TYPE(vm86ret) == VM86_INTx) {
		/* for demomstration purpose we just do n times */
		my_plugin_need_poll--;
		fprintf(stderr, "PLUGIN: poll, INT%02x happened\n",
			VM86_ARG(vm86ret));
	}
}
