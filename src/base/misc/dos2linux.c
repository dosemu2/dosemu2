/*
 *
 * DANG_BEGIN_MODULE
 * REMARK
 *
 * This file contains a simple system for passing information through to
 * DOSEMU from the Linux side. It does not allow dynamic message passing,
 * but it intended to provide useful information for the DOS user.
 *
 * As such, the current set of implemented commands are :
 * GET_USER_ENVVAR and GET_COMMAND
 *
 * These are made available to the DOSEMU by using the DOS_HELPER interrupt
 * (Currently 0xE6) and writing a string into a location passed to this
 * interrupt handler using the registers. In the case of GET_USER_ENVVAR the
 * string also contains the name of the environment variable to interrogate.
 * (The string is overwritten with the reply).
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 *	$Log$
 *	Revision 1.16  2006/01/28 09:45:34  bartoldeman
 *	Move disclaimer and install prompts from dos_post_boot to banner code,
 *	using BIOS calls.
 *
 *	Revision 1.15  2005/12/31 06:23:11  bartoldeman
 *	Mostly from Clarence: unix.com patches in dosemu-exec-friendly.diff,
 *	so that "dosemu keen1.exe" works again.
 *	But do not use heuristics that split options from commands, by making
 *	quotes of filenames that contain spaces compulsory.
 *
 *	Revision 1.14  2005/12/18 07:58:44  bartoldeman
 *	Put some basic checks into dos_read/dos_write, and memmove_dos2dos to
 *	avoid touching the protected video memory.
 *	Fixes #1379806 (gw stopped to work under X).
 *
 *	Revision 1.13  2005/11/29 18:24:31  stsp
 *
 *	call_msdos() must switch to real mode before calling DOS, if needed.
 *
 *	Revision 1.12  2005/11/29 10:25:04  bartoldeman
 *	Adjust video.c init so that -dumb does not pop up an X window.
 *	Make -dumb quiet until the command is executed if a command is given. So
 *	dosemu -dumb dir
 *	gives a directory listing and nothing else.
 *
 *	Revision 1.11  2005/11/21 18:12:51  stsp
 *
 *	- Map the DOS<-->unix STDOUT/STDERR properly for unix.com.
 *	- Improved the DOS console reading for unix.com
 *
 *	Revision 1.10  2005/11/19 00:54:40  stsp
 *
 *	Make unix.com "keyboard-aware" (FR #1360156). It works like a tty
 *	in a -icanon mode.
 *	system() replaced with execlp() to make it possible to emulate ^C
 *	with SIGINT.
 *	Also reworked the output-grabbing again - it was still skipping an
 *	output sometimes.
 *
 *	Revision 1.9  2005/11/18 21:47:56  stsp
 *
 *	Make unix.com to use DOS/stdout instead of the direct video mem access,
 *	so that the file redirection to work.
 *
 *	Revision 1.8  2005/11/02 04:52:55  stsp
 *
 *	Fix run_unix_command (#1345102)
 *
 *	Revision 1.7  2005/09/30 22:15:59  stsp
 *
 *	Avoid using LOWMEM for the non-const addresses.
 *	This fixes iplay (again), as it read()s directly to the EMS frame.
 *	Also, moved dos_read/write to dos2linux.c to make them globally
 *	available. And removed the __builtin_constant_p() check in LINEAR2UNIX -
 *	gcc might still be able to optimize the const addresses, but falling
 *	back for dosaddr_to_unixaddr() will happen less frequently.
 *
 *	Revision 1.6  2005/09/05 09:47:56  bartoldeman
 *	Moved much of X_change_config to dos2linux.c. Much of it will be shared
 *	with SDL.
 *
 *	Revision 1.5  2005/06/20 17:12:11  stsp
 *
 *	Remove the ancient (unused) ioctl queueing code.
 *
 *	Revision 1.4  2005/05/20 00:26:09  bartoldeman
 *	It's 2005 this year.
 *
 *	Revision 1.3  2005/03/21 17:24:09  stsp
 *
 *	Fixed (and re-enabled) the terminate-after-execute feature (bug #1152829).
 *	Also some minor adjustments/leftovers.
 *
 *	Revision 1.2  2004/01/16 20:50:27  bartoldeman
 *	Happy new year!
 *
 *	Revision 1.1.1.1  2003/06/23 00:02:07  bartoldeman
 *	Initial import (dosemu-1.1.5.2).
 *
 *
 * DANG_END_CHANGELOG
 *
 */



