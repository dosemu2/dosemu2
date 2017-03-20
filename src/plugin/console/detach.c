#include "emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/vt.h>
#include <sys/kd.h>
#endif
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

  sprintf(path, "%s%d", vt_base, vt);
  return open (path, O_RDWR);
}

/* open the main console */
static int open_console (void)  {
  int console = -1;
  int pos;
  for (pos = 0; CONSOLE[pos]; pos++)  {
    errno = 0;
    if ((console = open (CONSOLE[pos], O_WRONLY)) >= 0)  {
      return console;
    }
  }
  return -1;
}

unsigned short detach (void) {

#ifdef __linux__
  struct vt_stat vts;
#endif
  int pid;
  int fd;
    struct stat statout, staterr;

  if ((fd = open_console()) < 0) {
    fprintf(stderr, "Could not open current VT.\n");
    return(0);
  }

#ifdef __linux__
  if (ioctl(fd, VT_GETSTATE, &vts) < 0) {
    perror("VT_GETSTATE");
    return(0);
  }
#endif

  if (ioctl(fd, VT_OPENQRY, &dosemu_vt) < 0) {
    perror("VT_OPENQRY");
    return(0);
  }

  if (dosemu_vt < 1) {
    fprintf(stderr, "No free vts to open\n");
    return(0);
  }

  if (ioctl(fd, VT_ACTIVATE, dosemu_vt) < 0) {
    perror("VT_ACTIVATE");
    return(0);
  }

  if (ioctl(fd, VT_WAITACTIVE, dosemu_vt) < 0) {
    perror("VT_WAITACTIVE");
    return(0);
  }

  if ((pid = fork()) < 0) {
    perror("fork");
    return(0);
  }

  if (pid) {
    _exit(0);
  }

  close(fd);

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
  fchown (0, get_orig_uid(), get_orig_gid());

  /* set the permissions to stop other people accessing the vt */
  fchmod (0, S_IRUSR | S_IWUSR);

  setsid();
#ifdef __linux__
  return(vts.v_active); /* return old VT. */
#endif
}


void restore_vt (unsigned short vt) {

  int console = 0;  /* stdin by default */
  errno = 0;

  if (ioctl(console, VT_ACTIVATE, vt) < 0) {

    /* open the console manually and try again */
    console = open_console();
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
    fchown (vt_fd, orig_stat.st_uid, orig_stat.st_gid);
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
