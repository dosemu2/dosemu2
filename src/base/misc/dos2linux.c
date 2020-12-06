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



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bsd/unistd.h>
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
#ifdef __GLIBC__
#include <alloca.h>
#endif
#include <semaphore.h>

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
#include "disks.h"
#include "mapping.h"
#include "redirect.h"
#include "translate/translate.h"
#include "../../dosext/mfs/lfn.h"
#include "../../dosext/mfs/mfs.h"
#include "mmio_tracing.h"

#define com_stderr      2

#ifndef max
#define max(a,b)       ((a)>(b)? (a):(b))
#endif

#define GET_USER_ENVVAR      0x52
#define EXEC_USER_COMMAND    0x50

static char *misc_dos_options;
int com_errno;

char *misc_e6_options(void)
{
  return misc_dos_options;
}

void misc_e6_store_options(char *str)
{
  size_t olen = 0;
  size_t slen = strlen(str);
  /* any later arguments are collected as DOS options */
  if (misc_dos_options) {
    olen = strlen(misc_dos_options);
    misc_dos_options = realloc(misc_dos_options, olen + slen + 2);
    misc_dos_options[olen] = ' ';
    strcpy(misc_dos_options + olen + 1, str);
  } else {
    misc_dos_options = malloc(slen + 1);
    strcpy(misc_dos_options, str);
  }
  g_printf ("Storing Options : %s\n", misc_dos_options);
}


static int pty_fd;
static int pty_done;
static int cbrk;
static sem_t *pty_sem;
static char sem_name[256];

