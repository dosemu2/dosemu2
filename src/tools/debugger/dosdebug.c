/*
 * DOSEMU debugger,  1995 Max Parke <mhp@light.lightlink.com>
 *
 * This is file dosdebug.c
 *
 * Terminal client for DOSEMU debugger v0.2
 * by Hans Lermen <lermen@elserv.ffm.fgan.de>
 * It uses /var/run/dosemu.dbgXX.PID for connections.
 *
 * The switch-console code is from Kevin Buhr <buhr@stat.wisc.edu>
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>	/* for struct timeval */
#include <time.h>	/* for CLOCKS_PER_SEC */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#include <sys/vt.h>
#include <sys/ioctl.h>

#define    TMPFILE_VAR		"/var/run"
#define    TMPFILE_HOME		".dosemu/run"

#define DBGFNIN "dosemu.dbgin."
#define DBGFNOUT "dosemu.dbgout."

#define MHP_BUFFERSIZE 8192

#define FOREVER ((((unsigned int)-1) >> 1) / CLOCKS_PER_SEC)
#define KILL_TIMEOUT 2

fd_set readfds;
struct timeval timeout;
int kill_timeout=FOREVER;

static  char *pipename_in, *pipename_out;
int fdout, fdin;


static int check_pid(int pid)
{
  int fd;
  char buf[32], name[128];

  memset(buf, 0, sizeof(buf));
  sprintf(name, "/proc/%d/stat", pid);
  if ((fd = open(name, O_RDONLY)) ==-1) return 0;
  if (read(fd, buf, sizeof(buf)-1) <=0) {
    close(fd);
    return 0;
  }
  close(fd);
  return strstr(buf,"dos") ? 1 : 0;
}

static int filter_dosemu_zombies(const struct dirent *entry) {
	char *dot;
	int pid;

	if(strncmp(entry->d_name, DBGFNIN, strlen(DBGFNIN)) != 0) {
		return 0;
	}

	dot = strrchr(entry->d_name, '.');
	if(!dot)
		return 0;

	pid = atoi(dot+1);
	if(!pid)
		return 0;

	return check_pid(pid);
}

static int find_dosemu_pid(char *dir) {
	struct dirent **namelist;
	char *dot;
	int i, n, pid = 0;

	n = scandir(dir, &namelist, &filter_dosemu_zombies, alphasort);
	if (n < 0) {
		fprintf(stderr, "find_dosemu_pid: scandir error\n");
	} else if (n == 0) {
		fprintf(stderr, "find_dosemu_pid: no dosemu process found\n");
	} else {
		/* hook up to the first found */
		dot = strrchr(namelist[0]->d_name, '.');
		pid = atoi(dot+1);

		if (n>1) {
			fprintf(stderr,
"Multiple dosemu processes running. Using the first process found or you can\n"
"restart dosdebug with one of the following pids as the first arg:");
			for(i=0; i<n ; i++) {
				dot = strrchr(namelist[i]->d_name, '.');
				fprintf(stderr, " %s", dot+1);
			}
			fprintf(stderr, "\n");
		}
		/* free the structure */
		while (n--) {
			free(namelist[n]);
		}
		free(namelist);
	}

	return pid;
}

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
  char buf[MHP_BUFFERSIZE];
  static char sbuf[MHP_BUFFERSIZE]="\n";
  static int sn=1;
  int n, ret;

  n=read(0, buf, sizeof(buf));
  if (n>0) {
    if (n==1 && buf[0]=='\n') {
      ret = write(fdout, sbuf, sn);
      if (ret < 0)
        perror("write to fdout failed");
    } else {
      if (!strncmp(buf,"console ",8)) {
        switch_console(buf[8]);
        return;
      }
      if (!strncmp(buf,"kill",4)) {
        kill_timeout=KILL_TIMEOUT;
      }
      ret = write(fdout, buf, n);
      if (ret < 0)
        perror("write to fdout failed");
      memcpy(sbuf, buf, n);
      sn=n;
      if (buf[0] == 'q') exit(1);
    }
  }
}


static void handle_dbg_input(void)
{
  char buf[MHP_BUFFERSIZE], *p;
  int n, ret;
  do {
    n=read(fdin, buf, sizeof(buf));
  } while (n < 0 && errno == EAGAIN);
  if (n >0) {
    if ((p=memchr(buf,1,n))!=NULL) n=p-buf;
    ret = write(1, buf, n);
    if (ret < 0)
      perror("write to stdout failed");
    if (p!=NULL) exit(0);
  }
  if (n == 0)
    exit(1);
}


