/* 
 * Various sundry utilites for dos emulator.
 *
 * $Id: utilities.c,v 1.2 1995/04/08 22:30:40 root Exp $
 */
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <sys/time.h>
#include <unistd.h>
#include "emu.h"
#include "machcompat.h"
#include "bios.h"

#define INITIAL_LOGBUFSIZE      0
static char logbuf_[INITIAL_LOGBUFSIZE+1025];
char *logptr=logbuf_;
char *logbuf=logbuf_;
int logbuf_size = INITIAL_LOGBUFSIZE;

int
 log_printf(int flg, const char *fmt,...) {
  va_list args;
  int i;
  static int first_time = 1;
#ifdef SHOW_TIME
  static int is_cr =  1;
#endif

  if (!flg || !dbg_fd)
    return 0;


  if(first_time)  {
	first_time = 0;
  }
	
  va_start(args, fmt);
  i = vsprintf(logptr, fmt, args);
  va_end(args);

#ifdef SHOW_TIME
  if(is_cr) {
	struct timeval tv;
	int result;
	static char tmpbuf[1024];
	result = gettimeofday(&tv, NULL);
	sprintf(tmpbuf, "[%05d%03d] %s", (tv.tv_sec%10000), (tv.tv_usec/1000), logptr);
	strcpy(logptr, tmpbuf);
	i = strlen(logptr);
  }
  is_cr = (logptr[i-1]=='\n');
#endif

  logptr += i;
  if ((dbg_fd==stderr) || ((logptr-logbuf) > logbuf_size) || (flg == -1)) {
    int fsz = logptr-logbuf;
    if (terminal_pipe) {
      write(terminal_fd, logptr, fsz);
    }
    write(fileno(dbg_fd), logbuf, fsz);
    logptr = logbuf;
  }
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
        
