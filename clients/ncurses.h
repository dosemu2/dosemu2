#define NCURSES_H 1

/* 
 * $Date: 1995/01/14 15:33:33 $
 * $Source: /home/src/dosemu0.60/clients/RCS/ncurses.h,v $
 * $Revision: 2.3 $
 * $State: Exp $
 *
 * $Log: ncurses.h,v $
 * Revision 2.3  1995/01/14  15:33:33  root
 * ncurses.h  header file
 *
 * Revision 2.2  1994/06/16  23:04:51  root
 * Change to 0.52.
 *
 * Revision 2.1  1994/06/12  23:18:11  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.3  1994/04/27  21:37:08  root
 * Jochen's Latest.
 *
 * Revision 1.2  1994/04/23  20:52:58  root
 * Get new stack over/underflow working in VM86 mode..
 *
 * Revision 1.1  1994/03/28  23:48:34  root
 * Initial revision
 *
 */

#ifdef TRUE
#undef TRUE
#undef FALSE
#endif
#include <signal.h> /* For emu.h */
#include "memory.h"
#include "emu.h"

#undef NCURSES_H
