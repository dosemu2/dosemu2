/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>

#include "emu.h"
#include "init.h"
#include "int.h"
#include "utilities.h"
#include "timers.h"
#include "memory.h"
#include "doshelpers.h"
#include "dos2linux.h"
#include "builtins.h"

#include "commands.h"
#include "lredir.h"
#include "xmode.h"
#include "unix.h"
#include "system.h"
#include "dosdbg.h"
#include "blaster.h"

/* ============= old .com ported ================= */

static int do_doshelper(int ax, int bx)
{
    switch (ax & 0xff) {
    case DOS_HELPER_SET_HOGTHRESHOLD:
	g_printf("IDLE: Setting hogthreshold value to %u\n", bx);
	config.hogthreshold = bx;
	break;
    case DOS_HELPER_CDROM_HELPER:
	HI(ax) = bx;
	cdrom_helper(NULL, NULL, 0);
	break;
    case DOS_HELPER_EXIT:
	if (ax == DOS_HELPER_REALLY_EXIT) {
	    /* terminate code is in bx */
	    dbug_printf("DOS termination requested\n");
	    if (!config.dumb_video)
		p_dos_str("\n\rLeaving DOS...\n\r");
	    leavedos(bx);
	}
	break;
    case DOS_HELPER_GARROT_HELPER:	/* Mouse garrot helper */
	if (!bx)	/* Wait sub-function requested */
	    idle(0, 50, 0, "mouse_garrot");
	else {			/* Get Hogthreshold value sub-function */
	    LWORD(ebx) = config.hogthreshold;
	    LWORD(eax) = config.hogthreshold;
	}
	break;
    default:
	error("invalid doshelper %x\n", ax);
	break;
    }
    return -1;
}

int dpmi_main(int argc, char **argv)
{
	if (argc == 1) {
		int len;
		com_printf("dosemu DPMI control program.\n\n");
		com_printf("Usage: dpmi <switch> <value>\n\n");
		com_printf("The following table lists the available parameters, "
			"their current values\nand switches that can be used to "
			"modify the particular parameter.\n\n");
		com_printf("+--------------------------+-----------+----+---------------------------------+\n");
		com_printf("| Parameter                |   Value   | Sw | Description                     |\n");
		com_printf("+--------------------------+-----------+----+---------------------------------+\n");
		com_printf("|$_dpmi                    |");
		if (config.dpmi)
			com_printf("%#x%n", config.dpmi, &len);
		else
			com_printf("%7s%n", "off", &len);
		com_printf("%*s| -m | DPMI memory size in Kbytes      |\n", 11-len, "");
#if 0
		if (config.dpmi_base == -1)
			com_printf("|$_dpmi_base               |    auto   | -b | Address of the DPMI memory pool |\n");
		else
			com_printf("|$_dpmi_base               |0x%08" PRIxPTR " | -b | Address of the DPMI memory pool |\n", config.dpmi_base);
#endif
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
		    c = getopt(argc, argv, "m:p:n:t:");
		    switch (c) {
			case 'm':
			    if (optarg) {
				config.dpmi = strtoll(optarg, NULL, 0);
			    }
			    break;
#if 0
			case 'b':
			    if (optarg) {
				config.dpmi_base = strtoll(optarg, NULL, 0);
			    }
			    break;
#endif
			case 'p':
			    if (optarg) {
				if (strequalDOS(optarg, "ON"))
				    config.pm_dos_api = 1;
				else if (strequalDOS(optarg, "OFF"))
				    config.pm_dos_api = 0;
				else
				    com_printf("invalid value: %s\n", optarg);
			    }
			    break;
			case 'n':
			    if (optarg) {
				if (strequalDOS(optarg, "ON"))
				    config.no_null_checks = 1;
				else if (strequalDOS(optarg, "OFF"))
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

int eject_main(int argc, char **argv)
{
	do_doshelper(DOS_HELPER_CDROM_HELPER, 0xc00);	/* unlock door */
	optind = 0;
	switch (getopt(argc, argv, "t")) {
		case 't':
			do_doshelper(DOS_HELPER_CDROM_HELPER, 0xe00);	/* close tray */
			break;
		default:
			do_doshelper(DOS_HELPER_CDROM_HELPER, 0xd00);	/* eject disk */
	}
	if (LO(ax)) {
		com_printf("cdrom eject failed, error code %i\n", LO(ax));
	}
	return 0;
}

int exitemu_main(int argc, char **argv)
{
	int rc = 0;
	if (argc >= 2)
		rc = atoi(argv[1]);
	do_doshelper(DOS_HELPER_REALLY_EXIT, rc);
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

static int emufs_main(int argc, char **argv)
{
	int ret = EXIT_FAILURE;
	int fd = com_dosopen("EMUFS$", O_RDWR);
	uint16_t *ioc_buf;
	if (fd == -1) {
		com_printf("emufs.sys not loaded in config.\n");
		return EXIT_FAILURE;
	}
	ioc_buf = lowmem_heap_alloc(4);
	pre_msdos();
	LWORD(eax) = 0x440c;
	LWORD(ebx) = fd;
	HI(cx) = 0xff;
	LO(cx) = EMUFS_IOCTL_GET_ENTRY;
	SREG(ds) = DOSEMU_LMHEAP_SEG;
	LWORD(edx) = DOSEMU_LMHEAP_OFFS_OF(ioc_buf);
	call_msdos();
	if (!(REG(eflags) & CF)) {
		HI(ax) = EMUFS_HELPER_REDIRECT;
		do_call_back(ioc_buf[1], ioc_buf[0]);
	}
	if (REG(eflags) & CF) {
		com_printf("emufs: redirector call failed\n");
	} else {
		ret = EXIT_SUCCESS;
		com_printf("emufs: redirector enabled\n");
	}
	post_msdos();
	lowmem_heap_free(ioc_buf);
	com_dosclose(fd);
	return ret;
}

CONSTRUCTOR(static void commands_plugin_init(void))
{
	/* old xxx.S files */
	register_com_program("DPMI", dpmi_main);
	register_com_program("EJECT", eject_main);
	register_com_program("EXITEMU", exitemu_main);
	register_com_program("SPEED", speed_main);

	register_com_program("LREDIR", lredir_main);
	register_com_program("LREDIR2", lredir2_main);
	register_com_program("XMODE", xmode_main);
	register_com_program("EMUMOUSE", emumouse_main);
	register_com_program("DOSDBG", dosdbg_main);
	register_com_program("UNIX", unix_main);
	register_com_program("SYSTEM", system_main);
	register_com_program("EMUFS", emufs_main);

	register_com_program("SOUND", sound_main);
	register_com_program("BLASTER", blaster_main);
}
