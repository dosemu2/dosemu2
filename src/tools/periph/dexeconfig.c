/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


static void usage(void)
{
  fprintf(stderr,
    "USAGE:\n"
    "   dexeconfig [-M] [-p {w|W}] -i configfile dexefile\n"
    "   dexeconfig -x configfile dexefile\n"
    "   dexeconfig -v  dexefile\n"
    "   dexeconfig -p {w|W}  dexefile\n\n"
    "where is\n"
    "   -i      insert a config file\n"
    "   -x      extract a config file\n"
    "   -p w    clear write permission\n"
    "   -p W    set write permission\n"
    "   -v      view status information\n"
  );
  exit(1);
}



int insert = 0, makemagic = 0;
int extract =0;
int view = 0;
int setflags = 0;
char *cfile=0, *dexefile=0;

#define DEXE_CONFBUF_SIZE	0x2000
#define DEXE_CONF_START		0x280
#define DEXE_MAGIC		0x5845440e /* 0x0e,'D','E','X' */
#define HDIMAGE_MAGIC		0x45534f44 /* 'D','O','S','E' */

static char buf[DEXE_CONFBUF_SIZE];


/* definitions for 'dexeflags' in 'struct disk' and 'struct image_header' */
#define  DISK_IS_DEXE           1
#define  DISK_DEXE_RDWR         2

struct image_header {
  char sig[7];			/* always set to "DOSEMU", null-terminated
				   or to "\x0eDEXE" */
  long heads;
  long sectors;
  long cylinders;
  long header_end;	/* distance from beginning of disk to end of header
			 * i.e. this is the starting byte of the real disk
			 */
  char dummy[1];	/* someone did define the header unaligned,
  			 * we correct that atleast for the future
  			 */
  long dexeflags;
} __attribute__((packed)) ;

static void viewconf(void)
{
  int fd, size;
  struct image_header hdr;

  fd = open(dexefile, O_RDONLY);
  if (fd < 0) {
    perror("cannot open DEXE file");
    exit(1);
  }
  if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    fprintf(stderr, "not a DEXE file\n");
    close(fd);
    exit(1);
  }
  if (*((uint32_t *)(&hdr.sig)) != DEXE_MAGIC) {
    fprintf(stderr, "not a DEXE file\n");
    close(fd);
    exit(1);
  }
  printf(
	"\nConfiguration of DEXE file %s:\n\n"
	"   headersize:% 8ld bytes\n"
	"   heads:     % 8ld\n"
	"   sectors:   % 8ld\n"
	"   cylinders: % 8ld\n"
	"   total:     % 8ld Kbyte\n"
	"   dexeflags: %8s\n",
	dexefile, hdr.header_end,
	hdr.heads, hdr.sectors, hdr.cylinders,
	(hdr.heads * hdr.sectors * hdr.cylinders) /2,
	(hdr.dexeflags & DISK_DEXE_RDWR) ? "read/write" : "none"
  );
  lseek(fd, DEXE_CONF_START, SEEK_SET);
  read(fd, &size, 4);
  size = read(fd, buf, size);
  if (size == -1) {
    perror("read error on DEXE file");
    close(fd);
    exit(1);
  }
  close(fd);
  printf("\ncontents of imbedded .dosrc:\n\n");
  fwrite(buf, size, 1, stdout);
}

static void changeflags(char *flags)
{
  int fd;
  struct image_header hdr;

  fd = open(dexefile, O_RDONLY);
  if (fd < 0) {
    perror("cannot open DEXE file");
    exit(1);
  }
  if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    fprintf(stderr, "not a DEXE file\n");
    close(fd);
    exit(1);
  }
  close(fd);
  if (*((uint32_t *)(&hdr.sig)) != DEXE_MAGIC) {
    fprintf(stderr, "not a DEXE file\n");
    exit(1);
  }

  switch (flags[0]) {
    case 'w':		/* delete write flag */
      hdr.dexeflags &= ~DISK_DEXE_RDWR;
      break;
    case 'W':		/* set write flag */
      hdr.dexeflags |= DISK_DEXE_RDWR;
      break;
    default:
      fprintf(stderr, "illegal flag setting\n");
      exit(1);
  }

  fd = open(dexefile, O_RDWR);
  if (fd < 0) {
    close(fd);
    perror("cannot open DEXE file");
    exit(1);
  }
  if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    perror("cannot update DEXE file");
  }
  close(fd);
}


static void putconf(void)
{
  int fd, fin, size;

  fin = open(cfile, O_RDONLY);
  if (fin < 0) {
    perror("cannot open config file");
    exit(1);
  }
  fd = open(dexefile, O_RDWR);
  if (fd < 0) {
    close(fin);
    perror("cannot open DEXE file");
    exit(1);
  }
  read(fd, &size, 4);
  if (makemagic) {
    if (size != DEXE_MAGIC) {
      if (size != HDIMAGE_MAGIC) {
        fprintf(stderr, "not a HDIMAGE file\n");
        close(fd);
        close(fin);
        exit(1);
      }
      lseek(fd, 0, SEEK_SET);
      size = DEXE_MAGIC;
      write(fd, &size, 4);
      size = 'E';
      write(fd, &size, 2);
    }
  }
  else {
    if (size != DEXE_MAGIC) {
      fprintf(stderr, "not a DEXE file\n");
      close(fd);
      close(fin);
      exit(1);
    }
  }

  size = read(fin, buf, DEXE_CONFBUF_SIZE -4);
  if (size == -1) {
    perror("read error on config file");
    close(fd);
    close(fin);
    exit(1);
  }
  lseek(fd, DEXE_CONF_START, SEEK_SET);
  write(fd, &size, 4);
  write(fd, buf, size);
  close(fd);
  close(fin);
}


static void getconf(void)
{
  int fd, fc, size;

  fd = open(dexefile, O_RDONLY);
  if (fd < 0) {
    perror("cannot open DEXE file");
    exit(1);
  }
  read(fd, &size, 4);
  if (size != DEXE_MAGIC) {
    fprintf(stderr, "not a DEXE file\n");
    close(fd);
    exit(1);
  }
  lseek(fd, DEXE_CONF_START, SEEK_SET);
  read(fd, &size, 4);
  size = read(fd, buf, size);
  if (size == -1) {
    perror("read error on DEXE file");
    close(fd);
    exit(1);
  }
  close(fd);
  fc = open(cfile, O_WRONLY | O_CREAT | O_TRUNC);
  if (fc < 0) {
    perror("cannot open config file");
    exit(1);
  }
  write(fc, buf, size);
  close(fc);
}



int main(int argc, char **argv)
{
  char c;
  char *flags=0;

  optind = 0;
  while ((c = getopt(argc, argv, "vi:x:Mp:")) !=EOF) {
    switch (c) {
      case 'i':
        cfile = optarg;
	insert = 1;
        break;
      case 'x':
        extract = 1;
        cfile = optarg;
        break;
      case 'M':
        makemagic = 1;
        break;
      case 'v':
        view = 1;
        break;
      case 'p':
        setflags = 1;
        flags = optarg;
        break;
      default: usage();
    }
  }

  dexefile = argv[optind];
  if (!dexefile) usage();

  if ((insert || extract) && !cfile) usage();

  if (insert) {
    putconf();
    if (setflags) changeflags(flags);
  }
  else if (view) viewconf();
  else if (setflags) changeflags(flags);
  else if (extract) getconf();
  else usage();
  
  return 0;
}
