/* dos emulator, Matthias Lautner */
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1994/06/12 23:15:37 $
 * $Source: /home/src/dosemu0.60/RCS/dos.c,v $
 * $Revision: 2.1 $
 * $State: Exp $
 *
 * $Log: dos.c,v $
 * Revision 2.1  1994/06/12  23:15:37  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.6  1994/04/13  00:07:09  root
 * Jochen's patches.
 *
 * Revision 1.5  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.4  1994/03/04  14:46:13  root
 * Jochen patches.
 *
 * Revision 1.3  1994/01/25  20:02:44  root
 * Exchange stderr <-> stdout
 *
 * Revision 1.2  1994/01/20  21:14:24  root
 * Indent update.
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.2  1993/07/07  01:34:29  root
 * removed outdated message about video buffer
 *
 */

#include <stdio.h>
#include <locale.h>
#include "config.h"

void (*dosemu) ();

#if !STATIC
char dummy[1088 * 1024];	/* ensure that the lower 1MB+64K is unused */

#endif

int
main(int argc, char **argv)
{
#if STATIC
  int emulate(int, char **);

  fprintf(stderr, "running static, emulate @ %x!\n", emulate);
  fflush(stderr);
  fprintf(stderr, "WARNING: running static, emulate @ %x!\n", emulate);
  emulate(argc, argv);
#else
  if (uselib("/usr/lib/libdosemu") != 0) {
    fprintf(stderr, "cannot load shared library /usr/lib/libdosemu!\n");
    exit(1);
  }
  setlocale(LC_CTYPE,"");
  dosemu = (void *) LIBSTART;
  dosemu(argc, argv);
#endif

}
