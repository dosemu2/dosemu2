/*
 * dosmemulator, Matthias Lautner
 * 
 * DANG_BEGIN_MODULE
 * 
 * This handles the keyboard.
 * 
 * Two keyboard modes are handled 'raw' and 'xlate'. 'Raw' works with codes
 * as sent out by the kernel and 'xlate' uses plain ASCII as used over
 * serial lines. The mapping for different languages & the two ALT-keys is
 * done here, but the definitions are elsewhere. Only the default (US)
 * keymap is stored here.
 * 
 * DANG_END_MODULE
 * 
 * 
 * DANG_BEGIN_CHANGELOG
 * 
 * $Date: 1995/04/08 22:34:30 $ $Source: /usr/src/dosemu0.60/keyboard/RCS/keyboard-client.c,v $ $Revision: 1.1 $ $State: Exp $ $Log: keyboard-client.c,v $
 *
 * Initial revision
 * DANG_END_CHANGELOG
 */

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/times.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#ifndef EDEADLOCK
  #define EDEADLOCK EDEADLK
#endif
#ifdef __linux__
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/time.h>
#endif
#ifdef __NetBSD__
#include <machine/pcvt_ioctl.h>
#endif
#include <sys/stat.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "termio.h"
#include "shared.h"
#include "slang.h"
#include "vc.h"

static int      tty_raw(int);
inline void     child_set_flags(int);
void		clear_raw_mode();
extern void     clear_console_video();
extern void     clear_consoleX_video();
extern void     clear_process_control();
void		set_raw_mode();
void            set_leds();
extern void     set_leds_area();
extern void     add_scancode_to_queue(u_short);
extern void     vt_activate(int);
extern int      terminal_initialize();
extern void     terminal_close();
void            convascii(int *);

#define put_queue(psc) (queue = psc)

void            getKeys();

void            child_set_flags(int sc);

void            child_set_kbd_flag(int),
                child_clr_kbd_flag(int);
int             child_kbd_flag(int);
extern int      kbd_flag(int);

/* initialize these in keyboard_init()! */
unsigned int    child_kbd_flags = 0;

static int      old_kbd_flags = -1;	/* flags for STDIN before our
					 * fcntl */

int             kbcount = 0;
unsigned char   kbbuf[KBBUF_SIZE],
               *kbp,
                erasekey;
static struct termios oldtermios;	/* original terminal modes */

#if 0
int
do_ioctl(int fd, int req, int param3)
{
    int             tmp;

    if (in_sighandler && in_ioctl) {
	k_printf("KBD: do_ioctl(): in ioctl %d 0x%04x 0x%04x.\nqueuing: %d 0x%04x 0x%04x\n",
		 curi.fd, curi.req, curi.param3, fd, req, param3);
	queue_ioctl(fd, req, param3);
	errno = EDEADLOCK;
#ifdef SYNC_ALOT
	fflush(stdout);
	sync();			/* for safety */
#endif
	return -1;
    } else {
	in_ioctl = 1;
	curi.fd = fd;
	curi.req = req;
	curi.param3 = param3;
	if (iq.queued) {
	    k_printf("KBD: detected queued ioctl in do_ioctl(): %d 0x%04x 0x%04x\n",
		     iq.fd, iq.req, iq.param3);
	}
	k_printf("KBD: IOCTL fd=0x%x, req=0x%x, param3=0x%x\n", fd, req, param3);
	tmp = ioctl(fd, req, param3);
	in_ioctl = 0;
	return tmp;
    }
}

int
queue_ioctl(int fd, int req, int param3)
{
    if (iq.queued) {
	error("ioctl already queued: %d 0x%04x 0x%04x\n", iq.fd, iq.req,
	      iq.param3);
	return 1;
    }
    iq.fd = fd;
    iq.req = req;
    iq.param3 = param3;
    iq.queued = 1;

    return 0;			/* success */
}

void
do_queued_ioctl(void)
{
    if (iq.queued) {
	iq.queued = 0;
	do_ioctl(iq.fd, iq.req, iq.param3);
    }
}

void
vt_activate(int con_num)
{
    if (in_ioctl) {
	k_printf("KBD: can't ioctl for activate, in a signal handler\n");
	do_ioctl(kbd_fd, VT_ACTIVATE, con_num);
    } else
	do_ioctl(kbd_fd, VT_ACTIVATE, con_num);
}

#endif

