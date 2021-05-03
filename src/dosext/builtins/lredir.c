/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/***********************************************
 * File: LREDIR.C
 *  Program for Linux DOSEMU disk redirector functions
 * Written: 10/29/93 by Tim Bird
 * Fixes: 1/7/95 by Tim Josling TEJ: check correct # args
 *      :                            display read/write status correctly
 * Changes: 3/19/95 by Kang-Jin Lee
 *  Removed unused variables
 *  Changes to eliminate warnings
 *  Display read/write status when redirecting drive
 *  Usage information is more verbose
 *
 * Changes: 11/27/97 Hans Lermen
 *  new detection stuff,
 *  rewrote to use plain C and intr(), instead of asm and fiddling with
 *  (maybe used) machine registers.
 *
 * Changes: 990703 Hans Lermen
 *  Checking for DosC and exiting with error, if not redirector capable.
 *  Printing DOS errors in clear text.
 *
 * Changes: 010211 Bart Oldeman
 *   Make it possible to "reredirect": LREDIR drive filepath
 *   can overwrite the direction for the drive.
 *
 * Changes: 20010402 Hans Lermen
 *   Ported to buildin_apps, compiled directly into dosemu
 *
 * emudrv is written by Stas Sergeev
 *
 ***********************************************/


#include <stdio.h>    /* printf  */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "emu.h"
#include "memory.h"
#include "msetenv.h"
#include "doshelpers.h"
#include "utilities.h"
#include "builtins.h"
#include "lpt.h"
#include "int.h"
#include "disks.h"
#include "redirect.h"
#include "lredir.h"

#define	intr	com_intr
#define	strncmpi strncasecmp
#define FP_OFF(x) DOSEMU_LMHEAP_OFFS_OF(x)
#define FP_SEG(x) DOSEMU_LMHEAP_SEG

#define CARRY_FLAG    1 /* carry bit in flags register */

#include "doserror.h"


static int isInitialisedMFS(void)
{
    struct vm86_regs preg;

    preg.ebx = DOS_SUBHELPER_MFS_REDIR_STATE;
    if (mfs_helper(&preg) == TRUE) {
       return ((preg.eax & 0xffff) == 1);
    }
    return 0;
}

static int get_unix_cwd(char *buf)
{
    char dcwd[MAX_PATH_LENGTH];
    int err;

    err = getCWD_cur(dcwd, sizeof dcwd);
    if (err)
        return -1;

    err = build_posix_path(buf, dcwd, 0);
    if (err < 0)
        return -1;

    return 0;
}

/*************************************
 * ShowMyRedirections
 *  show my current drive redirections
 * NOTES:
 *  I show the read-only attribute, cdrom unit number
 *  and disabled state for each drive
 *************************************/
static void
ShowMyRedirections(void)
{
    int driveCount;
    uint16_t redirIndex, deviceOptions;
    uint8_t deviceStatus;
    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char resourceStr[MAX_RESOURCE_LENGTH_EXT];

    redirIndex = 0;
    driveCount = 0;

    while (get_redirection_ux(redirIndex, deviceStr, sizeof deviceStr,
                              resourceStr, sizeof resourceStr,
                              NULL, &deviceOptions,
                              &deviceStatus) == CC_SUCCESS) {
      if (driveCount == 0) {
        com_printf("Current Drive Redirections:\n");
      }
      driveCount++;
      com_printf("%-2s = %-20s ", deviceStr, resourceStr);

      /* read attribute is returned in the device options */
      com_printf("attrib = ");
      if (deviceOptions & REDIR_DEVICE_CDROM_MASK)
        com_printf("CDROM:%i, ", (deviceOptions & REDIR_DEVICE_CDROM_MASK) >> 1);
      if (deviceOptions & REDIR_DEVICE_READ_ONLY)
        com_printf("READ ONLY");
      else
        com_printf("READ/WRITE");

      if (deviceStatus & REDIR_STATUS_DISABLED)
        com_printf(", DISABLED");

      com_printf("\n");

      redirIndex++;
    }

    if (driveCount == 0) {
      com_printf("No drives are currently redirected to Linux.\n");
    }
}

