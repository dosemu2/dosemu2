/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DOSEMU built-in COMMAND.COM
 *
 * Copyright (c) 2001 Hans Lermen <lermen@fgan.de>
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "emu.h"
#include "memory.h"
#include "bios.h"
#include "video.h"	/* for CARD_NONE */
#include "doshelpers.h"
#include "utilities.h"
#include "../coopthreads/coopthreads.h"

#include "comcom.h"
#include "msetenv.h"
#include "commands.h"
#include "lredir.h"
#include "xmode.h"

#define printf  com_printf

#include "doserror.h"

/* ============== configurable options ================ */
/* define this to use cached batchfile reading,
 * it turned out that it doesn't really speed up processing,
 * so power is eaten by the somehow too simple parser code and the
 * lots of DOS functions we are calling while parsing.
 */
#undef CMD_USE_CACHED_BATCH

/* define this to disable the buildin commandline editor and to
 * read commands directly from INT21,AH=0A
 */
#undef USE_PLAIN_DOS_FUNCT0A

/* define this if you want lowmem eating built-ins of the DOSEMU
 * support programs. Because those take lowmem from the comcom heap,
 * you need to increase the static heapsize of comcom by about 1K.
 * Further more, currently some of them have memory leaks (in fact they
 * rely on running as separate programs) and don't free lowmem.
 * So before you enable this option, check the code of those built-ins ;-)
 */
#undef USE_HEAP_EATING_BUILTINS


/* define here the max number of args possible, including the terminating NULL
 * NOTE: the _whole_ commandline must fit and in the worst case,
 *	 if one arg is just a blank plus one character, this computes as
 *
 *	    MAXARGS = 126 / 2
 *
 *	 However, 48 should be enough for reasonable usage
 */
#define MAXARGS	48

/* define this, if you want to use plain BIOS reads for keyboard input.
 * Note that you may not be able to use ansi.sys, because it bypasses
 * DOS. This options was needed for some older freedos kernels.
 */
#undef USE_BIOSKBD

/* ============== end of configurable options ========= */


struct dos0a_buffer {
	unsigned char	max_len;  /* max bytes in buf including CR */
	unsigned char	read_len; /* read bytes in buf _not_ including CR */
	char		buf[127];
} __attribute__((packed));

struct batchdata {
	struct batchdata *parent; /* parent batch stream, to which
				   * to return after processing this one
				   */
	int mode;	/* 0 = interactive
			 * 1 = read from batch file
			 */
	int argc;	/* number of positional parameters ($0 ... $9) */
	char **argv;	/* content of positional parameters */
	int argcsub;	/* argc to use in child batches */
	char **argvsub;	/* argv to use in child batches */
	int argshift;	/* for use by cmd_shift */
	int stdoutredir;	/* STDOUT is redirected */
	long nextline;		/* fileposition from which to read next line */
	char filename[128];	/* fullqualified filename of batchfile */
	struct dos0a_buffer buf0a; /* line buffer (as for INT21, AH=0A) */
#ifdef CMD_USE_CACHED_BATCH
	char batchbuf[0x2000];	/* cachbuffer for batchfiles */
	int batchsize;		/* filesize in batchbuf, or 0 if not opened */
#endif
};

struct res_dta {
	struct batchdata *current_bdta;
	int exitcode;
	int errorlevel; /* exit code for external commands */
	int need_errprinting;
	int echo_on;
	int envsize;	/* size of enlarged enviroment */
	int option_p;	/* 1 = this command.com is primary */
	int cannotexit;	/* 1 = this command.com cannot exit safely */
	char toplevel;	/* 1 = toplevel command.com */
	int break_pending;
	int fatal_handler_return_possible;
	struct vm86_regs fatal_handler_return_regs;
	jmp_buf fatal_handler_return;
};


static int command_inter_preter_loop(int, char *, int, char **);
static void print_prompt(int fd);
static int com_exist_file(char *file);
static int dopath_exec(int argc, char **argv);

#define CMD_LINKAGE \
	struct res_dta *rdta = UDATAPTR

#define CMD_LINKAGE2 \
	CMD_LINKAGE; \
	struct batchdata *bdta = rdta->current_bdta

#define CMD_LINKAGE3 \
	CMD_LINKAGE2; \
	struct dos0a_buffer *buf0a = &bdta->buf0a

#define LEN0A (buf0a->read_len)
#define STR0A (buf0a->buf)
#define EXITCODE (rdta->exitcode)
#define ECHO_ON (rdta->echo_on)
#define CLEAR_EXITCODE EXITCODE = 0
#define SET_CHILD_ARGS(i) ({if (bdta) { \
		bdta->argvsub = argv+i; \
		bdta->argcsub = argc-i; \
		bdta->argshift = 0; \
	}})
#define BREAK_PENDING ( \
	rdta->break_pending ? rdta->break_pending = 0, 1 : 0 )


static int getkey(void)
{
#ifdef USE_BIOSKBD
	int ret = com_bioskbd(0);
	if (ret & 255) return ret & 255;
	return (ret >> 8) | 0x100;
#else
	int oldhog = config.hogthreshold;
	config.hogthreshold = 1;	/* be 'nice' while waiting */
	HI(ax) = 7;
	call_msdos();
	if (LO(ax)) {
		config.hogthreshold = oldhog;
		return LO(ax);	/* simple ascii */
	}
	HI(ax) = 8;
	call_msdos();
	HI(ax) = 1;
	config.hogthreshold = oldhog;
	return LWORD(eax);		/* extended keycode */
#endif
}



#ifndef USE_PLAIN_DOS_FUNCT0A

#define HISTMAX	32
static char history[HISTMAX][128];
static int histi = -1;
static int history_empty = 1;

void save_comcom_history(char *fname)
{
	FILE *f;
	int i;

	if (!fname || histi <0 || history_empty) return;
	f = fopen(fname, "w");
	if (!f) return;
	i = (histi + 1) & ~(-HISTMAX);
	while (i != histi) {
		if (history[i][0] != -1) {
			fwrite(history[i]+1, history[i][0], 1, f);
			fputc('\n', f);
		}
		i = (i + 1) & ~(-HISTMAX);
	}
	fwrite(history[i]+1, history[i][0], 1, f);
	fputc('\n', f);
	fclose(f);
}


void load_comcom_history(char *fname)
{
	FILE *f;
	int i, len;
	char buf[1024];

	if (!fname) return;
	f = fopen(fname, "r");
	if (!f) return;

	for (i=0; i < HISTMAX; i++) history[i][0] = -1;
	i = 0;
	history_empty = 1;
	while (1) {
		if (!fgets(buf, sizeof(buf), f)) {
			if (history_empty) histi = -1;
			else histi = (i - 1) & ~(-HISTMAX);
			break;
		}

		len = strlen(buf) - 1;
		if (len < 0) continue;
		if (len > 126) len = 126;

		memcpy(history[i]+1, buf, len);
		history[i][0] = len;
		history[i][len+1] = '\r';
		i = (i + 1) & ~(-HISTMAX);
		history_empty = 0;
	}
	fclose(f);
}


static void builtin_funct0a(char *buf)
{
	struct com_starter_seg  *ctcb = CTCB;
	int histl = -1;
	char *p, *b;
	char lastb[128] = "";
	int lastblen = 0;
	char *saved_line = 0;
	int i, lastkey = 0;
	int count, cursor, winstart, attrib;
	int screenw, screenpage, posx, posy, leftmostx, linelen;
	int dumbterm = config.cardtype == CARD_NONE;

	static void start_line(void)
	{
		screenw = com_biosvideo(0x0f00) >> 8;
		if (dumbterm) screenw = 79;
		screenpage = HI(bx);
		com_biosvideo(0x0300);
		posy = HI(dx);
		leftmostx = posx = LO(dx);
		linelen = screenw - leftmostx -1;
		if (linelen >128) linelen = 128;
		winstart = cursor = 0;
		attrib = com_biosvideo(0x800) >> 8;
		if (!attrib) attrib = 7;
	}

	static void putkey(int key)
	{
		LWORD(ebx) = screenpage;
		com_biosvideo((key & 255) | 0x0e00);
		cursor++;
	}

	static void setcursorpos(int x)
	{
		HI(dx) = posy;
		LO(dx) = posx + x;
		HI(bx) = screenpage;
		com_biosvideo(0x200);
	}

	static void refresh_dumb(int len)
	{
		static char bspaces[128] = "";
		int commonlen, i;
		if (!bspaces[0]) for (i=0; i<128; i++) bspaces[i] = '\b';
		if (len < lastblen) {
			/* clear the tail */
			for (i=len; i < lastblen; i++) {
				com_doswrite(2, "\b \b", 3);
			}
			commonlen = len;
		}
		else commonlen = lastblen;
		if (memcmp(lastb, b, commonlen)) {
			/* no common contents */
			com_doswrite(2, bspaces, commonlen);
			com_doswrite(2, b, len);
		}
		else {
			if (len > lastblen) {
				com_doswrite(2, b+lastblen, len - lastblen);
			}
		}
		memcpy(lastb, b, len);
		lastblen = len;
	}

	static void update_display(int forcerefresh)
	{
		int len;

		if (count > linelen) {
			if (cursor >= linelen) { /* shift right */
				if ((count - winstart) > linelen) {
					int currentchar = winstart + cursor;

					if (currentchar > count)
						currentchar = count;
					winstart = currentchar - linelen;

					forcerefresh = 1;
				}
				cursor = linelen;
			}
			else if (cursor < 0) {
				if (winstart >0) winstart--;
				cursor = 0;
				forcerefresh = 1;
			}
		}
		else {
			if (cursor < 0) {
				cursor = 0;
				return;
			}
			if (cursor > count) {
				cursor = count;
				return;
			}
		}

		if (!forcerefresh) {
			setcursorpos(cursor);
			return;
		}
		len = count - winstart;
		if (len > linelen) len = linelen;

		memcpy(b, p+winstart, len);
		memset(b+len, ' ', linelen - len);

		if (dumbterm) {
			refresh_dumb(len);
			setcursorpos(cursor);
			return;
		}
		HI(bx) = screenpage;
		LO(bx) = attrib;
		LWORD(ecx) = linelen;
		HI(dx) = posy;
		LO(dx) = posx;
		LWORD(es) = COM_SEG;
		LWORD(ebp) = COM_OFFS_OF(b);
		com_biosvideo(0x1300);
		setcursorpos(cursor);
	}

	static void insert(int c)
	{
		int i, j = winstart + cursor;
		for (i = count; i > j; i--) p[i] = p[i-1];
		p[j] = c;
		count++;
		cursor++;
	}

	static void insertstring(const char *s, const int len)
	{
		int i, j = winstart + cursor;

		for (i = count - 1; i >= j; i--)
			p [i + len] = p [i];
		memcpy (p + j, s, len);

		count += len;
		cursor += len;
	}

	static void delete(int pos)
	{
		int i, j = cursor+pos+winstart;
		if (j < 0) return;
		for (i=j; i < count - 1; i++) p[i] = p[i+1];
		count--;
		cursor = cursor+pos;
		if (cursor < 0) cursor = 0;
		if (winstart + cursor > count) cursor = count - winstart;
		if (count >= linelen && winstart > 0) {
			winstart--;
			cursor++;
		}
	}

	static void replace_line(char *line)
	{
		memcpy(buf, line, line[0]+2);
		p = buf+1;
		winstart = 0;
		cursor = count = buf[0];
		if (cursor >linelen) {
			cursor = linelen;
			winstart = count - linelen;
		}
	}

	static void from_history(int direction)
	{
		if (history_empty) return;
		switch (lastkey) {
		    case 0x147:
		    case 0x148:
		    case 0x14B:
		    case 0x14D:
		    case 0x14F:
		    case 0x150:
			break;
		    default:
			saved_line = history[(histi + 1) & ~(-HISTMAX)];
			memcpy(saved_line+1, buf+1, count);
			saved_line[0] = count;
			histl = histi;
			if (direction <0) histl = (histl +1) & ~(-HISTMAX);
			break;
		}
		histl = (histl + direction) & ~(-HISTMAX);
		while (history[histl][0] == -1) {
			histl = (histl + direction) & ~(-HISTMAX);
		}
		replace_line(history[histl]);
	}

	static int readline(char *buf)
	{
		int c, d;

		/* note: not a true implementation of the "template"-based command-line completion keys... */
		int h = histi & ~(-HISTMAX);
		char *template_len = NULL, *template = NULL;
		if (!history_empty)
		{
			template_len = history [h] + 0;
			template = history [h] + 1;
		}

		p = buf+1;
		start_line();
		count = cursor = 0;
		while (1) {
			c = getkey();
			switch (c) {
			case 0x013B:	/* F1 */
				if (!history_empty) {
					int l = winstart + cursor;

					if (count > 126) break;

					if (l < *template_len) {
						insert (template [l]);
						update_display (1);
					}
				}
				break;

#define minimum(a,b) ((a)<(b)?(a):(b))

			case 0x013C:	/* F2 */
				d = getkey();

				if (!history_empty) {
					int l = winstart + cursor;

					if (l < *template_len) {
						char *start = template + l;
						char *loc;
						if ((loc = strchr (start, d)) != NULL) {
							/* copy up to the character specified in the template
								(but not including that character) */
							int len = minimum (loc - start, 127 - count);
							insertstring (template + l, len);
							update_display(1);
						}
					}
				}
				break;

			case 0x013D:	/* F3 */
				if (!history_empty) {
					int l = winstart + cursor;

					if (l < *template_len) {
						int len = minimum (*template_len - l, 127 - count);
						insertstring (template + l, len);
						update_display(1);
					}
				}
				break;

#undef minimum

		/* can't implement this because Del is already used for comcom history */
		#if 0
			case 0x0153:		/* Del */
				if (!history_empty) {
					int l = winstart + cursor;

					if (l < *template_len) {
						int i;

						for (i = l + 1; i <= *template_len	/* +NUL */; i++)
							template [i - 1] = template [i];

						(*template_len)--;
					}
				}
				break;
		#endif

			case 0x013E:	/* F4 */
				d = getkey();

				if (!history_empty) {
					int l = winstart + cursor;

					if (l < *template_len) {
						char *start = template + l;
						char *loc;
						if ((loc = strchr (start, d)) != NULL) {
							char *left = start, *right = loc;

							/* delete up to the character specified in the template
								(but not including that character) */
							for (; *right; left++, right++)
								*left = *right;
							*left = '\0';

							(*template_len) -= (loc - start);
						}
					}
				}
				break;

			/* any volunteers? */
			case 0x013F:		/* F5 */
			case 0x0140:		/* F6 */
			case 0x0152:		/* INS */
				break;

			case 0x014D:	/* right arrow key */
			    cursor++;
			    update_display(0);
			    break;
			case 0x0150:	/* down arrow key */
			    from_history(1);
			    update_display(1);
			    break;
			case 0x0148:	/* up arrow key */
			    from_history(-1);
			    update_display(1);
			    break;
			case 0x014B:	/* left arrow key */
			    if (!dumbterm) {
				cursor--;
				update_display(0);
				break;
			    }
			    /* else fall through */
			case 8:		/* backspace */
			    if (count > 0) {
				delete(-1);
				update_display(1);
			    }
			    break;
		#if 1
			case 0x0153:		/* Del */
			    if (count > 0 && winstart + cursor < count) {
				delete(0);
				update_display(1);
			    }
			    break;
		#endif
			case 0x0147:		/* Home key */
			case 'A' & ~0x40:	/* ^A */
			    cursor = 0;
			    winstart = 0;
			    update_display(1);
			    break;
			case 0x014F:		/* End key */
			case 'E' & ~0x40:	/* ^E */
			    cursor = count;
			    winstart = count - linelen;
			    if (winstart <0) winstart = 0;
			    update_display(1);
			    break;
			case '\r':
			    p[count] = 0;
			    buf[0] = count;
			    return buf[0];
			case 'C' & ~0x40:	/* ^C */
			    putkey('^'); putkey('C');
			    buf[0] = p[0] = 0;
			    return 0;
			case 'Z' & ~0x40:	/* ^Z */
			    putkey('^'); putkey('Z');
				 update_display(1);
				 break;
			case 32 ... 126:
			case 128 ... 255:
			    if (count > 126) break;
			    if (winstart + cursor < count) {
				insert(c);
			    }
			    else {
				p[count++] = c;
				cursor++;
			    }
			    update_display(1);
			    break;
		    }
		    lastkey = c;
		};
	}

	b = lowmem_alloc(128);
	if (!b) {
		buf[0] = 0;
		buf[1] = '\r';
		return;
	}

	if (histi == -1) {
		for (i=0; i <HISTMAX; i++) history[i][0] = -1;
		histi = 0;
	}

	count = readline(buf);

	if (count && strcasecmp(buf+1, history[histi]+1)) {
		histi = (histi + 1) & ~(-HISTMAX);
		memcpy(history[histi], buf, buf[0]+2);
		history_empty = 0;
	}
	buf[count+1] = '\r';
	lowmem_free(b, 128);
}

