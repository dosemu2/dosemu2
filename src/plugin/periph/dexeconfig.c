#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "disks.h"

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
#define HDIMAGE_MAGIC		0x45534f44 /* 'D','O','S','E' */

static char buf[DEXE_CONFBUF_SIZE];

static void viewconf(void)
{
  int fd, size;
  uint32_t sig;
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
  memcpy(&sig, &hdr.sig, 4);
  if (sig != DEXE_MAGIC) {
    fprintf(stderr, "not a DEXE file\n");
    close(fd);
    exit(1);
  }
  printf(
	"\nConfiguration of DEXE file %s:\n\n"
	"   headersize:% 8d bytes\n"
	"   heads:     % 8d\n"
	"   sectors:   % 8d\n"
	"   cylinders: % 8d\n"
	"   total:     % 8d Kbyte\n"
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
  uint32_t sig;

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
  memcpy(&sig, &hdr.sig, 4);
  if (sig != DEXE_MAGIC) {
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
  fc = creat(cfile, S_IWUSR | S_IRUSR);
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
