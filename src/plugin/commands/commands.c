/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * This implements the usual DOS tools, which come with
 * DOSEMU, formerly being pure .com or .exe files,
 * now executed within 32bit DOSEMU code.
 *
 * Copyright (c) 2001 Hans Lermen <lermen@fgan.de>
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "emu.h"
#include "utilities.h"
#include "memory.h"
#include "doshelpers.h"
#include "../coopthreads/coopthreads.h"

#include "commands.h"
#include "lredir.h"
#include "xmode.h"
#include "unix.h"
#include "dosdbg.h"
#include "cmdline.h"
#include "comcom.h"

/* ============= old .com ported ================= */

/*
 * Remark: I know, the way I ported the xxx.S files is a bit an overkill,
 * we could do it directly. However, for backward compitiblity reasons
 * we have to keep the old interface for a while.
 * ... and maintaining two different interfaces is a pain, hence we just
 * use the DOSHELPERs here too and get rid of those tons of xxx.S files.
 *
 *                                 -- Hans, 2001/04/07
 */

static int do_doshelper(int ax, int bx)
{
	LWORD(eax) = ax;
	LWORD(ebx) = bx;
	call_vm86_from_com_thread(-1, DOS_HELPER_INT);
	return LWORD(ebx);
}

int bootoff_main(int argc, char **argv)
{
	do_doshelper(DOS_HELPER_BOOTDISK, 0);
	return 0;
}


int booton_main(int argc, char **argv)
{
	do_doshelper(DOS_HELPER_BOOTDISK, 1);
	return 0;
}


int ecpuon_main(int argc, char **argv)
{
	do_doshelper(DOS_HELPER_CPUEMUON, 0);
	return 0;
}

int ecpuoff_main(int argc, char **argv)
{
	do_doshelper(DOS_HELPER_CPUEMUOFF, 1);
	return 0;
}

int eject_main(int argc, char **argv)
{
	do_doshelper(DOS_HELPER_CDROM_HELPER+0xc00, 0);	/* unlock door */
	do_doshelper(DOS_HELPER_CDROM_HELPER+0xd00, 0);	/* eject disk */
	return 0;
}

int exitemu_main(int argc, char **argv)
{
	do_doshelper(DOS_HELPER_REALLY_EXIT, 0);
	return 0;
}


int speed_main(int argc, char **argv)
{
	if (argc > 2) {
		com_printf("USAGE: speed [hogthreshold]\n");
		return 1;
	}
	if (argc == 2) {
		int value = strtoul(argv[1], 0, 0);
		do_doshelper(DOS_HELPER_SET_HOGTHRESHOLD, value);
	}
	com_printf("Current hogthreshold value = %d\n",
		do_doshelper(DOS_HELPER_GARROT_HELPER, 1));
	return 0;
}

int system_main(int argc, char **argv)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *c = lowmem_alloc(256);
	int i;

	if (argc <= 1) {
		com_printf("Run system command_to_execute\n");
		return 1;
	}

	c[0] = 0;
	strcat(c, argv[1]);
	for (i=2; i < argc; i++) {
		strcat(c, " ");
		strcat(c, argv[i]);
	}
	LWORD(edx) = COM_OFFS_OF(c);	/* DS should already be right */
	do_doshelper(DOS_HELPER_0x53, 0);
	return 0;
}

int uchdir_main(int argc, char **argv)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	if (argc <= 1) {
		com_printf("Run chdir newpath\n");
		return 1;
	}
	LWORD(edx) = COM_OFFS_OF(ctcb->DTA+1);
	do_doshelper(DOS_HELPER_CHDIR, 0);
	return 0;
}

int ugetcwd_main(int argc, char **argv)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *s = lowmem_alloc(256);
	
	LWORD(ecx) = 1024;
	LWORD(edx) = COM_OFFS_OF(s);
	do_doshelper(DOS_HELPER_GETCWD, 0);
	com_printf("%s\n", s);
	return 0;
}


int vgaoff_main(int argc, char **argv)
{
	do_doshelper(DOS_HELPER_CONTROL_VIDEO, 0);
	return 0;
}

int vgaon_main(int argc, char **argv)
{
	/* this is dangerous! */

	LWORD(ecx) = 2;
	set_CF();
	do_doshelper(DOS_HELPER_ADJUST_IOPERMS, 0x70);
	do_doshelper(DOS_HELPER_CONTROL_VIDEO, 1);
	LWORD(ecx) = 2;
	clear_CF();
	do_doshelper(DOS_HELPER_ADJUST_IOPERMS, 0x70);
	return 0;
}


static int test1ip = 0x1d0;
static int test2ip = 0x1e0;

