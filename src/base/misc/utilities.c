/* 
 * Various sundry utilites for dos emulator.
 *
 * $Id: utilities.c,v 1.2 1995/04/08 22:30:40 root Exp $
 */
#include <stdio.h>
#include <stdarg.h>
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
 log_printf(unsigned int flg, const char *fmt,...) {
  va_list args;
  int i;
#ifdef SHOW_TIME
  static int first_time = 1;
  static int show_time =  0;
#endif

  if (!flg || !dbg_fd)
    return 0;


#ifdef SHOW_TIME
  if(first_time)  {
	if(getenv("SHOWTIME"))
		show_time = 1;
	first_time = 0;
	logptr = logbuf;
  }
#endif
	
  va_start(args, fmt);
  i = vsprintf(logptr, fmt, args);
  va_end(args);

#ifdef SHOW_TIME
  if(show_time) {
	struct timeval tv;
	int result;
	char tmpbuf[1024];
	result = gettimeofday(&tv, NULL);
	assert(0 == result);
	sprintf(tmpbuf, "%d.%d: %s", tv.tv_sec, tv.tv_usec, logptr);
	strcpy(logptr, tmpbuf);
	i = strlen(logptr);
  }
#endif

  logptr += i;
  if (((logptr-logbuf) > logbuf_size) || (flg == -1)) {
    write(fileno(dbg_fd), logbuf, logptr-logbuf);
    if (terminal_pipe) {
      write(terminal_fd, logptr, logptr-logbuf);
    }
    logptr = logbuf;
  }
  return i;
}


/* write string to dos? */
void
p_dos_str(char *fmt,...) {
  va_list args;
  char buf[1025], *s;
  int i;

  va_start(args, fmt);
  i = vsprintf(buf, fmt, args);
  va_end(args);

  s = buf;
  while (*s) 
	char_out(*s++, READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
}
        