void
keyboard_close(void)
{
    if (kbd_fd != -1) {

	if (config.console_keyb) {
	    k_printf("KBD: keyboard_close:clear raw keyb\n");
	    clear_raw_mode();
	}
	if (config.console_video) {
	    k_printf("KBD: keyboard_close:clear console video\n");
	    clear_console_video();
	}
	if (config.usesX) {
	    v_printf("CloseKeyboard:clear console video\n");
	    clear_consoleX_video();
	}
	if ((config.console_keyb || config.console_video) && !config.usesX) {
	    k_printf("KBD: keyboard_close: clear process control\n");
	    clear_process_control();
	}
	fcntl(kbd_fd, F_SETFL, old_kbd_flags);
	tcsetattr(kbd_fd, TCSANOW, &oldtermios);

	close(kbd_fd);
	kbd_fd = -1;
    }
}

static struct termios save_termios;

static void
print_termios(struct termios term)
{
    k_printf("KBD: TERMIOS Structure:\n");
    k_printf("KBD: 	c_iflag=%x\n", term.c_iflag);
    k_printf("KBD: 	c_oflag=%x\n", term.c_oflag);
    k_printf("KBD: 	c_cflag=%x\n", term.c_cflag);
    k_printf("KBD: 	c_lflag=%x\n", term.c_lflag);
#ifdef __linux__
    k_printf("KBD: 	c_line =%x\n", term.c_line);
#endif
}

#ifdef X_SUPPORT
void
keyboard_X_init(void)
{
    scr_state.console_no = 0;
    config.console_keyb = 0;
    config.console_video = 0;
    config.console = 0;
    config.mapped_bios = 0;
    config.vga = 0;
    config.graphics = 0;
    if (config.speaker == SPKR_NATIVE)
	config.speaker = SPKR_EMULATED;
    child_kbd_flags = 0;
    scr_state.current = 1;

}

#endif

int init_slang_keymaps(void); /* defined in slang-keyboard.c */

/*
 * DANG_BEGIN_FUNCTION keyboard_init
 * 
 * description: 
 * Initialize the keyboard to DOSEMU deafaults plus those
 * requested in the configs if allowable.
 * 
 * DANG_END_FUNCTION
 */
int
keyboard_init(void)
{
    struct termios  newtermio;	/* new terminal modes */
#ifdef __NetBSD__
    struct screeninfo info;
#endif
#ifdef __linux__
    struct stat     chkbuf;
    int             major,
                    minor;
#endif

#ifdef X_SUPPORT
    if (config.X) {
	keyboard_X_init();
	return 0;
    }
#endif

    if (config.usesX) {
	kbd_fd = dup(keypipe);
	if (kbd_fd < 0) {
	    k_printf("ERROR: Couldn't duplicate STDIN !\n");
	    memset(&oldtermios, 0x0, sizeof(oldtermios));
#if 0
	    leavedos(66);
#endif
	}
    } else {
	kbd_fd = STDIN_FILENO;
    }

#ifdef __NetBSD__
    if (ioctl(kbd_fd, VGAGETSCREEN, &info) == 0) {
	/* it's zero-based, we use 1-based */
	scr_state.console_no = info.screen_no + 1;
	config.console = 1;
    }
#endif
#ifdef __linux__
    fstat(kbd_fd, &chkbuf);
    major = chkbuf.st_rdev >> 8;
    minor = chkbuf.st_rdev & 0xff;

    /* console major num is 4, minor 64 is the first serial line */
    if ((major == 4) && (minor < 64)) {
	scr_state.console_no = minor;	/* get minor number */
	config.console = 1;
    } 
#endif
    else {
	if (config.console_keyb || config.console_video)
	    k_printf("ERROR: STDIN not a console-can't do console modes!\n");
	scr_state.console_no = 0;
	config.console_keyb = 0;
	config.console_video = 0;
	config.mapped_bios = 0;
	config.vga = 0;
	config.graphics = 0;
	config.console = 0;
	if (config.speaker == SPKR_NATIVE)
	    config.speaker = SPKR_EMULATED;
    }

    old_kbd_flags = fcntl(kbd_fd, F_GETFL);
    fcntl(kbd_fd, F_SETFL, O_RDONLY | O_NONBLOCK);

    if (use_sigio && !config.kbd_tty) {
	k_printf("KBD: Using SIGIO\n");
	add_to_io_select(kbd_fd, 1);
    } else {
	k_printf("KBD: Not using SIGIO\n");
	add_to_io_select(kbd_fd, 0);
    }

    if (tcgetattr(kbd_fd, &oldtermios) < 0) {
	k_printf("ERROR: Couldn't tcgetattr(STDIN,...) !\n");
	memset(&oldtermios, 0x0, sizeof(oldtermios));
#if 0
	leavedos(66);
#endif
    }
    /*
     * DANG_BEGIN_REMARK
     * 
     * Code is called at start up to set up the terminal line for non-raw
     * mode.
     * 
     * DANG_END_REMARK
     */
    newtermio = oldtermios;
    newtermio.c_iflag &= (ISTRIP | IGNBRK | IXON | IXOFF);	/* (IXON|IXOFF|IXANY|ISTR
								 * IP|IGNBRK); */
    /*
     * newtermio.c_oflag &= ~OPOST; newtermio.c_cflag &= ~(HUPCL);
     */
    newtermio.c_cflag &= ~(CLOCAL | CSIZE | PARENB);
    newtermio.c_cflag |= CS8;
    newtermio.c_lflag &= 0;	/* ISIG */
    newtermio.c_cc[VMIN] = 1;
    newtermio.c_cc[VTIME] = 0;
    erasekey = newtermio.c_cc[VERASE];
    cfgetispeed(&newtermio);
    cfgetospeed(&newtermio);
    if (tcsetattr(kbd_fd, TCSANOW, &newtermio) < 0) {
	k_printf("ERROR: Couldn't tcsetattr(STDIN,TCSANOW,...) !\n");
    }
    child_kbd_flags = 0;
    scr_state.current = 1;
    if (config.console_keyb) {
	set_raw_mode();
    }
    if (-1 == init_slang_keymaps()) {
	error("ERROR: Unable to initialize S-Lang keymaps.\n");
	leavedos(31);
    }
    dbug_printf("TERMIO: $Header: /usr/src/dosemu0.60/keyboard/RCS/keyboard-client.c,v 1.1 1995/04/08 22:34:30 root Exp $\n");
    return 0;
}

