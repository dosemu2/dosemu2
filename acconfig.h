/* 
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * BIG FAT WARNING:
 *
 * This file (this is acconfig.h, _NOT_ config.h.in) is the only place 
 * where you can add #defines. If you edit config.h.in, your efforts
 * will become useless the next time you run autoheader.
 *
 * All lines before the TOP macro or after the BOTTOM macro are copied
 * unchanged into config.h.in. Read the GNU autoconf documentation
 * to learn more about configure & friends.
 *
 */

#ifndef CONFIG_H
#define CONFIG_H 1

#ifndef __ASM__
#include "extern.h"

/* not overridable file/path constants */
#define  DOSEMU_RC          ".dosemurc"       /* per user config file */
#define  OLD_DOS_RC         ".dosrc"          /* old, obsolete user config file */
#define  LOCALDIR_BASE_NAME ".dosemu"         /* base directory in $HOME */
#define  DOSEMULIB_DEFAULT  "/var/lib/dosemu" /* system wide dosemu directory */
#define  DOSEMU_CONF        "dosemu.conf"     /* standard configuration file */
#define  DOSEMU_USERS       "dosemu.users"    /* access right configuration file */
#define  CONFIG_SCRIPT_NAME "global.conf"     /* main configuration script */
#define  DOSEMU_LOGLEVEL    "dosemu.loglevel" /* logging configuration file */
#define  ALTERNATE_ETC      "/etc/dosemu"     /* alternate config. directory */
#define  DOSEMU_MIDI        "dosemu-midi"     /* fifo for midi daemon */

/* overridable file/path constants */
EXTERN char *config_file_path INIT("/etc/" DOSEMU_CONF);
EXTERN char *config_script_path INIT(DOSEMULIB_DEFAULT "/" CONFIG_SCRIPT_NAME);
EXTERN char *dosemu_users_file_path INIT("/etc/" DOSEMU_USERS);
EXTERN char *dosemu_loglevel_file_path INIT("/etc/" DOSEMU_LOGLEVEL);
EXTERN char *dosemu_tmpfile_path INIT("~/" LOCALDIR_BASE_NAME "/run/dosemu.");
EXTERN char *dosemu_tmpdir_path INIT("~/" LOCALDIR_BASE_NAME "/tmp");
EXTERN char *dosemu_tmpdir_process_path INIT("~/" LOCALDIR_BASE_NAME "/tmp/<pid>");
EXTERN char *dosemu_rundir_path INIT("~/" LOCALDIR_BASE_NAME "/run");
EXTERN char *dosemu_localdir_path INIT("~/" LOCALDIR_BASE_NAME);
EXTERN char *dosemu_lib_dir_path INIT(DOSEMULIB_DEFAULT);
EXTERN char *keymap_load_base_path INIT(DOSEMULIB_DEFAULT "/");
EXTERN char *keymap_dir_path INIT("keymap/");
EXTERN char *owner_tty_locks INIT("uucp");
EXTERN char *tty_locks_dir_path INIT("/var/lock");
EXTERN char *tty_locks_name_path INIT("LCK..");
EXTERN char *dexe_load_path INIT(DOSEMULIB_DEFAULT);
EXTERN char *ipx_dos_ini_path INIT(ALTERNATE_ETC "/dos.ini");
EXTERN char *dosemu_map_file_name INIT("/usr/src/dosemu/bin/dosemu.map");
EXTERN char *dosemu_midi_path INIT("~/" LOCALDIR_BASE_NAME "/run/" DOSEMU_MIDI);

#define    CONFIG_FILE           config_file_path
#define    CONFIG_SCRIPT         config_script_path
#define    DOSEMU_USERS_FILE     dosemu_users_file_path
#define    DOSEMU_LOGLEVEL_FILE  dosemu_loglevel_file_path
#define    TMPFILE               dosemu_tmpfile_path
#define    TMPDIR                dosemu_tmpdir_path
#define    TMPDIR_PROCESS        dosemu_tmpdir_process_path
#define    RUNDIR                dosemu_rundir_path
#define    LOCALDIR              dosemu_localdir_path
#define    DOSEMU_LIB_DIR        dosemu_lib_dir_path
#define    KEYMAP_LOAD_BASE_PATH keymap_load_base_path
#define    KEYMAP_DIR            keymap_dir_path
#define    OWNER_LOCKS           owner_tty_locks
#define    PATH_LOCKD            tty_locks_dir_path
#define    NAME_LOCKF            tty_locks_name_path
#define    DEXE_LOAD_PATH        dexe_load_path
#define    IPX_DOS_INI_PATH      ipx_dos_ini_path
#define    DOSEMU_MAP_PATH       dosemu_map_file_name
#define    DOSEMU_MIDI_PATH      dosemu_midi_path

#endif /* not __ASM__ */

@TOP@

/* Define the host for which Dosemu is configured */
#undef CONFIG_HOST

/* Define the configure time */
#undef CONFIG_TIME

/* Define the Dosemu version */
#undef VERSION

/* Define the Dosemu version code  */
#undef DOSEMU_VERSION_CODE

/* Define the Dosemu version release date */
#undef VERDATE

/* Define the Dosemu sublevel */
#undef SUBLEVEL

/* Define the Dosemu patchlevel */
#undef PATCHLEVEL

/* Define the version string */
#undef VERSTR

/* Define the EMU version */
#undef EMUVER

/* 
 * Define if the compiler doesn't have __FUNCTION__ macro
 * This is very gcc specific (and very useful!). It is defined
 * as "" if the compiler doesn't have it.
 */
#undef __FUNCTION__

/* 
 * Define if the compiler doesn't have __FILE__ macro
 * Most compilers have this one, but just to be sure...
 */
#undef __FILE__

/* 
 * Define if the compiler doesn't have __LINE__ macro
 * Most compilers have this one, but just to be sure...
 */
#undef __LINE__

/* DEFINE this, if you want the extended vm86 support (vm86plus)
 * You then either have to patch your kernel or load emumodule
 */
#undef REQUIRES_VM86PLUS

/* DEFINE this, if you have vm86plus built into the kernel */
#undef BUILTIN_VM86PLUS

/* DPMI test windows */
#undef WANT_WINDOWS

/* DEFINE this, if you want the dosdebugger (mhpdbg) */
#undef USE_MHPDBG

/* Define these for X support */
#undef X_SUPPORT
#undef X2_SUPPORT
#undef X_GRAPHICS

/*
 *  DEFINE this, if you have joystick support in the kernel
 *  or have the joystick module loaded (needing <linux/joystick.h>)
 */
#undef USE_MRP_JOYSTICK

/*
 * This is DEFINED when SB emulation is required. Just changing this is not
 * enough - you must reconfigure.
 */
#undef USE_SBEMU

/*
 * This is DEFINED when the Ultra libraries are available
 */
#undef USE_ULTRA

/* Define this if you want to use MITSHM */
#undef HAVE_MITSHM

/* Define this if you want to use a monoton micro timing (Bernd Paysan) */
#undef MONOTON_MICRO_TIMING

/* Define this if you want dosemu permanently run as (suid) root and only gain
 * user privileges, when accessing secure relevant resources.
 * Otherwise dosemu will permanently run as user and only temporary take over
 * root privilege when needed.
 */
#undef RUN_AS_ROOT

/* Define this to use threading code */
#undef USE_THREADS

/* Define this to use the new interrupt handling code */
#undef USE_NEW_INT

/* Define this for ASPI (generic SCSI) support */
#undef ASPI_SUPPORT

/* Define this to use the X86 CPU emulator */
#undef X86_EMULATOR

@BOTTOM@

#endif /* CONFIG_H */
