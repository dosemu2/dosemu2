/*
 * (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * This is a complete software emulation of the Game Port and BIOS joystick
 * routines for DOSEMU.
 *
 * If you have set up the joystick in Linux, everything should work like it
 * does in real DOS ie. you can have two joysticks (untested) or one joystick,
 * in which this code will do the appropriate (and correct) axis and button
 * mapping or of course, no joysticks, which is also supported :).
 *
 * maintainer:
 *  Clarence Dang <dang@kde.org>
 *
 * DANG_END_MODULE
 *
 * Copyright (c) 2002 Clarence Dang <dang@kde.org>
 * (not to be confused with DANG, which is the Dosemu Alterer Novices Guide :)
 *
 * Last Modified: 8 Jun 2002
 */


/* -------------------------------------------------------------------------------- */
/* --------------- END these things do not belong in joystick.c! ---------------- */
/* -------------------------------------------------------------------------------- */


/*
 * Compile-time Debugging Options
 *
 * Specify what debugging code you want to compile in (recommended: all of them).
 * You will still need to activate them at runtime (-Dj) to have any visible effect.
 */

/* DEFINE this if you want initialisation messages, errors and warnings */
#define JOY_INIT_DEBUG

/* DEFINE this if you want to debug the joystick port emulation code */
#define JOY_PORT_DEBUG

/* DEFINE this if you want to debug the BIOS joystick emulation code */
#define JOY_BIOS_DEBUG

/* DEFINE this if you want to debug the Linux joystick-interfacing code */
#define JOY_LINUX_DEBUG


/*
 * Runtime Debugging (-D[#]j)
 *
 * 1 = init messages
 * 2 = AND port + bios emulation
 * 3 = AND linux interfacing
 *
 */
#define joy_init_printf(f,a...)	j_printf ("JOY: " f,##a)

#define joy_port_printf(f,a...)	if (debug_level ('j') >= 2) j_printf ("JOY: " f,##a)
#define joy_bios_printf(f,a...)	if (debug_level ('j') >= 2) j_printf ("JOY: BIOS: " f,##a)

#define joy_linux_printf(f,a...)	if (debug_level ('j') >= 3) j_printf ("JOY: linux: " f,##a)


/*
 * Includes
 */

#include "config.h"
#include "joystick.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef USE_PTHREADS
	#include <pthread.h>
#endif
						 
#include "Linux/joystick.h"

#include "cpu.h"
#include "emu.h"
#include "iodev.h"

/* from linux/version.h */
#define JOY_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))


/*
 * Internal constants
 */

/* emulated joystick number */
#define JOY_0	0
#define JOY_1	1

/* joystick axes */
#define JOY_X	0
#define JOY_Y	1

#define JOY_AXIS_INVALID	(-32800)


/*
 * Internal variables
 */

/* joystick device */
static int joy_fd [2];
static int joy_status = -1;
static int joy_version = -1;

static int joy_linux_min, joy_linux_max, joy_linux_range;

/* joystick stats */
static char joy_numaxes [2], joy_numbuttons [2];

/* current state of joystick */
static int joy_buttons;
static int joy_axis [2][4];

/* axis counters for joystick port */
static int joy_port_x [2], joy_port_y [2];

/* axis range for joystick */
static int joy_dos_range;

#ifdef USE_PTHREADS
	static pthread_t thread [2];
	static pthread_mutex_t joy_buttons_mutex, joy_axis_mutex;
	static int joy [2] = {JOY_0, JOY_1};
	static void *joy_linux_thread_read (void *injoynum);
#endif


/*
 * Some function prototypes
 */

/* internal interface functions: Linux->DOS */
static void joy_emu_button_set (const int joynum, const int button, const int pressed);
static void joy_emu_axis_set (const int joynum, const int axis, const int value);
static int joy_emu_axis_conv (const int linux_val, const int invalid_val);

/* port emulation */
Bit8u joy_port_inb (ioport_t port);
Bit16u joy_port_inw (ioport_t port);
void joy_port_outb (ioport_t port, Bit8u value);
void joy_port_outw (ioport_t port, Bit16u value);


/*
 * DANG_BEGIN_REMARK
 *
 * Linux joystick readers:
 *
 * We make a runtime decision based on the detected joystick API version (and #ifdef USE_PTHREADS)
 * on which reader to use.  We can select from the following readers:
 *
 * 1. joy_reader_nojoy: simply tells DOS programs that you have no joystick
 * 2. joy_reader_old: uses old, non-blocking joystick API (<1.0.0); limited to 2 axes
 * 3. joy_reader_new: uses new, non-blocking joystick API (>=1.0.0); a (little) slower than joy_reader_new_threaded
 * 4. joy_reader_new_threaded: uses new, BLOCKING joystick API (>=1.0.0); efficient but requires pthreads
 *
 * DANG_END_REMARK
 */

/*
 * Function prototypes for reader funcs
 */

/* no joystick */
static int joy_linux_read_buttons_nojoy (void);
static int joy_linux_read_axis_nojoy (const int joynum, const int axis, const int invalid_val);

