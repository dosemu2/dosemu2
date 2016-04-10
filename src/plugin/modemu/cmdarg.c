#include <stdio.h>	/*stderr*/
#include <stdlib.h>
#include "cmdarg.h"	/*cmdarg*/
#include "defs.h"	/*VERSION_...*/

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
