/*
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
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
 * does in real DOS i.e. you can have two joysticks (untested) or one joystick,
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
 *
 * Last Modified: $Date$ (Version 2)
 */


/*
 * Compile-time Debugging Options
 *
 * Specify what debugging code you want to compile in (recommended: all).
 * You will still need to activate them at runtime (-Dj) for any visible effect.
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

#include "types.h"
#include "timers.h"

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
static int joy_latency_us = 0;	/* latency in microseconds */

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
void joy_port_outb (ioport_t port, Bit8u value);


/*
 * DANG_BEGIN_REMARK
 *
 * We make a runtime decision based on the detected joystick API version
 * and #ifdef USE_PTHREADS, on the way in which we obtain the joystick
 * status from Linux (a "driver"):
 *
 * 1. joy_driver_nojoy: simply tells DOS programs that you have no
 *                      joystick(s)
 *
 * 2. joy_driver_old: uses old, non-blocking joystick API (<1.0.0);
 *                    limited to 2 axes; supported because DOSEMU
 *                    supports old kernels
 *
 * 3. joy_driver_new: uses new, non-blocking joystick API (>=1.0.0);
 *                    a (little) slower than joy_driver_new_threaded
 *
 * 4. joy_driver_new_threaded: uses new, BLOCKING joystick API (>=1.0.0);
 *                             efficient but requires pthreads (which
 *                             is known to make DOSEMU unstable!)
 *
 * The same driver is used for both joysticks.
 *
 * DANG_END_REMARK
 */

typedef struct
{
	int (*read_buttons) (void);
	int (*read_axis) (const int joynum, const int axis,
							const int invalid_val, const int update);

	int linux_min;
	int linux_max;
	int linux_range;
} JOY_DRIVER;

/* no joystick */
static int joy_linux_read_buttons_nojoy (void);
static int joy_linux_read_axis_nojoy (const int joynum, const int axis,
													const int invalid_val, const int update);
static JOY_DRIVER joy_driver_nojoy =
{
	joy_linux_read_buttons_nojoy,
	joy_linux_read_axis_nojoy,

	0,
	0,
	0
};


/* old, non-blocking API (<1.0.0) */
static int joy_linux_read_buttons_old (void);
static int joy_linux_read_axis_old (const int joynum, const int axis,
												const int invalid_val, const int update);
static JOY_DRIVER joy_driver_old =
{
	joy_linux_read_buttons_old,
	joy_linux_read_axis_old,

	+1,
	+255,
	255
};

/* new, non-blocking API (>=1.0.0) */
static int joy_linux_read_buttons_new (void);
static int joy_linux_read_axis_new (const int joynum, const int axis,
												const int invalid_val, const int update);
static JOY_DRIVER joy_driver_new =
{
	joy_linux_read_buttons_new,
	joy_linux_read_axis_new,

	-32767,
	+32767,
	65535
};

/* new, BLOCKING API + threading */
#ifdef USE_PTHREADS
static int joy_linux_read_buttons_new_threaded (void);
static int joy_linux_read_axis_new_threaded (const int joynum, const int axis,
															const int invalid_val, const int update);
static JOY_DRIVER joy_driver_new_threaded =
{
	joy_linux_read_buttons_new_threaded,
	joy_linux_read_axis_new_threaded,

	-32767,
	+32767,
	65535
};
#endif

/* current joystick driver */
static JOY_DRIVER *joy_driver = &joy_driver_nojoy;
static int joy_driver_decided;


/*
 * Init functions
 */