/* old API */
static int joy_linux_read_buttons_old (void);
static int joy_linux_read_axis_old (const int joynum, const int axis, const int invalid_val);

/* new API */
static int joy_linux_read_buttons_new (void);
static int joy_linux_read_axis_new (const int joynum, const int axis, const int invalid_val);

/* new API + threading */
#ifdef USE_PTHREADS
static int joy_linux_read_buttons_new_threaded (void);
static int joy_linux_read_axis_new_threaded (const int joynum, const int axis, const int invalid_val);
#endif

/*
 * Reader structures
 */

typedef struct
{
	int (*read_buttons) (void);
	int (*read_axis) (const int joynum, const int axis, const int invalid_val);
} JOY_READER;

JOY_READER joy_reader_nojoy = {joy_linux_read_buttons_nojoy, joy_linux_read_axis_nojoy};
JOY_READER joy_reader_old = {joy_linux_read_buttons_old, joy_linux_read_axis_old};
JOY_READER joy_reader_new = {joy_linux_read_buttons_new, joy_linux_read_axis_new};
#ifdef USE_PTHREADS
JOY_READER joy_reader_new_threaded = {joy_linux_read_buttons_new_threaded, joy_linux_read_axis_new_threaded};
#endif

JOY_READER *joy_reader = &joy_reader_nojoy;	/* current reader */
int joy_reader_decided;


/*
 * Init functions
 */

/*
 * DANG_BEGIN_FUNCTION joy_reader_set
 *
 * Assigns the Linux joystick reader to the one specified.
 * Sets the Linux axis ranges based on what joystick API is being used.
 *
 * DANG_END_FUNCTION
 */
void joy_reader_set (JOY_READER *reader)
{
	joy_reader = reader;

	/* old joystick API (<1.0.0) */
	if (reader == &joy_reader_old)
	{
		joy_linux_min = +1;
		joy_linux_max = +255;
	}
	/* new joystick API */
#ifdef USE_PTHREADS
	else if (reader == &joy_reader_new || reader == &joy_reader_new_threaded)
#else
	else if (reader == &joy_reader_new)
#endif
	{
		joy_linux_min = -32767;
		joy_linux_max = +32767;
	}

	joy_linux_range = joy_linux_max - joy_linux_min + 1;

	joy_reader_decided = 1;
}

