/* 
 * Various sundry utilites for dos emulator.
 *
 */
#include <stdio.h>
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
#ifdef USE_THREADS
#include "lt-threads.h"
#endif
#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

#define SHOW_TIME	1		/* 0 or 1 */

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

static inline char *prhex8 (char *p, unsigned long v)
{
  static char hxtab[16]="0123456789abcdef";
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
  t = GETusTIME(0)/1000;
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


int vlog_printf(int flg, const char *fmt, va_list args)
{
  int i;
  static int is_cr = 1;

  if (!flg || !dbg_fd ) return 0;

#ifdef USE_THREADS
  lock_resource(resource_libc);
#endif
  {
    char *q;

    q = (is_cr? timestamp(logptr) : logptr);
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

	if (!flg || !dbg_fd ) return 0;

	va_start(args, fmt);
	ret = vlog_printf(flg, fmt, args);
	va_end(args);
	return ret;
}

void error(const char *fmt, ...)
{
	va_list args;
	char fmtbuf[1025];

	va_start(args, fmt);
	if (fmt[0] == '@') {
		vlog_printf(10, fmt+1, args);
		vfprintf(stderr, fmt+1, args);
	}
	else {
		sprintf(fmtbuf, "ERROR: %s", fmt);
		vlog_printf(10, fmtbuf, args);
		vfprintf(stderr, fmtbuf, args);
	}
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
  i = vsprintf(buf, fmt, args);
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

