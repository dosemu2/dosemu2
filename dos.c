/* dos emulator, Matthias Lautner */
/* Extensions by Robert Sanders, 1992-93
 *
 * DANG_BEGIN_MODULE
 * 
 * Initial program executed to run DOSEMU. Gets access to libdosemu
 * and sets international character parms. Finally calls entry
 * point of DOSEMU emulate() function which is loaded above the
 * usual DOS memory area from 0 - 1meg. Emulate() is in emu.c.
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * $Date: 1994/09/26 23:10:13 $
 * $Source: /home/src/dosemu0.60/RCS/dos.c,v $
 * $Revision: 2.6 $
 * $State: Exp $
 *
 * $Log: dos.c,v $
 * Revision 2.6  1994/09/26  23:10:13  root
 * Prep for pre53_22.
 *
 * Revision 2.5  1994/09/23  01:29:36  root
 * Prep for pre53_21.
 *
 * Revision 2.4  1994/09/20  01:53:26  root
 * Prep for pre53_21.
 *
 * Revision 2.3  1994/08/25  00:49:34  root
 * HJ's patches for new linking (Actually new dos.c)
 *
 * Revision 2.2  1994/07/14  23:19:20  root
 * Jochen's Patches
 *
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
 * DANG_END_CHANGELOG
 *
 */

#include <stdio.h>
#include <a.out.h>
#include <locale.h>
#include "config.h"

#define LIBDOSEMU	"/usr/lib/libdosemu"

/*
 * DANG_BEGIN_FUNCTION dosemu
 *
 * arguments:
 * argc - Count of argumnents.
 * argc - Actual arguments.
 *
 * description:
 *  Function created by entry point into libdosemu. Called to
 *  jump into the emulate function of DOSEMU.
 *
 * DANG_END_FUNCTION
 */
void (*dosemu) (int argc, char **argv);

#ifndef STATIC
char buf [1088 * 1024];	/* ensure that the lower 1MB+64K is unused */

#endif

int
main(int argc, char **argv)
{
#ifdef STATIC
  int emulate(int, char **);

#if 0
  fprintf(stderr, "running static, emulate @ %x!\n", emulate);
  fflush(stderr);
  fprintf(stderr, "WARNING: running static, emulate @ %x!\n", emulate);
#endif
  emulate(argc, argv);
#else
  struct exec header;
  FILE *f;

  f = fopen(LIBDOSEMU, "r");
  if (f == NULL)
  {
    sprintf (buf, "%s: cannot open shared library: %s", argv [0],
	LIBDOSEMU);
    perror (buf);
    exit(1);
  }

  if (fread(&header, sizeof(header), 1, f) != 1)
  {
    sprintf (buf, "%s: cannot read shared library: %s", argv [0],
	LIBDOSEMU);
    perror (buf);
    exit(1);
  }

  if (N_BADMAG (header))
  {
    fprintf (stderr, "%s: invalid shared library format: %s\n", argv [0],
	LIBDOSEMU);
    exit(1);
  }

  if (uselib(LIBDOSEMU) != 0) {
    sprintf (buf, "%s: cannot load shared library: %s", argv [0],
	LIBDOSEMU);
    perror (buf);
    exit(1);
  }
  setlocale(LC_CTYPE,"");

  dosemu = (void *) header.a_entry;

  (* dosemu)(argc, argv);
#endif

}
