/* 
 *
 * $Date: $
 * $Source: $
 * $Revision: $
 * $State: $
 *
 */

/*
   Size of area to share memory
   This has been taken from JL's spec.
 */
#define SHARED_QUEUE_FLAGS_AREA (4+4+4+4+108+1208+484+108)
#define SHARED_VIDEO_AREA (0xc0000-0xa0000)

#define TMPFILE "/tmp/dosemu."

#define SHARED_KEYBOARD_OFFSET 1816
