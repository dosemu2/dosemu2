/* putrom.c; put VBIOS ROM image into /dev/mem (should fail except for weird caches!)
 *
 * $Date: 1995/02/25 22:38:26 $
 * $Source: /home/src/dosemu0.60/periph/RCS/putrom.c,v $
 * $Revision: 2.3 $
 * $State: Exp $
 *
 * $Log: putrom.c,v $
 * Revision 2.3  1995/02/25  22:38:26  root
 * *** empty log message ***
 *
 * Revision 2.2  1994/06/17  00:14:24  root
 * Let's wrap it up and call it DOSEMU0.52.
 *
 * Revision 2.2  1994/06/17  00:14:24  root
 * Let's wrap it up and call it DOSEMU0.52.
 *
 * Revision 2.1  1994/06/12  23:17:32  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 2.1  1994/06/12  23:17:32  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.5  1994/03/23  23:26:14  root
 * Add support for Video Bios at E000
 *
 * Revision 1.4  1994/03/13  01:08:52  root
 * Poor attempt to optimize.
 *
 * Revision 1.3  1994/01/20  21:18:35  root
 * Indent.
 *
 * Revision 1.2  1993/11/30  21:27:27  root
 * Freeze before 0.49pl3
 *
 * Revision 1.1  1993/11/12  12:41:41  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  21:20:03  root
 * Initial revision
 *
 */

#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#define ROM_SIZE	(64*1024)
#ifndef VIDEO_E000
#define ROM_ADDR	(0xc0000)
#else
#define ROM_ADDR	(0xe0000)
#endif

#define die(s) do { fprintf(stderr,"putrom: %s\n", s); exit(1); } while(0)
#define diee(s) do { fprintf(stderr,"putrom: error %s while %s\n", \
			strerror(errno), s); exit(1); } while(0)

#define dump(p,s) { int i; for (i=0; i<s; i++) fprintf(stderr, "%02x ",\
		       *((u_char*)p + i)); fprintf(stderr,"\n"); }

#define scan(p,s) { int i; volatile u_char j,*jp=&j; for (i=0; i<ROM_SIZE/4096; i++) \
		      *jp=*((volatile u_char *)p + i*4096); }

void
main(int argc, char **argv)
{
  int mem_fd, file_fd, rd;
  u_char *memptr = valloc(ROM_SIZE);
  u_char *tmpptr = malloc(ROM_SIZE);

  if (argc != 2)
    die("usage: putrom VBIOSFILE\n");

  if ((mem_fd = open("/dev/mem", O_RDWR)) == -1)
    die("opening /dev/mem");

  if ((file_fd = open(argv[1], O_RDONLY)) == -1)
    diee("opening VBIOSFILE");

  if (mmap(memptr,
	   ROM_SIZE,
	   PROT_READ | PROT_WRITE | PROT_EXEC,
	   MAP_SHARED | MAP_FIXED,
	   mem_fd,
	   ROM_ADDR
      ) == (void *) -1)
    diee("mmaping");

  scan(memptr, ROM_SIZE);

  if ((rd = read(file_fd, tmpptr, ROM_SIZE)) == -1)
    diee("reading VBIOSFILE");
  else
    fprintf(stderr, "read %d bytes\n", rd);

  memcpy(memptr, tmpptr, ROM_SIZE);

  fprintf(stderr, "done");
  close(mem_fd);
  close(file_fd);
}
