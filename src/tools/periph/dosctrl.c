/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DOSEMU control terminal,  1999 Hans lermen
 *
 * This is file dosctrl.c
 *
 * Terminal client for DOSEMU user hook facility
 *
 * The switch-console code is from Kevin Buhr <buhr@stat.wisc.edu>
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>

#include <sys/vt.h>
#include <sys/ioctl.h>

static void usage(void)
{
  printf(
    "USAGE:\n"
    "  dosctrl pipe_into_dosemu pipe_from_dosemu\n"
  );
  exit(1);
}



#define UHOOK_BUFFERSIZE 8192
#define FOREVER ((((unsigned long)-1L) >> 1) / CLOCKS_PER_SEC)


fd_set readfds;
struct timeval timeout;

static  char *pipename_in, *pipename_out;
int fdout, fdin;

static int switch_console(char new_console)
{
  int newvt;
  int vt;

  if ((new_console < '1') || (new_console > '8')) {
    fprintf(stderr,"wrong console number\n");
    return -1;
  }

  newvt = new_console & 15;
  vt = open( "/dev/tty1", O_RDONLY );
  if( vt == -1 ) {
    perror("open(/dev/tty1)");
    return -1;
  }
  if( ioctl( vt, VT_ACTIVATE, newvt ) ) {
    perror("ioctl(VT_ACTIVATE)");
    return -1;
  }
  if( ioctl( vt, VT_WAITACTIVE, newvt ) ) {
    perror("ioctl(VT_WAITACTIVE)");
    return -1;
  }

  close(vt);
  return 0;
}


static void handle_console_input(void)
{
  char buf[UHOOK_BUFFERSIZE];
  static char sbuf[UHOOK_BUFFERSIZE]="\n";
  static int sn=1;
  int n;
  
  n=read(0, buf, sizeof(buf));
  if (n>0) {
    if (!strncmp(buf, "q\n", 2)) exit(0);
    if (n==1 && buf[0]=='\n') write(fdout, sbuf, sn);
    else {
      if (!strncmp(buf,"console ",8)) {
        switch_console(buf[8]);
        return;
      }
      write(fdout, buf, n);
      memcpy(sbuf, buf, n);
      sn=n;
    }
  }
}


static void handle_uhook_input(void)
{
  char buf[UHOOK_BUFFERSIZE];
  int n;
  do {
    n=read(fdin, buf, sizeof(buf));
  } while (n == EAGAIN);
  if (n >0) {
    write(1, buf, n);
  }
}

int main (int argc, char **argv)
{
  int numfds;
  
  FD_ZERO(&readfds);

  if (argc <= 2) usage();

  pipename_in = argv[1];
  pipename_out = argv[2];

  /* NOTE: need to open read/write else O_NONBLOCK would fail to open */
  if ((fdout = open(pipename_in, O_RDWR | O_NONBLOCK)) == -1) {
    perror("can't open fifo to feedin commands to DOSEMU");
    exit(1);
  }
  /* NOTE: need to open read/write else if the sending process closes,
           select() will trigger as hell and eat CPU time */
  if ((fdin = open(pipename_out, O_RDWR | O_NONBLOCK)) == -1) {
    close(fdout);
    perror("can't open fifo to get results from DOSEMU");
    exit(1);
  }

  do {
    FD_SET(fdin, &readfds);
    FD_SET(0, &readfds);   /* stdin */
    timeout.tv_sec=FOREVER;
    timeout.tv_usec=0;
    numfds=select( fdin+1 /* max number of fds to scan */,
                   &readfds,
                   NULL /*no writefds*/,
                   NULL /*no exceptfds*/, &timeout);
    if (numfds > 0) {
      if (FD_ISSET(0, &readfds)) handle_console_input();
      if (FD_ISSET(fdin, &readfds)) handle_uhook_input();
    }
  } while (1);
  return 0; /* just to make gcc happy */
}