void joy_init (void)
{
	emu_iodev_t io_device;
	int joynum;

#ifdef JOY_INIT_DEBUG
	#ifdef USE_PTHREADS
		joy_init_printf ("joy_init() [thread-able] CALLED!\n");
	#else
		joy_init_printf ("joy_init() [unthreaded] CALLED!\n");
	#endif
#endif

	joy_reader_decided = 0;

	/* check joystick axis range */
	if (config.joy_dos_min <= 0 || config.joy_dos_max <= config.joy_dos_min || config.joy_dos_max > 1000)
	{
	#ifdef JOY_INIT_DEBUG
		joy_init_printf ("WARNING! joystick axis min (%i) and/or max (%i) out of range\n",
								config.joy_dos_min, config.joy_dos_max);
		joy_init_printf ("--> setting joystick range to hard-coded default 3-150\n");
	#endif
		config.joy_dos_min = 3;
		config.joy_dos_max = 150;
	}
	joy_dos_range = config.joy_dos_max - config.joy_dos_min + 1;

	/* check joystick granularity */
	if (config.joy_granularity <= 0 || config.joy_granularity > joy_dos_range)
	{
	#ifdef JOY_INIT_DEBUG
		joy_init_printf ("WARNING! joystick granularity (%i) out of range\n", config.joy_granularity);
		joy_init_printf ("--> disabling granularity setting\n");
	#endif
		config.joy_granularity = 1;
	}

#ifdef JOY_INIT_DEBUG
	joy_init_printf ("joystick range: %i-%i (%i)\n",
						 	config.joy_dos_min, config.joy_dos_max, joy_dos_range);
	joy_init_printf ("joystick granularity: %i\n", config.joy_granularity);
#endif

/* it doesn't matter if we init them here but never end up using them... */
#ifdef USE_PTHREADS
	pthread_mutex_init (&joy_buttons_mutex, NULL);
	pthread_mutex_init (&joy_axis_mutex, NULL);
#endif

	/* initial state of buttons (all off) -- 0x0F is _not_ a typo! */
	joy_buttons = 0x0F;

	/* globals had better be initialised before use... */
	for (joynum = 0; joynum < 2; joynum++)
	{
		int axis;

		/* set to initial values */
		for (axis = 0; axis < 4; axis++)
			joy_axis [joynum][axis] = JOY_AXIS_INVALID;

		joy_port_x [joynum] = joy_port_y [joynum] = 0;
		joy_fd [joynum] = -1;

		joy_numbuttons [joynum] = 0;
		joy_numaxes [joynum] = 0;
	}

	/* init 2 joystick devices */
	for (joynum = 0; joynum < 2; joynum++)
	{
		/* does the user even want this joystick? */
		if (strlen (config.joy_device [joynum]) == 0)
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("joystick 0x%X: will not be initialised\n", joynum);
		#endif
			continue;	/* I wonder if a real PC would treat the 2nd joystick as the
								the 2nd joystick if the 1st failed to init... */
		}

		/* open joystick device */
	#ifdef USE_PTHREADS
		/*
		 * 1. old joy API (<1.0.0): only supports NONBLOCK no matter what
		 * 2. new joy API         : we default to BLOCKING
		 */
		joy_fd [joynum] = open (config.joy_device [joynum], O_RDONLY);
	#else
		/*
		 * 1. old joy API (<1.0.0): only supports NONBLOCK no matter what
		 * 2. new joy API         : we default to NONBLOCKING (no pthreads)
		 */
		joy_fd [joynum] = open (config.joy_device [joynum], O_RDONLY | O_NONBLOCK);
	#endif

		if (joy_fd [joynum] >= 0)
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("joystick 0x%X: opened \"%s\"\n", joynum, config.joy_device [joynum]);
		#endif
		}
		else
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("joystick 0x%X: ERROR! can't open \"%s\"\n", joynum, config.joy_device [joynum]);
		#endif
			continue;	/* try to open next joystick */
		}

		/*
		 * NOTE: this assumes that both joystick devices are using the same API
		 *       -- if they are not, then you probably deserve the code
		 *          malfunctioning :)
		 */
		if (!joy_reader_decided)	/* set by joy_reader_set() */
		{
			if (ioctl (joy_fd [joynum], JSIOCGVERSION, &joy_version))
			{
				/* the driver is not 1.0.0+ */
			#ifdef JOY_INIT_DEBUG
				joy_init_printf ("joystick 0x%X: WARNING! ioctl (version) failed, assuming old joystick API (< 1.0.0)\n",
										joynum);
			#endif

				joy_version = JOY_VERSION (0,0,0);	/* < 1.0.0 :) */
			}
		#ifdef JOY_INIT_DEBUG
			else
				joy_init_printf ("joystick 0x%X: API Version Detected: 0x%06X\n", joynum, joy_version);
		#endif
			
			if (joy_version < JOY_VERSION (1,0,0))
			{
			#ifdef JOY_INIT_DEBUG
				joy_init_printf ("joystick 0x%X: Using OLD joystick API\n", joynum);
			#endif
				joy_reader_set (&joy_reader_old);
			}
			else
			{
			#ifdef USE_PTHREADS
				#ifdef JOY_INIT_DEBUG
					joy_init_printf ("joystick 0x%X: Using NEW joystick API (threaded)\n", joynum);
				#endif
				joy_reader_set (&joy_reader_new_threaded);
			#endif

				if (!joy_reader_decided)
				{
				#ifdef JOY_INIT_DEBUG
					joy_init_printf ("joystick 0x%X: Using NEW joystick API (unthreaded)\n", joynum);
				#endif
					joy_reader_set (&joy_reader_new);
				}
			}
		}	/* if (!joy_reader_decided)	{ */

		/* get num axis */
		if (ioctl (joy_fd [joynum], JSIOCGAXES, &joy_numaxes [joynum]))
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("joystick 0x%X: ERROR! ioctl (num axes) failed, assuming 2\n", joynum);
		#endif
			joy_numaxes [joynum] = 2;	/* assume 2 axes */
		}

		/* get num buttons */
		if (ioctl (joy_fd [joynum], JSIOCGBUTTONS, &joy_numbuttons [joynum]))
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("joystick 0x%X: ERROR! ioctl (num buttons) failed, assuming 2\n", joynum);
		#endif
			joy_numbuttons [joynum] = 2;	/* assume 2 buttons */
		}

	#ifdef JOY_INIT_DEBUG
		/* print stats */
		joy_init_printf ("joystick 0x%X: %i axes %i buttons\n",
								joynum, joy_numaxes [joynum], joy_numbuttons [joynum]);
	#endif

	#ifdef USE_PTHREADS
		if (joy_reader == &joy_reader_new_threaded)
		{
			/* start thread to read joystick events */
			if (pthread_create (&thread [joynum], NULL, joy_linux_thread_read, &joy [joynum]))
			{
			#ifdef JOY_INIT_DEBUG
				joy_init_printf ("joystick 0x%X: ERROR! Cannot create thread!  Falling back to unthreaded code!\n", joynum);
			#endif

				if (fcntl (joy_fd [joynum], F_SETFL, O_NONBLOCK))
				{
				#ifdef JOY_INIT_DEBUG
					joy_init_printf ("joystick 0x%X: ERROR! Cannot switch to NONBLOCKING mode (%i: %s)! "
											"Falling back to OLD joystick API which didn't care about blocking.\n",
											joynum, errno, strerror (errno));
				#endif

					/* if this is the 2nd joystick, cancel the 1st's thread */
					if (joynum == JOY_1)
						if (joy_fd [JOY_0] >= 0)
							pthread_cancel (thread [JOY_0]);

					joy_reader_set (&joy_reader_old);
				}
				else
					joy_reader_set (&joy_reader_new);
			}
		}
	#endif	/* USE_PTHREADS */
	}

	/* handle joystick port/game card routines */
	/* DANG_FIXTHIS does this code work for ports other than 0x201? */
	io_device.read_portb   = joy_port_inb;
	io_device.write_portb  = joy_port_outb;
	io_device.read_portw   = joy_port_inw;
	io_device.write_portw  = joy_port_outw;
	io_device.read_portd   = NULL;
	io_device.write_portd  = NULL;
	io_device.handler_name = "Joystick Port Emulation";
	io_device.start_addr   = 0x200;
	io_device.end_addr     = 0x20F;
	io_device.irq          = EMU_NO_IRQ;
	io_device.fd           = -1;

	/*
	 * DANG_BEGIN_REMARK
	 *
	 * Register the joystick ports so that correct port values are returned
	 * for programs that try to detect the joystick (or the lack of one).
	 *
	 * DANG_END_REMARK
	 */
	if (port_register_handler (io_device, 0) != 0)
	{
	#ifdef JOY_INIT_DEBUG
		joy_init_printf ("ERROR! Couldn't register joystick port handler!\n");
	#endif
	}

	/* no joysticks at all! */
	if (joy_fd [JOY_0] < 0 && joy_fd [JOY_1] < 0)
	{
	#ifdef JOY_INIT_DEBUG
		joy_init_printf ("WARNING! No joysticks enabled!\n");
	#endif
		joy_status = 0;
	}
	else
		joy_status = 1;
}

