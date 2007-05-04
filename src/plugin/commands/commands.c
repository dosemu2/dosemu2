/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
#include <unistd.h>
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
#include "blaster.h"

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

int dpmi_main(int argc, char **argv)
{
	if (argc == 1) {
		com_printf("dosemu DPMI control program.\n\n");
		com_printf("Usage: dpmi <switch> <value>\n\n");
		com_printf("The following table lists the available parameters, "
			"their current values\nand switches that can be used to "
			"modify the particular parameter.\n\n");
		com_printf("+--------------------------+-----------+----+---------------------------------+\n");
		com_printf("| Parameter                |   Value   | Sw | Description                     |\n");
		com_printf("+--------------------------+-----------+----+---------------------------------+\n");
		com_printf("|$_dpmi                    |%#6x%s| -m | DPMI memory size in Kbytes      |\n",
			    config.dpmi, config.dpmi ? "     " : "(off)");
		com_printf("|$_dpmi_base               |0x%08lx | -b | Address of the DPMI memory pool |\n", config.dpmi_base);
		com_printf("|$_pm_dos_api              |    %s    | -p | Protected mode DOS API support  |\n",
			    config.pm_dos_api ? "on " : "off");
		com_printf("|$_ignore_djgpp_null_derefs|    %s    | -n | Disable DJGPP NULL-deref protec.|\n",
			    config.no_null_checks ? "on " : "off");
		com_printf("|$_cli_timeout             |%i %s   | -t | See EMUfailure.txt, sect. 1.6.1 |\n",
			    config.cli_timeout, config.cli_timeout ? "     " : "(off)");
		com_printf("+--------------------------+-----------+----+---------------------------------+\n\n");
	} else {
		int c = 0;
		optind = 0;
		while (c != -1) {
		    c = getopt(argc, argv, "m:b:p:n:t:");
		    switch (c) {
			case 'm':
			    if (optarg) {
				config.dpmi = strtoll(optarg, NULL, 0);
			    }
			    break;
			case 'b':
			    if (optarg) {
				config.dpmi_base = strtoll(optarg, NULL, 0);
			    }
			    break;
			case 'p':
			    if (optarg) {
				if (strcasecmp(optarg, "on") == 0)
				    config.pm_dos_api = 1;
				else if (strcasecmp(optarg, "off") == 0)
				    config.pm_dos_api = 0;
				else
				    com_printf("invalid value: %s\n", optarg);
			    }
			    break;
			case 'n':
			    if (optarg) {
				if (strcasecmp(optarg, "on") == 0)
				    config.no_null_checks = 1;
				else if (strcasecmp(optarg, "off") == 0)
				    config.no_null_checks = 0;
				else
				    com_printf("invalid value: %s\n", optarg);
			    }
			    break;
			case 't':
			    if (optarg) {
				config.cli_timeout = strtoll(optarg, NULL, 0);
			    }
			    break;
		    }
		}
	}
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
	optind = 0;
	switch (getopt(argc, argv, "t")) {
		case 't':
			do_doshelper(DOS_HELPER_CDROM_HELPER+0xe00, 0);	/* close tray */
			break;
		default:
			do_doshelper(DOS_HELPER_CDROM_HELPER+0xd00, 0);	/* eject disk */
	}
	if (LO(ax)) {
		com_printf("cdrom eject failed, error code %i\n", LO(ax));
	}
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
	char c[256];
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
	run_system_command(c);
	return 0;
}

int uchdir_main(int argc, char **argv)
{
	struct PSP *psp = COM_PSP_ADDR;
	char c[256];
	if (argc <= 1) {
		com_printf("Run chdir newpath\n");
		return 1;
	}
	strncpy(c, psp->cmdline, psp->cmdline_len);
	c[psp->cmdline_len] = 0;
	chdir(skip_white_and_delim(c, ' '));
	return 0;
}

int ugetcwd_main(int argc, char **argv)
{
	char s[256];
	
	getcwd(s, sizeof(s));
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

static int generic_main(int argc, char **argv)
{
	com_printf("generic.com should not be invoked directly.\n");
	return 0;
}


void commands_plugin_init(void)
{
	static int done = 0;

	if (done) return;
	done = 1;

	register_com_program("generic", generic_main);

	/* old xxx.S files */
	register_com_program("bootoff", bootoff_main);
	register_com_program("booton", booton_main);
	register_com_program("dpmi", dpmi_main);
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

	register_com_program("blaster", blaster_main);

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


