#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


static void usage(void)
{
  fprintf(stderr,
    "USAGE:\n"
    "   dexeconfig {-i|-x} [-M] configfile dexefile\n\n"
  );
  exit(1);
}



int insert = 0, makemagic = 0;
char *cfile=0, *dexefile=0;

#define DEXE_CONFBUF_SIZE	0x2000
#define DEXE_CONF_START		0x280
#define DEXE_MAGIC		0x5845440e /* 0x0e,'D','E','X' */
#define HDIMAGE_MAGIC		0x45534f44 /* 'D','O','S','E' */

static char buf[DEXE_CONFBUF_SIZE];

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

  optind = 0;
  while ((c = getopt(argc, argv, "i:x:M")) !=EOF) {
    switch (c) {
      case 'i':
        cfile = optarg;
	insert = 1;
        break;
      case 'x':
        cfile = optarg;
        break;
      case 'M':
        makemagic = 1;
        break;
      default: usage();
    }
  }

  dexefile = argv[optind];
  if (!dexefile || !cfile) usage();

  if (insert) putconf();
  else getconf();
  return 0;
}