static void joy_driver_set (JOY_DRIVER *driver)
{
	joy_driver = driver;
	joy_driver_decided = 1;
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

	joy_driver_decided = 0;


	/* ========================= CHECK CONFIG ========================= */


	/* check joystick axis range */
	if (config.joy_dos_min <= 0 ||
			config.joy_dos_max <= config.joy_dos_min ||
			config.joy_dos_max > 1000)
	{
	#ifdef JOY_INIT_DEBUG
		joy_init_printf ("WARNING! joystick axis min (%i) and/or max (%i) out of range\n",
								config.joy_dos_min, config.joy_dos_max);
		joy_init_printf ("--> setting joystick range to hard-coded default 1-150\n");
	#endif
		config.joy_dos_min = 1;
		config.joy_dos_max = 150;
	}
	joy_dos_range = config.joy_dos_max - config.joy_dos_min + 1;

	/* check joystick granularity */
	if (config.joy_granularity <= 0 || config.joy_granularity > joy_dos_range)
	{
	#ifdef JOY_INIT_DEBUG
		joy_init_printf ("WARNING! joystick granularity (%i) out of range\n",
								config.joy_granularity);
		joy_init_printf ("--> disabling granularity setting\n");
	#endif
		config.joy_granularity = 1;
	}

	/* check joystick latency */
	if (config.joy_latency < 0 || config.joy_latency > 200)
	{
	#ifdef JOY_INIT_DEBUG
		joy_init_printf ("WARNING! joystick latency (%i) out of range (0-200ms)\n",
								config.joy_latency);
		joy_init_printf ("--> disabling latency setting\n");
	#endif
		config.joy_latency = 0;
	}
	joy_latency_us = config.joy_latency * 1000;

#ifdef JOY_INIT_DEBUG
	joy_init_printf ("joystick range      : %i-%i (%i)\n",
						 	config.joy_dos_min, config.joy_dos_max, joy_dos_range);
	joy_init_printf ("joystick granularity: %i\n", config.joy_granularity);
	joy_init_printf ("joystick latency    : %ims\n", config.joy_latency);
#endif


	/* ========================= INIT GLOBALS ========================= */


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


	/* ========================= INIT 2 JOYSTICKS ========================= */


	for (joynum = 0; joynum < 2; joynum++)
	{
		/* does the user even want this joystick? */
		if (strlen (config.joy_device [joynum]) == 0)
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("joystick 0x%X: will not be initialised\n", joynum);
		#endif
			continue;
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
			joy_init_printf ("joystick 0x%X: opened \"%s\"\n",
									joynum, config.joy_device [joynum]);
		#endif
		}
		else
		{
		#ifdef JOY_INIT_DEBUG
			joy_init_printf ("joystick 0x%X: ERROR! can't open \"%s\"\n",
									joynum, config.joy_device [joynum]);
		#endif
			continue;	/* try to open next joystick */
		}

		/*
		 * NOTE: this assumes that both joystick devices are using the same API
		 *       -- if they are not, then you probably deserve the code
		 *          malfunctioning :)
		 */
		if (!joy_driver_decided)	/* set by joy_driver_set() */
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
				joy_init_printf ("joystick 0x%X: API Version Detected: 0x%06X\n",
										joynum, joy_version);
		#endif

			if (joy_version < JOY_VERSION (1,0,0))
			{
			#ifdef JOY_INIT_DEBUG
				joy_init_printf ("joystick 0x%X: Using OLD joystick API\n", joynum);
			#endif

				/*
				 * To quote linux/Documentation/input/joystick-api.txt:
				 *
				 *	"The axis values do not have a defined range in the original 0.x
				 *  driver, except for that the values are non-negative."
				 *
				 * Hence, we signify that we cannot depend on values being in a
				 * defined range by setting it to 0.
				 *
				 */
				joy_driver_old.linux_range = 0;

				joy_driver_set (&joy_driver_old);
			}
			else
			{
			#ifdef USE_PTHREADS
				#ifdef JOY_INIT_DEBUG
					joy_init_printf ("joystick 0x%X: Using NEW joystick API (threaded)\n", joynum);
				#endif
				joy_driver_set (&joy_driver_new_threaded);
			#endif

				if (!joy_driver_decided)
				{
				#ifdef JOY_INIT_DEBUG
					joy_init_printf ("joystick 0x%X: Using NEW joystick API (unthreaded)\n", joynum);
				#endif
					joy_driver_set (&joy_driver_new);
				}
			}
		}	/* if (!joy_driver_decided)	{ */

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
		if (joy_driver == &joy_driver_new_threaded)
		{
			/* start thread to read joystick events */
			if (pthread_create (&thread [joynum], NULL, joy_linux_thread_read, &joy [joynum]))
			{
			#ifdef JOY_INIT_DEBUG
				joy_init_printf ("joystick 0x%X: ERROR! Cannot create thread!  Falling back to unthreaded code!\n",
										joynum);
			#endif

				/* if this is the 2nd joystick, cancel the 1st's thread */
				if (joynum == JOY_1)
					if (joy_fd [JOY_0] >= 0)
						pthread_cancel (thread [JOY_0]);

				if (fcntl (joy_fd [joynum], F_SETFL, O_NONBLOCK))
				{
				#ifdef JOY_INIT_DEBUG
					joy_init_printf ("joystick 0x%X: ERROR! Cannot switch to NONBLOCKING mode (%i: %s)! "
											"Falling back to OLD joystick API which didn't care about blocking.\n",
											joynum, errno, strerror (errno));
				#endif

					/*
					 * To quote linux/Documentation/input/joystick-api.txt again:
					 *
					 * "The axis values do not have a defined range in the original
					 * 0.x driver, except for that the values are non-negative..."
					 */
					if (joy_version < JOY_VERSION (1,2,8))
						/* cannot depend on values being in a defined range... */
						joy_driver_old.linux_range = 0;	/* ...signify with 0 */
					/*
					 * "1.2.8+ drivers use a fixed range for reporting the values,
					 * 1 being the minimum, 128 the center, and 255 maximum value."
					 */
					else
						joy_driver_old.linux_range = 255;

					joy_driver_set (&joy_driver_old);
				}
				else	/* successfully switched device to NONBLOCKing mode... */
					joy_driver_set (&joy_driver_new);
			}
		}
	#endif	/* USE_PTHREADS */
	}

	/* handle joystick port/game card routines */
	/* DANG_FIXTHIS does this code work for ports other than 0x201?
	 */
	io_device.read_portb   = joy_port_inb;
	io_device.write_portb  = joy_port_outb;
	io_device.read_portw   = NULL;
	io_device.write_portw  = NULL;
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
		if (joy_driver == &joy_driver_new_threaded)
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