int main (int argc, char **argv)
{
  int numfds, ret, dospid = 0;
  char *home_p;

  FD_ZERO(&readfds);

  home_p = getenv("HOME");

  if ((argc > 1) && argv[1]) {
    dospid=strtol(argv[1], 0, 0);
    if (!check_pid(dospid)) {
      fprintf(stderr, "no dosemu running on pid %d\n", dospid);
      exit(1);
    }
  }

  if (home_p) {
    char *dosemu_tmpfile_pat;

    ret = asprintf(&dosemu_tmpfile_pat, "%s/" TMPFILE_HOME, home_p);
    assert(ret != -1);

    dospid=find_dosemu_pid(dosemu_tmpfile_pat);
    free(dosemu_tmpfile_pat);
  }
  if (dospid < 1) {
    dospid=find_dosemu_pid(TMPFILE_VAR);
  }

  /* NOTE: need to open read/write else O_NONBLOCK would fail to open */
  fdout = -1;
  if (home_p) {
    ret = asprintf(&pipename_in, "%s/%s/%s%d", home_p, TMPFILE_HOME, DBGFNIN, dospid);
    assert(ret != -1);

    ret = asprintf(&pipename_out, "%s/%s/%s%d", home_p, TMPFILE_HOME, DBGFNOUT, dospid);
    assert(ret != -1);

    fdout = open(pipename_in, O_RDWR | O_NONBLOCK);
    if (fdout == -1) {
      free(pipename_in);
      free(pipename_out);
    }
  }
  if (fdout == -1) {
    /* if we cannot open pipe and we were trying $HOME/.dosemu/run directory,
       try with /var/run/dosemu directory */
    ret = asprintf(&pipename_in, "%s/%s%d", TMPFILE_VAR, DBGFNIN, dospid);
    assert(ret != -1);

    ret = asprintf(&pipename_out, "%s/%s%d", TMPFILE_VAR, DBGFNOUT, dospid);
    assert(ret != -1);

    fdout = open(pipename_in, O_RDWR | O_NONBLOCK);
  }
  if (fdout == -1) {
    perror("can't open output fifo");
    exit(1);
  }
  if ((fdin = open(pipename_out, O_RDONLY | O_NONBLOCK)) == -1) {
    close(fdout);
    perror("can't open input fifo");
    exit(1);
  }

  ret = write(fdout,"r0\n",3);
  if (ret < 0)
    perror("write to fdout failed");

  do {
    FD_SET(fdin, &readfds);
    FD_SET(0, &readfds);   /* stdin */
    timeout.tv_sec=kill_timeout;
    timeout.tv_usec=0;
    numfds=select( fdin+1 /* max number of fds to scan */,
                   &readfds,
                   NULL /*no writefds*/,
                   NULL /*no exceptfds*/, &timeout);
    if (numfds > 0) {
      if (FD_ISSET(0, &readfds)) handle_console_input();
      if (FD_ISSET(fdin, &readfds)) handle_dbg_input();
    }
    else {
      if (kill_timeout != FOREVER) {
        if (kill_timeout > KILL_TIMEOUT) {
          struct stat st;
          int key;
          if (stat(pipename_in,&st) != -1) {
            fprintf(stderr, "...oh dear, have to do kill SIGKILL\n");
            kill(dospid, SIGKILL);
            fprintf(stderr, "dosemu process (pid %d) is killed\n",dospid);
            fprintf(stderr, "If you want to switch to another console,\n"
                            "then enter a number between 1..8, else just type enter:\n");
            key=fgetc(stdin);
            if ((key>='1') && (key<='8')) switch_console(key);
            fprintf(stderr,
              "dosdebug terminated\n"
              "NOTE: If you had a totally locked console,\n"
              "      you may have to blindly type in 'kbd -a; texmode'\n"
              "      on the console you switched to.\n"
            );
          }
          else fprintf(stderr, "dosdebug terminated, dosemu process (pid %d) is killed\n",dospid);
          exit(1);
        }
        fprintf(stderr, "no reaction, trying kill SIGTERM\n");
        kill(dospid, SIGTERM);
        kill_timeout += KILL_TIMEOUT;
      }
    }
  } while (1);
  return 0;
}