#endif /* not USE_PLAIN_DOS_FUNCT0A */

#ifndef CMD_USE_CACHED_BATCH
  #define BATCH_OPEN	com_dosopen
  #define BATCH_READ	com_dosreadtextline
  #define BATCH_CLOSE	com_dosclose
#else
  #define BATCH_OPEN	cache_open_batch
  #define BATCH_READ	cache_read_batch
  #define BATCH_CLOSE	cache_close_batch

static void cache_batch_file(void)
{
	CMD_LINKAGE2;
	int size;
	int fd = com_dosopen(bdta->filename, 0);

	bdta->batchsize = 0;
	if (fd < 0) return;
	do {
		size = com_dosread(fd, bdta->batchbuf+bdta->batchsize, 128);
		if (size != -1) bdta->batchsize += size;
	} while ((size == 128) && (bdta->batchsize < sizeof(bdta->batchbuf)));
	com_dosclose(fd);
	if (size < 0) bdta->batchsize = 0;
}

static int cache_open_batch(char *fname, int access)
{
	CMD_LINKAGE2;
	if (!bdta->batchsize) return com_dosopen(fname, access);
	return 0;
}

static int cache_close_batch(int fd)
{
	CMD_LINKAGE2;
	if (!bdta->batchsize) return com_dosclose(fd);
	return 0;
}

static int cache_read_batch(int fd, char *buf, int bsize, long *lastseek)
{
	CMD_LINKAGE2;
	static char eolpattern[] = {'\r', '\n', 0x1a, 0};
	int size;
	long newseek;
	char *s, *p;

	if (!bdta->batchsize) {
		return com_dosreadtextline(fd, buf, bsize, lastseek);
	}
	if (*lastseek >bdta->batchsize) {
		buf[0] = 0;
		return -1;
	}
	newseek = *lastseek;
	s = bdta->batchbuf + newseek;
	if (s[0] == 0x1a) {
		buf[0] = 0;
		return -1;
	}
	p = strpbrk(s, eolpattern);
	if (p && (p - s) < bsize) {
		size = (p - s);
		if (*p == '\r') p +=2;
		else if (*p != 0x1a) p++;
		newseek += p - s;
	}
	else {
		size = bsize;
		newseek += bsize;
	}
	*lastseek = newseek;
	memcpy(buf, s, size);
	if (size < bsize) buf[size] = 0;
	return size;
}

#endif /* CMD_USE_CACHED_BATCH */

static int lowlevel_read_next_command(void)
{
	CMD_LINKAGE3;
	int fd, len;

	buf0a->max_len = sizeof(buf0a->buf);
	if (!bdta->mode) {
		/* interactive, using DOS_0A to read the line */

#ifdef USE_PLAIN_DOS_FUNCT0A
		struct com_starter_seg  *ctcb = CTCB;
		struct dos0a_buffer *s = (void *)lowmem_alloc(256);
		s->max_len = 127;
		HI(ax) = 0x0a;
		LWORD(ds) = COM_SEG;
		LWORD(edx) = COM_OFFS_OF(s);
		call_msdos();
		memcpy(&buf0a->read_len, &s->read_len, s->read_len+1);
		lowmem_free((char *)s, 256);
#else
		buf0a->max_len = 127;
		builtin_funct0a(&buf0a->read_len);
#endif
		return 1;	/* means: more available */
	}

	/* batchmode, reading next 'current' line from batchfile */


	fd = BATCH_OPEN(bdta->filename, 0);
	if (fd == -1) {
		rdta->exitcode = DOS_ENOENT;
		return -1;	/* means: error */
	}

	len = BATCH_READ(fd, buf0a->buf, buf0a->max_len, &bdta->nextline);
	BATCH_CLOSE(fd);
	if (len == -1) {
		buf0a->read_len = 0;
		return 0;	/* means: EOF */
	}

	if (len == buf0a->max_len) {
		buf0a->buf[buf0a->max_len-1] = '\r';
		len--;
	}
	buf0a->buf[len] = '\r';
	buf0a->read_len = len;
	return 1;		/* means: more available */
}

static char *skip2nonwhite(char *s)
{
	static char eoldelim[] = " \t\r";
	return strpbrk(s, eoldelim);
}

static char *skipwhite(char *s)
{
	while(*s == ' ' || *s == '\t') s++;
	return s;
}

static int read_next_command(void)
{
	CMD_LINKAGE3;
	int ret, this_echo;
	char *p;

	static void substitute_positional(char **t_, char **s_)
	{
		struct batchdata *pd = bdta->parent;
		char *s = *s_, *t = *t_, *p;
		int i = s[0] - '0';
		int len;

		if (pd) {
			i += pd->argshift;
			if (i < pd->argcsub) {
				p = pd->argvsub[i];
				len = strlen(p);
				memcpy(t, p, len);
				*t_ = t + len;
			}
		}
		while (isdigit(*s)) s++;
		*s_ = s;
		return;
	}

	static void substitute_env(char **t, char **s_, char *p)
	{
		char *s = *s_;
		char buf[128];
		int len;

		*p = 0;
		if (strpbrk(s, " \t")) {
			/* ignore it, '%' belongs to a subsequen arg */
			**t = '%';
			*t +=1;
			*p = '%';
			return;
		}
		if (!strcasecmp(s, "_exitcode_")) {
			s = buf;
			sprintf(s, "%d", EXITCODE);
		}
		else s = com_getenv(s);
		if (s) {
			len = strlen(s);
			memcpy(*t, s, len);
			*t += len;
		}
		*p = '%';
		*s_ = p + 1;
	}

	static void variable_substitution(void)
	{
		char buf[1024];	/* big enough to catch overflows */
		char *s = STR0A, *p, *t;
		int len = LEN0A;
		
		s[len] = 0;
		p = strpbrk(s, "%|<>");
		if (!p || ((p - s) > 126)) {
			/* usual case */
			s[len] = '\r';
			return;
		}

		memcpy(buf, s, (p - s));
		t = buf + (p - s);
		s = p;
		while (*s) {
		    switch(*s) {
			case '|':  case '<': case '>':
			    if ((s != buf) && (s[-1] != ' ')) {
			    	*t++ = ' '; 
			    }
			    *t++ = *s++;
			    if (*s == '>') *t++ = *s++;
			    break;
			case '%': {
			    if (isdigit(s[1])) {
				s++;
				substitute_positional(&t, &s);
			    }
			    else if (s[1] == '%') {
				s++;
				*t++ = *s++;
			    }
			    else {
				p = strchr(s+1, '%');
				if (p) {
				    s++;
				    substitute_env(&t, &s, p);
				}
				else *t++ = *s++;
			    }
			    break;
			}
			default:
			    *t++ = *s++;
			    break;
		    }
		}
		len = (t - buf);
		if (len > 126) len = 126;
		buf[len] = '\r';
		memcpy(STR0A, buf, len+1);
		LEN0A = len;
	}

	static void echo_handling(void)
	{
		this_echo = STR0A[0] != '@';
		if (!this_echo) {
			memcpy(STR0A, STR0A+1, LEN0A);
			LEN0A--;
		}
	}

	while (1) {
		ret = lowlevel_read_next_command();
		if (ret <= 0) return ret;
		echo_handling();
		variable_substitution();
		if (!bdta->mode) return ret;

		p = skipwhite(STR0A);
		if (p[0] == ':') continue;	/* ignore all label lines
						 * NOTE: some people use
						 * this as comment lines
						 */
		if (this_echo && ECHO_ON) {
			print_prompt(2);
			com_doswrite(2, STR0A, LEN0A);
			com_doswrite(2, "\r\n", 2);
		}
		return ret;
	}
}

static int findoption(char *option, int argc, char **argv)
{
	int i, len;
	char *p = strchr(option, ':');

	if (p) {
		len = (p - option) + 1;
		for (i=1; i < argc; i++)
			if (!strncasecmp(option, argv[i], len)) {
				argv[i] += len;
				return i;
			}
		return 0;
	}
	for (i=1; i < argc; i++)
		if (!strcasecmp(option, argv[i])) return i;
	return 0;
}

static int remove_arg(int arg, int argc, char **argv)
{
	for (; arg < argc; arg++) argv[arg] = argv[arg+1];
	return --argc;
}

static int findoption_remove(char *option, int *argc, char **argv)
{
	int i = findoption(option, *argc, argv);
	if (!i) return 0;
	*argc = remove_arg(i, *argc, argv);
	return i;
}

static int concat_args(char *buf, int first, int last, char **argv)
{
	char *delim = "";
	int size = 0;
	for (; first <= last; first++) {
		size += sprintf(buf+size, "%s%s", delim, argv[first]);
		delim = " ";
	}
	return size;
}

static int com_exist_file(char *file)
{
	return com_dosfindfirst(file, DOS_ATTR_FILE) == 0;
}

#if 0
static int com_exist_anyentry(char *file)
{
	return com_dosfindfirst(file, DOS_ATTR_ALL) == 0;
}
#endif

static int com_exist_dir(char *dir)
{
	struct findfirstnext_dta *dta = (void *)CTCB;
	int ret = com_dosfindfirst(dir, DOS_ATTR_DIR);
	return (!ret && (dta->attrib & DOS_ATTR_DIR)); 
}

