#include <stdio.h>	/*stderr*/
#include <signal.h>	/*SIGCHLD*/
#include <unistd.h>	/*(fork,execv)*/
#include <stdlib.h>
#include <string.h>	/*(strdup)*/
#include <sys/wait.h>	/*WNOHANG*/
#include "defs.h"
#include "commx.h"	/*(commxForkExec)*/
#include "verbose.h"	/*VERB_MISC*/

static int commxPid;

static void
sigchld(int dummy)
{
    int s;

    /* wait() for commx exit() only */
    /* waitpid(-1,,) messes SOCKS up */
    if (waitpid(commxPid, &s, WNOHANG) > 0) {
	fprintf(stderr, "Comm program exited.\r\n");
	verboseOut(VERB_MISC, "Child returns status %d.\r\n", WEXITSTATUS(s));
	exit(0);
    }
}

static void
forkExec(char *s)
{
    const char *argv[] = {"sh", "-c", "", NULL};

    signal(SIGCHLD, sigchld);
    commxPid = fork();
    switch (commxPid) {
    case -1: /*error*/
	perror("fork()");
	exit(1);
    case 0: /*child*/
	signal(SIGCHLD, SIG_DFL);
	argv[2] = s;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
	execv("/bin/sh", argv);
#pragma GCC diagnostic pop
	fprintf(stderr, "Could not exec sh.\n");
	_exit(127);
    default: /*parent*/
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
    }
}

#ifdef HAVE_GRANTPT
void
commxForkExec(const char *cmd, char *ptyslave)
{
    char *s;
    s = malloc(strlen(cmd) + strlen(ptyslave)+1);
    if (strcmp("/dev/", ptyslave) == 0 )
       ptyslave += 5;
    sprintf(s, cmd, ptyslave);
    forkExec(s);
}
#else
void
commxForkExec(const char *cmd, char c10, char c01)
{
    char c[16];
    char *s;

    strcpy(c, "tty");
    c[3] = c10; c[4] = c01; c[5] = 0;
    s = malloc(strlen(cmd) + strlen(c)+1);
    sprintf(s, cmd, c); /*'%s' -> 'p1' or sth*/
    forkExec(s);
}
#endif