#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

#include "emu.h"
#include "cpu-emu.h"
#include "int.h"
#include "dpmi.h"
#include "timers.h"
#include "video.h"
#include "lowmem.h"
#include "coopth.h"
#include "utilities.h"
#include "dos2linux.h"
#include "vgaemu.h"
#include "keyb_server.h"

#include "redirect.h"
#include "../../dosext/mfs/lfn.h"
#include "../../dosext/mfs/mfs.h"

#define com_stderr      2

#ifndef max
#define max(a,b)       ((a)>(b)? (a):(b))
#endif

#define GET_USER_ENVVAR      0x52
#define GET_USER_COMMAND     0x51
#define EXEC_USER_COMMAND    0x50

#define MAX_DOS_COMMAND_LEN  256

static char *misc_dos_command;
static char *misc_dos_options;
static int exec_ux_path;
int com_errno;
static struct vm86_regs saved_regs;

int misc_e6_envvar (char *str)
{
  char *tmp;

  g_printf ("Environment Variable Check : %s", str);

  tmp = getenv (str);

  if (tmp == NULL)
  {
    str[0] = '\0';

    g_printf (" is undefined\n");

    return 1;
  } else {
    strcpy (str, tmp);

    g_printf (" is %s\n", str);

    return 0;
  }

  /* doesn't get this far */
}


int misc_e6_commandline (char *str, int *is_ux_path)
{

  g_printf ("Command Line Check : ");

  if (misc_dos_command == NULL)
  {
    str[0] = '\0';

    g_printf ("%s\n", str);

    return 1;
  } else {
    strcpy (str, misc_dos_command);
    *is_ux_path = exec_ux_path;
    g_printf ("%s\n", str);

    return 0;
  }

  /* doesn't get this far */
}

char *misc_e6_options(void)
{
  return misc_dos_options;
}

int misc_e6_need_terminate(void)
{
  return config.exit_on_cmd;
}

void misc_e6_store_command(char *str, int ux_path)
{
  size_t slen = strlen(str), olen = 0;
  if (slen > MAX_DOS_COMMAND_LEN) {
    error("DOS command line too long, exiting");
    leavedos(1);
  }
  if (misc_dos_command == NULL) {
    misc_dos_command = strdup(str);
    exec_ux_path = ux_path;

    g_printf ("Storing Command : %s\n", misc_dos_command);
    return;
  }
  /* any later arguments are collected as DOS options */
  if (misc_dos_options)
    olen = strlen(misc_dos_options);
  misc_dos_options = realloc(misc_dos_options, olen + slen + 2);
  misc_dos_options[olen] = ' ';
  memcpy(misc_dos_options + olen + 1, str, slen + 1);
  if (strlen(misc_dos_command) + olen + slen + 2 > MAX_DOS_COMMAND_LEN) {
    error("DOS command line too long, exiting");
    leavedos(1);
  }
  g_printf ("Storing Options : %s\n", misc_dos_options);
}

static char *make_end_in_backslash (char *s)
{
  int len = strlen (s);
  if (len && s [len - 1] != '/')
    strcpy (s + len, "/");
  return s;
}