void joy_uninit (void)
{
	int joynum;
#ifdef JOY_INIT_DEBUG
	joy_init_printf ("joy_uninit() CALLED!\n");
#endif

	joy_status = 0;

	for (joynum = 0; joynum < 2; joynum++)
	{
	#ifdef USE_PTHREADS
		if (joy_reader == &joy_reader_new_threaded)
			pthread_cancel (thread [joynum]);
	#endif

		if (joy_fd [joynum] >= 0)
			close (joy_fd [joynum]);
	}

/* these had been initialised _no matter what_ so they can be safely destroyed */
#ifdef USE_PTHREADS
	pthread_mutex_destroy (&joy_axis_mutex);
	pthread_mutex_destroy (&joy_buttons_mutex);
#endif
}

void joy_term (void)
{
	joy_uninit ();
}

void joy_reset (void)
{
#ifdef JOY_INIT_DEBUG
	joy_init_printf ("joy_reset() CALLED!\n");
#endif

/* DANG_FIXTHIS joy_reset() is called immediately after joy_init(), which is rather inconvenient (anyone heard of a port_unregister_handler()?) so we don't bother resetting at all but in the future this could cause problems */

#if 0
	joy_uninit ();
	joy_init ();
#endif
}

/* called by src/base/init/init.c */
int joy_exist (void)
{
#ifdef JOY_INIT_DEBUG
	if (joy_status == -1)
		joy_init_printf ("WARNING! Call to joy_exist() before joy_init()\n");

	joy_init_printf ("joy_exist() returning value %i\n", joy_status);
#endif

	/* NOTE: if joy_init() hasn't been called, joy_status == -1,
	 *       but therefore we still claim that a joystick exists
	 */
	return joy_status;
}


/*
 * Emulation helper functions (DOS->Linux)
 */

/*
 * DANG_BEGIN_FUNCTION joy_emu_button_set
 *
 * Update the button status for each joystick.
 *
 * We must perform "button mapping" if only the first joystick is enabled
 * ie. we are required to map the "excessive" buttons (>2) from the first
 * joystick onto the second:
 *	-- 3rd button of 1st joy --> 1st button of 2nd joy
 *	-- 4th button of 1st joy --> 2nd button of 2nd joy
 *
 * DANG_END_FUNCTION
 */
static void joy_emu_button_set (const int joynum, const int button, const int pressed)
{
	int bnum;

	/*
	 * DANG_BEGIN_REMARK
	 *
	 * if the 2nd joystick is enabled, we ignore any button >= 2 regardless
	 * of which joystick it is (if it's the 1st, the 2nd joystick would
	 * overwrite its buttons; if it's the 2nd, it would be out of range)
	 *
	 * DANG_END_REMARK
	 */
	if (joy_fd [JOY_1] >= 0 && button >= 2)
		return;

	/* figure out button number in DOS */
	bnum = joynum * 2 + button;

	/* button in range? */
	if (bnum < 4)
	{
	#ifdef USE_PTHREADS
		pthread_mutex_lock (&joy_buttons_mutex);
	#endif

		/* NOTE: this seems reversed but is what happens in real DOS! */
		if (pressed)
			joy_buttons &= ~(1 << bnum);	/* clear bit */
		else
			joy_buttons |= (1 << bnum);		/* set bit */

	#ifdef USE_PTHREADS
		pthread_mutex_unlock (&joy_buttons_mutex);
	#endif
	}
}