/* DANG_FIXTHIS joy_reset() is called immediately after joy_init(), which is rather inconvenient (anyone heard of a port_unregister_handler()?) so we don't bother resetting at all but in the future this could cause problems
 */

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
	 *       therefore we still claim that a joystick exists
	 */
	return joy_status;
}


/*
 * Emulation helper functions (DOS->Linux)
 */

/*
 * DANG_BEGIN_FUNCTION joy_latency_over
 *
 * Tells DOSEMU whether or not it is time to update its internal status
 * of the joystick (for nonblocking reads only).
 *
 * DOS programs read/poll from the joystick port hundreds of thousands of
 * times per second so the idea is that we really don't need to read from
 * Linux for every such query (increasing performance by about 40%) because:
 *
 * 1. humans are incapable of changing the status of the joystick
 *    (moving, pressing buttons) more than about 10 times per second
 *
 * 2. no one will not notice a delay in DOS registering the joystick status
 *    (if it is in the order of a few milliseconds)
 *
 * Of course, this means that you should not set joy_latency in dosemu.conf
 * to more than 1000/(#times I can press a button/move joy per second * 2),
 * unless you want DOSEMU to miss quick axis/button changes and want to
 * wait a ridiculous amount of time before DOSEMU registers any changes at
 * all.
 *
 * DANG_END_FUNCTION
 */
