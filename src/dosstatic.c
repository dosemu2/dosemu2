/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdio.h>


__asm__(".org 0x110000");

#ifdef __NetBSD__
void __foo__()
{
/* just take up enough space to force large address initialization */
}
#endif

#ifdef __linux__
int
main(int argc, char **argv)
{
/* just run this function */
  int emulate(int, char **);

  emulate(argc, argv);
}
#endif
