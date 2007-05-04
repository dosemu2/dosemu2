/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * This implements a hook to allow user processes control the behave
 * of dosemu dynamically. Its purpose mainly is to support frontends
 * for dosemu such as kdos.
 *
 * Author:		Hans Lermen
 * With help from:	Steffen Winterfeld, Antonio Larrosa
 *			and Alistair MacDonald
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#include "emu.h"
#include "video.h"
#include "utilities.h"
#include "disks.h"
#include "userhook.h"
#include "redirect.h"
#include "keyboard.h"

static char *inpipename = 0;
static char *outpipename = 0;

int uhook_fdin = -1;
#define fdin 	uhook_fdin
static int fdout = -1;
static int nbytes = 0;
static int sendptr =0;
#define UHOOK_BUFSIZE	0x2000
static char *recvbuf = 0;
static char *sendbuf = 0;
static int need_ack = 0;
static int need_syn =0;
static int errorcode = 0;

static void uhook_printf(const char *fmt, ...);
static char *concatpath(char *dir, char *file);

static void uhook_ack(int argc, char **argv);
static void uhook_hello(int argc, char **argv);
static void uhook_kill(int argc, char **argv);
static void uhook_version(int argc, char **argv);
static void uhook_keystroke(int argc, char **argv);
static void uhook_log(int argc, char **argv);
static void uhook_hog(int argc, char **argv);
static void uhook_boot(int argc, char **argv);
static void uhook_xmode(int argc, char **argv);
static void uhook_lredir(int argc, char **argv);
static void uhook_help(int argc, char **argv);
static void uhook_ecpu(int argc, char **argv);
static void uhook_config(int argc, char **argv);

static const struct cmd_db cmdtab[] = {
	{"ack",		uhook_ack},
	{"hello",	uhook_hello},
	{"kill",	uhook_kill},
	{"version",	uhook_version},
	{"keystroke",	uhook_keystroke},
	{"log",		uhook_log},
	{"hog",		uhook_hog},
	{"boot",	uhook_boot},
	{"xmode",	uhook_xmode},
	{"lredir",	uhook_lredir},
	{"help",	uhook_help},
	{"ecpu",	uhook_ecpu},
	{"config",	uhook_config},
	{"",		NULL}
};

static char help_string[] =
	"ack [{on|off}]      get/set user hook handshake acknowledge\n"
	"hello               just for testing, any kinf of argument\n"
	"kill                kill the dosemu session from inside\n"
	"version             prints the dosemu version\n"
	"keystroke <strokes> insert keystrokes into the DOS session\n"
	"log [debugflags]    get/set debugflags which control log output\n"
	"hog [value]         get/set the HogThreshold\n"
	"boot [{on|off}]     get/set the mode of the vbootfloppy\n"
	"xmode [<args>]      set X parameters, without args gives help\n"
	"lredir n: dir [ro]  redirect directory 'dir' to DOS drive 'n:'\n"
	"help                this screen\n"
	"ecpu [{on|off}]     get/set cpuemu\n"
        "config              print the current configuration\n"
	;

static void do_syn(char *command)
{
	if (need_syn) uhook_printf("SYN: %s\n", command);
}

static void uhook_ack(int argc, char **argv)
{
	if (argv[1]) {
		int ack = need_ack;
		if (!strcmp(argv[1], "on")) ack = 1;
		else if (!strcmp(argv[1], "off")) ack = 0;
		if (need_ack) {
			if (ack) do_syn(argv[0]);
			else need_ack = need_syn = 0;
		}
		else {
			if (ack) {
				need_ack = need_syn = 1;
				do_syn(argv[0]);
			}
		}
	}
	else do_syn(argv[0]);
	uhook_printf("acknowledge is %s\n", need_ack ? "on" : "off");
}

static void uhook_hello(int argc, char **argv)
{
	do_syn(argv[0]);
	uhook_printf("Hello DOSEMU world, argc=%d, argv:", argc);
	while (*argv) uhook_printf(" %s", *argv++);
	uhook_printf("\n");
}

static void uhook_kill(int argc, char **argv)
{
	do_syn(argv[0]);
	uhook_printf("dosemu %d killed via user hook\n", getpid());
	if (need_ack) uhook_printf("ACK: code=%d\n", errorcode);
	leavedos(1);
}

static void uhook_version(int argc, char **argv)
{
	do_syn(argv[0]);
	uhook_printf("dosemu-%d.%d.%g\n", VERSION, SUBLEVEL, PATCHLEVEL);
}

static void uhook_keystroke(int argc, char **argv)
{
	int bufsize = 256, i=0, j;
	char *s = malloc(bufsize);

	do_syn(argv[0]);
	while (*++argv) {
		j = snprintf(s+i, bufsize-i, " %s", *argv);
		if (j == -1) {
			i = bufsize -1;
			break;
		}
		i += j;
	}
	s[i] = 0;
	if (s[0]) append_pre_strokes(s+1);
	else {
		uhook_printf("no strokes to feed in\n");
		errorcode = 1;
	}
	free(s);
}