static int com_exist_executable(char *buf, char *file)
{
	/* now, _this_ is the correct order of execution! */
	char *extlist[] = {
		"com", "exe", "bat"
	};
	int len = strlen(file);
	char *p = strcpy(buf, file) +len;
	int i;

	if ((len >4) && (file[len-4] == '.')) {
		/* '..\ws' does _not_ mean a file with extension '\ws' */
		boolean isext = TRUE;
		for (i = len - 3; i < len; i++) {
			if (!isalnum (file[i])) {
				isext = FALSE;
				break;
			}
		}
		if (isext) {
			for (i=0; i<3; i++)
				if (!strncasecmp(file+len-3, extlist[i], 3))
					if (com_exist_file(file)) return i+1;
			return 0;
		}
	}
	*p++ = '.';
	for (i=0; i<3; i++) {
		memcpy(p, extlist[i], 4);
		if (com_exist_file(buf)) return i+1;
	}
	return 0;
}

static int scan_path_env(char *buf, char **argv)
{
	char *path = com_getenv("PATH");
	char *program = argv[0];
	char buf_[256];
	char *p;
	int argi=0, i;

	if (!strcasecmp(argv[0], "call")) {
		argi++;
		program = argv[1];
	}
	i = com_exist_executable(buf, program);
	if (i) {
		if (!argi) return i;
		if (i == 3) return 11;
	}
	if (!path) return 0;	/* no chance to find it elsewhere */
	path = strcpy(buf_, path);
	do {
		p = strchr(path, ';');
		if (p) *p = 0;
		sprintf(buf, "%s\\%s", path, program);
		i = com_exist_executable(buf, buf);
		if (i) {
			if (!argi) return i;
			if (i == 3) return 11;
		}
		if (!p) break;
		path = p+1;
	} while (path[0]);
	return 0;
}

struct redir_stdxx {
	int std_fd;
	int saved_fd;
	int filefd;
};

static void init_redir_struct(int std, struct redir_stdxx *r)
{
	r->std_fd = std;
	r->saved_fd = r->filefd = -1;
}

static void com_restore_redirect(struct redir_stdxx *r)
{
	CMD_LINKAGE2;
	if (r->saved_fd == -1) return;
	com_dosforceduphandle(r->saved_fd, r->std_fd);
	com_dosclose(r->saved_fd);
	r->saved_fd = -1;
	com_dosclose(r->filefd);
	if (r->std_fd) bdta->stdoutredir = 0;
}

static int com_redirect_stdxx(char *fname, struct redir_stdxx *r, int append)
{
	CMD_LINKAGE2;
	if (r->saved_fd != -1) return 1;

	r->filefd = com_dosopen(fname,
			r->std_fd ? (append?2:DOS_ACCESS_CREATE) : 0);
	if (r->filefd == -1 && r->std_fd) {
		r->filefd = com_dosopen(fname, DOS_ACCESS_CREATE);
	}
	else if (r->std_fd) {
		if (append) com_dosseek(r->filefd, 0, 2);
	}
	r->saved_fd = com_dosduphandle(r->std_fd);
	if (r->saved_fd == -1) {
		com_dosclose(r->filefd);
		return 0;
	}
	if (com_dosforceduphandle(r->filefd, r->std_fd) == -1) {
		com_restore_redirect(r);
		return 0;
	}
	if (r->std_fd) bdta->stdoutredir = 1;
	return 1;
}


static int create_temp_file(char *namebuf)
{
	char *tmpdir = com_getenv("TEMP");
	int fd;

	if (!tmpdir) tmpdir = "";
	strcpy(namebuf, tmpdir);
	fd = com_doscreatetempfile(namebuf);
	if ((fd != -1) || !tmpdir[0]) return fd;
	namebuf[0] = 0;
	return com_doscreatetempfile(namebuf);
}

static void setexitcode(int code, int printfd)
{
	CMD_LINKAGE;
	EXITCODE = code;

	if (printfd < 0) {
		rdta->need_errprinting = -printfd;
		return;
	}
	com_fprintf(printfd, "%s\n", decode_DOS_error(code));
	rdta->need_errprinting = 0;
}

static char *basename_of(char *fname, int *baselen)
{
	int len = strlen(fname);
	char *p = fname + len;
	char c;

	if (!len) return fname;
	while (p != fname) {
		c = *--p;
		if ((c == '\\') || (c == '/') || (c == ':')) {
			p++;
			break;
		}
	}
	if (baselen) {
		*baselen = len - (p - fname);
	}
	return p;
}

static int mkstemm(char *buf, char *path, int expand)
{
	char *p;

	if (!expand || (expand && com_doscanonicalize(buf, path)))
		strcpy(buf, path);
	p = basename_of(buf,0);
	*p = 0;
	return p - buf;
}

static int kbdask(char *answerlist, char *fmt, ...)
{
	va_list ap;
	int i, len;
	char s[128], alist[128];
	unsigned char key, c, dfltkey = 0;
	va_start(ap, fmt);

	if (!answerlist || !answerlist[0]) return 0;
	com_vfprintf(2, fmt, ap);
	for (i=0; answerlist[i]; i++) {
		c = answerlist[i];
		if (isupper(c)) dfltkey = c = tolower(c);
		alist[i] = c;
		s[i<<1] = answerlist[i];
		s[(i<<1)+1] = '/';
	}
	len = i;
	s[(i<<1)-1] = 0;
	com_fprintf(2, " (%s)?", s);
	while (1) {
		key = getkey();
		if (key == 3) return -1;
		if (dfltkey && key == 13) return dfltkey;
		for (i=0; i < len; i++) {
			if (tolower(key) == alist[i]) return key;
		}
	}
}

/* ---------------- internal commands --------------------- */


static int cmd_echo(int argc, char **argv) {
	CMD_LINKAGE2;
	char buf[256] = "";
	int fd = 2;

	/* "echo" */
	if (argc == 1 && argv[0][4] != '.') {
		com_printf("ECHO is %s\n", ECHO_ON ? "on" : "off");
		return 0;
	}

	/* "echo on" or "echo off" */
	if (argc == 2) {
		if (!strcasecmp(argv[1], "off")) {
			ECHO_ON = 0;
			return 0;
		}
		if (!strcasecmp(argv[1], "on")) {
			ECHO_ON = 1;
			return 0;
		}
	}
	if (argc > 1) concat_args(buf, 1, argc-1, argv);
	if (bdta && bdta->stdoutredir) fd = 1;

	/* "echo." --> "" and "echo. message" --> " message" */
	if (argv[0][4] == '.' && argc > 1) com_fprintf (fd, " ");

	/* "message" */
	com_fprintf(fd, "%s\n", buf);

	return 0;
}

static int cmd_pause(int argc, char **argv) {
	/*CMD_LINKAGE;*/
	char tx[] = "Press any key to continue ...\n";
	int c;

	com_printf (tx);
	c = getkey();
	if ((c &255) == ('C' & ~0x40)) {
		/*rdta->break_pending++;*/
		com_printf ("^C");
	}
	com_printf("\r\n");
	return 0;
}

static int cmd_rem(int argc, char **argv) {
	CMD_LINKAGE2;
	char buf[256];

	if (!bdta->mode || !ECHO_ON) return 0;
	concat_args(buf, 0, argc-1, argv);
	com_printf("%s\n", buf);
	return 0;
}

static int cmd_goto(int argc, char **argv) {
	CMD_LINKAGE3;
	long oldpos, line;
	char *arg1 = argv[1];
	char *p;
	int len;

	if (!bdta->mode || !arg1) {
		return DOS_EINVAL;
	}
	oldpos = bdta->nextline;
	line = bdta->nextline = 0;
	len = strlen(arg1);
	for (; lowlevel_read_next_command() > 0; line = bdta->nextline) {
		p = STR0A;
		p[LEN0A] = 0;
		p = skipwhite(p);
		if (*p != ':') continue;
		p++;
		if (strncasecmp(p, arg1, len)) continue;

		/* found it */
		p = skip2nonwhite(p);
		p = skipwhite(p);
		if (*p) bdta->nextline = line + (p - STR0A);
		bdta->nextline = line;
		return 0;
	}
	bdta->nextline = oldpos;	/* restore old file position */
	return DOS_EINVAL;
}

static int cmd_for(int argc, char **argv)
{
	CMD_LINKAGE2;
	struct findfirstnext_dta *dta = (void *)CTCB;
	char s[256], *p;
	char *vname;
	int vnamelen;
	int i, g1, gn, arg0_new, len;
	char *repl[32];
	int repli, replen;
	char replbuf[256], callbuf[256];

	static void do_command(char *varcontents)
	{
		int i, j, vlen = strlen(varcontents);
		anyDTA saved_dta;
		char *p = callbuf+1;
		char *s = replbuf;
		
		for (i=0; i <repli; i++) {
			j = repl[i] - s;
			memcpy(p, s, j);
			p += j;
			memcpy(p, varcontents, vlen);
			p += vlen;
			s = repl[i] + vnamelen;
		}
		i = replen - (s - replbuf);
		memcpy(p, s, i+1);
		callbuf[0] = p - (callbuf+1) + i;
		/* WARNING! callbuf may be modified by com_argparse */
		j = com_argparse(callbuf, argv+arg0_new, MAXARGS - (argc-arg0_new) -1);
		saved_dta = PSP_DTA;
		SET_CHILD_ARGS(arg0_new);
		bdta->argcsub = j;	/* this may have more args then before */
		dopath_exec(j, argv+arg0_new);
		PSP_DTA = saved_dta;
	}


	CLEAR_EXITCODE;
	if (argc < 6) return DOS_EINVAL;
	if (strcasecmp(argv[2], "in")) return DOS_EINVAL;
	vname = argv[1];
	if (vname[0] != '%') return DOS_EINVAL;
	vnamelen = strlen(vname);

	/* find the range 'group' */
	g1 = 3;
	if (argv[g1][0] != '(') return DOS_EINVAL;
	if (argv[g1][1]) argv[g1]++;
	else g1++;
	gn = 0;
	for (i=g1; i < argc; i++) {
		p = strchr(argv[i], ')');
		if (p) {
			if (strcasecmp(argv[i+1], "do")) return DOS_EINVAL;
			arg0_new = i+2;
			gn = i;
			if (argv[gn] != p) *p = 0;
			else gn--;
			break;
		}
	}
	if (!gn || arg0_new >= argc ) return DOS_EINVAL;
	if (g1 > gn) return DOS_EINVAL;

	/* find all instances of 'variable' in the command tail */
	replen = concat_args(replbuf, arg0_new, argc-1, argv);
	repli = 0;
	p = replbuf;
	do {
		p = strstr(p, vname);
		if (p) {
			repl[repli++] = p;
			p += vnamelen;
		}
	} while (p);

	/* Ok, now for the real work */
	for (i=g1; i <= gn; i++) {
		if (strpbrk(argv[i], "*?")) {
		    /* wildcard, may result in empty entry */
		    if (!com_dosfindfirst(argv[i], DOS_ATTR_ALL)) {
			/* this is a file entry, may be wild card */
			len = mkstemm(s, argv[i], 0);
			strcpy(s+len, dta->name);
			do_command(s);
			while (!com_dosfindnext()) {
				strcpy(s+len, dta->name);
				do_command(s);
			}
		    }
		}
		else {
			/* this is plain text */
			do_command(argv[i]);
		}
	}
	return EXITCODE;
}


static int cmd_if(int argc, char **argv)
{
	CMD_LINKAGE2;
	int invers = 0;
	int argi = 1, argex, level;
	char *p;

	if (argc < 3) return DOS_EINVAL;
	if (!strcasecmp(argv[argi], "not")) {
		invers = 1;
		argi++;
	}
	if (!strcasecmp(argv[argi], "exist")) {
		if (argi+2 >= argc) return DOS_EINVAL;
		if ( (com_exist_file(argv[argi+1]) ? 1 : 0) ^ invers ) {
			argi += 2;
			SET_CHILD_ARGS(argi);
			dopath_exec(argc-argi, argv+argi);
			return EXITCODE; /* do NOT change exitcode */
		}
		return 0;
	}

	if (!strcasecmp(argv[argi], "errorlevel")) {
		if (argi+2 >= argc) return DOS_EINVAL;
		level = strtoul(argv[argi+1], 0, 10);
		if (((rdta->errorlevel >= level) ? 1 : 0) ^ invers) {
			argi += 2;
			SET_CHILD_ARGS(argi);
			dopath_exec(argc-argi, argv+argi);
			return EXITCODE; /* do NOT change exitcode */
		}
		return 0;
	}

	/* we come here on string compare */
	p = strchr(argv[argi], '=');
	if (p && p[1] == '=') {
		/* have to split the argument */
		*p = 0;
		p += 2;
		p = skipwhite(p);
		argex = argi + 1;
	}
	else {
		if (argi+3 >= argc) return DOS_EINVAL;
		if (strcmp(argv[argi+1], "==")) return DOS_EINVAL;
		p = argv[argi+2];
		argex = argi + 3;
	}
	/* NOTE 'if xxx == XXX' does case sensitive compare on MSDOS
	 * so we do a normal strcpy()
	 */
	if (((strcmp(argv[argi], p)) ? 0 : 1) ^ invers) {
		argi = argex;
		SET_CHILD_ARGS(argi);
		dopath_exec(argc-argi, argv+argi);
		return EXITCODE; /* do NOT change exitcode */
	}
	return 0;
}


