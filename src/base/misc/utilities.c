/* 
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * Various sundry utilites for dos emulator.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <malloc.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

#include "emu.h"
#include "machcompat.h"
#include "bios.h"
#include "timers.h"
#include "pic.h"
#include "dpmi.h"
#include "utilities.h"
#ifdef USE_THREADS
#include "lt-threads.h"
#endif
#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

#define SHOW_TIME	1		/* 0 or 1 */
#define SHOW_EIP	0

#ifndef INITIAL_LOGBUFSIZE
#define INITIAL_LOGBUFSIZE      0
#endif
#ifndef INITIAL_LOGBUFLIMIT
#define INITIAL_LOGFILELIMIT	(10*1024*1024)
#endif

static char logbuf_[INITIAL_LOGBUFSIZE+1025];
char *logptr=logbuf_;
char *logbuf=logbuf_;
int logbuf_size = INITIAL_LOGBUFSIZE;
int logfile_limit = INITIAL_LOGFILELIMIT;
int log_written = 0;

static char hxtab[16]="0123456789abcdef";

static inline char *prhex8 (char *p, unsigned long v)
{
  int i;
  for (i=7; i>=0; --i) { p[i]=hxtab[v&15]; v>>=4; }
  p[8]=' ';
  return p+9;
}

#if SHOW_TIME
static char *timestamp (char *p)
{
  unsigned long t;
  int i;

#ifdef X86_EMULATOR
  if ((config.cpuemu>1) && in_vm86) t=0; else
#endif
  t = pic_sys_time/1193;
  /* [12345678]s - SYS time */
#ifdef X86_EMULATOR
    if ((config.cpuemu>1) && in_vm86) strcpy(p,"[********] "); else
#endif
    {
      p[0] = '[';
      for (i=8; i>0; --i)
	{ if (t) { p[i]=(t%10)+'0'; t/=10; } else p[i]='0'; }
      p[9]=']'; p[10]=' ';
    }
  return p+11;
}
#else
#define timestamp(p)	(p)
#endif


#if SHOW_EIP
static char *eipstamp (char *p)
{
  if (in_dpmi) {
    sprintf(p,"[ %08lx] ",trc__neweip);
  }
  else {
    sprintf(p,"[%04x:%04x] ",REG(cs),LWORD(eip));
  }
  return p+12;
}
#else
#define eipstamp(p)	(p)
#endif

char *strprintable(char *s)
{
  static char buf[8][128];
  static int bufi = 0;
  char *t, c;

  bufi = (bufi + 1) & 7;
  t = buf[bufi];
  while (*s) {
    c = *s++;
    if ((unsigned)c < ' ') {
      *t++ = '^';
      *t++ = c | 0x40;
    }
    else if ((unsigned)c > 126) {
      *t++ = 'X';
      *t++ = hxtab[(c >> 4) & 15];
      *t++ = hxtab[c & 15];
    }
    else *t++ = c;
  }
  *t++ = 0;
  return buf[bufi];
}

char *chrprintable(char c)
{
  char buf[2];
  buf[0] = c;
  buf[1] = 0;
  return strprintable(buf);
}

int vlog_printf(int flg, const char *fmt, va_list args)
{
  int i;
  static int is_cr = 1;

  if (dosdebug_flags & DBGF_INTERCEPT_LOG) {
    /* we give dosdebug a chance to interrupt on given logoutput */
    extern int vmhp_log_intercept(int flg, const char *fmt, va_list args);
    i = vmhp_log_intercept(flg, fmt, args);
    if ((dosdebug_flags & DBGF_DISABLE_LOG_TO_FILE) || !dbg_fd) return i;
  }

  if (!flg || !dbg_fd || 
#ifdef USE_MHPDBG
      (shut_debug && (flg<10) && !mhpdbg.active)
#else
      (shut_debug && (flg<10))
#endif
     ) return 0;

#ifdef USE_THREADS
  lock_resource(resource_libc);
#endif
  {
    char *q;

    q = (is_cr? timestamp(logptr) : logptr);
    if (is_cr) q = eipstamp(q);
    i = vsprintf(q, fmt, args) + (q-logptr);
    if (i > 0) is_cr = (logptr[i-1]=='\n'); else i = 0;
  }
  logptr += i;

  if ((dbg_fd==stderr) || ((logptr-logbuf) > logbuf_size) || (flg == -1)) {
    int fsz = logptr-logbuf;

    /* writing a big buffer can produce timer bursts, which under DPMI
     * can cause stack overflows!
     */
    if (terminal_pipe) {
      write(terminal_fd, logptr, fsz);
    }
    write(fileno(dbg_fd), logbuf, fsz);
    logptr = logbuf;
    if (logfile_limit) {
      log_written += fsz;
      if (log_written > logfile_limit) {
        fseek(dbg_fd, 0, SEEK_SET);
        ftruncate(fileno(dbg_fd),0);
        log_written = 0;
      }
    }
  }
#ifdef USE_THREADS
  unlock_resource(resource_libc);
#endif
  return i;
}

