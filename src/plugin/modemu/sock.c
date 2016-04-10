#ifdef TERMNET
#include <termnet.h>
#endif
#include <sys/types.h>	/*->socket.h (u_short etc.)*/
#include <sys/socket.h>	/*AF_INET*/
#include <netinet/in.h>	/*sockaddr_in*/
#include <arpa/inet.h>	/*(inet_addr)*/
#include <string.h>	/*(memset)*/
#include <netdb.h>	/*hostent,(gethostbyname)*/
#ifdef USE_FILIO_H
#include <sys/filio.h>	/*FIONBIO*/
#else
#include <sys/ioctl.h>	/*FIONBIO*/
#endif
#include <stdlib.h>	/*(atoi)*/
#include <sys/time.h>	/*->ttybuf.h (timeval)*/
#include <unistd.h>	/*(close)*/
#include <errno.h>	/*EINPROGRESS*/
#include <stdio.h>

#include "sock.h"	/*sock*/
#include "defs.h"	/*->atcmd.h (uchar)*/
#include "atcmd.h"	/*atcmd*/
#include "telopt.h"	/*telOpt*/
#include "ttybuf.h"	/*tty*/
#include "timeval.h"	/*(timeval...)*/
#include "verbose.h"	/*VERB_MISC*/

void
sockClose(void)
{
    if (sock.fd <= 0) return;
    close(sock.fd);
    sock.fd = sock.alive = 0;
}

void
sockShutdown(void)
{
    if (sock.fd <= 0) return;
    shutdown(sock.fd, 2);
    sockClose();
}

#define DEFAULT_PORT 23

int
sockDial(void)
{
    struct sockaddr_in sa;
    struct hostent *hep;
    struct servent *sep;
    int tmp;

    memset(&sa, 0, sizeof(sa));

    switch (atcmd.d.addr.type) {
    case ATDA_NUM:
	sa.sin_addr.s_addr = inet_addr(atcmd.d.addr.str);
	break;
    case ATDA_STR:
	hep = gethostbyname(atcmd.d.addr.str);
	if (hep == NULL) {
	    verboseOut(VERB_MISC, "Host address lookup failed.\r\n");
	    return 1;
	}
	sa.sin_addr.s_addr = *(unsigned long *)hep->h_addr_list[0];
	break;
    }

    switch (atcmd.d.port.type) {
    case ATDP_NUL:
	sa.sin_port = htons(DEFAULT_PORT);
	break;
    case ATDP_NUM:
	sa.sin_port = htons(atoi(atcmd.d.port.str));
	telOpt.sentReqs = 1; /* skip sending option requests */
	break;
    case ATDP_STR:
	sep = getservbyname(atcmd.d.port.str, "tcp");
	if (sep == NULL) {
	    verboseOut(VERB_MISC, "Port number lookup failed.\r\n");
	    return 1;
	}
	sa.sin_port = sep->s_port;
	telOpt.sentReqs = 1; /* skip sending option requests */
	break;
    }

    sa.sin_family = AF_INET;

    sock.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock.fd < 0) {
	perror("socket()");
	return 1;
    }

    tmp = 1;
    if (setsockopt(sock.fd, SOL_SOCKET, SO_OOBINLINE, &tmp, sizeof(tmp)) < 0) {
	perror("setsockopt()");
	sockClose();
	return 1;
    }

#ifdef NO_DIAL_CANCELING
    /* blocking connect. */
    if (connect(sock.fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
	sockShutdown();
	perror("connect()");
	return 1;
    }
    sock.alive = 1;
    return 0;
#else /*!ifdef NO_DIAL_CANCELING*/
    {
	/* nonblocking connect. */
	/* SOCKS version 4.2 or higher is required for SOCKS support */
	fd_set rfds, wfds;
	struct timeval tv;
	struct timeval to, t;

	tmp = 1; ioctl(sock.fd, FIONBIO, &tmp); /* non-blocking i/o */

	/* but Term's connect() blocks here... */
	if (connect(sock.fd, (struct sockaddr *)&sa, sizeof(sa)) < 0
	    && errno != EINPROGRESS) {
	    perror("connect()");
	    sockShutdown();
	    return 1;
	}

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	tv.tv_sec = 0;

	timevalSet10ms(&t, atcmd.s[7] * 100); /* S7 sec */
	gettimeofday(&to, NULL);
	timevalAdd(&to, &t); /* S7 sec after */

	/* SOCKS Rselect() first checks if connected, then select(). */
	/* so, select() with large timeval is inappropriate */
	do {
	    if (!atcmd.pd) FD_SET(tty.rfd, &rfds);
	    FD_SET(sock.fd, &wfds);
	    tv.tv_usec = 200*1000; /* 0.2sec period */

	RETRY:
	    if (select(sock.fd+1, &rfds, &wfds, NULL, &tv) < 0) {
		if (errno == EINTR) goto RETRY;
		perror("select()");
		sockShutdown();
		return 1;
	    }
#if 0
	    verboseOut(VERB_MISC, "tty=%d, sock=%d\r\n",
		    FD_ISSET(tty.rfd, &rfds),
		    FD_ISSET(sock.fd, &wfds));
#endif
	    if (FD_ISSET(tty.rfd, &rfds)) {
		sockShutdown();
		verboseOut(VERB_MISC,
			   "Connecting attempt canceled by user input.\r\n");
		return 1;
	    }
	    /* check if really connected or not */

	    /*if (FD_ISSET(sock.fd, &wfds)
	      && getpeername(sock.fd, (struct sockaddr *)&sa, &tmp) == 0)*/

	    /* SOCKS requires this check method (ref: What_SOCKS_expects) */
	    if (FD_ISSET(sock.fd, &wfds)) {
		if (connect(sock.fd, (struct sockaddr *)&sa, sizeof(sa)) < 0
		    && errno != EISCONN) {
		    perror("connect()-2");
		    sockShutdown();
		    return 1;
		}
		tmp = 0; ioctl(sock.fd, FIONBIO, &tmp); /* blocking i/o */
		sock.alive = 1;
		return 0;
	    }

	    gettimeofday(&t, NULL);
	} while (timevalCmp(&t, &to) < 0);
	sockShutdown();
	verboseOut(VERB_MISC, "Connecting attempt timed out.\r\n");
	return 1; /* timeout */
    }
#endif /*ifdef NO_DIAL_CANCELING*/
}