static int cmd_shift(int argc, char **argv)
{
	CMD_LINKAGE2;
	struct batchdata *pd = bdta->parent;
	if (pd && (pd->argcsub > pd->argshift)) pd->argshift++;
	return 0;
}

static int cmd_cd(int argc, char **argv)
{
	int cwd = com_dosgetdrive();
	char *s = argv[1];
	int ret;

	/* cd. */
	if (argv [0][2] == '.')
		return 0;

	if (!s) {
		char buf[128];
		/* just display the current curectory */
		com_dosgetcurrentdir(0, buf);
		com_fprintf(2,"%c:\\%s\n", cwd + 'A', buf);
		return 0;
	}
	if (s[1] && s[1] == ':') {
		int d = toupper(s[0]) - 'A';
		com_dossetdrive(d);
		if (com_dosgetdrive () != d)
			return 0x0F; 	/* invalid drive */
		
		s += 2;
	}
	ret = com_dossetcurrentdir(s);
	com_dossetdrive(cwd);
	if (ret) {
		return DOS_ENOENT;
	}
	return 0;
}

static int cmd_type(int argc, char **argv)
{
	int fd, ret, err, bsize = 128;

	if (!argv[1]) {
		return DOS_EINVAL;
	}
	fd = com_dosopen(argv[1],0);
	if (fd <0) {
		return com_errno & 255;
	}
	do {
		ret = com_doscopy(1, fd, bsize, 1);
	} while (ret == bsize);
	err = com_errno;
	com_dosclose(fd);
	if (ret == -1) {
		return err & 255;
	}
	return 0;
}

static int cmd_del(int argc, char **argv)
{
	struct findfirstnext_dta *dta = (void *)CTCB;
	char s[256];
	int len, opt_p;

	static int delit(void)
	{
		if (dta->attrib & DOS_ATTR_PROTECTED) return 0;
		strcpy(s+len, dta->name);
		if (opt_p) {
			int c = kbdask("yN", "%s,    Delete", s);
			if (c < 0) {
				com_fprintf(2, "^C\n");
				return 1;
			}
			com_fprintf(2, "%c\n", c);
			if (c != 'y') return 0;
		}
		com_dosdeletefile(s);
		return 0;
	}

	opt_p = findoption_remove("/p", &argc, argv);

	if (!argv[1] || com_exist_dir(argv[1])) {
		return DOS_EINVAL;
	}
	if (!com_dosfindfirst(argv[1], DOS_ATTR_FILE)) {
		len = mkstemm(s, argv[1], 1);
		if (delit()) return 0;
		while (!com_dosfindnext()) if (delit()) return 0;
	}
	else
		return DOS_ENOENT;
	return 0;
}

struct tiny_dta {
	unsigned char	attrib;		/* 0x95 */
	unsigned short	filetime;	/* 0x96 */
	unsigned short	filedate;	/* 0x98 */
	unsigned long	filesize;	/* 0x9a */
	char		name[13];	/* 0x9e */
} __attribute__((packed));

static int qsort_dircmp(const void *e1, const void * e2)
{
	const struct tiny_dta *a = e1;
	const struct tiny_dta *b = e2;
	if ((a->attrib & DOS_ATTR_DIR) == (b->attrib & DOS_ATTR_DIR)) {
		return strcasecmp(a->name, b->name);
	}
	return (a->attrib & DOS_ATTR_DIR) ? -1 : 1;
}

static int cmd_dir(int argc, char **argv)
{
	struct findfirstnext_dta *dta = (void *)CTCB;
	struct tiny_dta *cache = 0;
	int maxfiles = 2000;
	int huge_enough = sizeof(struct tiny_dta) * maxfiles;
	int cachei = 0;
	char dirname[128];
	int dirnamelen;
	unsigned num_files=0, num_dirs=0, num_bytes=0;
	char *p, wildcard[256] = "*.*";
	int i, c;
	int opt_p, opt_w, opt_l, opt_b, opt_a;
	int incmask = DOS_ATTR_ALL | 0x80;
	int excmask = DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM;
	char commastring [100];

	static int expand_wild(char *b)
	{
		if (isalpha(b[0]) && (b[1]==':')) {
			char buf[256];
			if (com_dosgetcurrentdir(toupper(b[0]) -'A' +1, buf))
				return 0x0F;	/* invalid drive */
			if (!b[2]) {
			    if (buf[0]) sprintf(b+2, "\\%s\\*.*", buf);
			    else strcat(b, "\\*.*");
			    return 0;
			}
			if ((b[2]=='\\') && !b[3]) {
			    strcat(b, "*.*");
			    return 0;
			}
		}
		if (strpbrk(b, "?*") != 0) return 0;
		if (com_exist_dir(b)) {
			strcat(b, "\\*.*");
			return 0;
		}
		return com_exist_file(b) ? 0 : DOS_ENOENT;
	}

	static void cache_it(void)
	{
		if (cachei >= maxfiles) return; /* ... silently */
		if (!dta->attrib) dta->attrib = 0x80;
		if (excmask & dta->attrib) return;
		if (!(incmask & dta->attrib)) return;

		if (dta->attrib & DOS_ATTR_DIR) {
			/* directories do not have a size in DOS */
			num_dirs++;
		}
		else {
			num_files++;
			num_bytes += dta->filesize;
		}

		memcpy(cache+(cachei++), &dta->attrib, sizeof(struct tiny_dta));
	}

	static void sort_it(void)
	{
		if (!cachei) return;
		qsort(cache, cachei, sizeof(struct tiny_dta), qsort_dircmp);
	}

	/* returns num as a string, with commas separating groups of 3 digits */
	static char *get_comma_string (const unsigned num)
	{
		int i;
		char *s;
		int modres;
		
		char number [100];	/* "10^(100-1) bytes ought to be enough for anybody" :) */
		int number_len;
		
		sprintf (number, "%u", num);
		number_len = strlen (number);
		
		modres = number_len % 3;

		s = commastring;
		for (i = 0; i < number_len; i++)
		{
			if (i && i % 3 == modres) *s++ = ',';
			*s++ = number [i];
		}
		*s++ = '\0';

		return commastring;
	}

	static void displ_b(int i)
	{
		com_printf("%s\n", cache[i].name);
	}

	static void displ_w(int i)
	{
		struct tiny_dta *e = &cache[i];
		char b[32];
		char *f = e->name;
		if (e->attrib & DOS_ATTR_DIR) {
			sprintf(b, "[%s]", f);
			f = b;
		}
		if (i % 5 == 4) com_printf("%s\n", f);
		else com_printf("%-16s", f);
	}

	static void displ(int i)
	{
		struct tiny_dta *e = &cache[i];
		int d = e->filedate;
		int t = e->filetime;
		char *p = strchr(e->name, '.');
		if (e->name [0] != '.' && p) *p++ = 0;
		else p = "";

		/* <DIR> sizes are not displayed among other things */
		if (e->attrib & DOS_ATTR_DIR)
			com_printf("%-9s%-4s  <DIR>        ", e->name, p);
		else
			com_printf("%-9s%-3s%14s  ", e->name, p, get_comma_string (e->filesize));

		/* display date and time */
		com_printf ("%02d-%02d-%02d %2d:%02d%c\n",
					(d>>5) & 15,
					d & 31,
			(d>>9) < 20 ? (d>>9) + 80 : (d>>9) - 20,
					((t>>11) % 12) + (((t>>11) % 12) ? 0 : 12),
					(t>>5) & 63,
					(t>>11) >= 12 ? 'p' : 'a'
		);
	}

	static void displ_header(void)
	{
		char *volume = "no label", *p;
		char drive[] = "X:\\*.*";
		char *is = "has ";
		if (opt_b) return;
		drive[0] = dirname[0];
		if (!com_dosfindfirst(drive, DOS_ATTR_VOLID)) {
			volume = dta->name;
			p = strchr(volume, '.');
			if (p) strcpy(p, p+1);
			is = "is ";
		}
		com_printf("\n Volume in drive %c %s%s\n",
			drive[0], is, volume);
		com_printf(" Directory of %s\n\n", dirname);
	}

	static void displ_footer(void)
	{
		unsigned info[4];
		int ret;
		if (opt_b) return;
		ret = com_dosgetfreediskspace(toupper((dirname[0])-'A')+1, info);

		com_printf("%10d file(s) %14s bytes\n",
						num_files, get_comma_string (num_bytes));
		com_printf("%10d dir(s)  %14s bytes free",
						num_dirs, get_comma_string (info[0] * info[1] * info[2]));
	}

	static void display_it(void)
	{
		int i;
		void (*xx)(int);
		xx = displ;

		if (opt_w) xx = displ_w;
		else if (opt_b) xx = displ_b;

		displ_header();
		for (i=0; i<cachei; i++) {
			if (opt_l) strlower(cache[i].name);
			(*(xx))(i);
		}
		if (xx == displ_w && --i % 5 < 4)
			printf ("\n");
		displ_footer();
	}

	static void flush_it(void)
	{
		com_printf("\n");
	}

	opt_p = findoption_remove("/p", &argc, argv);
	opt_w = findoption_remove("/w", &argc, argv);
	opt_l = findoption_remove("/l", &argc, argv);
	opt_b = findoption_remove("/b", &argc, argv);
	opt_a = findoption_remove("/a", &argc, argv);

	if (opt_a) {
		incmask = DOS_ATTR_ALL + 0x80;
		excmask = 0;
	}
	else if ((i = findoption("/a:", argc, argv))!=0) {
		if (!argv[i][0]) argc = remove_arg(i, argc, argv);
		p = argv[i];
		if (!p) {
			return DOS_EINVAL;
		}
		opt_a = 1;
		i = 1;
		incmask = excmask = 0;
		while ((c = *p++) != 0) {
		    #define XXX(x) (i ? incmask : excmask) |= x; i = 1; break
		    switch (tolower(c)) {
			case '-': i = 0; break;
			case 'h': XXX(DOS_ATTR_HIDDEN);
			case 's': XXX(DOS_ATTR_SYSTEM);
			case 'd': XXX(DOS_ATTR_DIR);
			case 'a': // (i ? excmask : incmask) |= 0x80;
				  XXX(DOS_ATTR_ARCH);
			case 'r': XXX(DOS_ATTR_RDONLY);
		    }
		    #undef XXX
		}
		if (!incmask) incmask = DOS_ATTR_ALL +0x80;
		argc = remove_arg(i, argc, argv);
	}
	if (argc > 2) {
		return DOS_EINVAL;
	}
	cache = alloca(huge_enough);

	/* prepare for header */
	if (argc > 1) strcpy(wildcard, argv[1]);
	i = expand_wild(wildcard);
	if (i) return i;
	dirnamelen = mkstemm(dirname, wildcard, 1);
	/* strip trailing \ if not root */
	if (dirname [dirnamelen - 1] == '\\' && dirnamelen != 3)
		dirname [--dirnamelen] = '\0';

	if (!com_dosfindfirst(wildcard, incmask & 0x7f)) {
		cache_it();
		while (!com_dosfindnext()) cache_it();
	}
	if (!num_files && !num_dirs) return DOS_ENOENT;
	sort_it();
	display_it();
	flush_it();
	return 0;
}

static int rename_wild(char *buf, char *fname, char *w1, char *w2)
{
	char *b = buf;
	char *p = w2;
	char *s = fname;
	char *w;
	int i, j;

	static int next_wild(void)
	{
		int i;
		if (*s && *w1) {
			i = strcspn(w1, "?*"); /* jump over non-wild */
			s += i; w1 += i;
			i = strspn(w1, "?*"); /* number of wild characters */
			if (!i) return 0;
			if (*w1 == '?') {
				w = s++;
				w1 +=1;
				return 1;
			}
			w = s;
			w1 += i;
			if (*w1) {
				i = strcspn(w1, "?*"); /* jump over non-wild */
				while (*s && strncasecmp(s, w1, i)) s++;
				return s-w;
			}
			return strlen(w);
		}
		return 0;
	}

	while (*p) {
		i = strcspn(p, "?*");	/* head until wild */
		if (i) {
			memcpy(b, p, i);
			b += i; p += i;
		}
		i = strspn(p, "?*"); /* number of wild characters */
		if (i) {
			j = next_wild();
			if (j) memcpy(b, w, j);
			b += j;
			if (*p == '?') p += 1;
			else p += i;
		}
	}
	*b = 0;
	return b-buf;
}

