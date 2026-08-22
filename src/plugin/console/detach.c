#include "emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/vt.h>
#include <sys/kd.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/ioctl.h>

#include "priv.h"

/*
 * Update to do console disallocation on exit of dosemu
 * (C) 1994 under GPL: Wayne Meissner
 *
 * $Id$
 */

/* the new VT for dosemu - only needed by detach() and disallocate_vt() */
static int dosemu_vt = 0;

static struct stat orig_stat; /* original info of the VT */

/* One of these has to work */
static const char * CONSOLE[] =   {
#ifdef __linux__
  "/dev/console",
  "/dev/tty0",
  "/dev/vt00",
  "/dev/systty",
#endif
  0
};

static const char vt_base[] = "/dev/tty";

/* open a specific VT */

static int open_vt (int vt)  {
  char path[MAXPATHLEN];
  int fd;

  sprintf(path, "%s%d", vt_base, vt);
  enter_priv_on();
  fd = open (path, O_RDWR);
  leave_priv_setting();
  return fd;
}

/* open the main console */
static int open_console (void)  {
  int console = -1;
  int pos;
  for (pos = 0; CONSOLE[pos]; pos++)  {
    errno = 0;
    enter_priv_on();
    console = open (CONSOLE[pos], O_WRONLY);
    leave_priv_setting();
    if (console >= 0)  {
      return console;
    }
  }
  return -1;
}

unsigned short detach (void) {

  struct vt_stat vts;
  pid_t ppid, ppgid;
  int fd;
    struct stat statout, staterr;

  if ((fd = open_console()) < 0) {
    fprintf(stderr, "Could not open current VT.\n");
    return(0);
  }

  if (ioctl(fd, VT_GETSTATE, &vts) < 0) {
    perror("VT_GETSTATE");
    close(fd);
    return(0);
  }

  if (ioctl(fd, VT_OPENQRY, &dosemu_vt) < 0) {
    perror("VT_OPENQRY");
    close(fd);
    return(0);
  }

  if (dosemu_vt < 1) {
    fprintf(stderr, "No free vts to open\n");
    close(fd);
    return(0);
  }

  /* change PGID to the parent's PID to be able to do setsid()
     without fork(), technique borrowed from X server */
  ppid = getppid();
  ppgid = getpgid(ppid);
  setpgid(0, ppgid);
  if (setsid() < 0) {
    perror("setsid");
    close(fd);
    return(0);
  }

  // after setsid this will automatically be the
  // controlling terminal
  fd = open_vt(dosemu_vt);
  if (fd < 0) {
    perror("open_vt");
    return(0);
  }

  if (ioctl(fd, VT_ACTIVATE, dosemu_vt) < 0) {
    perror("VT_ACTIVATE");
    close(fd);
    return(0);
  }

  if (ioctl(fd, VT_WAITACTIVE, dosemu_vt) < 0) {
    perror("VT_WAITACTIVE");
    close(fd);
    return(0);
  }

  close(console_fd);
  console_fd = fd;

  /* only reassign stderr to the new VT if it isn't already redirected */
  fstat(2, &statout);
  fstat(1, &staterr);
  if (staterr.st_ino == statout.st_ino) {
    close (2);
    open_vt (dosemu_vt);
  }


  close(1);
  close(0);

  open_vt (dosemu_vt);
  open_vt (dosemu_vt);

  /* save the uid, gid, mode of the VT */
  fstat (0, &orig_stat);

  /* now set the console as owned by this user */
  int r1 = fchown (0, get_orig_uid(), get_orig_gid()); (void)r1;

  /* set the permissions to stop other people accessing the vt */
  fchmod (0, S_IRUSR | S_IWUSR);

  return(vts.v_active); /* return old VT. */
}


void restore_vt (unsigned short vt) {

  int console = 0;  /* stdin by default */
  errno = 0;

  if (ioctl(console, VT_ACTIVATE, vt) < 0) {

    /* open the console manually and try again */
    console = open_console();
    if (console < 0) {
      perror("VT_ACTIVATE(console not open)");
      return;
    }

    if (ioctl(console, VT_ACTIVATE, vt) < 0) {
      perror("VT_ACTIVATE");
      close (console);
      return;
    }
  }

  if (ioctl(console, VT_WAITACTIVE, vt) < 0) {
    perror("VT_WAITACTIVE");
    if (console > 0)
      close(console);
    return;
  }
  if (console > 0)
    close (console);

}

/* its not really critical if this succeeds */
void disallocate_vt (void) {
#ifdef VT_DISALLOCATE /* only use for >1.1.54 */
  int console;
  int vt_fd;
  struct stat statout, staterr;


  /* Restore the uid, gid, mode of the VC */
  if ((vt_fd = open_vt (dosemu_vt)) >= 0) {
    int r2 = fchown (vt_fd, orig_stat.st_uid, orig_stat.st_gid); (void)r2;
    fchmod (vt_fd, orig_stat.st_mode);
    close (vt_fd);
  }


  /* We have to close all fd attached to the console.
   * I think this should really be taken care of by the caller (leavedos()?)
   */
  fstat(2, &statout);
  fstat(1, &staterr);
  if (staterr.st_ino == statout.st_ino) {
    close (2);
  }
  close (1);
  close (0);

  console = open_console();

  if (console < 0) {
    return;
  }


  if (ioctl (console, VT_DISALLOCATE, dosemu_vt) < 0) {
    perror ("VT_DISALLOCATE");
    close (console);
    return;
  }

  close (console);
#endif /* VT_DISALLOCATE */
}