int log_printf(int flg, const char *fmt, ...)
{
	va_list args;
	int ret;

	if (!(dosdebug_flags & DBGF_INTERCEPT_LOG)) {
		if (!flg || !dbg_fd ) return 0;
	}
	va_start(args, fmt);
	ret = vlog_printf(flg, fmt, args);
	va_end(args);
	return ret;
}

void verror(const char *fmt, va_list args) 
{
	char fmtbuf[1025];

	if (fmt[0] == '@') {
		vlog_printf(10, fmt+1, args);
		vfprintf(stderr, fmt+1, args);
	}
	else {
		fmtbuf[sizeof(fmtbuf)-1] = 0;
		snprintf(fmtbuf, sizeof(fmtbuf)-1, "ERROR: %s", fmt);
		vlog_printf(10, fmtbuf, args);
		vfprintf(stderr, fmtbuf, args);
	}
	
}

void error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	verror(fmt, args);
	va_end(args);
}


/* write string to dos? */
void
p_dos_str(char *fmt,...) {
  va_list args;
  static char buf[1025];
  char *s;
  int i;

  va_start(args, fmt);
  buf[sizeof(buf)-1] = 0;
  i = vsnprintf(buf, sizeof(buf)-1,  fmt, args);
  va_end(args);

  s = buf;
  while (*s) 
	char_out(*s++, READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
}
        
/* some stuff to handle reading of /proc */

static char *procfile_name=0;
static char *procbuf=0;
static char *procbufptr=0;
#define PROCBUFSIZE	4096
static char *proclastpos=0;
static char proclastdelim=0;

void close_proc_scan(void)
{
  if (procfile_name) free(procfile_name);
  if (procbuf) free(procbuf);
  procfile_name = procbuf = procbufptr = proclastpos = 0;
}

void open_proc_scan(char *name)
{
  int size, fd;
  close_proc_scan();
  fd = open(name, O_RDONLY);
  if (fd == -1) {
    error("cannot open %s\n", name);
    leavedos(5);
  }
  procfile_name = strdup(name);
  procbuf = malloc(PROCBUFSIZE);
  size = read(fd, procbuf, PROCBUFSIZE-1);
  procbuf[size] = 0;
  procbufptr = procbuf;
  close(fd);
}

void advance_proc_bufferptr(void)
{
  if (proclastpos) {
    procbufptr = proclastpos;
    if (proclastdelim == '\n') procbufptr++;
  }
}

void reset_proc_bufferptr(void)
{
  procbufptr = procbuf; proclastpos = 0;
}

char *get_proc_string_by_key(char *key)
{

  char *p;

  if (proclastpos) {
    *proclastpos = proclastdelim;
    proclastpos =0;
  }
  p = strstr(procbufptr, key);
  if (p) {
    if ((p == procbufptr) || (p[-1] == '\n')) {
      p += strlen(key);
      while (*p && isblank(*p)) p++;
      if (*p == ':') {
        p++;
        while (*p && isblank(*p)) p++;
        proclastpos = p;
        while (*proclastpos && (*proclastpos != '\n')) proclastpos++;
        proclastdelim = *proclastpos;
        *proclastpos = 0;
        return p;
      }
    }
  }
#if 0
  error("Unknown format on %s\n", procfile_name);
  leavedos(5);
#endif
  return 0; /* just to make GCC happy */
}

int get_proc_intvalue_by_key(char *key)
{
  char *p = get_proc_string_by_key(key);
  int val;

  if (p) {
    if (sscanf(p, "%d",&val) == 1) return val;
  }
  error("Unknown format on %s\n", procfile_name);
  leavedos(5);
  return -1; /* just to make GCC happy */
}


int integer_sqrt(int x)
{
	unsigned y;
	int delta;

	if (x < 1) return 0;
	if (x <= 1) return 1;
        y = power_of_2_sqrt(x);
	if ((y*y) == x) return y;
	delta = y >> 1;
	while (delta) {
		y += delta;
		if (y*y > x) y -= delta;
		delta >>= 1;
	}
	return y;
}

int exists_dir(char *name)
{
	struct stat st;
	if (stat(name, &st)) return 0;
	return (S_ISDIR(st.st_mode));
}

int exists_file(char *name)
{
	struct stat st;
	if (stat(name, &st)) return 0;
	return (S_ISREG(st.st_mode));
}

char *strcatdup(char *s1, char *s2)
{
	char *s;
	if (!s1 || !s2) return 0;
	s = malloc(strlen(s1)+strlen(s2)+1);
	if (!s) return 0;
	strcpy(s,s1);
	return strcat(s,s2);
}

char *assemble_path(char *dir, char *file, int append_pid)
{
	char *s;
	char pid[32] = "";
	if (append_pid) sprintf(pid, "%d", getpid());
	s = malloc(strlen(dir)+1+strlen(file)+strlen(pid)+1);
	if (!s) {
		fprintf(stderr, "out of memory, giving up\n");
		exit(1);
	}
	sprintf(s, "%s/%s%s", dir, file, pid);
	return s;
}

char *mkdir_under(char *basedir, char *dir, int append_pid)
{
	char *s = basedir;

	if (dir) s = assemble_path(basedir, dir, append_pid);
	if (!exists_dir(s)) {
		if (mkdir(s, S_IRWXU)) {
			fprintf(stderr, "can't create local %s directory, giving up\n", s);
			exit(1);
		}
	}
	return s;
}

char *get_path_in_HOME(char *path)
{
	char *home = getenv("HOME");

	if (!home) {
		fprintf(stderr, "odd environment, you don't have $HOME, giving up\n");
		exit(1);
	}
	if (!path) {
		return strdup(home);
	}
	return assemble_path(home, path, 0);
}

char *get_dosemu_local_home(void)
{
	return mkdir_under(get_path_in_HOME(".dosemu"), 0, 0);
}

#if (GLIBC_VERSION_CODE >= 2000) && defined(PORTABLE_BINARY)
/*
 * Under glibc we can't make portable static binaries due to libnss stuff.
 * As we only need to look at /etc/passwd (and which non-libnss system won't
 * have it?) we work around this problem by doing getpwuid() getpwnam()
 * ourselves, if we have no /etc/nsswitch.conf.
 *                                                -- Hans, 981201
 */

static struct passwd *our_getpw(const char *name, uid_t uid)
{
  FILE *f;
  static struct passwd pw;
  static char buf[1024];
  char *p;

  f = fopen("/etc/passwd", "r");
  if (!f) return 0;
  buf[sizeof(buf)-1] = 0;
  while (fgets(buf, sizeof(buf)-1, f)) {
    pw.pw_name = strtok(buf, ":");
    if (!pw.pw_name) continue;
    if (name && strcmp(name, pw.pw_name)) continue;
    pw.pw_passwd = strtok(0, ":");
    if (!pw.pw_passwd) continue;
    p = strtok(0, ":");
    if (!p) continue;
    pw.pw_uid = (uid_t)strtoul(p,0,0);
    if (!name && pw.pw_uid != uid) continue;
    p = strtok(0, ":");
    if (!p) continue;
    pw.pw_gid = (gid_t)strtoul(p,0,0);
    /* we don't need more, hence we skip the rest */
    fclose(f);
    return &pw;
  }
  fclose(f);
  return 0;
}

static int have_libnss()
{
  static int have_it = -1;
  if (have_it >= 0 ) return have_it;
  have_it = exists_file("/etc/nsswitch.conf") ? 1: 0;
  return have_it;
}

#undef getpwuid
#undef getpwnam

struct passwd *our_getpwuid(uid_t uid)
{
  if (have_libnss()) return getpwuid(uid);
  return our_getpw(0, uid);
}

struct passwd *our_getpwnam(const char *name)
{
  if (have_libnss()) return getpwnam(name);
  return our_getpw(name, 0);
}

#endif /* GLIBC_VERSION_CODE >= 2000 */