static int cmd_ren(int argc, char **argv)
{
	struct findfirstnext_dta *dta = (void *)CTCB;
	static char wcard[] = "?*";
	char s1[256], s2[256];
	char *p1, *p2, *b1, *b2;
	int len1, len2, blen1, blen2;
	int ret;

	static int rename_it(void)
	{
		char b[256];
		int l = rename_wild(b, dta->name, b1, b2);
		if (!l) return DOS_EINVAL;
		strcpy(s1+len1, dta->name);
		memcpy(s2+len2, b, l+1);
		if (com_dosrenamefile(s1, s2)) return com_errno;
		return 0;
	}

	if (argc < 3) {
		return DOS_EINVAL;
	}

	len1 = mkstemm(s1, argv[1], 1);
	len2 = mkstemm(s2, argv[2], 1);

	if (strcmp(s1,s2)) {
		/* names not in the same directory or on different drives */
		return DOS_EINVAL;
	}

	b1 = basename_of(argv[1], &blen1);
	b2 = basename_of(argv[2], &blen2);
	p1 = strpbrk(b1, wcard);
	p2 = strpbrk(b2, wcard);
	if (!p1 && !p2) {
		/* simple case, only one file involved */
		if (!com_dosrenamefile(argv[1], argv[2])) return 0;
		return com_errno & 255;
	}

	/* renaming file group, check further */

	if (!p1 || !p2) {
		/* wrong case, both must be wilcards */
		return DOS_EINVAL;
	}

	/* OK, now to the hard part. We could do wildcard handling
	 * by simply calling INT21,AH=56 via INT21,AX-5D00 (service call)
	 * because then INT21,AH=56 will accept wildcards.
	 *
	 * However, this is pretty dangerous on non-MS DOSses and even
	 * under MSDOS not all works as expected on lredir'ed drives.
	 * So we do it 'the hard way' to be save.
	 */

	if (com_dosfindfirst(argv[1], DOS_ATTR_FILE)) return com_errno;
	if ((ret=rename_it())!=0) return ret;
	while (!com_dosfindnext()) if ((ret=rename_it())!=0) return ret;
	return 0;
}


static int cmd_copy(int argc, char **argv)
{
	CMD_LINKAGE2;
	struct findfirstnext_dta *dta = (void *)CTCB;
	struct src {
		unsigned istext:1;
		unsigned isappend:1;
		unsigned iswild:1;
		char name[128];
	};
	#define SRCMAX	32
	int srci = 0, targi = 0;
	struct src src[SRCMAX];
	int prompt_overwrite = 1;
	int refused_overwrite = 0;
	int same_file_ok = 0;
	int istext = -1;
	int truncate_to_text = 0;
	int append = 0, numappends = 0, numwilds = 0;
	int i, fcount = 0;
	int targ_is_wild, targ_is_dir, target_given = 0;
	int targlen, targdlen;
	char targd[256], targ[128];
	char buf[256], *s;

	static int appendbslash(char *b)
	{
		int l = strlen(b);
		if (!l || (b[l-1] != '\\')) {
			b[l++] = '\\';
			b[l] = 0;
		}
		return l;
	}

	static int exist_dir(char *b)
	{
		if (isalpha(b[0]) && (b[1]==':') && (b[2]=='\\') && !b[3]) {
			char dummy[256];
			return com_dosgetcurrentdir(toupper(b[0])-'A'+1, dummy) == 0;
		}
		return com_exist_dir(b);
	}

	static int expand_drive(char *b)
	{
		if (isalpha(b[0]) && (b[1]==':') && !b[2]) {
			if (com_getdriveandpath(toupper(b[0]) -'A' +1, b))
				return -1;
			return 1;
		}
		return 0;
	}

	static int set_target_to_CWD(void)
	{
		if (com_getdriveandpath(0, targd)) return com_errno;
		targdlen = appendbslash(targd);
		targ_is_dir = 1;
		targ_is_wild = 0;
		targ[0] = 0;
		targlen = 0;
		targi = 0;
		target_given = 0;
		return 0;
	}

	static int print_summary(int retcode)
	{
		if (!fcount) return retcode;
		com_fprintf(2,
		"% 9d file(s) copied\n", fcount);
		return retcode;
	}

	static void touch_all(void)
	{
		int i, dlen, fd;
		char dir[256];
		unsigned t = com_dosgettime();
		unsigned d = com_dosgetdate();
		d = (((d >> 16) - 1980) <<9)
			| ((d >> (8-5)) & 0x1e0) | (d & 31);
		t = ((t >> (24-11)) & 0xf800)
			| ((t >> (16-5)) & 0x7e0) | ((t >> 8) & 0x1f);
		t = (d << 16) | t;
		for (i=0; i < srci; i++) {
			dlen = mkstemm(dir, src[i].name, 1);
			if (com_dosfindfirst(src[i].name, DOS_ATTR_FILE)) continue;
			strcpy(dir+dlen, dta->name);
			fd = com_dosopen(dir, 0);
			com_dosgetsetfilestamp(fd, t);
			com_dosclose(fd);
			while (!com_dosfindnext()) {
				strcpy(dir+dlen, dta->name);
				fd = com_dosopen(dir, 0);
				com_dosgetsetfilestamp(fd, t);
				com_dosclose(fd);
			}
		}
	}

	static int overwrite_check(char *name)
	{
		/* NOTE: destroyes DTA */
		int c;
		refused_overwrite = 0;
		if (!prompt_overwrite) return 0;
		if (com_exist_file(name)) {
			c = kbdask("yNa", "file %s exists, overwrite", name);
			if (c < 0) {
				 com_fprintf(2, "^C\n");
				 return -1;
			}
			com_fprintf(2, "%c\n", c);
			switch(c) {
				case 'a': return prompt_overwrite = 0;
				case 'y': return 0;
				default: return refused_overwrite = 1;
			}
		}
		return 0;
	}

	static int same_file(char *a, char *b)
	{
		int ret;
		char n1[256], n2[256];
		com_doscanonicalize(n1, a);
		com_doscanonicalize(n2, b);
		ret = strcmp(n1, n2) == 0;
		if (ret && !same_file_ok)
			com_fprintf(2, "attempt to copy %s to itself\n", a);
		return ret;
	}

	static int single_copy(char *a, char *b, int textmode)
	{
		anyDTA saved_dta = PSP_DTA;
		int fdi, fdo;
		int ret, err, bsize = 128;

		truncate_to_text = 0;
		if (!same_file_ok && same_file(a, b))
				return com_errno = DOS_EINVAL;
		switch (overwrite_check(targd)) {
			case -1: com_errno = 0; return -1; /* ^C */
			case  1: com_errno = 0;
				PSP_DTA = saved_dta;
				return 0;  /* skipped */
		}
		if (same_file_ok) {
			if (same_file(a, b)) {
				char *p = basename_of(b,0);
				com_fprintf(2, "%s\n", p);
				fcount++;
				same_file_ok = 0;
				truncate_to_text = textmode;
				PSP_DTA = saved_dta;
				return com_errno = 0;
			}
			same_file_ok = 0;
		}

		fdi = com_dosopen(b, 0);
		if (fdi <0) {
			return com_errno;
		}
		fdo = com_dosopen(a, DOS_ACCESS_CREATE);
		if (fdo <0) {
			err = com_errno;
			com_dosclose(fdi);
			return com_errno = err;
		}
		do {
			ret = com_doscopy(fdo, fdi, bsize, textmode);
		} while (ret >0);
		err = com_errno;
		if (ret != -1) com_dosgetsetfilestamp(fdo,
					com_dosgetsetfilestamp(fdi,0));
		com_dosclose(fdi);
		com_dosclose(fdo);
		PSP_DTA = saved_dta;
		if (!ret) {
			char *p = basename_of(b,0);
			com_fprintf(2, "%s\n", p);
			fcount++;
		}
		if (ret == -1) return com_errno = err;
		return com_errno = 0;
	}


	static int single_append(char *t, char *s, int textmode)
	{
		anyDTA saved_dta = PSP_DTA;
		int ret, err, fdo, fdi;
		fdo = com_dosopen(t, 2);
		if (fdo < 0) return -1;
		if (truncate_to_text) {
			com_seektotextend(fdo);
			truncate_to_text = 0;
		}
		else com_dosseek(fdo, 0, 2);
		fdi = com_dosopen(s, 0);
		if (fdi < 0) {
			err = com_errno;
			com_dosclose(fdo);
			com_errno = err;
			return -1;
		}
		while ((ret=com_doscopy(fdo, fdi, 128, textmode))>0);
		err = com_errno;
		com_dosclose(fdi);
		com_dosclose(fdo);
		PSP_DTA = saved_dta;
		if (!ret) {
			char *p = basename_of(s,0);
			com_fprintf(2, "%s\n", p);
		}
		com_errno = err;
		return ret;
	}

	static int copy_wild(int matched)
	{
		char *w1, *w2;
		char s[256];
		int slen;
		int (*docopy)(void);
		int (*initialcombine)(void);

		static int matchcopy(void)
		{
			char b[256];
			int l = rename_wild(b, dta->name, w1, w2);
			if (!l) return com_errno = DOS_EINVAL;
			strcpy(s+slen, dta->name);
			memcpy(targd+targdlen, b, l+1);
			return single_copy(targd, s, src[0].istext);
		}

		static int dircopy(void)
		{
			strcpy(s+slen, dta->name);
			strcpy(targd+targdlen, dta->name);
			return single_copy(targd, s, src[0].istext);
		}


		static int combine(void)
		{
			int i, l;
			char s2[256], b[256];
			char *w2;
			int s2len;

			same_file_ok = 1;
			if ((*initialcombine)()) return com_errno;
			if (refused_overwrite) return 0;
			for (i=1; i< srci; i++) {
				s2len = mkstemm(s2, src[i].name, 1);
				w2 = basename_of(src[i].name,0);
				l = rename_wild(b, dta->name, w1, w2);
				memcpy(s2+s2len, b, l+1);
				if (single_append(targd, s2, src[i].istext)) return com_errno;
			}
			return 0;
		}

		switch (matched) {
			default: docopy = dircopy; break;
			case 1: docopy = matchcopy; break;
			case 2: docopy = combine;
				initialcombine = dircopy;
				break;
			case 3: docopy = combine;
				initialcombine = matchcopy;
				break;
		}

		w1 = basename_of(src[0].name, 0);
		slen = mkstemm(s, src[0].name, 1);
		w2 = targ;

		if (com_dosfindfirst(src[0].name, DOS_ATTR_FILE)) return com_errno;
		if ((*docopy)()) return com_errno;
		while (!com_dosfindnext()) {
			if ((*docopy)()) return com_errno;
		}
		return com_errno = 0;
	}

	static int combine_to_single(void)
	{
		int i;
		int slen;
		char s[256];

		for (i=0; i < srci; i++) {
			slen = mkstemm(s, src[i].name, 1);
			if (com_dosfindfirst(src[i].name, DOS_ATTR_FILE)) continue;
			strcpy(s+slen, dta->name);
			if (!targd[targdlen]) {
				memcpy(targd+targdlen, targ, targlen+1);
				same_file_ok = 1;
				if (single_copy(targd, s, src[0].istext))
						return com_errno;
				if (refused_overwrite) return 0;
			}
			else if (single_append(targd, s, src[i].istext))
						return com_errno;
			while (!com_dosfindnext()) {
				strcpy(s+slen, dta->name);
				if (single_append(targd, s, src[i].istext))
						return com_errno;
			}
		}
		return com_errno = 0;
	}


	/* first set prompt_overwrite and the defaults from COPYCMD env */
	prompt_overwrite = bdta->mode == 0;
	s = com_getenv("COPYCMD");
	if (s) {
		s = strpbrk(s, "yY");
		if (s) prompt_overwrite = s[-1] == '-';
	}

	/* The MSDOS copy command's commandline syntax and functionality
	 * is too sick to fit into any otherwise used standards,
	 * we have to handle it specially. This syntax is:
	 *
  COPY [/Y|/-Y] [/A|/B] src [/A|/B] [+src[/A|/B] [+ ...]][target [/A|/B]] [/V]
	 *
	 * with the additional complication, that trailing /A|/B mean the
	 * 'src' mentioned _before_ /A|/B, but nevertheless setting the default
	 * for all _following_ +src up to the next /A|/B.
	 */

	/* first restore a single line out of argv[] */
	concat_args(buf, 1, argc-1, argv);

	s = buf;
	while (*s) {
	    switch (*s++) {
		case '/': {
		    switch (tolower(*s)) {
			case '-':
				if (tolower(s[1]) != 'y') return DOS_EINVAL;
				prompt_overwrite = 1;
				s++;
				break;
			case 'y': prompt_overwrite = 0; break;
			case 'v': /* ignored, not applicable and needed
				   * on Linux filesystems
				   */
				break;
			case 'a':
				istext = 1;
				if (srci) src[srci-1].istext = 1;
				break;
			case 'b':
				istext = 0;
				if (srci) src[srci-1].istext = 0;
				break;
			default:
				return DOS_EINVAL;
		    }
		    s++;
		    break;
		}
		case ' ': case '\t':
		    s += strspn(s, " \t");
		    break;
		case '+':
		    if (!srci) return DOS_EINVAL;
		    if (istext<0) {
			istext = 1;
			src[srci-1].istext = 1;
		    }
		    append = 1;
		    numappends++;
		    break;
		default: {
		    src[srci].name[0] = s[-1];
		    i = strcspn(s, " \t+/");
		    memcpy(src[srci].name, s-1, i+1);
		    src[srci].name[i+1] = 0;
		    s += i;
		    src[srci].istext = (istext <0) ? 0 : istext;
		    src[srci].isappend = append;
		    src[srci].iswild = strpbrk(src[srci].name, "?*") != 0;
		    if (src[srci].iswild) numwilds++;
		    if (srci>1 && !src[srci-1].isappend) return DOS_EINVAL;
		    if (srci>0 && !append) target_given = 1;
		    append = 0;
		    srci += 1;
		    if (srci >= SRCMAX) return DOS_EINVAL;
		    break;
		}
	    }
	}


	/* what's the target? */

	if (!srci) return DOS_EINVAL;
	if (srci == 1) {
		/* no target given, copying to default drive/path*/
		if (set_target_to_CWD()) return com_errno;
	}
	else {
		srci -= 1;
		targi = srci;
		if (!strcmp(src[targi].name, ",,")) {
		   /*
		    * if there is _no_ target atall (targetname ",,"),
		    * _and_ this (phantom) has to be combined ("+,,"),
		    * just 'touch' the source file with the current date/time.
		    * (what the hell did the M$ programmer have in mind, when
		    * he 'invented' this stupid 'touch' method?)
		    */
			target_given = 0;
			if (!src[targi].isappend) return DOS_EINVAL;
			/* the M$ 'touch' case ;-) */
			touch_all();
			return 0;
		}
		if (src[targi].isappend) {
			/* there was no target given, the first file
			 * is the target all others shall be appended to */
			target_given = 0;
			targi = 0;
			srci++;
		}
		strcpy(targd, src[targi].name);
		if (expand_drive(targd) < 0) return com_errno;
		/* try to canonicalize, if we cannot, the target
		 * is not accessable
		 */
		if (com_doscanonicalize(targd, targd)) return com_errno;
		targ_is_wild = src[targi].iswild;
		targ_is_dir = !targ_is_wild && exist_dir(targd);
		if (targ_is_dir) {
			targdlen = appendbslash(targd);
			targlen = 0;
			targ[0] = 0;
		}
		else {
			char *p = basename_of(src[targi].name, &targlen);
			/* take the orig wildcard, because canonicalize
			 * did expand '*' to '???' */
			memcpy(targ, p, targlen+1);
			/* check existence os the stemm */
			p = basename_of(targd,0);
			if (p[-2] != ':') p--;
			*p = 0;
			if (!exist_dir(targd)) return com_errno;
			targdlen = appendbslash(targd);
		}
	}


	if (!numwilds && (srci==1) && target_given) {
		/* simple case: one file to copy */
		char *p;
		if (!targ_is_dir) memcpy(targd+targdlen, targ, targlen+1);
		else {
			p = basename_of(src[0].name, &i);
			memcpy(targd+targdlen, p, i+1);
		}
		single_copy(targd, src[0].name, src[0].istext);
		return print_summary(com_errno);
	}

	if (srci==1 && targ_is_dir && !targ_is_wild) {
		/* yet a simple case:
		 * copy a bunch of files in to one directory */
		return print_summary(copy_wild(0));
	}

	if (srci==1 && src[0].iswild && target_given && targ_is_wild) {
		/* a bit complex case:
		 * copy a bunch of files while matching wildcards */
		return print_summary(copy_wild(1));
	}

	if (numappends && (targ_is_dir || !target_given) && (numwilds == srci)) {
		/* complex combine case:
		 * combine groups while matching wilds into directory
		 */
		if (!target_given) {
			if (set_target_to_CWD()) return com_errno;
		}
		return print_summary(copy_wild(2));
	}

	if (numappends && target_given && !targ_is_dir && (numwilds == srci+1)) {
		/* more complex combine case:
		 * combine groups while matching wilds of sources _and_ target
		 */
		return print_summary(copy_wild(3));
	}

	if ((numappends || target_given) && !targ_is_dir && (!targi || !targ_is_wild)) {
		/* simple combine case:
		 * all files go into one single file
		 */
		if (!numappends && istext<0) src[0].istext = 1;
		i = combine_to_single();
		if (fcount) {
			fcount = 1;
			print_summary(i);
		}
		return i;
	}

	/* Ok, what falls through here isn't handled by us */
	return com_errno = DOS_EINVAL;
}

