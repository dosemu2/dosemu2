/* dos emulator, Matthias Lautner */
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/11/12 12:32:17 $
 * $Source: /home/src/dosemu0.49pl2/RCS/dos.c,v $
 * $Revision: 1.1 $
 * $State: Exp $
 *
 * $Log: dos.c,v $
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.2  1993/07/07  01:34:29  root
 * removed outdated message about video buffer
 *
 */

#include <stdio.h>
#include "config.h"

void (*dosemu)();

#if !STATIC
char dummy[1088*1024]; /* ensure that the lower 1MB+64K is unused */
#endif

int main(int argc, char **argv)
{
#if STATIC
  int emulate(int, char **);

  fprintf(stderr,"running static, emulate @ %x!\n", emulate);
  fflush(stderr);
  printf("WARNING: running static, emulate @ %x!\n", emulate);
  emulate(argc, argv);
#else
  if (uselib("/lib/libemu") != 0) {
    printf("cannot load shared lib\n");
    exit(1);
  }
  dosemu = (void *) LIBSTART;
  dosemu(argc, argv);
#endif

}

