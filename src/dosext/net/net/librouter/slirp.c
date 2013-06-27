/*
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation,  either version 3 of the License,  or (at your option)  any later
 * version.
 *
 * This program is  distributed in the hope that it will be useful,  but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Mateusz Viste 2013. All rights reserved.
 * Contact: <mateusz$viste-family.net> (replace the $ sign by a @)
 *
 * SLIP defines two control chars:
 * END = 192
 * ESC = 219
 *
 * Escaping in-packet data:
 *  END  ->  ESC, 220
 *  ESC  ->  ESC, 221
 *
 *  When the last byte of the PACKET is sent, send an 'END' char.
 */


#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "slirp.h"  /* include self for control */

#define READ 0
#define WRITE 1


static pid_t popen2(const char *command, int *slirpfd) {
  pid_t pid;
  int flags;

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, slirpfd) != 0) {
    printf("socketpair() failed: %s\n", strerror(errno));
    return(-1);
  }

  /* mark our end of the pipe non-blocking because of the timer based poll */
  flags = fcntl(slirpfd[0], F_GETFL);
  if (flags == -1) printf("fcntl(,F_GETFL) failed: %s\n", strerror(errno));
  flags |= O_NONBLOCK;
  if (fcntl(slirpfd[0], F_SETFL, flags) != 0) printf("fcntl(,F_SETFL,) failed: %s\n", strerror(errno));

  pid = fork();

  if (pid < 0) { /* fork failed! */
      return(pid);
    } else if (pid == 0) { /* child process */
      int nfd;

      /* close the parent's end of the socket, to avoid writing any garbage to it by mistake */
      close(slirpfd[0]);

      if (dup2(slirpfd[1], STDIN_FILENO) == -1) {
        fprintf(stderr, "dup2() failed: %s\n", strerror(errno));
      }
      /* slirp seems to use stdin bidirectionally instead of using stdout for SLIP output (?) */
      if (dup2(slirpfd[1], STDOUT_FILENO) == -1) {
        fprintf(stderr, "dup2() failed: %s\n", strerror(errno));
      }

      /* redirect stderr to null, because slirp uses stderr to print out its own infos, and I don't want this stuff to be printed on user's console as coming from dosemu */
      nfd = open("/dev/null", O_RDWR);
      if (nfd != -1) {
        dup2(nfd, STDERR_FILENO);
      }

      if (execlp(command, command, NULL) < 0) {
        fprintf(stderr, "execlp() failed when calling '%s': %s\n", command, strerror(errno));
      }
      exit(0); /* we need the child to die if it doesn't work properly, otherwise the user would end up with two DOSemu consoles on his desktop */
  }
  close(slirpfd[1]);
  return(pid);
}



int slirp_open(char *slirpcmd, int *slirpfd) {
  if (popen2(slirpcmd, slirpfd) <= 0) {
    puts("Unable to exec slirp");
    return(-1);
  }
  return(0);
}

void slirp_close(int *slirpfd) {
  close(slirpfd[0]);
  close(slirpfd[1]);
}


int slirp_send(uint8_t *buff, int bufflen, int slirpfd) {
  int x;
  uint8_t escEND[] = {219,220};
  uint8_t escESC[] = {219,221};
  uint8_t end[] = {192};
  /* printf("slirp_send (%d bytes)\n", bufflen); */
  /* send the 'end' char to flush any possible noise */
  if (write(slirpfd, end, 1) != 1) return(-1);
  for (x = 0; x < bufflen; x++) {
    switch (buff[x]) {
      case 192:  /* reserved 'END' char */
        if (write(slirpfd, escEND, 2) != 2) return(-1);
        break;
      case 219:  /* reserved 'ESC' char */
        if (write(slirpfd, escESC, 2) != 2) return(-1);
        break;
      default:
        if (write(slirpfd, &buff[x], 1) != 1) return(-1);
        break;
    }
  }
  /* send the 'end' char to mark end of packet */
  if (write(slirpfd, end, 1) != 1) return(-1);
  /* printf("sent: ");
  for (x = 0; x < bufflen; x++) printf("%02X", buff[x]);
  puts(""); */
  return(0);
}


int slirp_read(uint8_t *buff, int slirpfd) {
  int x;
  /* puts("slirp_read()"); */
  for (x = 0;; x++) {
    read(slirpfd, &buff[x], 1);
    if (buff[x] == 192) return(x);
    if (buff[x] == 219) {
      read(slirpfd, &buff[x], 1);
      if (buff[x] == 220) {
          buff[x] = 192;
        } else {
          buff[x] = 219;
      }
    }
  }
}
