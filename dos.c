/* dos emulator, Matthias Lautner 
 * Extensions by Robert Sanders, 1992-93
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
 * $Date: 1995/04/08 22:29:37 $
 * $Source: /usr/src/dosemu0.60/RCS/dos.c,v $
 * $Revision: 2.10 $
 * $State: Exp $
 *
 *
 * DANG_END_CHANGELOG
 *
 */

#include <stdio.h>
#include <a.out.h>
#include <locale.h>
#include <errno.h>
#include "config.h"

#ifndef LIBDOSEMU
#define LIBDOSEMU	"/usr/lib/libdosemu"
#endif

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
static void (*dosemu) (int argc, char **argv);

static char buf [1088 * 1024];	/* ensure that the lower 1MB+64K is unused */


int
main(int argc, char **argv)
{
  struct exec header;
  FILE *f;
  char *libpath = LIBDOSEMU; 

#if 0 /* Security hole */
  char *cp;

  cp = (unsigned char  *)getenv("LIBDOSEMU");
  if(cp) 
	libpath = cp;
#endif


  f = fopen(libpath, "r");
  if (f == NULL)
  {
    fprintf(stderr, "%s: cannot open shared library: %s\n", argv [0],
	libpath, strerror(errno));
    fprintf(stderr, "Check the LIBDOSEMU variable, default is %s\n",
		LIBDOSEMU);
    exit(1);
  }

  if (fread(&header, sizeof(header), 1, f) != 1)
  {
    fprintf (stderr, "%s: cannot read shared library: %s\n", argv [0],
	libpath, strerror(errno));
    exit(1);
  }

  if (N_BADMAG (header))
  {
    fprintf (stderr, "%s: invalid shared library format: %s\n", argv [0],
	libpath);
    exit(1);
  }

  if (uselib(libpath) != 0) {
    fprintf (stderr, "%s: cannot load shared library: %s\n", argv [0],
	libpath, strerror(errno));
    fprintf(stderr, "Try setting LIBDOSEMU\n");
    exit(1);
  }
  setlocale(LC_CTYPE,"");

  dosemu = (void *) header.a_entry;

  (* dosemu)(argc, argv);

}
