/* 
 *
 * $Date: 1995/02/05 16:54:16 $
 * $Source: /home/src/dosemu0.60/include/RCS/shared.h,v $
 * $Revision: 1.1 $
 * $State: Exp $
 *
 */

/*
   Size of area to share memory
   This has been taken from JL's spec.
 */
#define SHARED_QUEUE_FLAGS_AREA (4+4+4+4+108+1208+484+108)
#define SHARED_VIDEO_AREA (0xc0000-0xa0000+1)

#define TMPFILE "/var/run/dosemu."

#define SHARED_KEYBOARD_OFFSET 1816