static int cmd_set(int argc, char **argv)
{
	char *env = SEG2LINEAR(CTCB->envir_frame);
	char *p;
	char buf[256];

	if (!argv[1]) {
		while (*env) {
			com_printf("%s\n", env);
			env += strlen(env) + 1;
		}
		return 0;
	}

	/* Well, I _hate_ to duplicate stupidness, but to be 'compatible'
	 * we here just are forced to :-(
	 *
	 * MSDOS command.com does the following stupid things when putting
	 * variables onto the environment:
	 *
	 *    set aa bb cc = dd ee ff
	 *
	 * creates a variable "AA BB CC " with the content " dd ee ff"
	 * NOTE: with all blanks included in the name _and_ the content.
         * And of course the variable "AA BB CC " is different from "AA BB CC"
	 *
         * ... no further comment, this speaks for itself :-(
	 */

	concat_args(buf, 1, argc-1, argv);
	p = strchr(buf, '=');
	if (!p) {
		return DOS_EINVAL;
	}
	*p = 0;
	if (!com_msetenv(buf, p+1, _psp)) return 0;
	return DOS_ENOMEM;
}

static int cmd_path(int argc, char **argv)
{
	char *p=0;

	if (!argv[1]) {
		p = com_getenv("PATH");
		com_printf("%s\n", p ? p : "PATH not set");
		return 0;
	}
	if (!com_msetenv("PATH", argv[1], _psp)) return 0;
	return DOS_ENOMEM;
}

static int cmd_prompt(int argc, char **argv)
{
	char *p = argv[1];

	if (!argv[1]) p = "";
	if (!com_msetenv("PROMPT", argv[1], _psp)) return 0;
	return DOS_ENOMEM;
}

static int cmd_break(int argc, char **argv)
{
	if (!argv[1]) {
		LWORD(eax) = 0x3300;
		call_msdos();
		com_printf("BREAK is switched %s\n",
			LO(dx) ? "ON" : "OFF");
		return 0;
	}
	LWORD(eax) = 0x3301;
	LO(dx) = strcasecmp(argv[1], "on") ? 0 : 1;
	call_msdos();
	return 0;
}

static int cmd_verify(int argc, char **argv)
{
	if (!argv[1]) {
		HI(ax) = 0x54;
		call_msdos();
		com_printf("VERIFY is switched %s\n",
			LO(ax) ? "ON" : "OFF");
		return 0;
	}
	HI(ax) = 0x2e;
	LO(ax) = strcasecmp(argv[1], "on") ? 1 : 0;
	call_msdos();
	return 0;
}

static int cmd_mkdir(int argc, char **argv)
{
	char *s = argv[1];
	int ret;

	if (!s) {
		return DOS_ENOENT;
	}
	ret = com_dosmkrmdir(s, 0);
	if (ret) ret = com_errno & 255;
	return ret;
}

static int cmd_rmdir(int argc, char **argv)
{
	char *s = argv[1];
	int ret;

	if (!s) {
		return DOS_ENOENT;
	}
	ret = com_dosmkrmdir(s, 1);
	if (ret) ret = com_errno & 255;
	return ret;
}

static int cmd_date(int argc, char **argv)
{
	static char *dtable[] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Oooch?"
	};
	char buf[128];
	int ret, option_q = 0;

	static int changedate(char *s)
	{
		int month, day, year;
		char *p = s;
		if (!s[0]) {
			int len = com_dosread(0, s, 128);
			if (len <= 2) return 0;
			s[len-2] = 0;
		}
		if (!s[0]) return 0;
		p = strchr(s, '-');
		if (!p) return -1;
		*p = 0;
		month = strtoul(s, 0, 10);
		s = p+1; p = strchr(s, '-');
		if (!p) return -1;
		*p = 0;
		day = strtoul(s, 0, 10);
		p++;
		if (!p) return -1;
		year = strtoul(p, 0, 10);
		if (year >36 && year <100) year += 1900;
		else if (year <36) year += 2000;
		return com_dossetdate((year <<16) | (month<<8) | day);
	}

	option_q = strcasecmp(argv[1], "/q") == 0;

	if (argc == 1 || option_q) {
		int date = com_dosgetdate(); /* 0x0YYYWMDD */
		com_printf(
		  "Current date is %s %02d-%02d-%d\n",
		  dtable[ (date>>12) & 7],
		  (date>>8) & 15, date & 255, date >> 16
		);
		if (option_q) return 0;
		com_printf("Enter date (mm-dd-yy): ");
		buf[0] = 0;
		ret = changedate(buf);
		if (!ret) return 0;
		return DOS_EINVAL;
	}

	if (argc == 2) {
		ret = changedate(argv[1]);
		if (!ret) return 0;
	}
	return DOS_EINVAL;
}

static int cmd_time(int argc, char **argv)
{
	char buf[128];
	int option_q = 0;

	static int entertime(char *s)
	{
		int hour, min, sec;
		char *p;
		if (!s[0]) {
			int len = com_dosread(0, s, 128);
			if (len <= 2) return 0;
			s[len-2] = 0;
		}
		if (!s[0]) return 0;
		hour = strtoul(s, &p, 10);
		if (s != p) {
		  s = p+1;
		  min = strtoul(s, &p, 10);
		  if (s != p) {
		    s = p+1;
		    sec = strtoul(s, 0, 10);
		    if (!com_dossettime(
				(hour << 24) | (min << 16) | (sec << 8) ))
			return 0;
		  }
		}
		com_printf("invalid time\nEnter new time: ");
		return -1;
	}

	option_q = strcasecmp(argv[1], "/q") == 0;

	if (argc == 1 || option_q) {
		int time = com_dosgettime(); /* format: 0xHHMMSSss */
		com_printf(
		  "Current time is %2d:%02d:%02d.%02d%c\n",
		  ((time>>24) % 12) + (((time>>24) % 12) ? 0 : 12),
		  (time>>16) & 255,
		  (time>>8) & 255,
		  time & 255,
		(time>>24) >= 12 ? 'p' : 'a'
		);
		if (option_q) return 0;
		com_printf("Enter new time: ");
		buf[0] = 0;
		while (entertime(buf)) buf[0] = 0;
		return 0;
	}

	if (argc == 2) {
		strcpy(buf, argv[1]);
		while (entertime(buf)) buf[0] = 0;
	}
	return 0;
}


static float comcomversion =
#include "VERSION"
;

static int cmd_ver(int argc, char **argv)
{
	CMD_LINKAGE;
	int minor, major, rel, relflag, isfd=0;
	int revminor, revmajor, revseq, oemid;
	char *vdate = 0;

	HI(ax) = 0x30;
	call_msdos();
	minor = HI(ax);
	major = LO(ax);
	revminor = LO(cx);
	revmajor = HI(cx);
	revseq = LO(bx);
	oemid = HI(bx);

	clear_CF();
	LWORD(eax) = 0x3306;
	call_msdos();
	if (!(LWORD(eflags) & CF) && (LO(ax) != 0xFF)) {
		minor = HI(bx);
		major = LO(bx);
		rel = LO(dx);
		relflag = HI(dx);

		/* try wether its the freedos kernel */
		LWORD(eax) = 0x33ff;
		call_msdos();
		if (!(LWORD(eflags) & CF) && (LO(ax) != 0xFF)) {
			isfd = 1;
			vdate = strchr(MK_FP32(LWORD(edx),LWORD(eax)), '[');
		}
	}

	com_printf("\n"
		"DOSEMU built-in command.com version %#4.2g\n"
		"DOSEMU version is  %s, %s",
		comcomversion, VERSTR, VERDATE
	);
	if (isfd) {
		com_printf("\nFreeDOS kernel version %d.%d.%d %s",
			revmajor, revminor, revseq, vdate ? vdate : "");
	}
	else com_printf("\n\n");
	com_printf(
		"XX-DOS version reported is %d.%d\n\n",
		_osmajor, _osminor
	);
	return EXITCODE;	/* ... don't change */
}

static int cmd_which(int argc, char **argv)
{
	CMD_LINKAGE;
	char buf[256];

	if (scan_path_env(buf, argv+1)) {
		com_printf("%s\n", buf);
	}
	return EXITCODE;	/* ... don't change */
}

static int cmd_cls(int argc, char **argv)
{
	int cols, page;
	int rows = READ_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1);

	cols = com_biosvideo(0x0f00) >> 8;
	page = HI(bx);
	LWORD(ebx) = com_biosvideo(0x800);	/* get attribute */

	LWORD(ecx) = 0;		/* upper left corner */
	LO(dx) = cols-1;	/* lower right corner, x */
	HI(dx) = rows;		/* lower right corner, y */
	com_biosvideo(0x600);	/* scroll up window, entire screen */

	LWORD(edx) = 0;
	HI(bx) = page;
	com_biosvideo(0x200);	/* set cursor to 0:0 */
	return 0;
}

static int cmd_truename(int argc, char **argv)
{
	char buf [256];

	/* too many parameters.. */
	if (argc > 2) {
		com_fprintf (2, "Too many parameters - %s\n", argv[2]);
		return 0;
	}

	/* this is what the real command does :) */
	if (!strcmp (argv [1], "/?")) {
		com_printf ("Reserved command name\n");
		return 0;
	}

	com_doscanonicalize (buf, argc == 1 ? "" : argv [1]);
	com_printf ("\n%s\n", buf);

	return 0;
}



