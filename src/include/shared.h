/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _SHARED_H
#define _SHARED_H

#include "extern.h"

/*
   Size of area to share memory
   This has been taken from JL's spec.
 */
#define SHARED_QUEUE_FLAGS_AREA (4+4+4+4+108+1208+484+1008)
#define SHARED_VIDEO_AREA (0xc0000-0xa0000+1)

#define SHARED_KEYBOARD_OFFSET 1816
#define CLIENT_REQUEST_FLAG_AREA 8


EXTERN u_char *shared_qf_memory;
EXTERN void shared_memory_init(void);
EXTERN void shared_memory_exit(void);
EXTERN void shared_keyboard_init (void);

#endif