/*
 * Return the drive from which <linux_path_resolved> is accessible.
 * If there is no such redirection, it returns the next available drive * -1.
 * If there are no available drives (from >= 2 aka "C:"), it returns -26.
 * If an error occurs, it returns -27.
 *
 * In addition, if a drive is available, <linux_path_resolved> is modified
 * to have the drive's uncannonicalized linux root as its prefix.  This is
 * necessary as make_unmake_dos_mangled_path() will not work with a resolved
 * path if the drive was LREDIR'ed by the user to a unresolved path.
 */
int find_drive (char **plinux_path_resolved)
{
  int drive;
  char *linux_path_resolved = *plinux_path_resolved;

  j_printf ("find_drive (linux_path='%s')\n", linux_path_resolved);

  for (drive = 0; drive < 26; drive++) {
    char *drive_linux_root = NULL;
    int drive_ro, ret;
    char *drive_linux_root_resolved;

    if (GetRedirectionRoot (drive, &drive_linux_root, &drive_ro) == 0/*success*/) {
      drive_linux_root_resolved = canonicalize_file_name(drive_linux_root);
      if (!drive_linux_root_resolved) {
        com_fprintf (com_stderr,
                     "ERROR: %s.  Cannot canonicalize drive root path.\n",
                     strerror (errno));
        return -27;
      }

      /* make sure drive root ends in / */
      make_end_in_backslash (drive_linux_root_resolved);

      j_printf ("CMP: drive=%i drive_linux_root='%s' (resolved='%s')\n",
                 drive, drive_linux_root, drive_linux_root_resolved);

      /* TODO: handle case insensitive filesystems (e.g. VFAT)
       *     - can we just strlwr() both paths before comparing them? */
      if (strstr (linux_path_resolved, drive_linux_root_resolved) == linux_path_resolved) {
        j_printf ("\tFound drive!\n");
        ret = asprintf (plinux_path_resolved, "%s%s",
                  drive_linux_root/*unresolved*/,
                  linux_path_resolved + strlen (drive_linux_root_resolved));
        assert(ret != -1);

        j_printf ("\t\tModified root; linux path='%s'\n", *plinux_path_resolved);
	free (linux_path_resolved);

	free (drive_linux_root_resolved);
        free (drive_linux_root);
        return drive;
      }

      free (drive_linux_root_resolved);
      free (drive_linux_root);
    }
  }

  j_printf("find_drive() not found\n");
  return -26;
}

int find_free_drive(void)
{
  int drive;

  for (drive = 2; drive < 26; drive++) {
    char *drive_linux_root;
    int drive_ro, ret;

    ret = GetRedirectionRoot(drive, &drive_linux_root, &drive_ro);
    if (ret != 0)
      return drive;
    else
      free(drive_linux_root);
  }

  return -1;
}

static int pty_fd;
static int pty_done;
static int cbrk;

static void pty_thr(void)
{
    char buf[128];
    fd_set rfds;
    struct timeval tv;
    int retval, rd, wr;
    while (1) {
	rd = wr = 0;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&rfds);
	FD_SET(pty_fd, &rfds);
	retval = RPT_SYSCALL(select(pty_fd + 1, &rfds, NULL, NULL, &tv));
	switch (retval) {
	case -1:
	    g_printf("run_unix_command(): select error %s\n", strerror(errno));
	    pty_done++;
	    break;
	case 0:
	    break;
	default:
	    /* one of the pipes has data, or EOF */
	    rd = RPT_SYSCALL(read(pty_fd, buf, sizeof(buf)));
	    switch (rd) {
	    case -1:
		g_printf("run_unix_command(): read error %s\n", strerror(errno));
		/* no break */
	    case 0:
		pty_done++;
		break;
	    default:
		com_doswritecon(buf, rd);
		break;
	    }
	    break;
	}
	if (pty_done)
	    return;

	wr = com_dosreadcon(buf, sizeof(buf));
	if (wr > 0)
	    write(pty_fd, buf, wr);

	if (!rd && !wr)
	    coopth_wait();
    }
}