/*
 * DANG_BEGIN_FUNCTION joy_emu_axis_set
 *
 * Update the axis status for each joystick.
 *
 * We must perform "axis mapping" if only the first joystick is enabled
 * ie. we are required to map the "excessive" axes (>2) from the first
 * joystick onto the second:
 *	-- 3rd axis of 1st joy --> 2st axis of 2nd joy
 *	-- 4th axis of 1st joy --> 1st axis of 2nd joy
 *	   (yes, these are reversed deliberately because it's what happens in DOS)
 * DANG_END_FUNCTION
 */

/* DANG_FIXTHIS I've lost my joystick Y-cable (lets you connect two joysticks to one gameport) so the code to handle two joysticks is totally untested! */

static void joy_emu_axis_set (const int joynum, const int axis, const int value)
{
	int anum = axis;
	int jnum = joynum;

	/* 0 <= axis number < 4 */
	if (anum >= 4) return;

	/* need to perform axis mapping? */
	if (anum >= 2)
	{
		/*
		 * DANG_BEGIN_REMARK
		 *
		 * if the 2nd joystick is enabled, we ignore any axis >= 2 regardless
		 * of which joystick it is (if it's the 1st, the 2nd joystick would
		 * overwrite its axes; if it's the 2nd, it would be out of range)
		 *
		 * DANG_END_REMARK
		 */
		if (joy_fd [JOY_1] >= 0) return;

		jnum++;

		/* 0 <= virtual joystick number < 2 */
		if (jnum >= 2) return;

		/* basically swap axis 2 and 3 around (happens in real DOS)
		 * and set it to an axis in range (0 <= a < 2) */
		anum = 3 - anum;
	}

#ifdef USE_PTHREADS
	pthread_mutex_lock (&joy_axis_mutex);
#endif

	joy_axis [jnum][anum] = value;

#ifdef USE_PTHREADS
	pthread_mutex_unlock (&joy_axis_mutex);
#endif
}

/*
 * DANG_BEGIN_REMARK
 *
 * Here we set the range of possible axis readings for Linux.
 * You should _not_ change these values unless you know what you are doing.
 *
 * They are later used in joy_emu_axis_conv() as part of a calculation to
 * compress axis readings to an acceptable range for DOS programs
 * (usually somewhere between 5 and 150).
 *
 * DANG_END_REMARK
 */

/*
 * DANG_BEGIN_FUNCTION joy_emu_axis_conv
 *
 * Convert a Linux joystick reading to a DOS one by making use of the
 * differences in the allowable ranges of joystick readings.
 *
 * NOTE: I don't know whether or not Linux returns exponential readings for
 * the joystick but (apparently) DOS programs expect the reading to be
 * exponential and so if this is to be fixed, it should probably be done in
 * this function.
 *
 * DANG_END_FUNCTION
 */
#define joy_axis_round(num,gran) ((num) - ((num) % (gran)))
static int joy_emu_axis_conv (const int linux_val, const int invalid_val)
{
	/* virtual joystick axis doesn't exist? */
	if (linux_val == JOY_AXIS_INVALID)
		return invalid_val;

	if (joy_version >= JOY_VERSION (1,2,8)
	#ifdef USE_PTHREADS
		|| joy_reader == &joy_reader_new || joy_reader == &joy_reader_new_threaded)
	#else
		|| joy_reader == &joy_reader_new)
	#endif
	{
          int ret = (linux_val - joy_linux_min) * joy_dos_range / joy_linux_range;
		return joy_axis_round (ret, config.joy_granularity) + config.joy_dos_min;
	}
	/* joystick driver <1.2.8 doesn't have a defined range so we can't do an axis conversion */
	else
		return joy_axis_round (linux_val, config.joy_granularity);
}


/*
 * Linux joystick reading functions
 */

/*
 * DANG_BEGIN_FUNCTION joy_linux_process_event
 *
 * Update global joystick status variables given a Linux joystick event.
 * 
 * DANG_END_FUNCTION
 */
static void joy_linux_process_event (const int joynum, const struct js_event *event)
{
	if (event->type & JS_EVENT_INIT)
	{
	#ifdef JOY_LINUX_DEBUG
		joy_linux_printf ("joy 0x%X: init\n", joynum);
	#endif
		/* hmm, what do I do with this? */
	}

	if (event->type & JS_EVENT_BUTTON)
	{
	#ifdef JOY_LINUX_DEBUG
		joy_linux_printf ("joy 0x%X: button %i: value %i\n",
								joynum, event->number, event->value);
	#endif
		joy_emu_button_set (joynum, event->number, event->value);
	#ifdef JOY_LINUX_DEBUG
		joy_linux_printf ("joy 0x%X: button: status %i\n",
								joynum, joy_buttons);
	#endif
	}

	if (event->type & JS_EVENT_AXIS)
	{
	#ifdef JOY_LINUX_DEBUG
		joy_linux_printf ("joy 0x%X: axis %i: value %i\n",
								joynum, event->number, event->value);
	#endif
		joy_emu_axis_set (joynum, event->number, event->value);
	#ifdef JOY_LINUX_DEBUG
		joy_linux_printf ("joy 0x%X: axis: status %i\n",
								joynum, (event->number >= 4) ? -1234 : joy_axis [joynum][event->number]);
	#endif
	}

#ifdef JOY_LINUX_DEBUG
	joy_linux_printf ("joy 0x%X: %i msec\n", joynum, event->time);
#endif
}

