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
#include "int.h"
#include "utilities.h"
#include "memory.h"
#include "doshelpers.h"
#include "dos2linux.h"
#include "builtins.h"

#include "commands.h"
#include "lredir.h"
#include "xmode.h"
#include "unix.h"
#include "dosdbg.h"
#include "cmdline.h"
#include "comcom.h"

/* ============= old .com ported ================= */

static int do_doshelper(int ax, int bx)
{
	LWORD(eax) = ax;
	LWORD(ebx) = bx;
	dos_helper();
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
	REG(es) = FP_SEG32(c);
	LWORD(edx) = FP_OFF32(c);	/* DS should already be right */
	do_doshelper(DOS_HELPER_0x53, 0);
	return 0;
}

int uchdir_main(int argc, char **argv)
{
	struct PSP *psp = COM_PSP_ADDR;
	char *c = lowmem_alloc(256);
	if (argc <= 1) {
		com_printf("Run chdir newpath\n");
		return 1;
	}
	strncpy(c, psp->cmdline, psp->cmdline_len);
	c[psp->cmdline_len] = 0;
	REG(es) = FP_SEG32(c);
	LWORD(edx) = FP_OFF32(skip_white_and_delim(c, ' '));
	do_doshelper(DOS_HELPER_CHDIR, 0);
	return 0;
}

int ugetcwd_main(int argc, char **argv)
{
	char *s = lowmem_alloc(256);
	
	LWORD(ecx) = 256;
	REG(es) = FP_SEG32(s);
	LWORD(edx) = FP_OFF32(s);
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


void commands_plugin_init(void)
{
	static int done = 0;

	if (done) return;
	done = 1;

	/* old xxx.S files */
	register_com_program("bootoff", bootoff_main);
	register_com_program("booton", booton_main);
	register_com_program("ecpuon", ecpuon_main);
	register_com_program("ecpuoff", ecpuoff_main);
	register_com_program("eject", eject_main);
	register_com_program("exitemu", exitemu_main);
	register_com_program("speed", speed_main);
	register_com_program("system", system_main);
	register_com_program("uchdir", uchdir_main);
	register_com_program("ugetcwd", ugetcwd_main);
	register_com_program("vgaoff", vgaoff_main);
	register_com_program("vgaon", vgaon_main);


	/* old xxx.c files */
	register_com_program("lredir", lredir_main);
	register_com_program("xmode", xmode_main);
	register_com_program("emumouse", emumouse_main);
	register_com_program("dosdbg", dosdbg_main);
	register_com_program("unix", unix_main);
	register_com_program("cmdline", cmdline_main);

#if 0
	fprintf(stderr, "PLUGIN: commands_plugin_init called\n");
#endif
}

void commands_plugin_close(void)
{
#if 0
	fprintf(stderr, "PLUGIN: commands_plugin_close called\n");
#endif
}


