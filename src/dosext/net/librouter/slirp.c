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
#include <sys/socket.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "slirp.h"  /* include self for control */

static int slirp_fd[2];
static int slirp_stderr[2];

static pid_t slirp_popen2(const char *command, int *slirpfd, int *slirp_pipe) {
  pid_t pid;
  int flags;

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, slirpfd) || pipe(slirp_pipe)) {
    /* printf("socketpair() failed: %s\n", strerror(errno)); */
    return(-1);
  }

  /* mark our end of the pipe non-blocking */
  flags = fcntl(slirpfd[0], F_GETFL);
  /* if (flags == -1) printf("fcntl(,F_GETFL) failed: %s\n", strerror(errno)); */
  flags |= O_NONBLOCK;
  if (fcntl(slirpfd[0], F_SETFL, flags) != 0) {
    /* printf("fcntl(,F_SETFL,) failed: %s\n", strerror(errno)); */
  }

  pid = fork();

  if (pid < 0) { /* fork failed! */
      return(pid);
    } else if (pid == 0) { /* child process */
      int execres;

      /* close the parent's end of the socket, to avoid writing any garbage to it by mistake */
      close(slirpfd[0]);
      close(slirp_pipe[0]);

      dup2(slirpfd[1], STDIN_FILENO);
      /* slirp seems to use stdin bidirectionally instead of using stdout for SLIP output (?) */
      dup2(slirpfd[1], STDOUT_FILENO);
      dup2(slirp_pipe[1], STDERR_FILENO);

      if (command && command[0]) { /* if the user provided an executable, run it */
        execres = execlp(command, command, NULL);
      } else {
        /* if above command hasn't worked, or user haven't provided any command, try a hardcoded value */
        execres = execlp("slirp-fullbolt", "slirp-fullbolt", NULL);
        /* if above command hasn't worked, try another hardcoded value */
        execres = execlp("slirp", "slirp", NULL);
      }
      close(slirpfd[1]); /* close the communication socket before quitting */
      exit(execres);
    } else {  /* parent process */
      close(slirpfd[1]);
      close(slirp_pipe[1]);
      return(pid);
  }
}



pid_t librouter_slirp_open(char *slirpcmd, int *r_slirpfd) {
  char buf[1024];
  const char *str = "Slirp";
  int n;
  pid_t pid = slirp_popen2(slirpcmd, slirp_fd, slirp_stderr);
  if (pid < 0)
    return pid;
  n = read(slirp_stderr[0], buf, sizeof(buf));
  if (n <= strlen(str) || n == sizeof(buf) ||
        strncmp(buf, str, strlen(str)) != 0) {
    close(slirp_fd[0]);
    close(slirp_stderr[0]);
    return -1;
  }
  *r_slirpfd = slirp_fd[0];
  return pid;
}


void librouter_slirp_close(void) {
  close(slirp_fd[0]);
  close(slirp_stderr[0]);
}


int librouter_slirp_send(uint8_t *buff, int bufflen, int slirpfd) {
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


int librouter_slirp_read(uint8_t *buff, int slirpfd) {
  int x;
  /* puts("slirp_read()"); */
  for (x = 0;; x++) {
    if (read(slirpfd, &buff[x], 1) != 1) return(-1);
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
