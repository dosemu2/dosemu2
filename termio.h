/* dos emulator, Matthias Lautner */
#ifndef TERMIO_H
#define TERMIO_H
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/11/12 12:32:17 $
 * $Source: /home/src/dosemu0.49pl2/RCS/termio.h,v $
 * $Revision: 1.1 $
 * $State: Exp $
 *
 * $Log: termio.h,v $
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.14  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.13  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.12  1993/02/24  11:33:24  root
 * some general cleanups, fixed the key-repeat bug.
 *
 * Revision 1.11  1993/02/18  19:35:58  root
 * just added newline so diff wouldn't barf
 *
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
#define KKF_E0		0
#define KKF_E1		1
#define KKF_RCTRL	2
#define KKF_RALT	3
#define KKF_KBD102	4  /* set if 102-key keyboard installed */	

/* LED FLAGS (from Linux keyboard code) */
#define LED_SCRLOCK	0
#define LED_NUMLOCK	1
#define LED_CAPSLOCK	2

extern unsigned int kbd_flags, key_flags;
void set_screen_origin(int),
     set_vc_screen_page(int);

int vc_active(void);

/* signals for Linux's process control of consoles */
#define SIG_RELEASE	SIGWINCH
#define SIG_ACQUIRE	SIGUSR1

#define KBBUF_SIZE	80	     /* dosemu read buffer for keyboard */

struct screen_stat {
  int console_no,	/* our console number */
      vt_allow, 	/* whether to allow VC switches */
      vt_requested;	/* whether one was requested in forbidden state */

  int current;		/* boolean: is our VC current? */

  int curadd;		/* row*80 + col */
  int dcurgeom;		/* msb: start, lsb: end */
  int lcurgeom;		/* msb: start, lsb: end */

  int mapped,  		/* whether currently mapped */
      pageno;  		/* current mapped text page # */

  int dorigin;		/* origin in DOS */
  int lorigin;

  caddr_t virt_address;	/* current map address in DOS memory */

  int old_modecr, new_modecr;
};

#endif /* TERMIO_H */