static int DeleteDriveRedirection(char *deviceStr)
{
    uint16_t ccode;

    /* convert device string to upper case */
    strupperDOS(deviceStr);

    ccode = com_CancelRedirection(deviceStr);
    if (ccode) {
      if (ccode == DOS_EATT_REM_CUR_DIR)
        com_printf("Error %c: is the default drive, aborting\n", deviceStr[0]);
      else
        com_printf("Error %x (%s) canceling redirection on drive %s\n",
               ccode, decode_DOS_error(ccode), deviceStr);
      return -1;
    }

    com_printf("Redirection for drive %s was deleted.\n", deviceStr);
    return 0;
}

static int FindRedirectionByDevice(const char *deviceStr, char *presourceStr,
        int resourceLength, int *r_idx, int *r_enab)
{
    uint16_t redirIndex = 0, ccode, opts;
    uint8_t stat;
    char dStr[MAX_DEVICE_STRING_LENGTH];
    char dStrSrc[MAX_DEVICE_STRING_LENGTH];
    int ret = -1;

    snprintf(dStrSrc, MAX_DEVICE_STRING_LENGTH, "%s", deviceStr);
    strupperDOS(dStrSrc);
    while ((ccode = get_redirection(redirIndex, dStr, sizeof dStr,
                                       presourceStr, resourceLength,
                                       NULL, &opts, &stat)) ==
                                       CC_SUCCESS) {
      if (strcmp(dStrSrc, dStr) == 0) {
        *r_idx = (opts >> REDIR_DEVICE_IDX_SHIFT) & 0x1f;
        if (r_enab)
          *r_enab = !(stat & REDIR_STATUS_DISABLED);
        ret = 0;
        break;
      }
      redirIndex++;
    }

    return ret;
}

static int FindFATRedirectionByDevice(const char *deviceStr,
        char *presourceStr, int *r_idx, int *r_ro)
{
    struct DINFO *di;
    const char *dir;
    fatfs_t *f;
    int ret;

    if (!(di = (struct DINFO *)lowmem_heap_alloc(sizeof(struct DINFO))))
	return -1;
    pre_msdos();
    LWORD(eax) = 0x6900;
    LWORD(ebx) = toupperDOS(deviceStr[0]) - 'A' + 1;
    SREG(ds) = FP_SEG(di);
    LWORD(edx) = FP_OFF(di);
    call_msdos();
    if (REG(eflags) & CF) {
	_post_msdos();
	lowmem_heap_free((void *)di);
	com_printf("error retrieving serial, %#x\n", LWORD(eax));
	return -1;
    }
    post_msdos();
    f = get_fat_fs_by_serial(READ_DWORDP((unsigned char *)&di->serial), r_idx,
	    r_ro);
    lowmem_heap_free((void *)di);
    if (!f) {
	com_printf("error identifying FAT volume\n");
	return -1;
    }
    dir = fatfs_get_host_dir(f);
    ret = sprintf(presourceStr, LINUX_RESOURCE "%s", dir);
    assert(ret != -1);

    return 0;
}

static int do_repl(const char *argv, char *resourceStr, int resourceLength,
        int *r_idx, int *r_ro)
{
    int is_cwd, is_drv, ret;
    char *argv2;
    char deviceStr2[MAX_DEVICE_STRING_LENGTH];
    uint16_t ccode;

    is_cwd = (strncmp(argv, ".\\", 2) == 0 || strcmp(argv, ".") == 0);
    is_drv = argv[1] == ':';
    /* lredir c: d: */
    if (is_cwd) {
        char tmp[MAX_PATH_LENGTH];
        int err = getCWD_cur(tmp, sizeof tmp);
        if (err) {
          com_printf("Error: unable to get CWD\n");
          return -1;
        }
        ret = asprintf(&argv2, "%s\\%s", tmp, argv + 2);
        assert(ret != -1);
    } else if (is_drv) {
        argv2 = strdup(argv);
    } else {
        com_printf("Error: \"%s\" is not a DOS path\n", argv);
        return -1;
    }

    strncpy(deviceStr2, argv2, 2);
    deviceStr2[2] = 0;
    ccode = FindRedirectionByDevice(deviceStr2, resourceStr,
            resourceLength, r_idx, NULL);
    if (ccode != CC_SUCCESS)
        ccode = FindFATRedirectionByDevice(deviceStr2, resourceStr, r_idx,
                r_ro);
    if (ccode != CC_SUCCESS) {
        com_printf("Error: unable to find redirection for drive %s\n", deviceStr2);
        free(argv2);
        return -1;
    }
    if (strlen(argv2) > 3)
        strcat(resourceStr, argv2 + 3);
    free(argv2);
    return 0;
}

