#ifdef TERMNET
#include <termnet.h>
#endif
#include <stdio.h>	/*stderr,(perror)*/
#include <sys/types.h>	/*->socket.h (u_short etc.)*/
#include <sys/socket.h>	/*(send,recv)*/

#include "defs.h"	/*->sockbuf.h (uchar,SOCKBUFR_SIZE,TTYBUFR_SIZE)*/
#include "sockbuf.h"	/*sockBufR,sockBufW*/
#include "sock.h"	/*sock*/
#include "verbose.h"	/*VERB_MISC*/


/* reading socket */

void
sockBufRead(void)
{
    int l;

    l = recv(sock.fd, sockBufR.buf, sizeof(sockBufR.buf), 0);
    if (l <= 0) {
	sock.alive = 0;
	if (l == 0)
	    verboseOut(VERB_MISC, "Connection closed by peer.\r\n");
	else
	    /* PPP link down or something to reach here. */
	    /* v0.0 exited, which comm progs don't expect. */
	    /* now just NO CARRIERs. Thanks >> Rod May */
	    perror("recv()");
	return;
    }
    sockBufR.ptr = sockBufR.buf;
    sockBufR.end = sockBufR.buf + l;
}


/* writing socket */

void
sockBufWrite(void)
{
    int wl,l;

    wl = sockBufW.ptr - sockBufW.top;
    if (wl == 0) return;
    l = send(sock.fd, sockBufW.top, wl, 0);
    if (l <= 0) {
	sock.alive = 0;
	if (l == 0)
	    verboseOut(VERB_MISC, "Connection closed by peer.\r\n");
	else
	    perror("send()");
	return;
    } else if (l < wl) {
	sockBufW.top += l;
	/*return 1;*/ /* needs retry */
	return;
    }
    sockBufW.ptr = sockBufW.top = sockBufW.buf;
    sockBufW.stop = 0;
    return;
}

void
putSock1(uchar c)
{
    if (sockBufW.ptr >= sockBufW.buf + SOCKBUFW_SIZE) { /* limit */
	if (sockBufW.ptr >= sockBufW.buf + SOCKBUFW_SIZE_A) { /*actual limit*/
	    fprintf(stderr,"\asockBufW overrun.\n");
	    return;
	} else sockBufW.stop = 1; /* flow control */
    }
    *sockBufW.ptr++ = c;
}

void
putSockN(const char *cp, int n)
{
    for (; n > 0; n--,cp++) putSock1(*cp);
}
