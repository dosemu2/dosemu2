#include <stdio.h>	/*(printf)*/
#include <termios.h>	/*ICANON,TCSA...*/
#include <signal.h>	/*SIGTERM*/
#include <stdlib.h>	/*(atexit)*/
#include <unistd.h>	/*(isatty)*/
#include "stty.h"	/*(setTty)*/

/* stty -icannon -echo -isig -icrnl -inlcr */

static void
sigint(int i)
{
    exit(1);
}

static int pid;
static struct termios oldTermios;

static void
recoverTermios(void)
{
    if (getpid() != pid) return; /* SOCKS (at least v4.2) subprocess
				    calls exit() (why not _exit()??) */
    tcsetattr(0, TCSADRAIN, &oldTermios);
    printf("\nBye.\n");
}

#ifdef USE_ON_EXIT
#define atexit(f) on_exit((f),0)
#endif

void
setTty(void)
{
    struct termios t;

    if (!isatty(0)) return;
    pid = getpid();
    signal(SIGTERM, sigint);
    signal(SIGINT, sigint);
    tcgetattr(0, &t);
    oldTermios = t;
    atexit(recoverTermios);
    t.c_lflag &= ~ICANON & ~ECHO & ~ISIG;
    t.c_iflag &= ~ICRNL & ~INLCR;
    t.c_oflag &= ~OCRNL & ~ONLCR;
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSADRAIN, &t);
}