static int do_restore(const char *argv, char *resourceStr, int resourceLength,
        int *r_idx, int *r_ro)
{
    int enab;
    uint16_t ccode;

    ccode = FindRedirectionByDevice(argv, resourceStr, resourceLength,
            r_idx, &enab);
    if (ccode == CC_SUCCESS)
        return (enab ? -1 : 0);
    ccode = FindFATRedirectionByDevice(argv, resourceStr, r_idx, r_ro);
    if (ccode != CC_SUCCESS) {
        com_printf("Error: unable to find redirection for drive %s\n", argv);
        return -1;
    }
    return 0;
}

struct lredir_opts {
    int help;
    int cdrom;
    int ro;
    int nd;
    int force;
    int restore;
    int pwd;
    int show;
    char *del;
    int optind;
};

static int lredir_parse_opts(int argc, char *argv[],
	const char *getopt_string, struct lredir_opts *opts)
{
    char c;

    memset(opts, 0, sizeof(*opts));

    /* its too common for DOS progs to reply on /? */
    if (argc == 2 && !strcmp (argv[1], "/?")) {
	opts->help = 1;
	return 0;
    }

    optind = 0;		// glibc wants this to reser parser state
    while ((c = getopt(argc, argv, getopt_string)) != EOF) {
	switch (c) {
	case 'h':
	    opts->help = 1;
	    break;

	case 'd':
	    opts->del = optarg;
	    break;

	case 'f':
	    opts->force = 1;
	    break;

	case 'C':
	    opts->cdrom = (optarg ? atoi(optarg) : 1);
	    if (opts->cdrom < 1 || opts->cdrom > 3) {
		com_printf("Invalid CDROM unit (%d)\n", opts->cdrom);
		return -1;
	    }
	    break;

	case 'r':
	    opts->restore = 1;
	    break;

	case 'n':
	    opts->nd = 1;
	    break;

	case 'R':
	    opts->ro = 1;
	    break;

	case 's':
	    opts->show = 1;
	    break;

	case 'w':
	    opts->pwd = 1;
	    break;

	default:
	    com_printf("Unknown option %c\n", optopt);
	    return -1;
	}
    }

    if (!opts->help && !opts->pwd && !opts->del && !opts->restore &&
	    !opts->show && argc < optind + 2 - opts->nd) {
	com_printf("missing arguments\n");
	return -1;
    }
    opts->optind = optind;
    return 0;
}

static int fill_dev_str(char *deviceStr, char *argv,
	const struct lredir_opts *opts)
{
    if (!opts->nd) {
	if (argv[1] != ':' || argv[2]) {
	    com_printf("invalid argument %s\n", argv);
	    return -1;
	}
	strcpy(deviceStr, argv);
    } else {
	int nextDrive;
	nextDrive = find_free_drive();
	if (nextDrive < 0) {
	    if (config.boot_dos == FATFS_FD_D)
		com_printf("lredir is not supported with this freedos version\n");
	    else
		com_printf("Cannot redirect (maybe no drives available).");
	    return -1;
	}
	deviceStr[0] = nextDrive + 'A';
	deviceStr[1] = ':';
	deviceStr[2] = '\0';
    }
    return 0;
}

