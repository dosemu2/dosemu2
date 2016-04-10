#include <stdio.h>	/*stderr,(perror)*/
#include <unistd.h>	/*(read,write)*/
#include <stdlib.h>
#include <sys/time.h>	/*->ttybuf.h (timeval)*/

#include "defs.h"	/*->ttybuf.h (uchar,SOCKBUFR_SIZE,TTYBUFR_SIZE)*/
#include "ttybuf.h"	/*sockBufR,sockBufW*/
#include "sock.h"	/*(sockShutdown)*/
#include "verbose.h"	/*VERB_MISC*/


/* reading tty */

void
ttyBufRead(void)
{
    int l;

    l = read(tty.rfd, ttyBufR.buf, sizeof(ttyBufR.buf));
    if (l <= 0) {
	sockClose();
	verboseOut(VERB_MISC, "Pty closed. (read() returns %d)\r\n", l);
	if (l < 0) verbosePerror(VERB_MISC, "read()");
	exit(0);
    }
    ttyBufR.prevT = ttyBufR.newT;
    gettimeofday(&ttyBufR.newT, NULL);
    ttyBufR.ptr = ttyBufR.buf;
    ttyBufR.end = ttyBufR.buf + l;
}


/* writing tty */

void
ttyBufWrite(void)
{
    int wl,l;

    wl = ttyBufW.ptr - ttyBufW.top;
    if (wl == 0) return;
    l = write(tty.wfd, ttyBufW.top, wl);
    if (l <= 0) {
	sockClose();
	verboseOut(VERB_MISC, "Pty closed. (write() returns %d)\r\n", l);
	if (l < 0) verbosePerror(VERB_MISC, "write()");
	exit(0);
    } else if (l < wl) {
	ttyBufW.top += l;
	/*return 1;*/ /* needs retry */
	return;
    }
    ttyBufW.ptr = ttyBufW.top = ttyBufW.buf;
    ttyBufW.stop = 0;
    return;
}

void
putTty1(uchar c)
{
    if (ttyBufW.ptr >= ttyBufW.buf + TTYBUFW_SIZE) { /* limit */
	if (ttyBufW.ptr >= ttyBufW.buf + TTYBUFW_SIZE_A) { /*actual limit*/
	    fprintf(stderr,"\attyBufW overrun.\n");
	    return;
	} else ttyBufW.stop = 1; /* flow control */
    }
    *ttyBufW.ptr++ = c;
}

void
putTtyN(const char *cp, int n)
{
    for (; n > 0; n--,cp++) putTty1(*cp);
}
