/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifdef X_SUPPORT
#include <SDL_syswm.h>
#endif

void SDL_pre_init(void);
#ifdef X_SUPPORT
extern void *X_handle;
extern Display *x11_display;
#endif