void
clear_raw_mode(void)
{
    do_ioctl(kbd_fd, KDSKBMODE, K_XLATE);
    do_ioctl(kbd_fd, KDSETLED, 0xFF); /* peak, leds follow kernel flags again */
    if (tcsetattr(kbd_fd, TCSAFLUSH, &save_termios) < 0) {
	k_printf("KBD: Resetting Keyboard to K_XLATE mode failed.\n");
	return;
    }
}

void
set_raw_mode(void)
{
    k_printf("KBD: Setting keyboard to RAW mode\n");
    if (!config.console_video)
	fprintf(stderr, "\nKBD: Entering RAW mode for DOS!\n");
    do_ioctl(kbd_fd, KDSKBMODE, K_RAW);
    tty_raw(kbd_fd);
}

#ifdef __NetBSD__
#define IUCLC	0
#define OCRNL	ONLCR
#define OLCUC	0
#endif

static int
tty_raw(int fd)
{
    struct termios  buf;

    if (tcgetattr(fd, &save_termios) < 0)
	return (-1);

    buf = save_termios;

    buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    buf.c_iflag &= ~(IMAXBEL | IGNBRK | IGNCR | IGNPAR | BRKINT | INLCR | ICRNL | INPCK | ISTRIP | IXON | IUCLC | IXANY | IXOFF | IXON);
    buf.c_cflag &= ~(CSIZE | PARENB);
    buf.c_oflag &= ~(OCRNL | OLCUC | ONLCR | OPOST);
    buf.c_cflag |= CS8;
    buf.c_cc[VMIN] = 1;
    buf.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0) {
	k_printf("KBD: Setting to RAW mode failed.\n");
	return (-1);
    }
    if (tcgetattr(fd, &buf) < 0) {
	k_printf("Termios ERROR\n");
    }
    k_printf("KBD: Setting TERMIOS Structure.\n");
    print_termios(buf);

    return (0);
}

void
add_key_depress(u_short scan)
{
    if (scan < 0x80 && scan > 0) {
	k_printf("Adding Key-Release\n");
	add_scancode_to_queue(scan | 0x80);
    }
}

#include "slang-keyboard.c"

