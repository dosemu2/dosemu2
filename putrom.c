#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <malloc.h>

#define ROM_SIZE	(64*1024)
#define ROM_ADDR	(0xc0000)

#define die(s) do { fprintf(stderr,"putrom: %s\n", s); exit(1); } while(0)
#define diee(s) do { fprintf(stderr,"putrom: error %s while %s\n", \
			strerror(errno), s); exit(1); } while(0)

#define dump(p,s) { int i; for (i=0; i<s; i++) fprintf(stderr, "%02x ",\
		       *((u_char*)p + i)); fprintf(stderr,"\n"); }

#define scan(p,s) { int i; volatile u_char j,*jp=&j; for (i=0; i<ROM_SIZE/4096; i++) \
		      *jp=*((volatile u_char *)p + i*4096); }


int
main(int argc, char **argv)
{
  int mem_fd, file_fd, i;
  u_char *memptr=valloc(ROM_SIZE);
  u_char *tmpptr=malloc(ROM_SIZE);

  if (argc != 2) die("usage: putrom VBIOSFILE\n");

  if ( (mem_fd=open("/dev/mem",O_RDWR)) == -1)
    die("opening /dev/mem");

  if ( (file_fd=open(argv[1],O_RDONLY)) == -1)
    diee("opening VBIOSFILE");

  if (mmap(memptr, 
       ROM_SIZE,
       PROT_READ|PROT_WRITE|PROT_EXEC,
       MAP_SHARED|MAP_FIXED,
       mem_fd, 
       ROM_ADDR
       ) == (void *)-1)
    diee("mmaping");

  scan(memptr,ROM_SIZE);

  if ( (i=read(file_fd, tmpptr, ROM_SIZE)) == -1)
    diee("reading VBIOSFILE");
  else fprintf(stderr, "read %d bytes\n", i);

  memcpy(memptr, tmpptr, ROM_SIZE);

  fprintf(stderr, "done");
  close(mem_fd);
  close(file_fd);
}