static int test_program(int argc, char **argv)
{
	struct com_starter_seg  *ctcb = owntcb->params;
	char *p, *p1, *p2, *p3;
	int size, i;


	com_printf("builtin test: hello world\n");
	com_printf("thread=%p,\nstackptr=0x%lx, stackbottom=%p, stacksize=0x%lx\n",
		owntcb, STACKPTR, owntcb->stack, owntcb->stack_size);

	for (i=0; i < argc; i++) {
		com_printf(">%s<\n", argv[i]);
	}

	if (!strcmp(argv[1], "exec")) {
		char c[256] = "";
		strcat(c, argv[2]);
		for (i=3; i < argc; i++) {
			strcat(c, " ");
			strcat(c, argv[i]);
		}
		com_system(c);
		return(0);
	}
	if (!strcmp(argv[1], "makefault")) {
		/* testing hook24, assuming there is no floppy in A: */
		com_dosopen("a:\\x", DOS_ACCESS_CREATE);
		com_printf("we came back\n");
		return(0);
	}
	if (!strcmp(argv[1], "canonic")) {
		char buf[256];
		com_doscanonicalize(buf, argv[2]);
		com_printf(">%s<\n", buf);
		return(0);
	}
	if (!strcmp(argv[1], "ask")) {
		int c = com_bioskbd(0);
		com_printf("got 0x%x\n", c);
		return(0);
	}
#if 0
	if (!strcmp(argv[1], "dumpenv")) {
		struct com_MCB *mcb = (void *)((ctcb->envir_frame-1) << 4);
		com_hexdump((char *)mcb, 0, (mcb->size+1) << 4);
		return(0);
	}
#endif
	if (!strcmp(argv[1], "dir")) {
		if (!com_dosfindfirst(argv[2], 0x37)) {
			com_printf("%s\n", ctcb->DTA+0x1e);
			while (!com_dosfindnext()) {
				com_printf(">%s<\n", ctcb->DTA+0x1e);
			}
		}
		return(0);
	}
	if (!strcmp(argv[1], "txtest")) {
		/* testing text readline */
		#define BSIZE 256
		char s[BSIZE];
		long pos = 0;
		int fd = com_dosopen(argv[2] ? argv[2] : "txtest", 0);
		int count = 100;
		if (fd < 0) {
			com_printf("cannot open input file\n");
			return(1);
		}
		do {
			size = com_dosreadtextline(fd, s, BSIZE, &pos);
			if (size >= 0) {
				s[BSIZE-1] = 0;
				com_printf(">%s<\n", s);
			}
		} while (count-- && size >= 0);
		return(0);
	}
	if (!strcmp(argv[1], "getinput")) {
		char buf[256];
		int size;
		size = com_dosread(0, buf, 256);
		if (size >0) buf[size] = 0;
		com_printf(">%s<\n", buf);
		return(0);
	}
	size = 256; p1 = p = lowmem_alloc(size);
	com_printf("1 lowmem_alloc(%d): %p, seg:off = %04x:%04x\n",
		size, p, COM_SEG, COM_OFFS_OF(p));
	size = 999; p2 = p = lowmem_alloc(size);
	com_printf("2 lowmem_alloc(%d): %p, seg:off = %04x:%04x\n",
		size, p, COM_SEG, COM_OFFS_OF(p));
	
	lowmem_free(p1, 256);
	size = 128; p3 = p = lowmem_alloc(size);
	com_printf("2 lowmem_alloc(%d): %p, seg:off = %04x:%04x\n",
		size, p, COM_SEG, COM_OFFS_OF(p));


	com_printf("stack before calling 0x%x: 0x%x\n",
		test1ip, LWORD(esp));
	call_vm86_from_com_thread(0, test1ip);	/* test, we then get called backed */
	com_printf("back from call_vm86_from_com_thread(0, 0x%x),\n"
		"cs:ip = %x:%x, sp = %x\n",
		test1ip, LWORD(cs), LWORD(eip), LWORD(esp));
	return 0;
}

static void functions(void)
{
	com_printf("default_function, cs:ip = %x:%x, sp = %x\n",
		LWORD(cs), LWORD(eip), LWORD(esp));
	call_vm86_from_com_thread(LWORD(cs), test2ip);
	com_printf("back from call_vm86_from_com_thread(CS, 0x%x),\n"
		"cs:ip = %x:%x, sp = %x\n",
		test2ip,LWORD(cs), LWORD(eip), LWORD(esp));
}

static function_call_type *program_functions[] = {
	functions,
	0
};


static char *chistname = 0;

void commands_plugin_init(void)
{
	static int done = 0;

	if (done) return;
	done = 1;

		/* in case we get initialized too early */
	coopthreads_plugin_init();

	register_com_program("test",
		test_program, "f", program_functions);

	/* old xxx.S files */
	register_com_program("bootoff", bootoff_main, 0);
	register_com_program("booton", booton_main, 0);
	register_com_program("ecpuon", ecpuon_main, 0);
	register_com_program("ecpuoff", ecpuoff_main, 0);
	register_com_program("eject", eject_main, 0);
	register_com_program("exitemu", exitemu_main, 0);
	register_com_program("speed", speed_main, 0);
	register_com_program("system", system_main, 0);
	register_com_program("uchdir", uchdir_main, 0);
	register_com_program("ugetcwd", ugetcwd_main, 0);
	register_com_program("vgaoff", vgaoff_main, 0);
	register_com_program("vgaon", vgaon_main, 0);


	/* old xxx.c files */
	register_com_program("lredir", lredir_main, "h", 0x500);
	register_com_program("xmode", xmode_main, 0);
	register_com_program("emumouse", emumouse_main, 0);
	register_com_program("dosdbg", dosdbg_main, 0);
	register_com_program("unix", unix_main, "h", 0x500);
	register_com_program("cmdline", cmdline_main, 0);

	/* command.com replacement */
	 register_com_program("comcom", comcom_main, 0);


	/* try to load old history file for comcom */
	chistname = get_path_in_HOME(".dosemu/comcom.history");
	if (exists_file(chistname)) {
		load_comcom_history(chistname);
	}
#if 0
	fprintf(stderr, "PLUGIN: commands_plugin_init called\n");
#endif
}

void commands_plugin_close(void)
{
	if (!tcb0) return;

	save_comcom_history(chistname);

#if 0
	fprintf(stderr, "PLUGIN: commands_plugin_close called\n");
#endif
}