void
getKeys(void)
{
    int             cc;

    if (config.console_keyb) {	/* Keyboard at the console */
	if (scr_state.current != 1) {
	    k_printf("KBD: Not current screen!\n");
	    return;
	}
	kbcount = 0;
	kbp = kbbuf;

	cc = RPT_SYSCALL(read(kbd_fd, &kbp[kbcount], KBBUF_SIZE - 1));
	k_printf("KBD: cc found %d characters (Raw)\n", cc);
	if (cc == -1)
	    return;

	if (cc > 0) {
	    int             i;

	    for (i = 0; i < cc; i++) {
		k_printf("KEY: readcode: %d \n", kbp[kbcount + i]);
		child_set_flags(kbp[kbcount + i]);
		add_scancode_to_queue(kbp[kbcount + i]);
		k_printf("KBD: cc pushing %d'th character\n", i);
	    }
	}
    } else {			/* Not keyboard at the console (not
				 * config.console_keyb) */
	do_slang_getkeys();
    }
}

void
child_set_flags(int sc)
{
    switch (sc) {
    case 0xe0:
    case 0xe1:
	child_set_kbd_flag(4);
	return;
    case 0x2a:
	if (!child_kbd_flag(4))
	    child_set_kbd_flag(1);
	child_clr_kbd_flag(4);
	return;
    case 0x36:
	child_set_kbd_flag(1);
	child_clr_kbd_flag(4);
	return;
    case 0x1d:
    case 0x10:
	child_set_kbd_flag(2);
	child_clr_kbd_flag(4);
	return;
    case 0x38:
	if (!child_kbd_flag(4))
	    child_set_kbd_flag(3);
	child_clr_kbd_flag(4);
	return;
    case 0xaa:
	if (!child_kbd_flag(4))
	    child_clr_kbd_flag(1);
	child_clr_kbd_flag(4);
	return;
    case 0xb6:
	child_clr_kbd_flag(4);
	child_clr_kbd_flag(1);
	return;
    case 0x9d:
    case 0x90:
	child_clr_kbd_flag(4);
	child_clr_kbd_flag(2);
	return;
    case 0xb8:
	child_clr_kbd_flag(3);
	child_clr_kbd_flag(4);
	return;
    case 0x3b:
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x57:
    case 0x58:
	child_clr_kbd_flag(4);
	if (
#ifdef X_SUPPORT
	       !config.X &&
#endif
	(child_kbd_flag(3) && child_kbd_flag(2) && !child_kbd_flag(1))) {
	    int             fnum = sc - 0x3a;

	    k_printf("KDB: Doing VC switch\n");
	    if (fnum > 10)
		fnum -= 0x12;	/* adjust if f11 or f12 */

	    /*
	     * can't just do the ioctl() here, as ReadKeyboard will
	     * probably have been called from a signal handler, and
	     * ioctl() is not reentrant. hence the delay until out of the
	     * signal handler...
	     */
	    child_clr_kbd_flag(3);
	    child_clr_kbd_flag(2);
	    vt_activate(fnum);
	    return;
	}
	return;
    case 0x51:
	if (
#ifdef X_SUPPORT
	       !config.X &&
#endif
	(child_kbd_flag(2) && child_kbd_flag(3) && !child_kbd_flag(1))) {
	    dbug_printf("ctrl-alt-pgdn\n");
	    leavedos(42);
	}
	child_clr_kbd_flag(4);
	return;
    default:
	child_clr_kbd_flag(4);
	return;
    }
}

void
set_leds()
{
    unsigned int    led_state = 0;

    if (kbd_flag(KF_SCRLOCK)) {
	led_state |= (1 << LED_SCRLOCK);
    }
    if (kbd_flag(KF_NUMLOCK)) {
	led_state |= (1 << LED_NUMLOCK);
    }
    if (kbd_flag(KF_CAPSLOCK)) {
	led_state |= (1 << LED_CAPSLOCK);
    }
    k_printf("KBD: SET_LEDS() called\n");
    do_ioctl(kbd_fd, KDSETLED, led_state);
}

/*
 * These are added to allow the CHILD process to keep its own flags on
 * keyboard status
 */

void
child_set_kbd_flag(int flag)
{
    child_kbd_flags |= (1 << flag);
}

void
child_clr_kbd_flag(int flag)
{
    child_kbd_flags &= ~(1 << flag);
}

int
child_kbd_flag(int flag)
{
    return ((child_kbd_flags >> flag) & 1);
}