/* the thread (if using joy_reader_new_threaded) */
#ifdef USE_PTHREADS
static void *joy_linux_thread_read (void *injoynum)
{
	int numread;
	struct js_event event;
	int joynum = *((int *) injoynum);
	
	for (;;)
	{
		/* blocking read of joystick */
		numread = read (joy_fd [joynum], &event, 1 * sizeof (struct js_event));
		if (numread <= 0)
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("ERROR! Joystick 0x%X read failed unexpectedly (read: %i; %s)!\n",
									joynum, numread, strerror (errno));
		#endif
			/* we should _NEVER_ get here but if we do, we would rather quit
			 * (and lose joystick capability), than possibly suck all of the
			 * user's CPU... */
			return NULL;
		}

		joy_linux_process_event (joynum, &event);
	}
}
#endif	/* USE_PTHREADS */

/*
 * DANG_BEGIN_FUNCTION joy_linux_read_events
 *
 * Process the event queue for _both_ linux joysticks using nonblocking reads
 * with the new joystick API (joy_reader_new).
 * 
 * This must be done before any joystick status is given to DOS as all events
 * are queued until they are processed and we want to return the current
 * state of the joystick -- not what it was some time ago.  _Both_ joysticks
 * are processed here because of axis/button mapping affecting the status of
 * the emulated joysticks (what DOS sees).
 *
 * DANG_END_FUNCTION
 */
static void joy_linux_read_events (void)
{
	int joynum;

	for (joynum = 0; joynum < 2; joynum++)
	{
		int e;
		int numread;

		const int maxread = 255;
		struct js_event event [255];

		if (joy_fd [joynum] < 0) continue;

		numread = read (joy_fd [joynum], event, maxread * sizeof (struct js_event));

		/* nothing read but the queue is not empty? */
		if (numread <= 0 && errno != EAGAIN)
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("ERROR! Joystick 0x%X read failed unexpectedly (read: %i; %s)!\n",
									joynum, numread, strerror (errno));
		#endif
			continue;	/* go on to next joystick */
		}

		/* for some reason, sizeof() returns an unsigned... */
		numread /= (int) sizeof (struct js_event);
		if (numread > maxread)
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("ERROR! Joystick 0x%X read too much (read: %i; %s)!\n",
									joynum, numread, strerror (errno));
		#endif
			continue;	/* go on to next joystick */
		}

		#ifdef JOY_LINUX_DEBUG
			joy_linux_printf ("joystick 0x%X: Processing %i event(s)\n", joynum, numread);
		#endif

		for (e = 0; e < numread; e++)
			joy_linux_process_event (joynum, &event [e]);
	}
}

/*
 * DANG_BEGIN_FUNCTION joy_linux_read_buttons_(family)
 *
 * Eventually called from DOS to get the button status of joysticks.
 * The threaded version will simply get the readings from global variables.
 * The unthreaded versions will perform non-blocking reads using the old
 * joystick API.
 *
 * DANG_END_FUNCTION
 */

static int joy_linux_read_buttons_nojoy (void)
{
	return 0x0F;
}

static int joy_linux_read_buttons_old (void)
{
	int joynum;

	for (joynum = 0; joynum < 2; joynum++)
	{
		struct JS_DATA_TYPE js;
		int b;

		if (joy_fd [joynum] < 0) continue;

		/* non-blocking read */
		if (read (joy_fd [joynum], &js, JS_RETURN) != JS_RETURN)
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("joystick 0x%X: ERROR! joy_read_buttons(): %s\n",
									joynum, strerror (errno));
		#endif
			continue;
		}

		/* perform button mapping... */
		for (b = 0; b < 4; b++)
			joy_emu_button_set (joynum, b, js.buttons & (1 << b));
	}

	/* ...and return the virtual button reading */
	return joy_buttons;
}

static int joy_linux_read_buttons_new (void)
{
	/* process all queued Linux joystick events */
	joy_linux_read_events ();

	/* ... and return the virtual button reading */
	return joy_buttons;
}

#ifdef USE_PTHREADS
static int joy_linux_read_buttons_new_threaded (void)
{
	int ret;

	pthread_mutex_lock (&joy_buttons_mutex);
	ret = joy_buttons;
	pthread_mutex_unlock (&joy_buttons_mutex);

	return ret;
}
#endif

/*
 * DANG_BEGIN_FUNCTION joy_linux_read_axis_(family)
 *
 * Eventually called from DOS to get the axis status of joysticks.
 * The threaded version will simply get the readings from global variables.
 * The unthreaded versions will perform non-blocking reads using the old
 * joystick API.
 *
 * The @param invalid_val is the value to return to signify a non-existent
 * axis.
 *
 * DANG_END_FUNCTION
 */

