/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * Joystick support for DOSEMU
 * Copyright (c) 2002 Clarence Dang <dang@kde.org>
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

/* init functions */
void joy_init (void);
void joy_uninit (void);
void joy_term (void);		/* just calls joy_uninit() ! */
void joy_reset (void);

/* access functions */
int joy_exist (void);		/* used by src/base/init/init.c */
int joy_bios_read (void);	/* used by src/base/async/int.c */

#endif	/* JOYSTICK_H */
