/* dos emulator, Matthias Lautner */
#ifndef TERMIO_H
#define TERMIO_H
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/02/13 23:37:20 $
 * $Source: /usr/src/dos/RCS/termio.h,v $
 * $Revision: 1.10 $
 * $State: Exp $
 *
 * $Log: termio.h,v $
 * Revision 1.10  1993/02/13  23:37:20  root
 * latest version, no time to document!
 *
 * Revision 1.9  1993/02/10  20:56:56  root
 * for the circa-WP dosemu
 *
 * Revision 1.8  1993/02/08  04:17:23  root
 * dosemu 0.47.7
 *
 * Revision 1.7  1993/02/05  02:54:41  root
 * this is for 0.47.6
 *
 * Revision 1.6  1993/02/04  01:17:23  root
 * version 0.47.5
 *
 * Revision 1.5  1993/01/21  21:56:23  root
 * changed SCRN_BASE to VIRT_... and PHYS_... to distinguish.  one day,
 * these might actually point to useful runtime-determined variables.
 *
 * Revision 1.4  1993/01/15  01:06:11  root
 * tweaked a couple of the SCRN_ values for console_video.  will likely
 * end up tweaking them back
 *
 * Revision 1.3  1993/01/14  22:01:08  root
 * for termio.c and emu.c 1.5.  added the defines for the direct console
 * write mode.  these defines should NOT be defines.  there is NO guarantee
 * that the screen will be at 0xb8000 (this is always true for color EGA/VGA
 * text mode 3, however).  this is a quick hack.
 *
 * Revision 1.2  1993/01/12  01:27:39  root
 * added log RCS var
 *
 */

/*
 * These are the defines for the int that is dosemu's kbd flagger (kbd_flags)
 * These just happen to coincide with the format of the GET EXTENDED SHIFT
 * STATE of int16h/ah=12h, and the low byte to GET SHIFT STATE int16h,ah=02h
 *
 * A lot of the RAW VC dosemu stuff was copied from the Linux kernel (0.98pl6)
 * I hope Linux dosen't mind
 *
 * Robert Sanders 12/12/92
 */

#define KF_RSHIFT	0
#define KF_LSHIFT	1
#define KF_CTRL		2	/* either one */
#define KF_ALT		3	/* either one */
#define KF_SCRLOCK	4	/* ScrLock ACTIVE */
#define KF_NUMLOCK	5	/* NumLock ACTIVE */
#define KF_CAPSLOCK	6	/* CapsLock ACTIVE */
#define KF_INSERT	7	/* Insert ACTIVE */
#define EKF_LCTRL	8
#define EKF_LALT	9
#define EKF_RCTRL	10
#define EKF_RALT	11
#define EKF_SCRLOCK	12	/* ScrLock PRESSED */
#define EKF_NUMLOCK	13	/* NumLock PRESSED */
#define EKF_CAPSLOCK	14	/* CapsLock PRESSED */
#define EKF_SYSRQ	15	/* SysRq PRESSED */

/* KEY FLAGS */
#define KKF_E0	0
#define KKF_E1	1

/* LED FLAGS (from Linux keyboard code) */
#define LED_SCRLOCK	0
#define LED_NUMLOCK	1
#define LED_CAPSLOCK	2

extern unsigned int kbd_flags, key_flags;


/* Bios data area 16-key (32byte) keybuffer */
#define KBDA_ADDR	(unsigned short *)0x41e

/* signals for Linux's process control of consoles */
#define SIG_RELEASE	SIGWINCH
#define SIG_ACQUIRE	SIGUSR1

/* raw console stuff */
#ifdef MDA_VIDEO
#define PHYS_TEXT_BASE  0xB0000
#else
#define PHYS_TEXT_BASE	0xB8000      /* this is NOT true for MDA! */
#endif

#define VIRT_TEXT_BASE	0xB8000      /* the emulator is always in "[V|E]GA" */
#define TEXT_SIZE	0x2000       /* 8K text mode mem */

#define GRAPH_BASE 0xA0000
#define GRAPH_SIZE 0x10000

#define SCRN_BUF_ADDR	0x110000     /* buffer for storing screen @ 1MB+64K*/
#define SCRN_BUF_SIZE	(0x10000)    /* buffer of 64K */

#endif /* TERMIO_H */