int dos2tty_init(void)
{
    pty_fd = posix_openpt(O_RDWR);
    if (pty_fd == -1)
    {
        error("openpt failed %s\n", strerror(errno));
        return -1;
    }
    unlockpt(pty_fd);
    return 0;
}

void dos2tty_done(void)
{
    close(pty_fd);
}

static int dos2tty_open(void)
{
    int err, pts_fd;
    err = grantpt(pty_fd);
    if (err) {
	error("grantpt failed: %s\n", strerror(errno));
	return err;
    }
    pts_fd = open(ptsname(pty_fd), O_RDWR);
    if (pts_fd == -1) {
	error("pts open failed: %s\n", strerror(errno));
	return -1;
    }
    return pts_fd;
}

static void dos2tty_start(void)
{
    char a;
    int rd;
    cbrk = com_setcbreak(0);
    /* flush pending input first */
    do {
	rd = com_dosreadcon(&a, 1);
    } while (rd > 0);
    pty_done = 0;
    /* must run with interrupts enabled to read keypresses */
    set_IF();
    pty_thr();
}

static void dos2tty_stop(void)
{
    clear_IF();
    com_setcbreak(cbrk);
}

int run_unix_command(char *buffer)
{
    /* unix command is in a null terminate buffer pointed to by ES:DX. */

    /* IMPORTANT NOTE: euid=user uid=root (not the other way around!) */

    int pts_fd;
    int pid, status, retval, wt;
    sigset_t set, oset;
    struct timespec to = { 0, 0 };

    g_printf("UNIX: run '%s'\n",buffer);
#if 0
    dos2tty_init();
#endif
    /* open pts in parent to avoid reading it before child opens */
    pts_fd = dos2tty_open();
    if (pts_fd == -1) {
	error("run_unix_command(): open pts failed %s\n", strerror(errno));
	return -1;
    }
    sigemptyset(&set);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, &oset);
    /* fork child */
    switch ((pid = fork())) {
    case -1: /* failed */
	sigprocmask(SIG_SETMASK, &oset, NULL);
	g_printf("run_unix_command(): fork() failed\n");
	return -1;
    case 0: /* child */
	priv_drop();
	setsid();	// will have ctty
	close(0);
	close(1);
	close(2);
	dup(pts_fd);
	dup(pts_fd);
	dup(pts_fd);
	close(pts_fd);
	/* close signals, then unblock */
	signal_done();
	ioselect_done();
	/* flush pending signals */
	do {
	    wt = sigtimedwait(&set, NULL, &to);
	} while (wt != -1);
	sigprocmask(SIG_SETMASK, &oset, NULL);

	setenv("LC_ALL", "C", 1);	// disable i18n
	retval = execlp("/bin/sh", "/bin/sh", "-c", buffer, NULL);	/* execute command */
	error("exec /bin/sh failed\n");
	_exit(retval);
	break;
    }
    close(pts_fd);
    sigprocmask(SIG_SETMASK, &oset, NULL);

    assert(!isset_IF());
    dos2tty_start();
    while ((retval = waitpid(pid, &status, WNOHANG)) == 0)
	coopth_wait();
    if (retval == -1)
	error("waitpid: %s\n", strerror(errno));
    dos2tty_stop();
#if 0
    dos2tty_done();
#endif
    /* print child exitcode. not perfect */
    g_printf("run_unix_command() (parent): child exit code: %i\n",
            WEXITSTATUS(status));
    return WEXITSTATUS(status);
}

/*
 * This function provides parts of the interface to reconfigure parts
 * of X/SDL and the VGA emulation during a DOSEMU session.
 * It is used by the xmode.exe program that comes with DOSEMU.
 */
