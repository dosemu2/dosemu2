/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* dos emulator, Matthias Lautner 
 * Extensions by Robert Sanders, 1992-93
 *
 * DANG_BEGIN_MODULE
 * 
 * REMARK
 * Initial program executed to run DOSEMU. Gets access to libdosemu
 * and sets international character parms. Finally calls entry
 * point of DOSEMU emulate() function which is loaded above the
 * usual DOS memory area from 0 - 1meg. Emulate() is in emu.c.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 */

#include <stdio.h>
#include <unistd.h> 
#include <a.out.h>
#include <locale.h>
#include <errno.h>
#ifdef __NetBSD__
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#endif
#include "config.h"
#include "types.h"

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

#ifdef __NetBSD__
int uselib(const char *path);
#endif

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

  buf[0] = ' ';
  f = fopen(libpath, "r");
  if (f == NULL)
  {
    fprintf(stderr, "%s: cannot open shared library: %s-%s\n", argv [0],
	libpath, strerror(errno));
    fprintf(stderr, "Check the LIBDOSEMU variable, default is %s\n",
		LIBDOSEMU);
    exit(1);
  }

  if (fread(&header, sizeof(header), 1, f) != 1)
  {
    fprintf (stderr, "%s: cannot read shared library: %s-%s\n", argv [0],
	libpath, strerror(errno));
    exit(1);
  }

  if (N_BADMAG (header))
  {
    fprintf (stderr, "%s: invalid shared library format: %s\n", argv [0],
	libpath);
    exit(1);
  }

  (void) fclose(f);
  if (uselib(libpath) != 0) {
    fprintf (stderr, "%s: cannot load shared library: %s: %s\n", argv [0],
	libpath, strerror(errno));
    fprintf(stderr, "Try setting LIBDOSEMU\n");
    exit(1);
  }
  setlocale(LC_CTYPE,"");

  dosemu = (void *) header.a_entry;

  (* dosemu)(argc, argv);
  return(0); /* never reached, to keep gcc -Wall happy*/

}

#ifdef __NetBSD__
#include <nlist.h>

struct nlist nl[] = {
    { "curbrk" },
#define X_CURBRK 0
    {"_environ" },
#define X_ENVIRON 1
    { 0 },
};
static int		anon_fd = -1;
#ifndef MAP_ANON
#define MAP_ANON	0
#define anon_open() do {					\
	if ((anon_fd = open("/dev/zero", O_RDWR, 0)) == -1)	\
		err("open: %s", "/dev/zero");			\
} while (0)
#define anon_close() do {	\
	(void)close(anon_fd);	\
	anon_fd = -1;		\
} while (0)
#else
#define anon_open()
#define anon_close()
#endif

int
uselib(const char *path)
{
    /* map a.out file to its natural location. */
    int		fd;
    caddr_t		addr, raddr;
    struct exec	hdr;
    extern char **environ;

    if ((fd = open(path, O_RDONLY, 0)) == -1) {
	return 1;
    }
    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
	(void)close(fd);
	/*errno = x;*/
	return 1;
    }

    if (N_BADMAG(hdr)) {
	(void)close(fd);
	errno = EFTYPE;
	return 1;
    }
    if (nlist(path, nl) != 0) {
	errno = EINVAL;
	return 1;			/* XXX */
    }
    /* assume it wants to be mapped to just below its entry point.
     * It should be PIC, but just in case :)
     */
    raddr = (caddr_t)(hdr.a_entry & ~(N_PAGSIZ(hdr)-1));
    if ((addr = mmap(raddr,
		     hdr.a_text + hdr.a_data + hdr.a_bss,
		     PROT_READ|PROT_EXEC,
		     MAP_COPY|MAP_FIXED, fd, 0)) == (caddr_t)-1) {
	(void)close(fd);
	return 1;
    }
    if (addr != raddr) {
	(void) close(fd);
	errno = EFAULT;
	return 1;
    }
    if (mprotect(addr + hdr.a_text, hdr.a_data,
		 PROT_READ|PROT_WRITE|PROT_EXEC) != 0) {
	(void)close(fd);
	return 1;
    }
    anon_open();

    if (mmap(addr + hdr.a_text + hdr.a_data, hdr.a_bss,
	     PROT_READ|PROT_WRITE|PROT_EXEC,
	     MAP_ANON|MAP_COPY|MAP_FIXED,
	     anon_fd, 0) == (caddr_t)-1) {
	(void)close(fd);
	return 1;
    }
    (void)close(fd);
    anon_close();

    __asm("movl curbrk,%0" : "=r" (addr));
    *(caddr_t *) nl[0].n_value = addr;
    *(caddr_t *) nl[1].n_value = (caddr_t) environ;
#if 0
	/* Assume _DYNAMIC is the first data item */
	dp = (struct _dynamic *)(addr+hdr.a_text);

	/* Fixup __DYNAMIC structure */
	(long)dp->d_un.d_sdt += (long)addr;

	return alloc_link_map(path, sodp, smp, addr, dp);
#endif
    return 0;
}
#endif