static int joy_linux_read_axis_nojoy (const int joynum, const int axis, const int invalid_val)
{
	return invalid_val;	/* I don't have a joystick... */
}

static int joy_linux_read_axis_old (const int joynum, const int axis, const int invalid_val)
{
	int jnum;

	for (jnum = 0; jnum < 2; jnum++)
	{
		struct JS_DATA_TYPE js;

		if (joy_fd [jnum] < 0) continue;

		/* non-blocking read */
		if (read (joy_fd [jnum], &js, JS_RETURN) != JS_RETURN)
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("joystick 0x%X: ERROR! joy_read_axis(): %s\n",
									jnum, strerror (errno));
		#endif
			continue;
		}

		/* perform axis mapping... */
		joy_emu_axis_set (jnum, JOY_X, js.x);
		joy_emu_axis_set (jnum, JOY_Y, js.y);
	}

	/* ...and return the virtual axis reading */
	return (joy_emu_axis_conv (joy_axis [joynum][axis], invalid_val));
}

static int joy_linux_read_axis_new (const int joynum, const int axis, const int invalid_val)
{
	/* process all queued Linux joystick events */
	joy_linux_read_events ();

	/* ... and return the virtual axis reading */
	return (joy_emu_axis_conv (joy_axis [joynum][axis], invalid_val));
}

#ifdef USE_PTHREADS
static int joy_linux_read_axis_new_threaded (const int joynum, const int axis, const int invalid_val)
{
	int ret;

	pthread_mutex_lock (&joy_axis_mutex);
	ret = joy_axis [joynum][axis];
	pthread_mutex_unlock (&joy_axis_mutex);

	return (joy_emu_axis_conv (ret, invalid_val));
}
#endif

/*
 * Functions that DOSEMU/DOS call
 */

/*
 * DANG_BEGIN_REMARK
 *
 * Apparently, the Carry Flag is cleared if int15 function 0x84 exists
 * and it is set if it doesn't exist.
 *
 * But what does this mean?  Does the existence of such a BIOS function
 * depend on the existence of a Game Card/SoundBlaster, or does it just
 * mean that there is such an implemented BIOS function, regardless of
 * whether or not you have a joystick?
 *
 * I have never seen a real BIOS set the Carry Flag on this call, even
 * on a computer without a joystick -- so to mimick what happens in the real
 * world, I just clear the Carry Flag regardless of whether the user has a
 * joystick or not.  This could be incorrect behaviour so it may have to be
 * changed in the future.
 *
 * DANG_END_REMARK
 */

#if 0
	#define JOY_BIOS_API_YES  NOCARRY
	#define JOY_BIOS_API_NO   CARRY
#else
	#define JOY_BIOS_API_YES  NOCARRY
	#define JOY_BIOS_API_NO   NOCARRY
#endif

/*
 * DANG_BEGIN_FUNCTION joy_bios_read
 *
 * This is the int15 function 0x84 handler (ie. BIOS joystick emulation),
 * called from src/base/async/int.c.
 *
 * The real BIOS actually reads its values straight from the joystick port
 * (0x201) but we don't bother because we can read it from globals faster :)
 *
 * Because of this, it returns the joystick axis readings with the same range
 * as port 0x201 BUT for some reason, a real BIOS has a different range of
 * joystick readings than direct port access (anyone know why?).  No
 * programs seem to expect a certain range of values except for Alley Cat
 * which seems to want a range of about 1-50 but only from port 0x201.
 *
 * DANG_END_FUNCTION
 */

int joy_bios_read (void)
{
	switch (LWORD (edx))
	{
	/* read buttons */
	case 0:
	{
	#ifdef JOY_BIOS_DEBUG
		joy_bios_printf ("read buttons\n");
	#endif

		/*
		 * NOTE: this shift ensures that the bottom nibble is all '0' bits
		 *       so that the game, Earth Invasion, won't hang on startup
		 */
		LO(ax) = (joy_reader->read_buttons () << 4);

		JOY_BIOS_API_YES;
		break;
	}
	/* read axis */
	case 1:
	{
	#ifdef JOY_BIOS_DEBUG
		joy_bios_printf ("read axis\n");
	#endif

		LWORD(eax) = joy_reader->read_axis (JOY_0, JOY_X, 0);	/* 1st joystick x */
		LWORD(ebx) = joy_reader->read_axis (JOY_0, JOY_Y, 0);	/* 1st joystick y */
		LWORD(ecx) = joy_reader->read_axis (JOY_1, JOY_X, 0);	/* 2nd joystick x */
		LWORD(edx) = joy_reader->read_axis (JOY_1, JOY_Y, 0);	/* 2nd joystick y */

		JOY_BIOS_API_YES;
		break;
  }
	/* unknown call */
	default:
	#ifdef JOY_INIT_DEBUG
		/* DANG_FIX_THIS perhaps we should not have been called to start with? */
		joy_init_printf ("BIOS: ERROR! unknown joystick call %X\n", LWORD(edx));
	#endif

		JOY_BIOS_API_NO;
		return 1;
	}

	return 0;
}