int change_config(unsigned item, void *buf, int grab_active, int kbd_grab_active)
{
  static char title_emuname [TITLE_EMUNAME_MAXLEN] = {0};
  static char title_appname [TITLE_APPNAME_MAXLEN] = {0};
  int err = 0;

  g_printf("change_config: item = %d, buffer = %p\n", item, buf);

  switch(item) {

    case CHG_TITLE:
      {
	/* high-level write (shows name of emulator + running app) */
	char title [TITLE_EMUNAME_MAXLEN + TITLE_APPNAME_MAXLEN + 35] = {0};
	wchar_t wtitle [sizeof(title)];
	char *unixptr = NULL;
	char *s;

	/* app - DOS in a BOX */
	/* name of running application (if any) */
	if (config.X_title_show_appname && strlen (title_appname))
	  strcpy (title, title_appname);

	/* append name of emulator */
	if (strlen (title_emuname)) {
	    if (strlen (title)) strcat (title, " - ");
	    strcat (title, title_emuname);
	} else if (strlen (config.X_title)) {
	  if (strlen (title)) strcat (title, " - ");
	  unixptr = title + strlen (title);
	  /* foreign string, cannot trust its length to be <= TITLE_EMUNAME_MAXLEN */
	  snprintf (unixptr, TITLE_EMUNAME_MAXLEN, "%s ", config.X_title);
	}

	if (dosemu_frozen) {
	  if (strlen (title)) strcat (title, " ");

	  if (dosemu_user_froze)
	    strcat (title, "[paused - Ctrl+Alt+P] ");
	  else
	    strcat (title, "[background pause] ");
	}

	if (grab_active || kbd_grab_active) {
	  strcat(title, "[");
	  if (kbd_grab_active) {
	    strcat(title, "keyboard");
	    if (grab_active)
	      strcat(title, "+");
	  }
	  if (grab_active)
	    strcat(title, "mouse");
	  strcat(title, " grab] ");
	}

	/* now actually change the title of the Window */
	if (unixptr == NULL)
	  unixptr = strchr(title, '\0');
	for (s = title; s < unixptr; s++)
	  wtitle[s - title] = dos_to_unicode_table[(unsigned char)*s];
	wtitle[unixptr - title] = 0;
	if (*unixptr) {
	  if (mbstowcs(&wtitle[unixptr - title], unixptr, TITLE_EMUNAME_MAXLEN)
	      == -1)
	    wtitle[unixptr - title] = 0;
	  wtitle[unixptr - title + TITLE_EMUNAME_MAXLEN] = 0;
	}
	Video->change_config (CHG_TITLE, wtitle);
      }
       break;

    case CHG_TITLE_EMUNAME:
      g_printf ("change_config: emu_name = %s\n", (char *) buf);
      snprintf (title_emuname, TITLE_EMUNAME_MAXLEN, "%s", ( char *) buf);
      Video->change_config (CHG_TITLE, NULL);
      break;

    case CHG_TITLE_APPNAME:
      g_printf ("change_config: app_name = %s\n", (char *) buf);
      snprintf (title_appname, TITLE_APPNAME_MAXLEN, "%s", (char *) buf);
      Video->change_config (CHG_TITLE, NULL);
      break;

    case CHG_TITLE_SHOW_APPNAME:
      g_printf("change_config: show_appname %i\n", *((int *) buf));
      config.X_title_show_appname = *((int *) buf);
      Video->change_config (CHG_TITLE, NULL);
      break;

    case CHG_WINSIZE:
      config.X_winsize_x = *((int *) buf);
      config.X_winsize_y = ((int *) buf)[1];
      g_printf("change_config: set initial graphics window size to %d x %d\n", config.X_winsize_x, config.X_winsize_y);
      break;

    case CHG_BACKGROUND_PAUSE:
      g_printf("change_config: background_pause %i\n", *((int *) buf));
      config.X_background_pause = *((int *) buf);
      break;

    case GET_TITLE_APPNAME:
      snprintf (buf, TITLE_APPNAME_MAXLEN, "%s", title_appname);
      break;

    default:
      err = 100;
  }

  return err;
}

