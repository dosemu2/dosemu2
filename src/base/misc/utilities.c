/* 
 * Various sundry utilites for dos emulator.
 *
 */
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <sys/time.h>
#include <unistd.h>
#include "emu.h"
#include "machcompat.h"
#include "bios.h"
#include "timers.h"
#include "pic.h"
#include "dpmi.h"
#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

#define SHOW_TIME	1		/* 0 or 1 */

#ifndef INITIAL_LOGBUFSIZE
#define INITIAL_LOGBUFSIZE      0
#endif
static char logbuf_[INITIAL_LOGBUFSIZE+1025];
char *logptr=logbuf_;
char *logbuf=logbuf_;
int logbuf_size = INITIAL_LOGBUFSIZE;

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

  t = GETusTIME(0)/1000;
  /* [12345678]s - SYS time */
  p[0] = '[';
  for (i=8; i>0; --i) if (t) { p[i]=(t%10)+'0'; t/=10; } else p[i]='0';
  p[9]=']'; p[10]=' ';
  return p+11;
}
#else
#define timestamp(p)	(p)
#endif


int
log_printf(int flg, const char *fmt,...) {
  va_list args;
  int i;
  static int is_cr = 1;

  if (!flg || !dbg_fd ) return 0;

#ifdef USE_THREADS
  lock_resource(resource_libc);
#endif
  {
    char *q;

    va_start(args, fmt);
    q = (is_cr? timestamp(logptr) : logptr);

    i = vsprintf(q, fmt, args) + (q-logptr);
    va_end(args);
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
  }
#ifdef USE_THREADS
  unlock_resource(resource_libc);
#endif
  return i;
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
        
