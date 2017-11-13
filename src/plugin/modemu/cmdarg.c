#include <stdio.h>	/*stderr*/
#include <stdlib.h>
#include <unistd.h>
#include "cmdarg.h"	/*cmdarg*/
#include "defs.h"	/*VERSION_...*/
#include "ttybuf.h"
#include "commx.h"
#include "stty.h"

static const char *argv0;

/* LIT(A) -> "10" */
#define LIT_(s) #s
#define LIT(s) LIT_(s)

static void
showUsage(void)
{
    printf("modemu version " LIT(VERSION_MAJOR) "." LIT(VERSION_MINOR) "\n");
    printf("usage: %s [-c comm_prog] [-d pty_master] [-e ATxxx]"
	   " [-h] [-s] [-]\n", argv0);
}

void
cmdargParse(const char **argv)
{
    argv0 = argv[0];
    for (argv++; *argv != NULL; argv++) {
	if ((*argv)[0] == '-') {
	    switch ((*argv)[1]) {
	    case 'c': /* -c <commx args>*/
		cmdarg.ttymode = CA_COMMX;
		cmdarg.commx = *++argv;
		if (cmdarg.commx == NULL) goto FEWARG;
		break;
	    case 'd': /* -d <pty_device>*/
		cmdarg.ttymode = CA_DEVGIVEN;
		cmdarg.dev = *++argv;
		if (cmdarg.dev == NULL) goto FEWARG;
		break;
	    case 'e': /* -e <atcommands>*/
		cmdarg.atcmd = *++argv;
		if (cmdarg.atcmd == NULL) goto FEWARG;
		break;
	    case 'h': /* -h */
		showUsage();
		exit(0);
	    case 's': /* -s */
		cmdarg.ttymode = CA_SHOWDEV;
		break;
	    case '\0': /* - */
		cmdarg.ttymode = CA_STDINOUT;
		break;
	    default:
		fprintf(stderr, "Unknown option -%c.\n", (*argv)[1]);
		showUsage();
		exit(1);
	    }
	} else {
	    fprintf(stderr, "Error in command line.\n");
	    showUsage();
	    exit(1);
	}
    }
    return;

 FEWARG:
    fprintf(stderr, "'-%c' requires an argument.\n", (*--argv)[1]);
    exit(1);
}

void init_modemu(void);
int run_modemu(void);
#ifdef HAVE_GRANTPT
int getPtyMaster(char **line_return);
#else
getPtyMaster(char *tty10, char *tty01);
#endif
int openPtyMaster(const char *dev);

static int pre_init_modemu(void)
{
    switch (cmdarg.ttymode) {
#ifdef HAVE_GRANTPT
    char * ptyslave;
    case CA_SHOWDEV:
	tty.rfd = tty.wfd = getPtyMaster(&ptyslave);
	printf("%s\n", ptyslave);
	return 0;
    case CA_COMMX:
	tty.rfd = tty.wfd = getPtyMaster(&ptyslave);
	commxForkExec(cmdarg.commx, ptyslave);
	break;
#else
	char c10, c01;
    case CA_SHOWDEV:
	tty.rfd = tty.wfd = getPtyMaster(&c10, &c01);
	printf("%c%c\n", c10, c01);
	return 0;
    case CA_COMMX:
	tty.rfd = tty.wfd = getPtyMaster(&c10, &c01);
	commxForkExec(cmdarg.commx, c10, c01);
	break;
#endif
    case CA_STDINOUT:
	tty.rfd = 0;
	tty.wfd = 1;
	setTty();
	break;
    case CA_DEVGIVEN:
	tty.rfd = tty.wfd = openPtyMaster(cmdarg.dev);
	break;
    }

    return 1;
}

int
main(int argc, const char *argv[])
{
    int ret;

#ifdef SOCKS
    SOCKSinit(argv[0]);
#endif
    cmdargParse(argv);
    if (!pre_init_modemu())
	return 0;
    init_modemu();
    do {
	ret = run_modemu();
	usleep(10000);
    } while (ret == 2);

    return ret;
}