void memcpy_2unix(void *dest, dosaddr_t src, size_t n)
{
  if (vga.inst_emu && src >= 0xa0000 && src < 0xc0000)
    memcpy_from_vga(dest, src, n);
  else while (n) {
    /* EMS can produce the non-contig mapping. We need to iterate it
     * page-by-page or use a separate alias window... */
    dosaddr_t bound = (src & PAGE_MASK) + PAGE_SIZE;
    size_t to_copy = min(n, bound - src);
    MEMCPY_2UNIX(dest, src, to_copy);
    src += to_copy;
    dest += to_copy;
    n -= to_copy;
  }
}

void memcpy_2dos(dosaddr_t dest, const void *src, size_t n)
{
  if (vga.inst_emu && dest >= 0xa0000 && dest < 0xc0000)
    memcpy_to_vga(dest, src, n);
  else while (n) {
    dosaddr_t bound = (dest & PAGE_MASK) + PAGE_SIZE;
    size_t to_copy = min(n, bound - dest);
    MEMCPY_2DOS(dest, src, to_copy);
    src += to_copy;
    dest += to_copy;
    n -= to_copy;
  }
}

void memmove_dos2dos(dosaddr_t dest, dosaddr_t src, size_t n)
{
  /* XXX GW (Game Wizard Pro) does this.
     TODO: worry about overlaps; could be a little cleaner
     using the memcheck.c mechanism */
  if (vga.inst_emu && src >= 0xa0000 && src < 0xc0000)
    memcpy_dos_from_vga(dest, src, n);
  else if (vga.inst_emu && dest >= 0xa0000 && dest < 0xc0000)
    memcpy_dos_to_vga(dest, src, n);
  else while (n) {
    dosaddr_t bound1 = (src & PAGE_MASK) + PAGE_SIZE;
    dosaddr_t bound2 = (dest & PAGE_MASK) + PAGE_SIZE;
    size_t to_copy1 = min(bound1 - src, bound2 - dest);
    size_t to_copy = min(n, to_copy1);
    MEMMOVE_DOS2DOS(dest, src, to_copy);
    src += to_copy;
    dest += to_copy;
    n -= to_copy;
  }
}

void memcpy_dos2dos(unsigned dest, unsigned src, size_t n)
{
  /* Jazz Jackrabbit does DOS read to VGA via protmode selector */
  if (vga.inst_emu && src >= 0xa0000 && src < 0xc0000)
    memcpy_dos_from_vga(dest, src, n);
  else if (vga.inst_emu && dest >= 0xa0000 && dest < 0xc0000)
    memcpy_dos_to_vga(dest, src, n);
  else while (n) {
    dosaddr_t bound1 = (src & PAGE_MASK) + PAGE_SIZE;
    dosaddr_t bound2 = (dest & PAGE_MASK) + PAGE_SIZE;
    size_t to_copy1 = min(bound1 - src, bound2 - dest);
    size_t to_copy = min(n, to_copy1);
    MEMCPY_DOS2DOS(dest, src, to_copy);
    src += to_copy;
    dest += to_copy;
    n -= to_copy;
  }
}

int unix_read(int fd, void *data, int cnt)
{
  return RPT_SYSCALL(read(fd, data, cnt));
}

int dos_read(int fd, unsigned data, int cnt)
{
  int ret;
  /* GW also reads or writes directly from a file to protected video memory. */
  if (vga.inst_emu && data >= 0xa0000 && data < 0xc0000) {
    char buf[cnt];
    ret = unix_read(fd, buf, cnt);
    if (ret >= 0)
      memcpy_to_vga(data, buf, ret);
  }
  else
    ret = unix_read(fd, LINEAR2UNIX(data), cnt);
  if (ret > 0)
	e_invalidate(data, ret);
  return (ret);
}

int unix_write(int fd, const void *data, int cnt)
{
  return RPT_SYSCALL(write(fd, data, cnt));
}

