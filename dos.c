/* dos emulator, Matthias Lautner */
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/02/05 02:54:02 $
 * $Source: /usr/src/dos/RCS/dos.c,v $
 * $Revision: 1.5 $
 * $State: Exp $
 *
 * $Log: dos.c,v $
 * Revision 1.5  1993/02/05  02:54:02  root
 * this is for 0.47.6
 *
 * Revision 1.4  1993/01/14  21:59:43  root
 * goes with 1.5 of termio.c and emu.c.  just changes for the
 * video save buffer (currently unimplemented) for the direct
 * console mode, and no option checking (done in emulate()).
 *
 * Revision 1.3  1993/01/13  16:46:04  root
 * If I'm right, we want to keep this file as small as possible.
 * The DOS emulator's low memory will be protected over the range
 * of 0 to the size of dos.o; any write to there will, because of
 * the Linux kernel's text-space protection, cause a SIGSEGV.
 * One day I should look into this, but for now, we'll just deal with
 * the interruptions.
 *
 * Revision 1.2  1993/01/11  21:26:48  root
 * just put some comments in.
 *
 */

#include <stdio.h>

#define LIBSTART 	0x400000

void (*dosemu)();
char dummy[1088*1024 + 64*1024]; /* ensure that the lower 1MB+64K is unused */

/* the "+ 64*1024" reserves 64k second video buffer */

int main(int argc, char **argv)
{
  if (uselib("/lib/libemu") != 0) {
    printf("cannot load shared lib\n");
    exit(1);
  }
  dosemu = (void *) LIBSTART;
  dosemu(argc, argv);
}