static int do_redirect(char *deviceStr, char *resourceStr,
	const struct lredir_opts *opts, int idx)
{
    uint16_t ccode, deviceOptions = idx << REDIR_DEVICE_IDX_SHIFT;

    if (opts->ro)
      deviceOptions += REDIR_DEVICE_READ_ONLY;
    if (opts->cdrom)
      deviceOptions += opts->cdrom << 1;

    /* upper-case both strings */
    strupperDOS(deviceStr);

    /* now actually redirect the drive */
    ccode = com_RedirectDevice(deviceStr, resourceStr, REDIR_DISK_TYPE, deviceOptions);
    if (ccode == 0x55) {     /* duplicate redirection: try to reredirect */
      if (!opts->force) {
        com_printf("Error: drive %s already redirected.\n"
               "       Use -d to delete the redirection or -f to force.\n",
               deviceStr);
        return -1;
      } else {
        DeleteDriveRedirection(deviceStr);
        ccode = com_RedirectDevice(deviceStr, resourceStr, REDIR_DISK_TYPE, deviceOptions);
      }
    }

    if (ccode) {
      com_printf("Error %x (%s) while redirecting drive %s to %s\n",
             ccode, decode_DOS_error(ccode), deviceStr, resourceStr);
      if (ccode == 5 /*ACCESS_DENIED*/)
        com_printf("Add the needed path to $_lredir_paths list to allow\n");
      if (config.boot_dos == FATFS_FD_D && ccode == 0xf /* invalid drive */)
        com_printf("lredir is not supported with this freedos version\n");
      return -1;
    }

    com_printf("%s = %s", deviceStr, resourceStr);
    if (deviceOptions & REDIR_DEVICE_CDROM_MASK)
      com_printf(" CDROM:%d", (deviceOptions & REDIR_DEVICE_CDROM_MASK) >> 1);
    com_printf(" attrib = ");
    if (deviceOptions & REDIR_DEVICE_READ_ONLY)
      com_printf("READ ONLY\n");
    else
      com_printf("READ/WRITE\n");

    return 0;
}

static char *get_arg2(int argc, char **argv, const struct lredir_opts *opts)
{
    int idx = opts->optind + 1 - opts->nd;
    if (idx >= argc)
        return NULL;
    return argv[idx];
}

#define MAIN_RET(c) ((c) == 0 ? EXIT_SUCCESS :  EXIT_FAILURE)

int emudrv_main(int argc, char **argv)
{
    int ret;
    int mfs_idx;
    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char resourceStr[MAX_RESOURCE_LENGTH_EXT];
    const char *arg2;
    struct lredir_opts opts = {};
    const char *getopt_string = "d:fhnr:swC::R";

    /* check the MFS redirector supports this DOS */
    if (!isInitialisedMFS()) {
      com_printf("Unsupported DOS version or EMUFS.SYS not loaded\n");
      return(2);
    }

    if (argc > 1) {
	ret = lredir_parse_opts(argc, argv, getopt_string, &opts);
	if (ret)
	    return EXIT_FAILURE;
    }

    if (argc == 1 || opts.help) {
	com_printf("EMUDRV: tool for manipulating emulated drives\n");
	com_printf("Usage: EMUDRV <options> [drive:] [DOS_path]\n");
	com_printf("Redirect a drive to the specified DOS path.\n\n");
	com_printf("EMUDRV <options> X: C:\\tmp\n");
	com_printf("  Create drive X: as alias of C:\\tmp\n");
	com_printf("  Following options may be used:\n");
	com_printf("  -f: force the creation even if drive already exists\n");
	com_printf("  -R: create read-only drive\n");
	com_printf("  -C[n]: create CDROM n emulation drive (n=1..3, default=1)\n");
	com_printf("EMUDRV <options> -n C:\\tmp\n");
	com_printf("  Same as above, but use first available drive letter\n");
	com_printf("EMUDRV -d drive:\n");
	com_printf("  delete an emulated drive\n");
	com_printf("EMUDRV -r drive:\n");
	com_printf("  restore previously deleted emulated drive\n");
	com_printf("EMUDRV -w\n");
	com_printf("  show linux path for DOS CWD\n");
	com_printf("EMUDRV -s\n");
	com_printf("  show current emulated drive mapping to host paths\n");
	com_printf("EMUDRV\n");
	com_printf("  show this help screen\n");
	return 0;
    }

    if (opts.show) {
      ShowMyRedirections();
      return(0);
    }

    if (opts.del)
	return MAIN_RET(DeleteDriveRedirection(opts.del));
    if (opts.restore) {
	if (argc < 3) {
	    com_printf("syntax error\n");
	    return EXIT_FAILURE;
	}
	ret = do_restore(argv[2], resourceStr, sizeof(resourceStr), &mfs_idx,
		&opts.ro);
	if (ret)
	    return EXIT_FAILURE;
        return MAIN_RET(do_redirect(argv[2], resourceStr, &opts, mfs_idx));
    }

    if (opts.pwd) {
	char ucwd[MAX_RESOURCE_LENGTH_EXT];
	int err = get_unix_cwd(ucwd);
	if (err)
	    return EXIT_FAILURE;
	com_printf("%s\n", ucwd);
	return 0;
    }

    arg2 = get_arg2(argc, argv, &opts);
    if (arg2 && arg2[1] != ':' && arg2[0] != '.') {
	com_printf("invalid path %s\n", arg2);
	return EXIT_FAILURE;
    }
    if (!argv[opts.optind]) {
	com_printf("syntax error\n");
	return EXIT_FAILURE;
    }

    ret = fill_dev_str(deviceStr, argv[opts.optind], &opts);
    if (ret)
	return EXIT_FAILURE;

    ret = do_repl(arg2, resourceStr, sizeof(resourceStr), &mfs_idx, &opts.ro);
    if (ret)
	return EXIT_FAILURE;

    return MAIN_RET(do_redirect(deviceStr, resourceStr, &opts, mfs_idx));
}

