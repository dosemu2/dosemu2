#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "detect.h"

#define X_CHG_TITLE     1
#define X_CHG_FONT      2
#define X_CHG_MAP       3
#define X_CHG_UNMAP     4
#define X_CHG_WINSIZE    5

int X_change_config(unsigned, void *);

int main(int argc, char **argv)
{
  char *p;
  long l, ll[2];

  if (!is_dosemu()) {
    printf ("This program requires DOSEMU to run, aborting\n");
    exit (-1);
  }

  argv++;
  if(!--argc) {
    fprintf(stderr,
      "usage: xmode <some arguments>\n"
      "  -title <name>    set window name\n"
      "  -font <font>     use <font> as text font\n"
      "  -map <mode>      map window after graphics <mode> has been entered\n"
      "  -unmap <mode>    unmap window before graphics <mode> is left\n"
      "  -winsize <width> <height>    set initial graphics window size\n"
    );
    return 1;
  }

  while(argc) {
    if(!strcmp(*argv, "-title") && argc >= 2) {
      X_change_config(X_CHG_TITLE, argv[1]);
      argc -= 2; argv += 2;
    }
    else if(!strcmp(*argv, "-font") && argc >= 2) {
      X_change_config(X_CHG_FONT, argv[1]);
      argc -= 2; argv += 2;
    }
    else if(!strcmp(*argv, "-map") && argc >= 2) {
      l = strtol(argv[1], &p, 0);
      if(argv[1] == p) {
        fprintf(stderr, "invalid mode number \"%s\"\n", argv[1]);
        return 2;
      }
      X_change_config(X_CHG_MAP, &l);
      argc -= 2; argv += 2;
    }
    else if(!strcmp(*argv, "-unmap") && argc >= 2) {
      l = strtol(argv[1], &p, 0);
      if(argv[1] == p) {
        fprintf(stderr, "invalid mode number \"%s\"\n", argv[1]);
        return 2;
      }
      X_change_config(X_CHG_UNMAP, &l);
      argc -= 2; argv += 2;
    }
    else if(!strcmp(*argv, "-winsize") && argc >= 3) {
      ll[0] = strtol(argv[1], &p, 0);
      if(argv[1] == p) {
        fprintf(stderr, "invalid width \"%s\"\n", argv[1]);
        return 2;
      }
      ll[1] = strtol(argv[2], &p, 0);
      if(argv[2] == p) {
        fprintf(stderr, "invalid height \"%s\"\n", argv[2]);
        return 2;
      }
      X_change_config(X_CHG_WINSIZE, ll);
      argc -= 3; argv += 3;
    }
    else {
      fprintf(stderr, "Don't know what to do with argument \"%s\"; aborting here.", *argv);
      return 2;
    }
  }

  return 0;
}

int X_change_config(unsigned item, void *buf)
{
  struct REGPACK r;

  r.es = FP_SEG(buf);
  r.bx = FP_OFF(buf);
  r.dx = item;
  r.ax = 0xa0;

  intr(0xe6, &r);

  return r.ax;
} 