static void uhook_log(int argc, char **argv)
{
	char buf[1024];
	do_syn(argv[0]);
	if (argv[1]) {
		SetDebugFlagsHelper(argv[1]);
	}
	GetDebugFlagsHelper(buf, 0);
	uhook_printf("%s\n",buf);
}

static void uhook_hog(int argc, char **argv)
{
	do_syn(argv[0]);
	if (argv[1]) {
		int hog = strtol(argv[1], 0, 0);
		if (hog < 0) hog = 0;
		if (hog > 100 ) hog = 100;
		config.hogthreshold = hog;
	}
	uhook_printf("hogthreshold=%d\n", config.hogthreshold);
}

static void uhook_boot(int argc, char **argv)
{
	do_syn(argv[0]);
	if (argv[1]) {
		if (!strcmp(argv[1], "on")) use_bootdisk = 1;
		else if (!strcmp(argv[1], "off")) use_bootdisk = 0;
	}
	uhook_printf("bootdisk=%s\n", use_bootdisk ? "on" : "off");
}

static void uhook_xmode(int argc, char **argv)
{
  char *p;
  long l, ll[2];

  do_syn(argv[0]);
  argc--; argv++;
  errorcode = 1;

  if (Video->change_config == NULL) {
    do_syn(argv[0]);
    uhook_printf("xmode does not work for the console\n");
  }


  if(argc <= 0) {
    uhook_printf(
      "usage: xmode <some arguments>\n"
      "  -title <name>    set window name\n"
      "  -font <font>     use <font> as text font\n"
      "  -map <mode>      map window after graphics <mode> has been entered\n"
      "  -unmap <mode>    unmap window before graphics <mode> is left\n"
      "  -winsize <width> <height>   set initial graphics window size\n"
    );
    return;
  }
  while(argc >0) {
    if(!strcmp(*argv, "-title") && argc >= 2) {
      Video->change_config(CHG_TITLE, argv[1]);
      argc -= 2; argv += 2;
    }
    else if(!strcmp(*argv, "-font") && argc >= 2) {
      Video->change_config(CHG_FONT, argv[1]);
      argc -= 2; argv += 2;
    }
    else if(!strcmp(*argv, "-map") && argc >= 2) {
      l = strtol(argv[1], &p, 0);
      if(argv[1] == p) {
        uhook_printf("invalid mode number \"%s\"\n", argv[1]);
        return;
      }
      Video->change_config(CHG_MAP, &l);
      argc -= 2; argv += 2;
    }
    else if(!strcmp(*argv, "-unmap") && argc >= 2) {
      l = strtol(argv[1], &p, 0);
      if(argv[1] == p) {
        uhook_printf("invalid mode number \"%s\"\n", argv[1]);
        return;
      }
      Video->change_config(CHG_UNMAP, &l);
      argc -= 2; argv += 2;
    }
    else if(!strcmp(*argv, "-winsize") && argc >= 3) {
      ll[0] = strtol(argv[1], &p, 0);
      if(argv[1] == p) {
        uhook_printf("invalid width \"%s\"\n", argv[1]);
        return;
      }
      ll[1] = strtol(argv[2], &p, 0);
      if(argv[2] == p) {
        uhook_printf("invalid height \"%s\"\n", argv[2]);
        return;
      }
      Video->change_config(CHG_WINSIZE, ll);
      argc -= 3; argv += 3;
    }
    else {
      uhook_printf("wrong argument \"%s\"\n", *argv);
      return;
    }
  }
  errorcode = 0;
}

static void uhook_lredir(int argc, char **argv)
{
	char *s = "\\\\LINUX\\FS";
	char *n;
	int drive, ret;

	do_syn(argv[0]);
	errorcode = 1;

	if ((argc == 3) && (!strcmp(argv[1], "del")) && (strchr(argv[2], ':'))) {
		/* delete a redirection */
		drive = tolower(argv[2][0]) - 'a';
		if (drive < 2) {
			uhook_printf("wrong drive, must be >= C:\n");
			return;
		}
		errorcode = CancelDiskRedirection(drive);
		return;
	}

	if ((argc == 2) && (strchr(argv[1], ':'))) {
		/* get the redirection root */
		char *rootdir=NULL;
		int ro;

		drive = tolower(argv[1][0]) - 'a';

		if (drive < 2) {
                        uhook_printf("wrong drive, must be >= C:\n");
                        return;
                }

                errorcode = GetRedirectionRoot(drive,&rootdir,&ro);
		if (!errorcode) {
			uhook_printf("%c: %s %s\n", argv[1][0],rootdir, ro ? "RO" : "RW");
		        free(rootdir);
		} else {
			uhook_printf("Drive %c is not redirected\n",argv[1][0]);
		}
	

                return;
	}

	if ((argc < 3) || (!strchr(argv[1], ':'))) {
		uhook_printf("wrong arguments\n");
		return;
	}
	drive = tolower(argv[1][0]) - 'a';
	if (drive < 2) {
		uhook_printf("wrong drive, must be >= C:\n");
		return;
	}
	n = argv[2];
	if (n[0] == '/') n++;
	s = concatpath(s, n);
	if (!s) {
		uhook_printf("out of memory\n");
		return;
	}
	ret = RedirectDisk(drive, s, argv[3] ? 1 : 0);
	uhook_printf("redirecting %c: to %s %s\n", argv[1][0], argv[2],
		ret ? "failed" : "successful"
	);
	errorcode = ret;
	free(s);
}

