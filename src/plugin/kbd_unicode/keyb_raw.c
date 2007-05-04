/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/kd.h>

#include "emu.h"
#include "keyboard.h"
#include "keyb_clients.h"
#include "termio.h"

#include "keystate.h"

#define KBBUF_SIZE (KEYB_QUEUE_LENGTH / 2)

/* LED FLAGS (from Linux keyboard code) */
#define LED_SCRLOCK	0
#define LED_NUMLOCK	1
#define LED_CAPSLOCK	2

static int save_kbd_flags = -1;  	/* flags for STDIN before our fcntl */
static struct termios save_termios;	/* original terminal modes */
static int save_mode;                   /* original keyboard mode  */

static void set_kbd_leds(t_modifiers shiftstate)
{
  unsigned int led_state = 0;
  static t_modifiers prev_shiftstate = 0xffff;

  if (shiftstate==prev_shiftstate) return;
  prev_shiftstate=shiftstate;

  if (shiftstate & MODIFIER_SCR) {
    led_state |= (1 << LED_SCRLOCK);
  }
  if (shiftstate & MODIFIER_NUM) {
    led_state |= (1 << LED_NUMLOCK);
  }
  if (shiftstate & MODIFIER_CAPS) {
    led_state |= (1 << LED_CAPSLOCK);
  }
  k_printf("KBD(raw): kbd_set_leds() setting LED state\n");
  ioctl(kbd_fd, KDSETLED, led_state);
}


static t_shiftstate get_kbd_flags(void)
{
  unsigned int led_state = 0;
  t_modifiers s = 0;

  k_printf("KBD(raw): getting keyboard flags\n");

  /* note: this reads the keyboard flags, not the LED state (which would
   * be KDGETLED).
   */
  ioctl(kbd_fd, KDGKBLED, &led_state);

  if (led_state & (1 << LED_SCRLOCK))  s|=MODIFIER_SCR;
  if (led_state & (1 << LED_NUMLOCK))  s|=MODIFIER_NUM;
  if (led_state & (1 << LED_CAPSLOCK)) s|=MODIFIER_CAPS;

  return s;
}

static void do_raw_getkeys(void)
{
  int i,count;
  Bit8u buf[KBBUF_SIZE];

  count = RPT_SYSCALL(read(kbd_fd, &buf, KBBUF_SIZE - 1));
  k_printf("KBD(raw): do_raw_getkeys() found %d characters (Raw)\n", count);
  if (count == -1) {
    k_printf("KBD(raw): do_raw_getkeys(): keyboard read failed!\n");
    return;
  }

  for (i = 0; i < count; i++) {
    k_printf("KBD(raw): readcode: %02x \n", buf[i]);
    put_rawkey(buf[i]);
#if 0
    set_kbd_leds(get_shiftstate());
#endif
  }
}

#if 0 /* debug code */
static void print_termios(struct termios term)
{
  k_printf("KBD(raw): TERMIOS Structure:\n");
  k_printf("KBD(raw): 	c_iflag=%lx\n", term.c_iflag);
  k_printf("KBD(raw): 	c_oflag=%lx\n", term.c_oflag);
  k_printf("KBD(raw): 	c_cflag=%lx\n", term.c_cflag);
  k_printf("KBD(raw): 	c_lflag=%lx\n", term.c_lflag);
  k_printf("KBD(raw): 	c_line =%x\n", (int)term.c_line);
}
#endif

static inline void set_raw_mode(void)
{
  struct termios buf = save_termios;

  k_printf("KBD(raw): Setting keyboard to RAW mode\n");
  ioctl(kbd_fd, KDSKBMODE, K_RAW);

  buf.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  buf.c_iflag &= ~(IMAXBEL | IGNBRK | IGNCR | IGNPAR | BRKINT | INLCR | ICRNL | INPCK | ISTRIP | IXON | IUCLC | IXANY | IXOFF | IXON);
  buf.c_cflag &= ~(CSIZE | PARENB);
  buf.c_cflag |= CS8;
  buf.c_oflag &= ~(OCRNL | OLCUC | ONLCR | OPOST);
  buf.c_cc[VMIN] = 1;
  buf.c_cc[VTIME] = 0;

  k_printf("KBD(raw): Setting TERMIOS Structure.\n");
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &buf) < 0)
    k_printf("KBD(raw): Setting TERMIOS structure failed.\n");

#if 0 /* debug code */
  if (tcgetattr(kbd_fd, &buf) < 0) {
    k_printf("KBD(raw): Termios ERROR\n");
  }
  print_termios(buf);
#endif
}

/*
 * DANG_BEGIN_FUNCTION raw_keyboard_init
 * 
 * Initialize the keyboard for RAW mode.
 * 
 * DANG_END_FUNCTION
 */
static int raw_keyboard_init(void)
{
  k_printf("KBD(raw): raw_keyboard_init()\n");
   
  kbd_fd = STDIN_FILENO;

  ioctl(kbd_fd, KDGKBMODE, (int)&save_mode);

  if (tcgetattr(kbd_fd, &save_termios) < 0) {
    error("KBD(raw): Couldn't tcgetattr(kbd_fd,...) !\n");
    memset(&save_termios, 0, sizeof(save_termios));
    return FALSE;
  }

  save_kbd_flags = fcntl(kbd_fd, F_GETFL);
  fcntl(kbd_fd, F_SETFL, O_RDONLY | O_NONBLOCK);

  set_raw_mode();

  if (!isatty(kbd_fd)) {
    k_printf("KBD(raw): Using SIGIO\n");
    add_to_io_select(kbd_fd, 1, keyb_client_run);
  }
  else {
    k_printf("KBD(raw): Not using SIGIO\n");
    add_to_io_select(kbd_fd, 0, keyb_client_run);
  }
   
  return TRUE;
}

/*
 * DANG_BEGIN_FUNCTION raw_keyboard_reset
 * 
 * Reset the keyboard shiftstate to match the keyboard LED's
 * 
 * DANG_END_FUNCTION
 */
static void raw_keyboard_reset(void)
{
  /* initialise the server's shift state to the current keyboard state */ 
  set_shiftstate(get_kbd_flags());
	
}

/* something like this oughta be defined in linux/kd.h but isn't... */
/* reset LEDs to normal mode, reflecting keyboard state */
#define LED_NORMAL 0x08

static void raw_keyboard_close(void)
{
  if (kbd_fd != -1) {
    k_printf("KBD(raw): raw_keyboard_close: resetting keyboard to original mode\n");
    ioctl(kbd_fd, KDSKBMODE, save_mode);

    k_printf("KBD(raw): resetting LEDs to normal mode\n");
    ioctl(kbd_fd, KDSETLED, LED_NORMAL);
   
    k_printf("KBD(raw): Resetting TERMIOS structure.\n");
    if (tcsetattr(kbd_fd, TCSAFLUSH, &save_termios) < 0) {
      k_printf("KBD(raw): Resetting keyboard termios failed.\n");
    }
    fcntl(kbd_fd, F_SETFL, save_kbd_flags);

    kbd_fd = -1;
  }
}

static int raw_keyboard_probe(void)
{
	int result = FALSE;
	if (config.console_keyb) {
		result = TRUE;
	}
	return result;
}

struct keyboard_client Keyboard_raw =  {
   "raw",                      /* name */
   raw_keyboard_probe,	       /* probe */
   raw_keyboard_init,          /* init */
   raw_keyboard_reset,         /* reset */
   raw_keyboard_close,         /* close */
   do_raw_getkeys,             /* run */
   set_kbd_leds,       	       /* set_leds */
};