struct cmdlist {
	char *name;
	com_program_type *cmd;
	int flags;
};

struct cmdlist intcmdlist[] = {
	{"cd",		cmd_cd, 0},
	{"cd.",		cmd_cd, 0},
	{"chdir",	cmd_cd, 0},
	{"goto",	cmd_goto, 0},
	{"echo",	cmd_echo, 0},
	{"echo.",	cmd_echo, 0},
	{"pause",	cmd_pause, 0},
	{"rem",		cmd_rem, 0},
	{"if",		cmd_if, 0},
	{"for",		cmd_for, 0},
	{"shift",	cmd_shift, 0},
/*	{"call",	call_cmd, 0},    handled internally in dopath_exec() */
	{"type",	cmd_type, 0},
	{"del",		cmd_del, 0},
	{"erase",	cmd_del, 0},
	{"set",		cmd_set, 0},
	{"path",	cmd_path, 0},
	{"prompt",	cmd_prompt, 0},
	{"break",	cmd_break, 0},
	{"mkdir",	cmd_mkdir, 0},
	{"md",		cmd_mkdir, 0},
	{"rmdir",	cmd_rmdir, 0},
	{"rd",		cmd_rmdir, 0},
	{"verify",	cmd_verify, 0},
	{"ver",		cmd_ver, 1},
	{"ren",		cmd_ren, 0},
	{"date",	cmd_date, 0},
	{"time",	cmd_time, 0},
	{"dir",		cmd_dir, 0},
	{"copy",	cmd_copy, 0},
	{"which",	cmd_which, 0},	/* strictly speaking, this is not a DOS command */
	{"cls",		cmd_cls, 0},
	{"truename",	cmd_truename, 0},

	/* DOSEMU special support commands */

	{"exitemu",	exitemu_main, 1},
	{"speed",	speed_main, 1},
	{"bootoff",	bootoff_main, 1},
	{"booton",	booton_main, 1},
	{"ecpuoff",	ecpuoff_main, 1},
	{"ecpuon",	ecpuon_main, 1},
	{"eject",	eject_main, 1},
	{"emumouse",	emumouse_main, 1},
	{"vgaoff",	vgaoff_main, 1},
	{"vgaon",	vgaon_main, 1},

#ifdef USE_HEAP_EATING_BUILTINS
	{"ugetcwd",	ugetcwd_main, 1},
	{"lredir",	lredir_main, 1},
	{"unix",	unix_main, 1},
	{"dosdbg",	dosdbg_main, 1},
	{"xmode",	xmode_main, 1},
	{"system",	system_main, 1},
	{"uchdir",	uchdir_main, 1},
#endif
	{0,0}
};

static int do_internal_command(int argc, char **argv)
{
	CMD_LINKAGE;
	struct cmdlist *cmd = intcmdlist;
	char *arg0 = argv[0];
	int ret;
	if (isalpha(arg0[0]) && arg0[1] && !arg0[2] && arg0[1] == ':') {
		int d = toupper(arg0[0]) - 'A';
		com_dossetdrive(d);
		if (com_dosgetdrive () != d) {
#if 1
			/* messages like 'm:: invalid drive' look rather strange so we fake a message instead :) */
			com_fprintf(2, "Invalid drive specification\n");
			return 0;
#else
			setexitcode (0x0F, -2);	/* invalid drive */
			return -1;
#endif
		}
		else
			return 0;
	}
	for (; cmd->name; cmd++) {
		if (strcasecmp(arg0,cmd->name)) continue;
		if (rdta->cannotexit) {
			struct cmdlist *cmd_ = cmd;
			rdta->fatal_handler_return_regs = REGS;
	    		if (!setjmp(rdta->fatal_handler_return)) {
				rdta->fatal_handler_return_possible = 1;
				ret = (*(cmd_->cmd))(argc, argv);
				rdta->fatal_handler_return_possible = 0;
				if (cmd_->flags & 1) setexitcode(ret, -1);
				else setexitcode(ret, -2);
				if (!ret) ret = -1;
				return ret;
			}
			else {
				setexitcode(DOS_EGENERAL, -2);
				return -1;
			}
		}
		else {
			ret = (*(cmd->cmd))(argc, argv);
			if (cmd->flags & 1) setexitcode(ret, -1);
			else setexitcode(ret, -2);
			if (!ret) ret = -1;
			return ret;
		}
	};
	return -2;	/* means: not an internal command */
}

/* ---------------- <<< end internal command --------------- */

static int launch_child_program(char *name, char *cmdline)
{
	CMD_LINKAGE;
	struct com_starter_seg  *ctcb = CTCB;
	char *env = SEG2LINEAR(ctcb->envir_frame);
	char *p;
	int ret, shrinked_size, saved_cannotexit;

	/* first try to shrink the environment */
	p = env;
	while (*p) {
		p += strlen(p) + 1;
	}
	shrinked_size = ((p - env) + 31) & ~16;
	if (shrinked_size <  rdta->envsize) shrinked_size = rdta->envsize;
	com_dosreallocmem(ctcb->envir_frame, shrinked_size);

	/* launch the program */
	saved_cannotexit = rdta->cannotexit;
	rdta->cannotexit = 0;
	ret =  load_and_run_DOS_program(name, cmdline);
	rdta->errorlevel = ret;
	rdta->cannotexit = saved_cannotexit;

	/* re-enlarge the environment to maximum */
	com_dosreallocmem(ctcb->envir_frame, 0x8000);
	if (ret == -1) setexitcode(com_errno & 255, -1);
	else setexitcode(ret, -1);
	return ret;
}

static int dopath_exec(int argc, char **argv)
{
	CMD_LINKAGE2;
	char cmdline[128];
	char name[128];
	char tmpname[128] = "";
	int i;
	struct redir_stdxx redir_stdout;
	struct redir_stdxx redir_stdin;
	int redir_out = 0, redir_out_append = 0;
	int redir_in = 0;
	int ret = 0;

	if (!argv[0]) {
		setexitcode(DOS_EINVAL, -1);
		return -1;
	}
	init_redir_struct(0, &redir_stdin);
	init_redir_struct(1, &redir_stdout);


	/* first check for pipes and split the command execution
	 * into two parts, redirecting appropriate.
	 */

	for (i=1; i <argc; i++) {
	    if (argv[i][0] == '|') {
		/* ok, we have to split */
		char *saved_arg;
		int argc_end;
		int tmpfd = create_temp_file(tmpname);

		if (tmpfd == -1) {
			tmpname[0] = 0;
			break; /* ... giving up */
		}
		com_dosclose(tmpfd);	/* redirector will open it again */
		argc_end = i;
		if (!argv[i][1]) i++;
		else argv[i]++;

		/* we now redirect stdout to the tempfile */
		com_redirect_stdxx(tmpname, &redir_stdout, 0);

		/* we call us recursively on the head of the arglist... */
		saved_arg = argv[argc_end];
		argv[argc_end] = 0;
		dopath_exec(argc_end, argv);
		argv[argc_end] = saved_arg;

		/* now we are back, we close the redirection
		 * and set the same file up as stdin redirection
		 */
		com_restore_redirect(&redir_stdout);
		argv = argv+i;
		argc -= i;

		/* ... and continue with the tail */
		com_redirect_stdxx(tmpname, &redir_stdin, 0);
		ret = dopath_exec(argc, argv);
		com_restore_redirect(&redir_stdin);
		if (tmpname[0]) {
			com_dosdeletefile(tmpname);
			tmpname[0] = 0;
		}
		return ret;
	    }
	}

	/* check for explicit redirections and remove them from argv */
	for (i=1; i <argc; i++) {
	    switch (argv[i][0]) {
		case '>':
			if (argv [i][1] == '>')
				redir_out_append = 1;
			argv [i] += (redir_out_append + 1);
			
			if (!argv[i][0]) argc = remove_arg(i, argc, argv);
			if (redir_out || !argv[i]) {
				redir_out_append = 0;
				redir_out = 0;
				break;
			}
			if (!redir_out) redir_out =
				com_redirect_stdxx(argv[i], &redir_stdout, redir_out_append);
			argc = remove_arg(i, argc, argv);
			redir_out_append = 0;
			break;
		case '<':
			argv[i]++;	/* skip past '<' */
			if (!argv[i][0]) argc = remove_arg(i, argc, argv);
			if (!redir_in) redir_in =
				com_redirect_stdxx(argv[i], &redir_stdin, 0);
			break;
	    }
	}	

	ret = do_internal_command(argc, argv);
	if (ret == -2) {
	
		/* for the currently running app name (if any) */
		char saved_title_appname [TITLE_APPNAME_MAXLEN];

		/* shows currently running program in X window title */
		if (Video->change_config)
		{
			int i;
			
			/* for the new app that will be run */
			char appname [TITLE_APPNAME_MAXLEN];
			int appname_len;
			
			/* save name of running app */
			Video->change_config (GET_TITLE_APPNAME, saved_title_appname);
			
			/* get short name of running app */
			if (!strcasecmp (argv [0], "call"))
				snprintf (appname, TITLE_APPNAME_MAXLEN, "%s", basename_of (argv [1], &appname_len));
			else
				snprintf (appname, TITLE_APPNAME_MAXLEN, "%s", basename_of (argv [0], &appname_len));
			
			/* remove extension from name of running app */
			for (i = 0; i < TITLE_APPNAME_MAXLEN; i++)
			{
				/* 
				 * searching for a '.' from the start (instead of the end) of a filename
				 * is perfectly safe since DOS only allows one dot in a filename
				 */
				if (appname [i] == '.')
				{
					appname [i] = 0;
					break;
				}
			}
			
			Video->change_config (CHG_TITLE_APPNAME, strupr (appname));
		 }

	switch (scan_path_env(name, argv)) {
	    case 3:
		ret = command_inter_preter_loop(1, name, argc, argv);
		break;
	    case 11:
		SET_CHILD_ARGS(1);
		ret = command_inter_preter_loop(2, name, argc-1, argv+1);
		break;
#if 0
	    case 0:
		ret = do_internal_command(argc, argv);
		if (ret != -2) break;
		/* else fallthrough */
#endif
	    default: {
		cmdline[0] = 0;
		if (argv[1]) {
			concat_args(cmdline, 1, argc-1, argv);
		}
		ret =  launch_child_program(name, cmdline);
		break;
	    }
	 }
	 
	 	if (Video->change_config)
			/* revert to the previous app name */
			Video->change_config (CHG_TITLE_APPNAME, saved_title_appname);
	}
	if ((ret == -1 && rdta->need_errprinting == 1)
		|| (EXITCODE && rdta->need_errprinting == 2)) {
		if (EXITCODE) com_fprintf(2, "\n%s%s%s\n",
			argv[0] ? argv[0] : "", argv[0] ? ": " : "",
			decode_DOS_error(EXITCODE)
		);
		rdta->need_errprinting = 0;
	}

	com_restore_redirect(&redir_stdin);
	com_restore_redirect(&redir_stdout);

	if (ECHO_ON) com_fprintf (2, "\n");
	return ret;
}

static void print_prompt(int fd)
{
	char *fmt = com_getenv("PROMPT");
	char prompt[256] = "";
	char *p = prompt;
	char buf[128];
	int c;
	int drive = com_dosgetdrive();
	if (!fmt) fmt = "$n$g ";
	while ((c = *fmt++) != 0) {
	    switch(c) {
		case '$': {
		    c = *fmt++;
		    switch(tolower(c)) {
			case 0:
			    fmt--;
			    break;
			case 'p':
			    com_dosgetcurrentdir(0, buf);
			    p += sprintf(p, "%c:\\%s", drive +'A', buf);
			    break;
			case 'n':
			    *p++ = drive +'A';
			    break;
			case 'g':
			    *p++ = '>';
			    break;
			case 'l':
			    *p++ = '<';
			    break;
			case 'b':
			    *p++ = '|';
			    break;
			case 'e':
			    *p++ = '=';
			    break;
			default:
			    *p++ = c;
			    break;
		    }
		    break;
		}
		default:
		    *p++ = c;
		    break;
	    }
	}
	*p = 0;
	com_doswrite(fd, prompt, p-prompt);
}