int dos_write(int fd, unsigned data, int cnt)
{
  int ret;
  const unsigned char *d;
  unsigned char buf[cnt];
  if (vga.inst_emu && data >= 0xa0000 && data < 0xc0000) {
    memcpy_from_vga(buf, data, cnt);
    d = buf;
  } else {
    d = LINEAR2UNIX(data);
  }
  ret = unix_write(fd, d, cnt);
  g_printf("Wrote %10.10s\n", d);
  return (ret);
}

#define BUF_SIZE 1024
int com_vsnprintf(char *str, size_t msize, const char *format, va_list ap)
{
	char *s = str;
	int i, size;
	char scratch[BUF_SIZE];

	assert(msize <= BUF_SIZE);
	size = vsnprintf(scratch, msize, format, ap);
	for (i=0; i < size; i++) {
		if (s - str >= msize - 1)
			break;
		if (scratch[i] == '\n') {
			*s++ = '\r';
			if (s - str >= msize - 1)
				break;
		}
		*s++ = scratch[i];
	}
	if (msize)
		*s = 0;
	return s - str;
}

int com_vsprintf(char *str, const char *format, va_list ap)
{
	return com_vsnprintf(str, BUF_SIZE, format, ap);
}

int com_sprintf(char *str, char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vsprintf(str, format, ap);
	va_end(ap);
	return ret;
}

int com_vfprintf(int dosfilefd, char *format, va_list ap)
{
	int size;
	char scratch2[BUF_SIZE];

	size = com_vsprintf(scratch2, format, ap);
	if (!size) return 0;
	return com_doswrite(dosfilefd, scratch2, size);
}

int com_vprintf(char *format, va_list ap)
{
	int size;
	char scratch2[BUF_SIZE];

	size = com_vsprintf(scratch2, format, ap);
	if (!size) return 0;
	return com_dosprint(scratch2);
}

int com_fprintf(int dosfilefd, char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vfprintf(dosfilefd, format, ap);
	va_end(ap);
	return ret;
}

int com_printf(char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vprintf(format, ap);
	va_end(ap);
	return ret;
}

int com_puts(char *s)
{
	return com_printf("%s", s);
}

char *skip_white_and_delim(char *s, int delim)
{
	while (*s && isspace(*s)) s++;
	if (*s == delim) s++;
	while (*s && isspace(*s)) s++;
	return s;
}

void pre_msdos(void)
{
	saved_regs = REGS;
}

void call_msdos(void)
{
	do_int_call_back(0x21);
}

void post_msdos(void)
{
	REGS = saved_regs;
}

int com_doswrite(int dosfilefd, char *buf32, u_short size)
{
	char *s;
	u_short int23_seg, int23_off;
	int ret;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_heap_alloc(size);
	if (!s) return -1;
	memcpy(s, buf32, size);
	pre_msdos();
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	LWORD(ds) = DOSEMU_LMHEAP_SEG;
	LWORD(edx) = DOSEMU_LMHEAP_OFFS_OF(s);
	LWORD(eax) = 0x4000;	/* write handle */
	/* write() can be interrupted with ^C. Therefore we set int0x23 here
	 * so that even in this case it will return to the proper place. */
	int23_seg = ISEG(0x23);
	int23_off = IOFF(0x23);
	SETIVEC(0x23, CBACK_SEG, CBACK_OFF);
	call_msdos();	/* call MSDOS */
	SETIVEC(0x23, int23_seg, int23_off);	/* restore 0x23 ASAP */
	lowmem_heap_free(s);
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		post_msdos();
		return -1;
	}
	ret = LWORD(eax);
	post_msdos();
	return ret;
}

