/* Definitions for DOSEMU's helper functions */
/*
 * DANG_BEGIN_REMARK
 * The Helper Interrupt uses the following groups:
 *
 * 0x00      - Check for DOSEMU
 * 0x01-0x11 - Initialisation functions & Debugging
 * 0x12      - Set hogthreshold (aka garrot?)
 * 0x20      - MFS functions
 * 0x21-0x22 - EMS functions
 * 0x28      - Garrot Functions for use with the mouse
 * 0x29      - Serial functions
 * 0x30      - (removed functionality)
 * 0x33      - Mouse Functions
 * 0x40      - CD-ROM functions
 * 0x50-0x5f - DOSEMU/Linux communications
 *      50 -- run unix command in ES:DX
 *      51,52?
 *      53 -- do system(ES:DX)
 *	54 -- get CPU speed
 *	55 -- get terminal type
 * 0x60-0x6f - reserved for plug-ins
 * 0x7a      - IPX functions
 * 0x8x   -- utility functions
 *	0x80 -- getcwd(ES:DX, size AX)
 *	0x81 -- chdir(ES:DX)
 * 0xdc      - helper for DosC kernel
 * 0xfe      - called from our MBR, emulate MBR-code.
 * 0xff      - Terminate DOSEMU
 *
 * There are (as yet) no guidelines on choosing areas for new functions.
 * DANG_END_REMARK
 */

#define DOS_HELPER_INT              0xE6 /* The interrupt we use */
#define DOS_HELPER_MAGIC          0xaa55

#define DOS_HELPER_DOSEMU_CHECK     0x00
#define DOS_HELPER_SHOW_REGS        0x01
#define DOS_HELPER_SHOW_INTS        0x02
#define DOS_HELPER_ADJUST_IOPERMS   0x03  /* CY indicates get or set      */
#define DOS_HELPER_CONTROL_VIDEO    0x04  /* BL indicates init or release */
#define DOS_HELPER_SHOW_BANNER      0x05
#define DOS_HELPER_INSERT_INTO_KEYBUFFER 0x06 /* OLD, depreciated */
#define DOS_HELPER_GET_BIOS_KEY     0x07  /* OLD, depreciated */
#define DOS_HELPER_VIDEO_INIT       0x08
#define DOS_HELPER_VIDEO_INIT_DONE  0x09


#define DOS_HELPER_GET_DEBUG_STRING 0x10
#define DOS_HELPER_SET_DEBUG_STRING 0x11
#define DOS_HELPER_SET_HOGTHRESHOLD 0x12
#define DOS_HELPER_PRINT_STRING     0x13 /* ES:DX point to a NULL terminated string */


#define DOS_HELPER_MFS_HELPER       0x20
#define DOS_SUBHELPER_MFS_EMUFS_INIT 0
#define DOS_SUBHELPER_MFS_REDIR_INIT 5
#define DOS_SUBHELPER_MFS_REDIR_STATE 6

#define DOS_HELPER_EMS_HELPER       0x21
#define DOS_HELPER_EMS_BIOS         0x22
#define DOS_HELPER_XMS_HELPER       0x23
#define DOS_HELPER_EMUFS_HELPER     0x24
#define DOS_SUBHELPER_EMUFS_REDIRECT   0
#define DOS_SUBHELPER_EMUFS_IOCTL      1

#define EMUFS_IOCTL_GET_ENTRY          0
#define EMUFS_HELPER_REDIRECT          1

#define DOS_HELPER_GARROT_HELPER    0x28

#define DOS_HELPER_SERIAL_HELPER    0x29
#define DOS_SUBHELPER_SERIAL_FOSSIL_CHECK 0
#define DOS_SUBHELPER_SERIAL_TSR_INSTALL 1
#define DOS_SUBHELPER_SERIAL_FOSSIL_INIT 2
#define DOS_ERROR_SERIAL_ALREADY_INSTALLED 1
#define DOS_ERROR_SERIAL_CONFIG_DISABLED 2
#define DOS_ERROR_SERIAL_TSR_INVALID 3
#define DOS_ERROR_SERIAL_FOSSIL_VERSION 4
#define DOS_VERSION_SERIAL_FOSSIL 1


#define DOS_HELPER_BOOTDISK         0x30  /* OLD, removed functionality */


#define DOS_HELPER_MOUSE_HELPER     0x33

#define DOS_HELPER_CDROM_HELPER     0x40

#define DOS_HELPER_ASPI_HELPER      0x41

#define DOS_HELPER_REVECT_HELPER    0x42
#define DOS_SUBHELPER_RVC_VERSION_CHECK 0
#define DOS_SUBHELPER_RVC_CALL          1
#define DOS_SUBHELPER_RVC2_CALL         2
#define DOS_SUBHELPER_RVC_UNREVECT      3
#define DOS_SUBHELPER_RVC_NEXT_VEC      4

#define DOS_HELPER_GET_UNIX_ENV     0x52
#define DOS_HELPER_GET_CPU_SPEED    0x54 /* return CPU clock frequency in EAX,
					    Units: MHz * 0x10000, */
#define DOS_HELPER_GET_TERM_TYPE    0x55 /* return type-bits in EAX:
					    bit0...3 = Keyboard
						0 = raw
						1 = Slang
						2 = X
					    bit4 = console_video
					    bit5 = console graphics
					    bit6 = dualmon */

#define DOS_HELPER_PLUGIN	    0x60
#define DOS_HELPER_PLUGIN_ID_FDPP   0

#define DOS_HELPER_GETCWD           0x80
#define DOS_HELPER_CHDIR            0x81
#define DOS_HELPER_GETPID           0x82

#define DOS_HELPER_CPUEMUON         0x90
#define DOS_HELPER_CPUEMUOFF        0x91

#define DOS_HELPER_XCONFIG          0xa0

#define DOS_HELPER_DOSC		    0xdc

#define DOS_HELPER_READ_MBR         0xfc
#define DOS_HELPER_BOOTSECT         0xfd
#define DOS_HELPER_MBR              0xfe
#define DOS_HELPER_EXIT             0xff
#define DOS_HELPER_REALLY_EXIT      0xffff

/* sub-helpers - BX val */
#define DOS_SUBHELPER_MOUSE_START_VIDEO_MODE_SET 0xf0
#define DOS_SUBHELPER_MOUSE_END_VIDEO_MODE_SET   0xf1

#define USE_COMMANDS_PLUGIN 1

/* Increment this when the interface changes */
#define BUILTINS_PLUGIN_VERSION     (2 + CMDS_REV)

#define DOS_HELPER_COMMANDS         0xc0
#define DOS_HELPER_COMMANDS_DONE    0xc1
#define DOS_HELPER_SET_RETCODE      0xc2

#ifndef __ASSEMBLER__
extern int commands_plugin_inte6(void);
extern int commands_plugin_inte6_done(void);
extern int commands_plugin_inte6_set_retcode(void);
extern void commands_plugin_inte6_reset(void);

extern int register_plugin_call(int num, void (*call)(struct vm86_regs *));
extern int register_cleanup_handler(void (*call)(void));

typedef int (*run_dos_cb)(const char *command);
typedef unsigned short (*get_psp_cb)(int parent);
int run_command_plugin(const char *name, const char *argv0, char *cmdbuf,
    run_dos_cb run_cb, get_psp_cb psp_cb);
#endif
