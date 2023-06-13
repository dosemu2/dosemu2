/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Joystick support for DOSEMU
 * Copyright (c) 2002 Clarence Dang <dang@kde.org>
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#ifdef __linux__

/* init functions */
void joy_init (void);
void joy_uninit (void);
void joy_term (void);		/* just calls joy_uninit() ! */
void joy_reset (void);

/* access functions */
int joy_exist (void);		/* used by src/base/init/init.c */
int joy_bios_read (void);	/* used by src/base/async/int.c */

#else

static inline void joy_init(void) {}
static inline void joy_uninit(void) {}
static inline void joy_term(void) {}
static inline void joy_reset(void) {}
static inline int joy_exist(void) { return 0; }
static inline int joy_bios_read(void) { return 0; }

#endif

#endif	/* JOYSTICK_H */
