/* dos emulator, Matthias Lautner */

/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/01/21 21:56:23 $
 * $Source: /usr/src/dos/RCS/termio.h,v $
 * $Revision: 1.5 $
 * $State: Exp $
 *
 * $Log: termio.h,v $
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


/* LED FLAGS */
#define LED_SCRLOCK	0
#define LED_NUMLOCK	1
#define LED_CAPSLOCK	2

/* signals */
#define SIG_RELEASE	SIGWINCH
#define SIG_ACQUIRE	SIGUSR1
extern unsigned int kbd_flags, key_flags;

/* raw console stuff */
#define PHYS_SCRN_BASE	0xB8000      /* this is NOT true for MDA! */
#define VIRT_SCRN_BASE	0xB8000      /* the emulator is always in EGA */

#define SCRN_SIZE	0x1000       /* 4K text mode mem */
#define SCRN_BUF_ADDR	0x110000     /* buffer for storing screen */
#define SCRN_BUF_SIZE	(64 * 1024)