static inline int joy_latency_over (void)
{
	static hitimer_t last_update_time = 0;

	if (joy_latency_us)
	{
		/* hmm, isn't calling gettimeofday() on every joystick read inefficient? */
		hitimer_t current_time = GETusSYSTIME ();

		if (last_update_time == 0 /* first read ever */ ||
			current_time - last_update_time >= joy_latency_us)
		{
			last_update_time = current_time;
			return 1;	/* read from linux - status too old */
		}
		else
			return 0;	/* don't read from linux - status reasonably up-to-date */
	}
	/* no latency... */
	else
		return 1;	/* read from linux */
}

/*
 * DANG_BEGIN_FUNCTION joy_emu_button_set
 *
 * Update the button status for each joystick.
 *
 * We must perform "button mapping" if only the first joystick is enabled
 * i.e. we are required to map the "excessive" buttons (>2) from the first
 * joystick onto the second:
 *
 *	a) 3rd button of 1st joy --> 1st button of 2nd joy
 *
 *	b) 4th button of 1st joy --> 2nd button of 2nd joy
 *
 * DANG_END_FUNCTION
 */
static inline void joy_emu_button_set (const int joynum, const int button, const int pressed)
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
			joy_buttons &= ~(1 << bnum);		/* clear bit */
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
 * i.e. we are required to map the "excessive" axes (>2) from the first
 * joystick onto the second:
 *
 *	a) 3rd axis of 1st joy --> 2st axis of 2nd joy
 *
 *	b) 4th axis of 1st joy --> 1st axis of 2nd joy
 *	   (yes, these are reversed deliberately because it's what happens in DOS)
 *
 * DANG_END_FUNCTION
 */
static inline void joy_emu_axis_set (const int joynum, const int axis, const int value)
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
 * DANG_BEGIN_FUNCTION joy_emu_axis_conv
 *
 * Convert a Linux joystick axis reading to a DOS one by making use of
 * the differences in the allowable range of axis values.
 *
 * NOTE: I don't know whether or not Linux returns exponential values
 * for the joystick but (apparently) DOS programs expect the values to
 * be exponential and so if this is to be fixed, it should probably be
 * done in this function.
 *
 * DANG_END_FUNCTION
 */
#define joy_axis_round(num,gran) ((num) - ((num) % (gran)))
static inline int joy_emu_axis_conv (const int linux_val, const int invalid_val)
{
	/* virtual joystick axis doesn't exist? */
	if (linux_val == JOY_AXIS_INVALID)
		return invalid_val;

	/* do the conversion if the linux range is known */
	if (joy_driver->linux_range)
	{
		int ret = (linux_val - joy_driver->linux_min)
						* joy_dos_range / joy_driver->linux_range;
		return joy_axis_round (ret, config.joy_granularity) + config.joy_dos_min;
	}
	/* joystick driver <1.2.8 (_if_ using the old API) doesn't have a defined
	 * range so we can't do an axis conversion */
	else
		/* we add 1 in case the result=0, else any program using BIOS reads
		 * wouldn't detect the joystick if it was in the upper-left corner
		 */
		return joy_axis_round (linux_val, config.joy_granularity) + 1;
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
static inline void joy_linux_process_event (const int joynum, const struct js_event *event)
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

/* the thread (if using joy_driver_new_threaded) */
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
 * Process the event queue for _both_ linux joysticks using nonblocking
 * reads with the new joystick API (joy_driver_new).
 *
 * This should be done before (well, actually, not before _every_
 * single read -- see joy_latency_over()) the joystick status is returned
 * to DOS as all Linux joystick events are queued until they are processed
 * and we want to return a reasonably current state of the joystick
 * -- not what it was a long time ago.  _Both_ joysticks are processed
 * here because of axis/button mapping affecting the status of both emulated
 * joysticks (what DOS sees).
 *
 * DANG_END_FUNCTION
 */
static inline void joy_linux_read_events (void)
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
 * DANG_BEGIN_FUNCTION joy_linux_read_status
 *
 * Read both the current position and current button status of the joystick
 * from Linux (joy_driver_old).
 *
 * DANG_END_FUNCTION
 */
static inline void joy_linux_read_status (void)
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
			joy_init_printf ("joystick 0x%X: ERROR! joy_read_status(): %s\n",
									joynum, strerror (errno));
		#endif
			continue;
		}

	#ifdef JOY_LINUX_DEBUG
		joy_linux_printf ("joy 0x%X: x=%i y=%i buttons=%i\n",
								joynum, js.x, js.y, js.buttons);
	#endif

		/* perform axis mapping... */
		joy_emu_axis_set (joynum, JOY_X, js.x);
		joy_emu_axis_set (joynum, JOY_Y, js.y);

	#ifdef JOY_LINUX_DEBUG
		joy_linux_printf ("joy 0x%X: emu-x=%i emu-y=%i\n",
								joynum, joy_axis [joynum][JOY_X], joy_axis [joynum][JOY_Y]);
	#endif

		/* perform button mapping... */
		for (b = 0; b < 4; b++)
			joy_emu_button_set (joynum, b, js.buttons & (1 << b));

	#ifdef JOY_LINUX_DEBUG
		joy_linux_printf ("joy 0x%X: emu-button=%i\n", joynum, joy_buttons);
	#endif
	}	/* for (joynum = 0; joynum < 2; joynum++)	{ */
}
 