int lredir_main(int argc, char **argv)
{
    int ret;
    char deviceStr[MAX_DEVICE_STRING_LENGTH];
    char resourceStr[MAX_RESOURCE_LENGTH_EXT];
    struct lredir_opts opts = {};
    const char *getopt_string = "fhd:C::Rn";

    /* check the MFS redirector supports this DOS */
    if (!isInitialisedMFS()) {
      com_printf("Unsupported DOS version or EMUFS.SYS not loaded\n");
      return(2);
    }

    /* need to parse the command line */
    /* if no parameters, then just show current mappings */
    if (argc == 1) {
      ShowMyRedirections();
      return(0);
    }

    ret = lredir_parse_opts(argc, argv, getopt_string, &opts);
    if (ret)
	return EXIT_FAILURE;

    if (opts.help) {
	com_printf("Usage: LREDIR <options> [drive:] [linux_path]\n");
	com_printf("Redirect a drive to the Linux file system.\n\n");
	com_printf("LREDIR X: /tmp\n");
	com_printf("  Redirect drive X: to /tmp of Linux file system for read/write\n");
	com_printf("  Following options may be used:\n");
	com_printf("  -f: force the redirection even if already redirected\n");
	com_printf("  -R: read-only redirection\n");
	com_printf("  -C[n]: create CDROM n emulation (n=1..3, default=1)\n");
	com_printf("  -n: use next available drive letter\n");
	com_printf("LREDIR -d drive:\n");
	com_printf("  delete a drive redirection\n");
	com_printf("LREDIR\n");
	com_printf("  show current drive redirections\n");
	com_printf("LREDIR -h\n");
	com_printf("  show this help screen\n");
	return 0;
    }

    if (opts.del)
	return MAIN_RET(DeleteDriveRedirection(opts.del));

    ret = fill_dev_str(deviceStr, argv[opts.optind], &opts);
    if (ret)
	return EXIT_FAILURE;

    resourceStr[0] = '\0';
    /* accept old unslashed notation */
    if (strncasecmp(get_arg2(argc, argv, &opts), LINUX_RESOURCE + 2,
		strlen(LINUX_RESOURCE) - 2) == 0) {
	strcpy(resourceStr, "\\\\");
	strcat(resourceStr, get_arg2(argc, argv, &opts));
    } else if (get_arg2(argc, argv, &opts)[0] == '\\') {
	strcpy(resourceStr, get_arg2(argc, argv, &opts));
    } else {
	char *rp = expand_path(get_arg2(argc, argv, &opts));
	if (!rp) {
	    com_printf("invalid path\n");
	    return EXIT_FAILURE;
	}
	strcpy(resourceStr, LINUX_RESOURCE);
	strcat(resourceStr, rp);
	free(rp);
    }

    return MAIN_RET(do_redirect(deviceStr, resourceStr, &opts, 0));
}