static int command_inter_preter_loop(int batchmode, char *batchfname,
	int argc, char **argv)
{
	CMD_LINKAGE;
	struct batchdata bdta;	/* resident here for inner recursion */
	struct dos0a_buffer *buf0a = &bdta.buf0a;

	char argbuf[256];	/* need this to keep argv[] over recursion */
	int ret = 0;
	int saved_echo_on = ECHO_ON;

	bdta.parent = rdta->current_bdta;
	rdta->current_bdta = &bdta;
	if (bdta.parent) bdta.stdoutredir = bdta.parent->stdoutredir;
	else bdta.stdoutredir = 0;

	if (argc) {
		/* batchmode */
		com_doscanonicalize(bdta.filename, batchfname);
		if (!com_exist_file(bdta.filename)) {
			rdta->current_bdta = bdta.parent;
			rdta->exitcode = DOS_ENOENT;
			return -1;
		}
		bdta.mode = batchmode;
		bdta.nextline = 0;
#ifdef CMD_USE_CACHED_BATCH
		/* try to cache the batchfile */
		cache_batch_file();
#endif
		#undef XXreturn
	}
	else {
		/* interactive */
		bdta.mode = 0;
	}

	while (1) {
		char *argv[MAXARGS];
		int argc;

		if (!bdta.mode && ECHO_ON) print_prompt(2);
		ret = read_next_command();
		if (BREAK_PENDING) {
		    if (bdta.parent && bdta.parent->mode) {
			bdta.parent->nextline = 99999; /* force EOF */
		    }
		    rdta->exitcode = 0;
		    break;
		}
		if (ret <= 0) {
			rdta->exitcode = 0;
			break;
		}
		if (!bdta.mode) com_doswrite(2, "\r\n", 2);

		memcpy(argbuf, &LEN0A, LEN0A+2); /* save contents */
		/* WARNING! argbuf may be modified by com_argparse */
		argc = com_argparse(argbuf, argv, MAXARGS -1);
		bdta.argcsub = bdta.argc = argc; /* save positional variables */
		bdta.argvsub = bdta.argv = argv;
		bdta.argshift = 0;
		if (!strcasecmp (argv [0], "exit")) {
			if (!bdta.mode && !rdta->cannotexit) {
				rdta->exitcode = 0;
				ret = 0;
				
				if (rdta->toplevel)
					leavedos (0);
				else
					break;
			}
			if (ECHO_ON) com_fprintf (2, "\n");
		}
		else
			ret = dopath_exec(argc, argv);
	};

	ECHO_ON = saved_echo_on;
	
	if (bdta.mode == 1) {
		/* stupid old not-return DOS batching */
		if (bdta.parent && bdta.parent->mode) {
			bdta.parent->nextline = 99999; /* force EOF */
		}
	}
	rdta->current_bdta = bdta.parent;
	return ret;
}



static void com_fatal_handler(int param)
{
   CMD_LINKAGE;
   struct vm86_regs saved_regs = REGS; /* first save all x86 regs */
   int this_psp;

   switch(param) {

     case 0x23:	/* Ctrl-C */

	/* we never allow abortion of command.com, but set a flag
	 * the internal command can poll to abort themselves
	 */
	HI(ax) = 0x62;	/* get PSP */
	call_msdos();
	this_psp = LWORD(ebx);
	REGS = saved_regs;
	if (this_psp != _psp) {
		/*
		 * We come here because we inherited our handler
		 * to our childs.
		 * As evry instance of comcom has its own int23 handler,
		 * we will come here only for non-comcom instances.
		 * However, these may be coopthreads ones too.
		 * So we check wether we have to delete the thread
		 * before we return with CARRY set.
		 */
		struct com_starter_seg *this_ctcb = SEG2LINEAR(this_psp);
		unsigned char * ssp = SEG2LINEAR(LWORD(ss));
		unsigned long sp = (unsigned long) LWORD(esp);
		if (this_ctcb->magic == COM_THREAD_MAGIC) {
			/* ... fingers crossed, hope DOS never will try
			 * to acces this thread again at all :-(
			 */
			com_delete_other_thread(this_ctcb);
		}
		set_CF();	/* tells DOS to abort the program */
				/* now simulate a RETF 0 (yes, you've read
				 * correctly: _not_ a RETF 2)
				 */
		LWORD(eip) = popw(ssp, sp);
		LWORD(cs) = popw(ssp, sp);
		LWORD(esp) += 4;
		return;
	}
	/* we are at ourselves */
	rdta->break_pending++;
	return;

     case 0x24: { /* critical error */

	static char unknownerr[] = "Unknown Error";
	static char *ioerrors[] = {
		"Write Protect",
		"Unknown Unit",
		"Device Not Ready",
		"Unknown Command",
		"Crc Error",
		"Bad Length",
		"Seek Error",
		"Unknown media type",
		"Sector Not Found",
		"Printer out of Paper",
		"Write Fault",
		"Read Fault",
		"General Failure",
		/* fill up to 15 */
		unknownerr, unknownerr, unknownerr
	};
	char *area[] = {
		"DOS",
		"FAT",
		"directory",
		"data"
	};
	struct devhdr {
		FAR_PTR next;
		unsigned short attr;
		unsigned short strat, intr;
		char name[8];
	};
	int promptreturn;
	int inherited;
	unsigned char key;
	int allowed = HI(ax);
	int failallowed, retryallowed, ignoreallowed;
	char coutbuf[256], *cout = coutbuf, *p;

	/*
	 * As we are not allowed to call function 0x62 (get PSP),
	 * we have to look at the stack wether we are called as an
	 * inherited handler or not.
	 *
	 * Along MSDOS programmers guide we have the following stacklayout
	 * upon entry of this handler:
	 *
	 * SP + 0	IP,CS,FLAGS from _this_ int24
	 *    + 6	AX,BX,CX,DX,SI,DI,BP,DS,ES
	 *		saved user registers from original int21
	 *    +24	IP,CS,FLAGS from original int21
	 *
	 * We are assuming, that when content of SP+26 (CS) is equal to
	 * our psp, we are interrupted for ourselve.
	 */

	inherited = *((unsigned short*)SEGOFF2LINEAR(LWORD(ss),LWORD(esp)+26));
	inherited = inherited != _psp;

	if (HI(ax) & 0x80) {
		struct devhdr *dhdr = ((void *)SEGOFF2LINEAR(LWORD(ebp),LWORD(esi)));
		if (dhdr->attr & 0x8000) {
			/* is device */
			char name[9];
			memcpy(name, dhdr->name, 8);
			name[8] = 0;
			cout += com_sprintf(cout, "%s on device %s\n",
				ioerrors[LWORD(edi)&15], name
			);
		}
		else {
			/* is block, but FAT destroyed */
			cout += com_sprintf(cout, "FAT image destroyed\n");
		}
	}
	else {
		/* disk error */
		cout += com_sprintf(cout, "%s while %s %s area of drive %c:\n",
			ioerrors[LWORD(edi)&15],
			HI(ax) & 1 ? "writing" : "reading",
			area[(HI(ax)>>1) & 3],
			LO(ax) + 'A'
		);
	}

	failallowed = allowed & 0x8;
	retryallowed = allowed & 0x10;
	ignoreallowed = allowed & 0x20;

	p = cout;
	if (inherited) cout += com_sprintf(cout, "(A)bort");
	if (failallowed)
		cout += com_sprintf(cout, "%s(F)ail", p==cout?"":", ");
	if (retryallowed)
		cout += com_sprintf(cout, "%s(R)etry", p==cout?"":", ");
	if (ignoreallowed)
		cout += com_sprintf(cout, "%s(I)gnore", p==cout?"":", ");

	com_doswriteconsole(coutbuf);
	key = getkey();
	com_doswriteconsole("\r\n");
	switch ((tolower(key))) {
		case 'a': promptreturn = 2; break;
		case 'f': promptreturn = failallowed ? 3 : 2; break;
		case 'r': promptreturn = retryallowed ? 1 : 3; break;
		case 'i': promptreturn = ignoreallowed ? 0 : 3; break;
		default:  promptreturn = 2; break;
	}
	if (rdta->cannotexit
		&& promptreturn == 3
		&& !failallowed) promptreturn = 2;

	if (!inherited && promptreturn == 2) {
		/* error happened for ourselves but we cannot abort,
		 * so check further how to deal with it.
		 *
		 * We have two cases to handle:
		 * A) we try to 'fail' (good case)
		 * B) if we are not allowed, we force us back here
		 *    and longjump to our initial loop.
		 *    This has the disadvantage, that lowmem allocations
		 *    may not be freed and perhaps cause later problems
		 */
		
		if (failallowed) {
			promptreturn = 3;
		}
		else {	/* odd case */

			if (!rdta->fatal_handler_return_possible) {
				error("COMCOM, unable to cleanly recover from int24\n");
				leavedos(99);
			}
			REGS = rdta->fatal_handler_return_regs;
			/* now issue _one_ MSDOS function above 0x0C
			 * in order to 'stabilize' the system
			 * (as tells us MSDOS programmers guide)
			 */
			HI(ax) = 0x59;
			LWORD(ebx) = 0;
			call_msdos();
			longjmp(rdta->fatal_handler_return, 1);
			/* we never come here */
		}
	}

	REGS = saved_regs;
	LO(ax) = promptreturn;
     }
   }
}

static int duplicate_env(int size)
{
	struct com_starter_seg	*ctcb = CTCB;
	struct com_MCB *mcb = NULL;
	char *newenv;
	int oldsize = 0;

	size = (size+15) & -16;
	if (ctcb->envir_frame != 0) {
		mcb = (void *)((ctcb->envir_frame-1) << 4);	   
		oldsize = mcb->size << 4;
		if (size <= oldsize) size = oldsize;
	}
	newenv = (char *)com_dosallocmem(size);
	if ((int)newenv < 0) return oldsize;	/* giving up */
	memcpy(newenv, ((char *)mcb)+16, oldsize);
	ctcb->envir_frame = ((unsigned int)newenv) >> 4;
	return size;
}

int comcom_main(int argc, char **argv)
{
	struct com_starter_seg *ctcb = CTCB;
	struct res_dta rdta_buff;
	struct res_dta *rdta = &rdta_buff;

	int argcX;
	char *argvX[] = {0,0};
	int i;
	static int toplevel = 1;
	int option_k = 0;

	UDATAPTR = rdta;
	rdta->current_bdta = 0;
	rdta->break_pending = 0;
	rdta->fatal_handler_return_possible = 0;
	rdta->cannotexit = 0;
	rdta->toplevel = 0;
	rdta->option_p = 0;
	rdta->need_errprinting = 0;

	EXITCODE = 0;
	rdta->errorlevel = 0;
	ECHO_ON = 1;

	/* first free all memory we don't need for ourselves, such that
	 * we can allocate arbitrary DOS memory block when needed
	 */
	com_free_unused_dosmem();


	/* establish our fatal handlers */
	register_com_hook_function(0x23, com_fatal_handler, 0, 0x23);
	register_com_hook_function(0x24, com_fatal_handler, 1, 0x24);

	/*
	 * here we enlarge the env accordingly to /E:nnnn
	 */
	rdta->envsize = 0;
	i = findoption("/e:", argc, argv);
	if (i) {
		rdta->envsize = strtoul(argv[i], 0, 0);
		argc = remove_arg(i, argc, argv);
	}

	/*
	 * now alloc a max sized env and copy the old one to it,
	 * when starting an other program, we will shrink it to
	 * the minimum or to rdta->envsize, if that was given
	 * via option /E:
	 */
	duplicate_env(0x8000);


	i = findoption("/p", argc, argv);
	if (i) {
		char *p, *comspec=0;
		char comspec_[16];
		char drive = com_dosgetdrive() + 'A';
		char buf[128];

		rdta->option_p = 1;
		rdta->toplevel = toplevel;
		if (!toplevel)
			rdta->cannotexit = 1;
		toplevel = 0;

		/* force the int23/24 entry in PSP to point
		 * to our own routines, so we are sure we inherit it.
		 */
		ctcb->int23_copy = *((FAR_PTR *)(0x23<<2));
		ctcb->int24_copy = *((FAR_PTR *)(0x24<<2));

		/* we need to set COMSPEC from the the first
		 * non-option argument
		 */
		for (i=1; i <argc; i++) {
			p = argv[i];
			if (!(*p == '/' && strchr("pkcey", tolower(p[1])))) {
				comspec = p;
				break;
			} else if (strchr("kc", tolower(p[1]))) {
				i++;
			}
		}			
		if (!comspec) {
			comspec = comspec_;
			sprintf(comspec, "%c:\\COMMAND.COM", drive);
		}
		com_msetenv("COMSPEC", comspec, _psp);
		argc = remove_arg(i, argc, argv);

		/* we now try to exec autoexec.bat or its substitute */
		i = findoption("/k", argc, argv);
		if (i && argv[++i]) {
			strcpy(buf, argv[i]);
		} else {
			sprintf(buf, "%c:\\AUTOEXEC.BAT", drive);
		}
		argcX = 1;
		if (com_exist_file(buf)) {
			argvX[0] = buf;
			dopath_exec(argcX, argvX);
		}
		else {
			argvX[0] = "date";
			cmd_date(argcX, argvX);
			argvX[0] = "time";
			cmd_time(argcX, argvX);
		}
	}


	i = findoption("/c", argc, argv);
	if (!i && !rdta->option_p) {
		i = findoption("/k", argc, argv);
		option_k = 1;
	}
	if (i) {
		/* we just start one program and exit after it terminates */
		if (!argv[++i]) return 0;
		dopath_exec(argc-i, argv+i);
		/* NOTE: there is no way to return the exitcode of the program,
		 *	 we need to return _our_ exitcode.
		 */
		if (!option_k && !rdta->option_p) return 0;
		/* else fall through and start interactively */
	}

	/* signon */
	printf("\nDOSEMU built-in command.com version %#4.2g\n\n",comcomversion);

	CLEAR_EXITCODE;
	if (command_inter_preter_loop(1, 0, 0, argvX+1 /*pointer to NULL*/)) {
		com_fprintf(1, "%s\n", decode_DOS_error(rdta->exitcode));
	}

	/* in case of option_p we never should come here */
	return 0;
}

