/* dos emulator, Matthias Lautner */
#ifndef TERMIO_H
#define TERMIO_H
/* Extensions by Robert Sanders, 1992-93
 *
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

#include "extern.h"

#ifndef NEW_KBD_CODE

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
#define EKF_SYSRQ	10
#define EKF_PAUSE	11
#define EKF_SCRLOCK	12	/* ScrLock PRESSED */
#define EKF_NUMLOCK	13	/* NumLock PRESSED */
#define EKF_CAPSLOCK	14	/* CapsLock PRESSED */
#define EKF_INSERT	15	/* SysRq PRESSED */


/*  dead keys for accents */
#define DEAD_GRAVE         1
#define DEAD_ACUTE         2
#define DEAD_CIRCUMFLEX    3
#define DEAD_TILDE         4
#define DEAD_BREVE         5
#define DEAD_ABOVEDOT      6
#define DEAD_DIAERESIS     7
#define DEAD_ABOVERING     8
#define DEAD_DOUBLEACUTE   10
#define DEAD_CEDILLA       11
#define DEAD_IOTA          12



struct dos_dead_key {
   unsigned char d_key;
   unsigned char in_key;
   unsigned char out_key;
};

extern struct dos_dead_key dos850_dead_map[];
extern unsigned char dead_key_table[];

/* KEY FLAGS */
#define KKF_E1		0
#define KKF_E0		1
#define KKF_RCTRL	2
#define KKF_RALT	3
#define KKF_KBD102	4	/* set if 102-key keyboard installed */
#define KKF_FORCENUM	5
#define KKF_FIRSTID	6
#define KKF_READID	7
#define KKF_SCRLOCK	8
#define KKF_NUMLOCK	9
#define KKF_CAPSLOCK	10

/* LED FLAGS (from Linux keyboard code) */
#define LED_SCRLOCK	0
#define LED_NUMLOCK	1
#define LED_CAPSLOCK	2

#if 0
#define key_flags *(KEYFLAG_ADDR)
#define kbd_flags *(KBDFLAG_ADDR)
#endif
#define key_flags READ_WORD(KEYFLAG_ADDR)
#define kbd_flags READ_WORD(KBDFLAG_ADDR)

#define KBBUF_SIZE	80	/* dosemu read buffer for keyboard */

extern void set_leds(void);

/* int15 fn=4f will clear CF if scan code should not be used,
   I set keepkey to reflect CF */
EXTERN u_char keepkey INIT(1);

EXTERN void insert_into_keybuffer(void);
EXTERN void set_keyboard_bios(void);
EXTERN void do_irq1(void);
EXTERN int keyboard_init(void);
EXTERN void add_scancode_to_queue (u_short scan);
EXTERN void child_set_flags(int sc);
EXTERN int kbd_flag (int flag);


#endif /* not NEW_KBD_CODE */

extern void set_screen_origin(int), set_vc_screen_page(int);

extern int vc_active(void);

struct screen_stat {
  int console_no,		/* our console number */
   vt_allow,			/* whether to allow VC switches */
   vt_requested;		/* whether one was requested in forbidden state */

  int current;			/* boolean: is our VC current? */

  int curadd;			/* row*80 + col */
  int dcurgeom;			/* msb: start, lsb: end */
  int lcurgeom;			/* msb: start, lsb: end */

  int mapped,			/* whether currently mapped */
   pageno;			/* current mapped text page # */

  int dorigin;			/* origin in DOS */
  int lorigin;

  caddr_t virt_address;		/* current map address in DOS memory */
  char *phys_address;		/* current map address in Linux memory */

  int old_modecr, new_modecr;
};

#endif /* TERMIO_H */