static void uhook_help(int argc, char **argv)
{
	do_syn(argv[0]);
	uhook_printf("%s", help_string);
}


static void uhook_ecpu(int argc, char **argv)
{
	do_syn(argv[0]);
#ifdef X86_EMULATOR
	if (argv[1]) {
/*FIXME, need to ask Alberto*/
		uhook_printf("setting cpuemu not yet implemented\n");
	}
	uhook_printf("cpuemu=%s\n", config.cpuemu ? "on" : "off");
#else
	uhook_printf("cpuemu not compiled in\n");
#endif
}

static void uhook_config(int argc, char **argv)
{
	do_syn(argv[0]);
#if 0
/* FIXME
 * turning off SIGALRM has ugly sideeffects here (freezing DOS for a some seconds),
 * we need an other technique to avoid signal queue overflows.
 * Currently, when the receiver is waiting, this makes no problems
 * so we skip it for now.
 */
	sigalarm_onoff(0);	/* duh, this is ugly, but the amount of printed
				   data maybe too big
				 */
#endif
	dump_config_status(&uhook_printf);
#if 0
	sigalarm_onoff(1);
#endif
}

void uhook_input(void)
{
	if (fdin == -1) return;
	nbytes = read(fdin, recvbuf+nbytes, UHOOK_BUFSIZE-nbytes);
}

static void uhook_send(void)
{
	if (!sendptr || (fdout == -1)) return;
	write (fdout, sendbuf, sendptr);
	sendptr = 0;
}

static void vuhook_printf(const char *fmt, va_list args)
{
	if (fdout == -1) return;
	sendptr = vsnprintf(sendbuf, UHOOK_BUFSIZE, fmt, args);
	if (!sendptr) return;
	if (sendptr < 0) sendptr = UHOOK_BUFSIZE -1;
	uhook_send();
}

static void uhook_printf(const char *fmt, ...)
{
	va_list args;
	if (fdout == -1) return;
	va_start(args, fmt);
	vuhook_printf(fmt, args);
	va_end(args);
}

static char *concatpath(char *dir, char *file)
{
	char *p;
	int len;

	len = strlen(dir) + 1 + strlen(file) +1;
	p = malloc(len);
	if (!p) return 0;
	snprintf(p, len, "%s/%s", dir, file);
	return p;
}

void uhook_poll(void)
{
	if (fdin == -1) return;
	/* NOTE: We assume uhook_poll() is called _only_ after handle_signals()
	 *	 so we avoid calling it again for the first run of the
	 *	 below loop
	 */
	while (nbytes > 0) {
		int i =0;
		while ((recvbuf[i] != '\n') && (i < nbytes)) i++;
		if (i < nbytes) {
			recvbuf[i] = 0;
			errorcode = 0;
			call_cmd(recvbuf, 32, cmdtab, uhook_printf);
			if (need_ack) uhook_printf("ACK: code=%d\n", errorcode);
			uhook_send();
			i++;
			nbytes -= i;
			if (nbytes) {
				memcpy(recvbuf, recvbuf+i, nbytes);
			}
		}
		handle_signals();
		/* NOTE: if there is input on fdin, as result of handle_signals
		 *	 io_select() is called and this then calls uhook_input,
		 *	 which then sets nbytes ( all clear ? )
		 */
	}
}


void init_uhook(char *pipes)
{
	if (!pipes || !pipes[0]) return;
	inpipename = strdup(pipes);
	outpipename = strchr(inpipename, ':');
	if (outpipename) {
		outpipename[0] = 0;
		outpipename++;
	}
	/* NOTE: need to open the pipe read/write here, else if the sending side
	 * closes the pipe, we would get endless 0-byte reads
	 * after this (select will trigger again and again)
	 */
	fdin = open(inpipename, O_RDWR | O_NONBLOCK);
	if (fdin != -1) {
		recvbuf = malloc(UHOOK_BUFSIZE);
		if (!recvbuf) {
			close(fdin);
			fdin = -1;
			return;
		}
		add_to_io_select(fdin, 0, uhook_input);
		if (outpipename) {
			/* NOTE: need to open read/write
			 * else O_NONBLOCK would fail to open */
			fdout = open(outpipename, O_RDWR | O_NONBLOCK);
			if (fdout != -1) {
				sendbuf = malloc(UHOOK_BUFSIZE);
				if (!sendbuf) {
					close(fdout);
					fdout = -1;
				}
			}
			else {
				fprintf(stderr,"cannot open output pipe %s\n", outpipename);
				leavedos(1);
			}
		}
	}
	else {
		fprintf(stderr,"cannot open input pipe %s\n", inpipename);
		leavedos(1);
	}
}

void close_uhook(void)
{
	if (fdin == -1) return;
	g_printf("closing user hook\n");
	close(fdin);
	fdin = -1;
	if (fdout == -1) return;
	close(fdout);
	fdout = -1;
}
