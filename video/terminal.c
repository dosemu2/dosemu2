/* 
 * video/terminal.c - contains the video-functions for terminals 
 *
 */
#ifdef USE_SLANG
#include "terminal.c.slang"
#endif
#ifndef USE_SLANG
#include "terminal.c.ncurses"
#endif