int com_dosread(int dosfilefd, char *buf32, u_short size)
{
	char *s;
	u_short int23_seg, int23_off;
	int ret;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_heap_alloc(size);
	if (!s) return -1;
	pre_msdos();
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	LWORD(ds) = DOSEMU_LMHEAP_SEG;
	LWORD(edx) = DOSEMU_LMHEAP_OFFS_OF(s);
	LWORD(eax) = 0x3f00;
	/* read() can be interrupted with ^C, esp. when it reads from a
	 * console. Therefore we set int0x23 here so that even in this
	 * case it will return to the proper place. */
	int23_seg = ISEG(0x23);
	int23_off = IOFF(0x23);
	SETIVEC(0x23, CBACK_SEG, CBACK_OFF);
	call_msdos();	/* call MSDOS */
	SETIVEC(0x23, int23_seg, int23_off);	/* restore 0x23 ASAP */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		post_msdos();
		lowmem_heap_free(s);
		return -1;
	}
	memcpy(buf32, s, min(size, LWORD(eax)));
	ret = LWORD(eax);
	post_msdos();
	lowmem_heap_free(s);
	return ret;
}

int com_dosreadcon(char *buf32, u_short size)
{
	u_short rd;

	if (!size)
		return 0;
	pre_msdos();
	for (rd = 0; rd < size; rd++) {
		LWORD(eax) = 0x600;
		LO(dx) = 0xff;
		call_msdos();
		if (LWORD(eflags) & ZF)
			break;
		buf32[rd] = LO(ax);
	}
	post_msdos();
	return rd;
}

int com_doswritecon(char *buf32, u_short size)
{
	u_short rd;

	if (!size)
		return 0;
	pre_msdos();
	for (rd = 0; rd < size; rd++) {
		LWORD(eax) = 0x600;
		LO(dx) = buf32[rd];
		call_msdos();
	}
	post_msdos();
	return rd;
}

int com_dosprint(char *buf32)
{
	char *s;
	u_short int23_seg, int23_off, size;
	size = strlen(buf32);
	if (!size) return 0;
	com_errno = 8;
	s = lowmem_heap_alloc(size);
	if (!s) return -1;
	memcpy(s, buf32, size);
	pre_msdos();
	LWORD(ebx) = STDOUT_FILENO;
	LWORD(ecx) = size;
	LWORD(ds) = DOSEMU_LMHEAP_SEG;
	LWORD(edx) = DOSEMU_LMHEAP_OFFS_OF(s);
	HI(ax) = 0x40;
	/* write() can be interrupted with ^C. Therefore we set int0x23 here
	 * so that even in this case it will return to the proper place. */
	int23_seg = ISEG(0x23);
	int23_off = IOFF(0x23);
	SETIVEC(0x23, CBACK_SEG, CBACK_OFF);
	call_msdos();	/* call MSDOS */
	SETIVEC(0x23, int23_seg, int23_off);	/* restore 0x23 ASAP */
	post_msdos();
	lowmem_heap_free(s);
	return size;
}

int com_bioscheckkey(void)
{
	int ret;
	pre_msdos();
	HI(ax) = 1;
	do_int_call_back(0x16);
	ret = !(LWORD(eflags) & ZF);
	post_msdos();
	return ret;
}

int com_biosgetch(void)
{
	int ret;
	do {
		ret = com_bioscheckkey();
	} while (!ret);
	pre_msdos();
	HI(ax) = 0;
	do_int_call_back(0x16);
	ret = LO(ax);
	post_msdos();
	return ret;
}

int com_biosread(char *buf32, u_short size)
{
	u_short rd = 0;
	int ch;

	if (!size) return 0;
	while (rd < size) {
		ch = com_biosgetch();
		if (ch == '\b') {
			if (rd > 0) {
				p_dos_str("\b \b");
				rd--;
			}
			continue;
		}
		if (ch != '\r')
			buf32[rd++] = ch;
		else
			buf32[rd++] = '\n';
		p_dos_str("%c", buf32[rd - 1]);
		if (ch == '\r' || ch == '\3')
			break;
	}
	return rd;
}

int com_setcbreak(int on)
{
	int old_b;
	pre_msdos();
	LWORD(eax) = 0x3300;
	call_msdos();
	old_b = LO(dx);
	LO(ax) = 1;
	LO(dx) = on;
	call_msdos();
	post_msdos();
	return old_b;
}