static void pty_thr(void)
{
    char buf[128];
    fd_set rfds;
    struct timeval tv;
    int retval, rd, wr;
    struct char_set_state kstate;
    struct char_set_state dstate;

    init_charset_state(&kstate, trconfig.keyb_charset);
    init_charset_state(&dstate, trconfig.dos_charset);
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
	    rd = RPT_SYSCALL(read(pty_fd, buf, sizeof(buf) - 1));
	    switch (rd) {
	    case -1:
		g_printf("run_unix_command(): read error %s\n", strerror(errno));
		/* no break */
	    case 0:
		pty_done++;
		break;
	    default: {
		int rc;
		const char *p = buf;
		buf[rd] = 0;
		while (*p) {
		    #define MAX_LEN 256
		    t_unicode uni[MAX_LEN];
		    const t_unicode *u = uni;
		    char buf2[MAX_LEN * MB_LEN_MAX];
		    rc = charset_to_unicode_string(&kstate, uni, &p, strlen(p),
			    MAX_LEN);
		    if (rc <= 0)
			break;
		    rc = unicode_to_charset_string(&dstate, buf2, &u, rc,
			    sizeof(buf2));
		    if (rc <= 0)
			break;
		    com_doswritecon(buf2, rc);
		}
		break;
	    }
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
    cleanup_charset_state(&kstate);
    cleanup_charset_state(&dstate);
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
    snprintf(sem_name, sizeof(sem_name), "/dosemu_pty_sem_%i", getpid());
    pty_sem = sem_open(sem_name, O_CREAT, S_IRUSR | S_IWUSR, 0);
    if (!pty_sem)
    {
        error("sem_open failed %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void dos2tty_done(void)
{
    close(pty_fd);
    sem_close(pty_sem);
    sem_unlink(sem_name);
}

static int dos2tty_open(void)
{
    int err, pts_fd;
    err = grantpt(pty_fd);
    if (err) {
	error("grantpt failed: %s\n", strerror(errno));
	return err;
    }
    pts_fd = open(ptsname(pty_fd), O_RDWR | O_CLOEXEC);
    if (pts_fd == -1) {
	error("pts open failed: %s\n", strerror(errno));
	return -1;
    }
    sem_post(pty_sem);
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
    sem_wait(pty_sem);
    pty_thr();
}

static void dos2tty_stop(void)
{
    clear_IF();
    com_setcbreak(cbrk);
}

static int do_run_cmd(const char *path, int argc, char * const *argv,
        int use_stdin, int close_from)
{
    sigset_t set, oset;
    int pid, status, retval, wt;
    int pts_fd;
    struct timespec to = { 0, 0 };

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
	if (close_from != -1)
	    closefrom(close_from);
	setsid();	// will have ctty
	/* open pts _after_ setsid, or it won't became a ctty */
	pts_fd = dos2tty_open();
	if (pts_fd == -1) {
	    error("run_unix_command(): open pts failed %s\n", strerror(errno));
	    _exit(EXIT_FAILURE);
	}
	close(0);
	close(1);
	close(2);
	if (use_stdin)
	    dup(pts_fd);
	else
	    open("/dev/null", O_RDONLY);
	dup(pts_fd);
	dup(pts_fd);
	close(pts_fd);
	close(pty_fd);
	/* close signals, then unblock */
	signal_done();
	/* flush pending signals */
	do {
	    wt = sigtimedwait(&set, NULL, &to);
	} while (wt != -1);
	sigprocmask(SIG_SETMASK, &oset, NULL);

	retval = execve(path, argv, dosemu_envp);	/* execute command */
	error("exec failed: %s\n", strerror(errno));
	_exit(retval);
	break;
    }
    sigprocmask(SIG_SETMASK, &oset, NULL);

    assert(!isset_IF());
    dos2tty_start();
    while ((retval = waitpid(pid, &status, WNOHANG)) == 0)
	coopth_wait();
    if (retval == -1)
	error("waitpid: %s\n", strerror(errno));
    dos2tty_stop();
    /* print child exitcode. not perfect */
    g_printf("run_unix_command() (parent): child exit code: %i\n",
            WEXITSTATUS(status));
    return WEXITSTATUS(status);
}

int run_unix_command(int argc, char **argv)
{
    const char *path;
    char *p;

    path = findprog(argv[0], getenv("PATH"));
    if (!path) {
	com_printf("unix: %s not found\n", argv[0]);
	return -1;
    }
    /* check if path allowed */
    p = config.unix_exec ? strstr(config.unix_exec, path) : NULL;
    if (p) {
	/* make sure the found string is entire word */
	int l = strlen(path);
	if ((p > config.unix_exec && p[-1] != ' ') ||
		(p[l] != '\0' && p[l] != ' '))
	    p = NULL;
    }
    if (!p) {
	com_printf("unix: execution of %s is not allowed.\n"
		"Add %s to $_unix_exec list.\n",
		argv[0], path);
	error("execution of %s is not allowed.\n"
		"Add %s to $_unix_exec list.\n",
		argv[0], path);
	return -1;
    }

    g_printf("UNIX: run %s, %i args\n", path, argc);
    return do_run_cmd(path, argc, argv, 1, -1);
}

/* no PATH searching, no arguments allowed, no stdin, no inherited fds */
int run_unix_secure(char *prg)
{
    char *path;
    char *argv[2];
    int ret;

    path = assemble_path(DOSEMULIBEXEC_DEFAULT, prg);
    if (!exists_file(path)) {
	com_printf("unix: %s not found\n", path);
	free(path);
	return -1;
    }
    argv[0] = prg;
    argv[1] = NULL;	/* no args allowed */
    g_printf("UNIX: run_secure %s '%s'\n", path, prg);
    ret = do_run_cmd(path, 1, argv, 0, STDERR_FILENO + 1);
    free(path);
    return ret;
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

/* set of 4096 addresses rounded down to page boundaries where
   bits 12..23 equal the index: if the page is in this set we quickly
   know that regular reads and writes are valid.
   Initialize with invalid entries */
static dosaddr_t unprotected_page_cache[PAGE_SIZE] = {0xffffffff};

void invalidate_unprotected_page_cache(dosaddr_t addr, int len)
{
  unsigned int page;
  for (page = addr >> PAGE_SHIFT;
       page <= (addr + len - 1) >> PAGE_SHIFT; page++)
    unprotected_page_cache[page & (PAGE_SIZE-1)] = 0xffffffff;
}

static inline int mem_likely_protected(dosaddr_t addr, int len)
{
  /* hash = low 12 bits of page number, add len-1 to fail on boundaries.
     This gives no clashes if all addresses are under 16MB and >99.99% hit
     rate when addresses over 16MB are present while still using a small
     cache table.
     See http://www.emulators.com/docs/nx08_stlb.htm for background on
     this technique.
   */
  int hash = (addr >> PAGE_SHIFT) & (PAGE_SIZE-1);
  return unprotected_page_cache[hash] != ((addr + len - 1) & PAGE_MASK);
}

static inline void set_unprotected_page(dosaddr_t addr)
{
  unprotected_page_cache[(addr >> PAGE_SHIFT) & (PAGE_SIZE-1)] = addr & PAGE_MASK;
}

void default_sim_pagefault_handler(dosaddr_t addr, int err, uint32_t op, int len)
{
  if (err & 2)
    dosemu_error("Invalid write to addr %#x, ptr %p, len %d\n",
		 addr, MEM_BASE32(addr), len);
  else
    dosemu_error("Invalid read from addr %#x, ptr %p\n",
		 addr, MEM_BASE32(addr));
  leavedos_main(1);
}

static void check_read_pagefault(dosaddr_t addr,
				 sim_pagefault_handler_t handler)
{
  if (addr >= LOWMEM_SIZE + HMASIZE) {
    if (!dpmi_read_access(addr))
      /* uncommitted page is never "present" */
      handler(addr, 4, 0, 0);
    /* only add writable pages to the cache! */
    if (!dpmi_write_access(addr))
      return;
  }
  set_unprotected_page(addr);
}

uint8_t do_read_byte(dosaddr_t addr, sim_pagefault_handler_t handler)
{
  if (mem_likely_protected(addr, 1)) {
    /* use vga_write_access instead of vga_read_access here to avoid adding
       read-only addresses to the cache */
    if (vga_write_access(addr))
      return vga_read(addr);
    if (config.mmio_tracing && mmio_check(addr))
      return mmio_trace_byte(addr, READ_BYTE(addr), MMIO_READ);
    check_read_pagefault(addr, handler);
  }
  return READ_BYTE(addr);
}

uint16_t do_read_word(dosaddr_t addr, sim_pagefault_handler_t handler)
{
  if (mem_likely_protected(addr, 2)) {
    if (((addr+1) & (PAGE_SIZE-1)) == 0)
      /* split if spanning a page boundary */
      return do_read_byte(addr, handler) |
	((uint16_t)do_read_byte(addr+1, handler) << 8);
    if (vga_write_access(addr))
      return vga_read_word(addr);
    if (config.mmio_tracing && mmio_check(addr))
      return mmio_trace_word(addr, READ_WORD(addr), MMIO_READ);
    check_read_pagefault(addr, handler);
  }
  return READ_WORD(addr);
}

uint32_t do_read_dword(dosaddr_t addr, sim_pagefault_handler_t handler)
{
  if (mem_likely_protected(addr, 4)) {
    if (((addr+3) & (PAGE_SIZE-1)) < 3)
      return do_read_word(addr, handler) |
	((uint32_t)do_read_word(addr+2, handler) << 16);
    if (vga_write_access(addr))
      return vga_read_dword(addr);
    if (config.mmio_tracing && mmio_check(addr))
      return mmio_trace_dword(addr, READ_DWORD(addr), MMIO_READ);
    check_read_pagefault(addr, handler);
  }
  return READ_DWORD(addr);
}

uint64_t do_read_qword(dosaddr_t addr, sim_pagefault_handler_t handler)
{
  return do_read_dword(addr, handler) |
    ((uint64_t)do_read_dword(addr+4, handler) << 32);
}

static int check_write_pagefault(dosaddr_t addr, uint32_t op, int len,
				 sim_pagefault_handler_t handler)
{
  if (addr >= LOWMEM_SIZE + HMASIZE && !dpmi_write_access(addr)) {
    handler(addr, 6 + dpmi_read_access(addr), op, len);
    return 1;
  }
  set_unprotected_page(addr);
  return 0;
}

void do_write_byte(dosaddr_t addr, uint8_t byte, sim_pagefault_handler_t handler)
{
  if (mem_likely_protected(addr, 1)) {
    if (vga_write_access(addr)) {

      vga_write(addr, byte);
      return;
    }
    if (config.mmio_tracing && mmio_check(addr))
      mmio_trace_byte(addr, byte, MMIO_WRITE);
    if (check_write_pagefault(addr, byte, 1, handler))
      return;
  }
  WRITE_BYTE(addr, byte);
}

void do_write_word(dosaddr_t addr, uint16_t word, sim_pagefault_handler_t handler)
{
  if (mem_likely_protected(addr, 2)) {
    if (((addr+1) & (PAGE_SIZE-1)) == 0) {
      do_write_byte(addr, word & 0xff, handler);
      do_write_byte(addr+1, word >> 8, handler);
    }
    if (vga_write_access(addr)) {
      vga_write_word(addr, word);
      return;
    }
    if (config.mmio_tracing && mmio_check(addr))
      mmio_trace_word(addr, word, MMIO_WRITE);
    if (check_write_pagefault(addr, word, 2, handler))
      return;
  }
  WRITE_WORD(addr, word);
}

void do_write_dword(dosaddr_t addr, uint32_t dword, sim_pagefault_handler_t handler)
{
  if (mem_likely_protected(addr, 4)) {
    if (((addr+3) & (PAGE_SIZE-1)) < 3) {
      do_write_word(addr, dword & 0xffff, handler);
      do_write_word(addr+2, dword >> 16, handler);
    }
    if (vga_write_access(addr)) {
      vga_write_dword(addr, dword);
      return;
    }
    if (config.mmio_tracing && mmio_check(addr))
      mmio_trace_dword(addr, dword, MMIO_WRITE);
    if (check_write_pagefault(addr, dword, 4, handler))
      return;
  }
  WRITE_DWORD(addr, dword);
}

void do_write_qword(dosaddr_t addr, uint64_t qword, sim_pagefault_handler_t handler)
{
  do_write_dword(addr, qword & 0xffffffff, handler);
  do_write_dword(addr+4, qword >> 32, handler);
}

uint8_t read_byte(dosaddr_t addr)
{
  return do_read_byte(addr, default_sim_pagefault_handler);
}

uint16_t read_word(dosaddr_t addr)
{
  return do_read_word(addr, default_sim_pagefault_handler);
}

uint32_t read_dword(dosaddr_t addr)
{
  return do_read_dword(addr, default_sim_pagefault_handler);
}

uint64_t read_qword(dosaddr_t addr)
{
  return do_read_qword(addr, default_sim_pagefault_handler);
}

void write_byte(dosaddr_t addr, uint8_t byte)
{
  do_write_byte(addr, byte, default_sim_pagefault_handler);
}

void write_word(dosaddr_t addr, uint16_t word)
{
  do_write_word(addr, word, default_sim_pagefault_handler);
}

void write_dword(dosaddr_t addr, uint32_t dword)
{
  do_write_dword(addr, dword, default_sim_pagefault_handler);
}

void write_qword(dosaddr_t addr, uint64_t qword)
{
  do_write_qword(addr, qword, default_sim_pagefault_handler);
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
  else {
    e_invalidate(dest, n);
    while (n) {
      dosaddr_t bound = (dest & PAGE_MASK) + PAGE_SIZE;
      size_t to_copy = min(n, bound - dest);
      MEMCPY_2DOS(dest, src, to_copy);
      src += to_copy;
      dest += to_copy;
      n -= to_copy;
    }
  }
}

void memset_dos(dosaddr_t dest, char ch, size_t n)
{
  if (vga.inst_emu && dest >= 0xa0000 && dest < 0xc0000)
    vga_memset(dest, ch, n);
  else {
    e_invalidate(dest, n);
    while (n) {
      dosaddr_t bound = (dest & PAGE_MASK) + PAGE_SIZE;
      size_t to_copy = min(n, bound - dest);
      MEMSET_DOS(dest, ch, to_copy);
      dest += to_copy;
      n -= to_copy;
    }
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
  else {
    e_invalidate(dest, n);
    while (n) {
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
}

void memcpy_dos2dos(unsigned dest, unsigned src, size_t n)
{
  /* Jazz Jackrabbit does DOS read to VGA via protmode selector */
  if (vga.inst_emu && src >= 0xa0000 && src < 0xc0000)
    memcpy_dos_from_vga(dest, src, n);
  else if (vga.inst_emu && dest >= 0xa0000 && dest < 0xc0000)
    memcpy_dos_to_vga(dest, src, n);
  else {
    e_invalidate(dest, n);
    while (n) {
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
  unsigned char *buf;

  if (!cnt)
    return 0;
  buf = alloca(cnt);
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

int com_sprintf(char *str, const char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vsprintf(str, format, ap);
	va_end(ap);
	return ret;
}

int com_vfprintf(int dosfilefd, const char *format, va_list ap)
{
	int size;
	char scratch2[BUF_SIZE];

	size = com_vsprintf(scratch2, format, ap);
	if (!size) return 0;
	return com_doswrite(dosfilefd, scratch2, size);
}

int com_vprintf(const char *format, va_list ap)
{
	int size;
	char scratch2[BUF_SIZE];

	size = com_vsprintf(scratch2, format, ap);
	if (!size) return 0;
	return com_dosprint(scratch2);
}

int com_fprintf(int dosfilefd, const char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vfprintf(dosfilefd, format, ap);
	va_end(ap);
	return ret;
}

int com_printf(const char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vprintf(format, ap);
	va_end(ap);
	return ret;
}

int com_puts(const char *s)
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

void call_msdos(void)
{
	do_int_call_back(0x21);
}

int com_doswrite(int dosfilefd, char *buf32, u_short size)
{
	char *s;
	u_short int23_seg, int23_off;
	int ret = -1;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_heap_alloc(size);
	if (!s) return -1;
	memcpy(s, buf32, size);
	pre_msdos();
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	SREG(ds) = DOSEMU_LMHEAP_SEG;
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
	if (LWORD(eflags) & CF)
		com_errno = LWORD(eax);
	else
		ret = LWORD(eax);
	post_msdos();
	return ret;
}

int com_dosread(int dosfilefd, char *buf32, u_short size)
{
	char *s;
	u_short int23_seg, int23_off;
	int ret = -1;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_heap_alloc(size);
	if (!s) return -1;
	pre_msdos();
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	SREG(ds) = DOSEMU_LMHEAP_SEG;
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
	} else {
		memcpy(buf32, s, min(size, LWORD(eax)));
		ret = LWORD(eax);
	}
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

int com_doswritecon(const char *buf32, u_short size)
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

int com_dosprint(const char *buf32)
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
	SREG(ds) = DOSEMU_LMHEAP_SEG;
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

int com_dosopen(const char *name, int flags)
{
	int ret = -1;
	int len = strlen(name) + 1;
	char *s = lowmem_heap_alloc(len);
	strcpy(s, name);
	pre_msdos();
	HI(ax) = 0x3d;
	switch (flags & O_ACCMODE) {
	case O_RDONLY:
	default:
		LO(ax) = 0;
		break;
	case O_WRONLY:
		LO(ax) = 1;
		break;
	case O_RDWR:
		LO(ax) = 2;
		break;
	}
	if (flags & O_CLOEXEC)
		LO(ax) |= 1 << 7;
	SREG(ds) = DOSEMU_LMHEAP_SEG;
	LWORD(edx) = DOSEMU_LMHEAP_OFFS_OF(s);
	LWORD(ecx) = 0;
	call_msdos();
	if (LWORD(eflags) & CF)
		com_errno = LWORD(eax);
	else
		ret = LWORD(eax);
	post_msdos();
	lowmem_heap_free(s);
	return ret;
}

int com_dosclose(int fd)
{
	int ret = -1;
	pre_msdos();
	HI(ax) = 0x3e;
	LWORD(ebx) = fd;
	call_msdos();
	if (LWORD(eflags) & CF)
		com_errno = LWORD(eax);
	else
		ret = 0;
	post_msdos();
	return ret;
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