/*
 * DANG_BEGIN_FUNCTION joy_linux_read_buttons_(family)
 *
 * Eventually called from DOS to get the button status of the joysticks.
 * The threaded version will simply get the status from global variables.
 * The unthreaded versions will perform non-blocking reads.
 *
 * DANG_END_FUNCTION
 */

static int joy_linux_read_buttons_nojoy (void)
{
	return 0x0F;
}

static int joy_linux_read_buttons_old (void)
{
	if (joy_latency_over ())
		joy_linux_read_status ();

	/* ...and return the virtual button reading */
	return joy_buttons;
}

static int joy_linux_read_buttons_new (void)
{
	if (joy_latency_over ())
	{
		/* process all queued Linux joystick events */
		joy_linux_read_events ();
	}

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
 * Eventually called from DOS to get the axis status of the joysticks.
 * The threaded version will simply get the values from global variables.
 * The unthreaded versions will perform non-blocking reads.
 *
 * arguments:
 *  invalid_val - value to return to signify a non-existent axis
 *
 *  update - whether DOSEMU should update its internal axis values from Linux
 *           (for each read of the joystick position, set this flag _only_
 *            on the first of the 4 calls to this function unless you want
 *            the axis positions to be from different points in time :)
 *
 * DANG_END_FUNCTION
 */

static int joy_linux_read_axis_nojoy (const int joynum, const int axis,
													const int invalid_val, const int update)
{
	return invalid_val;	/* I don't have a joystick... */
}

static int joy_linux_read_axis_old (const int joynum, const int axis,
												const int invalid_val, const int update)
{
	if (update)
	{
		if (joy_latency_over ())
			joy_linux_read_status ();
	}

	/* ...and return the virtual axis reading */
	return (joy_emu_axis_conv (joy_axis [joynum][axis], invalid_val));
}

static int joy_linux_read_axis_new (const int joynum, const int axis,
												const int invalid_val, const int update)
{
	if (update)
	{
		if (joy_latency_over ())
		{
			/* process all queued Linux joystick events */
			joy_linux_read_events ();
		}
	}

	/* ... and return the virtual axis reading */
	return (joy_emu_axis_conv (joy_axis [joynum][axis], invalid_val));
}

#ifdef USE_PTHREADS
static int joy_linux_read_axis_new_threaded (const int joynum, const int axis,
															const int invalid_val, const int update)
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
 * on a computer without a joystick -- so to mimick what happens in the
 * real world, I just clear the Carry Flag regardless of whether the user
 * has a joystick or not.  This could be incorrect behaviour so it may
 * have to be changed in the future.
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
 * This is the int15 function 0x84 handler (i.e. BIOS joystick emulation),
 * called from src/base/async/int.c.
 *
 * The real BIOS actually reads its values straight from the joystick port
 * (0x201) but we don't bother because we can do it faster :)
 *
 * Because of this, it returns the joystick axis values with the same
 * range as port 0x201 BUT the range for a real BIOS varies between
 * computers as it is dependant on how it reads from the port
 * (hopefully this won't cause any problems).
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
		LO(ax) = (joy_driver->read_buttons () << 4);

		JOY_BIOS_API_YES;
		break;
	}
	/* read axis */
	case 1:
	{
	#ifdef JOY_BIOS_DEBUG
		joy_bios_printf ("read axis\n");
	#endif

		/* 1st joystick */
		LWORD(eax) = joy_driver->read_axis (JOY_0, JOY_X, 0, 1);	/* x */
		LWORD(ebx) = joy_driver->read_axis (JOY_0, JOY_Y, 0, 0);	/* y */
		
		/* 2nd joystick */
		LWORD(ecx) = joy_driver->read_axis (JOY_1, JOY_X, 0, 0);	/* x */
		LWORD(edx) = joy_driver->read_axis (JOY_1, JOY_Y, 0, 0);	/* y */

		JOY_BIOS_API_YES;
		break;
  }
	/* unknown call */
	default:
	#ifdef JOY_INIT_DEBUG
		/* DANG_FIXTHIS perhaps we should not have been called to start with?
		 */
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
 * This function emulates reads from the joystick port (0x201) -- this is
 * the most common way of detecting and reading the joystick.
 *
 * The real implementation of this port sets a bit for each axis for a
 * certain period of time, corresponding to analog measurements of the
 * position of each axis (so "if you count the analog values in software,
 * a faster machine yields different values from a slow machine [unless
 * you use a timer]" - DOS 6: A Developer's Guide).
 *
 * In contrast, this implementation sets the bits high for a certain number
 * of port reads, corresponding to the position of each axis (independent
 * of time).  This means that, for most programs, the axis range will be
 * that specified in dosemu.conf (which is rather convenient) and avoids
 * the issue of super-fast computers causing DOS program axis counters to
 * overflow (e.g. in a real system, if the program used an 8-bit variable
 * for storing the position of an axis and the system was fast enough to
 * read from the port more than 127 or 255 times, there would be trouble).
 *
 * DANG_END_FUNCTION
 */
Bit8u joy_port_inb (ioport_t port)
{
	Bit8u ret = 0;
	int joynum;

#ifdef JOY_INIT_DEBUG
	joy_port_printf ("port 0x%X: inb(): %i %i %i %i\n",
							port,
							joy_port_x [JOY_0], joy_port_y [JOY_0],
							joy_port_x [JOY_1], joy_port_y [JOY_1]);
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
	ret |= (joy_driver->read_buttons () << 4);

	return ret;
}

void joy_port_outb (ioport_t port, Bit8u value)
{
#ifdef JOY_PORT_DEBUG
	joy_port_printf ("port 0x%X: outb()\n", port);
#endif

/* DEFINE this to fake values from the joystick port (for debugging) */
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
	/*
	 * read in joystick position (preparing for joy_port_inb())
	 */

	/* 1st joystick */
	joy_port_x [JOY_0] = joy_driver->read_axis (JOY_0, JOY_X, -1, 1);	/* x */
	joy_port_y [JOY_0] = joy_driver->read_axis (JOY_0, JOY_Y, -1, 0);	/* y */

	/* 2nd joystick */
	joy_port_x [JOY_1] = joy_driver->read_axis (JOY_1, JOY_X, -1, 0);	/* x */
	joy_port_y [JOY_1] = joy_driver->read_axis (JOY_1, JOY_Y, -1, 0);	/* y */

#endif
}

/* end of joystick.c */
