#define NCURSES_H 1

/* 
 * $Date: 1994/04/27 21:37:08 $
 * $Source: /home/src/dosemu0.60/clients/RCS/ncurses.h,v $
 * $Revision: 1.3 $
 * $State: Exp $
 *
 * $Log: ncurses.h,v $
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
#include "memory.h"
#include "emu.h"

#undef NCURSES_H