/*
 * DANG_BEGIN_FUNCTION joy_port_inb
 *
 * This function emulates reads from the joystick port (0x201) -- this is the
 * most common way of detecting and reading the joystick.
 *
 * The real implementation of this port actually times out and resets the
 * equivalent of the joy_port_x and joy_port_y axis counters if the time
 * between 2 reads is too great (how great?) but no programs, I have seen,
 * make use of this behaviour.
 *
 * DANG_END_FUNCTION
 */
Bit8u joy_port_inb (ioport_t port)
{
	Bit8u ret = 0;
	int joynum;

#ifdef JOY_INIT_DEBUG
	joy_port_printf ("port 0x%X: joy_port_inb(): %i %i %i %i\n", port,
							joy_port_x [JOY_0], joy_port_y [JOY_0], joy_port_x [JOY_1], joy_port_y [JOY_1]);
#endif

	/*
	 * DANG_BEGIN_REMARK
	 *
	 * Here we set bits based on joystick axis counters.
	 * The code here is particularly tricky and if you try to change it, you
	 * will probably break it :)
	 *
	 * DANG_END_REMARK
	 */
	for (joynum = 0; joynum < 2; joynum++)
	{
		/* set the bit if we are counting down the axis reading
		 * OR if it's an invalid axis
		 */
		if (joy_port_x [joynum])
		{
			ret |= (1 << (0 + joynum * 2));

			/* if (joy_port_x [joynum] == -1)
			 * 	it's an invalid axis
			 * 	so we don't decrement the counter so we count forever!
			 */
			if (joy_port_x [joynum] > 0) joy_port_x [joynum]--;
		}

		/* set the bit if we are counting down the axis reading
		 * OR if it's an invalid axis
		 */
		if (joy_port_y [joynum])
		{
			ret |= (1 << (1 + joynum * 2));

			/* if (joy_port_y [joynum] == -1)
			 * 	it's an invalid axis
			 * 	so we don't decrement the counter so we count forever!
			 */
			if (joy_port_y [joynum] > 0) joy_port_y [joynum]--;
		}
	}

	/*
	 * DANG_BEGIN_REMARK
	 *
	 * Here we read the button status from Linux (programs can read the
	 * button status from the port, _without_ making a dummy write to the
	 * port first so the Linux read must be done _here_) and return it.
	 *
	 * DANG_END_REMARK
	 */
	ret |= (joy_reader->read_buttons () << 4);

	return ret;
}

/* DANG_FIXTHIS What happens if you read a word from the joystick port?  Analysis of a real DOS system shows that it reads a byte and doubles it up but I don't know if this is the correct behaviour... */
Bit16u joy_port_inw (ioport_t port)
{
	Bit16u ret = (Bit16u) joy_port_inb (port);

#ifdef JOY_PORT_DEBUG
	joy_port_printf ("port 0x%X: UNEXPECTED! joy_port_inw()\n", port);
#endif
	return (ret << 8) | ret;
}

void joy_port_outb (ioport_t port, Bit8u value)
{
#ifdef JOY_PORT_DEBUG
	joy_port_printf ("port 0x%X: joy_port_outb()\n", port);
#endif

/* DEFINE this to fake readings from the joystick port (for debugging) */
/*#define TESTCENTRE*/

#ifdef TESTCENTRE
	{
		int x, y;

		FILE *infp = fopen ("/root/.joystick", "rt");
		if (!infp)
		{
			perror ("why");
			x = y = -1;
		}
		else
		{
			fscanf (infp, "%i %i", &x, &y);
			fclose (infp);
		}

		joy_port_x [JOY_0] = x;
		joy_port_y [JOY_0] = y;
		joy_port_x [JOY_1] = x;
		joy_port_y [JOY_1] = y;
	}
#else
	/* read in joystick position (preparing for joy_port_inb()) */
	joy_port_x [JOY_0] = joy_reader->read_axis (JOY_0, JOY_X, -1);	/* 1st joystick x */
	joy_port_y [JOY_0] = joy_reader->read_axis (JOY_0, JOY_Y, -1);	/* 1st joystick y */
	joy_port_x [JOY_1] = joy_reader->read_axis (JOY_1, JOY_X, -1);	/* 2nd joystick x */
	joy_port_y [JOY_1] = joy_reader->read_axis (JOY_1, JOY_Y, -1);	/* 2nd joystick y */
#endif
}

void joy_port_outw (ioport_t port, Bit16u value)
{
#ifdef JOY_PORT_DEBUG
	joy_port_printf ("port 0x%X: UNEXPECTED! joy_port_outw()\n", port);
#endif

	/* DANG_FIXTHIS joy_port_outw is not exactly correct at all but what would you do? */
	joy_port_outb (port, 0);
}

/* end of joystick.c */
