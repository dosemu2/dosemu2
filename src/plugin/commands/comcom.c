/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DOSEMU build-in COMMAND>COM
 *
 * Copyright (c) 2001 Hans Lermen <lermen@fgan.de>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "doshelpers.h"
#include "../coopthreads/coopthreads.h"

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
#undef USE_HEAP_EATING_BULTINS


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
	int need_errprinting;
	int echo_on;
	int envsize;	/* size of enlarged enviroment */
	int option_p;	/* 1 = this command.com is primary */
	int cannotexit;	/* 1 = this command.com cannot exit savely */
	int break_pending;
	int fatal_handler_return_possible;
	struct vm86_regs fatal_handler_return_regs;
	jmp_buf fatal_handler_return;
};


extern int com_msetenv(char *, char *, int);
static int command_inter_preter_loop(int batchmode, int argc, char **argv);
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


#ifndef USE_PLAIN_DOS_FUNCT0A

static void builtin_funct0a(char *buf)
{
	struct com_starter_seg  *ctcb = CTCB;
	#define HISTMAX	32
	static char history[HISTMAX][128];
	static int histi = -1;
	static int history_empty = 1;
	int histl = -1;
	char *p, *b;
	char *saved_line = 0;
	int i, lastkey = 0;
	int count, cursor, winstart, attrib;
	int screenw, screenpage, posx, posy, leftmostx, linelen;
	
	void start_line()
	{
		screenw = com_biosvideo(0x0f00) >> 8;
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

	int getkey()
	{
		int ret = com_bioskbd(0);
		if (ret & 255) return ret & 255;
		return (ret >> 8) | 0x100;
	}

	void putkey(int key)
	{
		LWORD(ebx) = screenpage;
		com_biosvideo((key & 255) | 0x0e00);
		cursor++;
	}

	void setcursorpos(int x)
	{
		HI(dx) = posy;
		LO(dx) = posx + x;
		HI(bx) = screenpage;
		com_biosvideo(0x200);
	}

	void update_display(int forcerefresh)
	{
		int len;

		if (count > linelen) {
			if (cursor >= linelen) { /* shift right */
				if ((count - winstart) > linelen) {
					winstart++;
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

	void insert(int c)
	{
		int i, j = cursor+winstart;
		for (i=count; i > j; i--) p[i] = p[i-1];
		p[j] = c;
		count++;
		cursor++;
	}

	void delete(int pos)
	{
		int i, j = cursor+pos+winstart;
		if (j < 0) return;
		for (i=j; i <= count; i++) p[i] = p[i+1];
		count--;
		cursor = cursor+pos;
		if (cursor < 0) cursor = 0;
		if (count >= linelen && winstart > 0) {
			winstart--;
			cursor++;
		}
	}

	void replace_line(char *line)
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

	void from_history(int direction)
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

	int readline(char *buf)
	{
		int c;
		p = buf+1;
		start_line();
		cursor = 0;
		while (1) {
		    c = getkey();
		    switch (c) {
			case 0x014B:	/* left arrow key */
			    cursor--;
			    update_display(0);
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
			case 8:		/* backspace */
			    if (count > 0) {
				delete(-1);
				update_display(1);
			    }
			    break;
			case 0x0153:		/* Del */
			    if (count > 0) {
				delete(0);
				update_display(1);
			    }
			    break;
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
			case 32 ... 126:
			case 128 ... 255:
			    if (count > 126) break;
			    if (cursor < linelen) {
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
	int ret;
	char *p;

	void substitute_positional(char **t_, char **s_)
	{
		struct batchdata *pd = bdta->parent;
		char *s = *s_, *t = *t_;
		int i = s[0] - '0';
		int len;

		if (pd) {
			if (i < pd->argc) {
				len = strlen(pd->argv[i]);
				memcpy(t, pd->argv[i], len);
				*t_ = t + len;
			}
		}
		while (isdigit(*s)) s++;
		*s_ = s;
		return;
	}

	void substitute_env(char **t, char **s_, char *p)
	{
		char *s = *s_;
		char buf[128];
		int len;

		*p = 0;
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

	void variable_substitution(void)
	{
		char buf[1024];	/* big enough to catch overflows */
		char *s = STR0A, *s_, *p, *t;
		int len = LEN0A;
		
		s[len] = 0;
		p = strchr(s, '%');
		if (!p || ((p - s) > 126)) {
			/* usual case */
			s[len] = '\r';
			return;
		}

		memcpy(buf, s, (p - s));
		t = buf + (p - s);
		s_ = s;
		s = p;
		while (*s) {
			if (s[0] == '%') {
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
			}
			else *t++ = *s++;
		}
		len = (t - buf);
		if (len > 126) len = 126;
		buf[len] = '\r';
		memcpy(STR0A, buf, len+1);
		LEN0A = len;
	}

	while (1) {
		ret = lowlevel_read_next_command();
		if (ret <= 0) return ret;
		variable_substitution();
		if (!bdta->mode) return ret;

		if (STR0A[0] != '@' && ECHO_ON) {
			print_prompt(1);
			com_doswrite(1, STR0A, LEN0A);
			com_doswrite(1, "\r\n", 2);
		}
		p = skipwhite(STR0A);
		if (*p != ':') return ret;

		/* have to remove label */
		p = skipwhite(skip2nonwhite(p));
		if (*p == '\r') continue;

		LEN0A -= p - STR0A;
		memcpy(STR0A, p, LEN0A + 1);
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
	char *extlist[] = {
		"bat", "com", "exe"
	};
	int len = strlen(file);
	char *p = strcpy(buf, file) +len;
	int i;

	if ((len >4) && (file[len-4] == '.')) {
		for (i=0; i<3; i++)
			if (!strncasecmp(file+len-3, extlist[i], 3))
				if (com_exist_file(file)) return i+1; 
		return 0;
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
		if (i == 1) return 11;
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
			if (i == 1) return 11;
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

static int mkstemm(char *buf, char *path, int expand)
{
	int len;
	char *p;

	if (!expand || (expand && com_doscanonicalize(buf, path)))
							strcpy(buf, path);
	len  = strlen(buf);
	p = buf + len;
	while (p != buf && !(*p == '\\' || *p == '/' || *p == ':')) p--;
	if (p != buf) p++;
	*p = 0;
	return p - buf;
}

static int kdbask(char *answerlist, char *fmt, ...)
{
	va_list ap;
	int i, len;
	char s[128], alist[128];
	unsigned char key, c, dfltkey;
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
	com_fprintf(2, " (%s)", s);
	while (1) {
		key = com_bioskbd(0);
		if (key == 3) return -1;
		for (i=0; i < len; i++) {
			if (tolower(key) == alist[i]) return key;
		}
	}
}

/* ---------------- internal command --------------------- */


static int cmd_echo(int argc, char **argv) {
	CMD_LINKAGE2;
	char buf[256] = "";
	int fd = 2;

	if (argv[0][0] == '@') {
		if (!strcasecmp(argv[1], "off")) ECHO_ON = 0;
		else if (!strcasecmp(argv[1], "on")) ECHO_ON = 1;
		return 0;
	}
	if (argc > 1) concat_args(buf, 1, argc-1, argv);
	if (bdta && bdta->stdoutredir) fd = 1;
	com_fprintf(fd, "%s\n", buf);
	return 0;
}

static int cmd_pause(int argc, char **argv) {
	char tx[] = "Press any key to continue ...";

	com_doswrite(2, tx, sizeof(tx)-1);
	com_bioskbd(0);
	com_doswrite(2, "\r\n", 2);
	return 0;
}

static int cmd_rem(int argc, char **argv) {
	CMD_LINKAGE2;
	char buf[256];

	if (!bdta->mode) return 0;
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
	CMD_LINKAGE;
	struct findfirstnext_dta *dta = (void *)CTCB;
	char s[256], *p;
	char *vname;
	int i, g1, gn, arg0_new, len;
	int repl[32], repli;


	void do_command(char *varcontents)
	{
		int i;
		char saved_dta[128];
		for (i=0; i <repli; i++) {
			argv[repl[i]] = varcontents;
		}
		memcpy(saved_dta, ((char *)dta)+128, 128);
		dopath_exec(argc-arg0_new, argv+arg0_new);
		memcpy(((char *)dta)+128, saved_dta, 128);
	}



	CLEAR_EXITCODE;
	if (argc < 6) return DOS_EINVAL;
	if (strcasecmp(argv[2], "in")) return DOS_EINVAL;
	vname = argv[1];
	if (vname[0] != '%') return DOS_EINVAL;

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
	repli = 0;
	for (i=arg0_new; i < argc; i++) {
		if (!strcasecmp(argv[i], vname)) repl[repli++] = i;
	}

	/* Ok, now for the real work */
	for (i=g1; i <= gn; i++) {
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
		else {
			/* this is plain text */
			do_command(argv[i]);
		}
	}
	return EXITCODE;
}


static int cmd_if(int argc, char **argv)
{
	CMD_LINKAGE;
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
			dopath_exec(argc-argi, argv+argi);
		}
		return EXITCODE; /* do NOT change exitcode */
	}

	if (!strcasecmp(argv[argi], "errorlevel")) {
		if (argi+2 >= argc) return DOS_EINVAL;
		level = strtoul(argv[argi+1], 0, 10);
		if (((EXITCODE >= level) ? 1 : 0) ^ invers) {
			argi += 2;
			dopath_exec(argc-argi, argv+argi);
		}
		return EXITCODE; /* do NOT change exitcode */
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
	if (((strcasecmp(argv[argi], p)) ? 0 : 1) ^ invers) {
		argi = argex;
		dopath_exec(argc-argi, argv+argi);
	}
	return EXITCODE; /* do NOT change exitcode */
}


static int cmd_shift(int argc, char **argv)
{
	CMD_LINKAGE2;
	struct batchdata *pd = bdta->parent;
	if (!pd || (pd->argc <=0)) return 0;
	pd->argc--;
	pd->argv++;
	return 0;
}

static int cmd_cd(int argc, char **argv)
{
	int cwd = com_dosgetdrive();
	char *s = argv[1];
	int ret;

	if (!s) {
		char buf[128];
		/* just display the current curectory */
		com_dosgetcurrentdir(0, buf);
		com_fprintf(2,"%c:\\%s\n", cwd + 'A', buf);
		return 0;
	}
	if (s[1] && s[1] == ':') {
		com_dossetdrive(toupper(s[0]) -'A');
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
	int fd, ret, bsize = 128;

	if (!argv[1]) {
		return DOS_EINVAL;
	}
	fd = com_dosopen(argv[1],0);
	if (fd <0) {
		return com_errno & 255;
	}
	do {
		ret = com_doscopy(1, fd, bsize);
	} while (ret == bsize);
	com_dosclose(fd);
	if (ret == -1) {
		return com_errno & 255;
	}
	return 0;
}

static int cmd_del(int argc, char **argv)
{
	struct findfirstnext_dta *dta = (void *)CTCB;
	char s[256];
	int len, opt_p;

	int delit(void)
	{
		if (dta->attrib & DOS_ATTR_PROTECTED) return 0;
		strcpy(s+len, dta->name);
		if (opt_p) {
			int c = kdbask("yN", "%s, delete?", s);
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

	opt_p = findoption("/p", argc-1, argv+1);

	if (!argv[1] || com_exist_dir(argv[1])) {
		return DOS_EINVAL;
	}
	if (!com_dosfindfirst(argv[1], DOS_ATTR_FILE)) {
		len = mkstemm(s, argv[1], 1);
		if (delit()) return 0;
		while (!com_dosfindnext()) if (delit()) return 0;
	}
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
	unsigned num_files=0, num_bytes=0;
	char *p, *wildcard = "*.*";
	int i, c;
	int opt_p, opt_w, opt_l, opt_b, opt_a;
	int incmask = DOS_ATTR_ALL | 0x80;
	int excmask = DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM;

	void cache_it(void)
	{
		if (cachei >= maxfiles) return; /* ... silently */
		if (!dta->attrib) dta->attrib = 0x80;
		if (excmask & dta->attrib) return;
		if (!(incmask & dta->attrib)) return;
		num_files++;
		num_bytes += dta->filesize;
		memcpy(cache+(cachei++), &dta->attrib, sizeof(struct tiny_dta));
	}

	void sort_it(void)
	{
		if (!cachei) return;
		qsort(cache, cachei, sizeof(struct tiny_dta), qsort_dircmp);
	}

	void displ_b(int i)
	{
		com_printf("%s\n", cache[i].name);
	}

	void displ_w(int i)
	{
		struct tiny_dta *e = &cache[i];
		char b[32];
		char *f = e->name;
		if (e->attrib & DOS_ATTR_DIR) {
			sprintf(b, "[%s]", f);
			f = b;
		}
		if ((i % 5) == 4) com_printf("%s\n", f);
		else com_printf("%-16s", f);
	}

	void displ(int i)
	{
		struct tiny_dta *e = &cache[i];
		int d = e->filedate;
		int t = e->filetime;
		char *p = strchr(e->name, '.');
		if (p) *p++ = 0;
		else p = "";
		com_printf("%-9s%-4s%-5s%8d %02d-%02d-%02d   %2d:%02d\n",
			e->name, p,
			(e->attrib & DOS_ATTR_DIR) ? "<DIR>" : "",
			e->filesize,
			(d>>5) & 15, d & 31,
			(d>>9) < 20 ? (d>>9) + 80 : (d>>9) - 20,
			(t>>11), (t>>5) & 63
		);
	}

	void displ_header(void)
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

	void displ_footer(void)
	{
		unsigned info[4];
		int ret;
		if (opt_b) return;
		ret = com_dosgetfreediskspace(toupper((dirname[0])-'A')+1, info);
		com_printf("\n  %10d file(s)   %10ld bytes\n",
			num_files, num_bytes);
		com_printf("                       %10lu bytes free\n\n",
			info[0] * info[1] * info[2]);
	}

	void display_it(void)
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
		displ_footer();
	}

	void flush_it(void)
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
		    #undef XXX(x)
		}
		if (!incmask) incmask = DOS_ATTR_ALL +0x80;
		argc = remove_arg(i, argc, argv);
	}
	if (argc > 2) {
		return DOS_EINVAL;
	}
	cache = alloca(huge_enough);

	/* prepare for header */
	if (argc > 1) wildcard = argv[1];
	dirnamelen = mkstemm(dirname, wildcard, 1);

	if (!com_dosfindfirst(wildcard, incmask & 0x7f)) {
		cache_it();
		while (!com_dosfindnext()) cache_it();
	}
	sort_it();
	display_it();
	flush_it();
	return 0;
}

static int cmd_ren(int argc, char **argv)
{
	if (argc < 3) {
		return DOS_EINVAL;
	}
	if (!com_dosrenamefile(argv[1], argv[2])) return 0;
	return com_errno & 255;
}

static int cmd_set(int argc, char **argv)
{
	char *env = SEG2LINEAR(CTCB->envir_frame);
	char *p;

	if (!argv[1]) {
		while (*env) {
			com_printf("%s\n", env);
			env += strlen(env) + 1;
		}
		return 0;
	}
	p = strchr(argv[1], '=');
	if (p) *p++=0;
	else p = "";
	if (!com_msetenv(argv[1], p, 1)) return 0;
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
	if (!com_msetenv("PATH", argv[1], 1)) return 0;
	return DOS_ENOMEM;
}

static int cmd_prompt(int argc, char **argv)
{
	char *p = argv[1];

	if (!argv[1]) p = "";
	if (!com_msetenv("PROMPT", argv[1], 1)) return 0;
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
	LO(dx) = strcasecmp(argv[1], "on") ? 1 : 0;
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
		"Sun", "Mon", "Tus", "Wed", "Thu", "Fri", "Sat", "Oooch?"
	};
	char buf[128];
	int ret, option_q = 0;

	int changedate(char *s)
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
		  "Current date is %s %d-%d-%d\n",
		  dtable[ (date>>12) & 7],
		  (date>>8) & 15, date & 255, date >> 16
		);
		if (option_q) return 0;
		com_printf("Enter date (mm-dd-yy): ");
		buf[0] = 0;
		ret = changedate(buf);
		com_printf("\n");
		if (!ret) return 0;
		return DOS_EINVAL;
	}

	if (argc == 2) {
		com_printf("\n");
		ret = changedate(argv[1]);
		if (!ret) return 0;
	}
	return DOS_EINVAL;
}

static int cmd_time(int argc, char **argv)
{
	char buf[128];
	int option_q = 0;

	int entertime(char *s)
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
		  "Current time is %02d:%02d:%02d.%02d\n",
		  time>>24, (time>>16) & 255, (time>>8) & 255, time & 255
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
		"DOSEMU built-in command.com version %4.2g\n"
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

struct cmdlist {
	char *name;
	com_program_type *cmd;
	int flags;
};

/* protos for DOSEMU support commands */
extern int exitemu_main(int argc, char **argv);
extern int speed_main(int argc, char **argv);
extern int bootoff_main(int argc, char **argv);
extern int booton_main(int argc, char **argv);
extern int ecpuon_main(int argc, char **argv);
extern int ecpuoff_main(int argc, char **argv);
extern int eject_main(int argc, char **argv);
extern int emumouse_main(int argc, char **argv);
extern int ugetcwd_main(int argc, char **argv);
extern int vgaoff_main(int argc, char **argv);
extern int vgaon_main(int argc, char **argv);
#ifdef USE_HEAP_EATING_BULTINS
extern int lredir_main(int argc, char **argv);
extern int unix_main(int argc, char **argv);
extern int dosdbg_main(int argc, char **argv);
extern int xmode_main(int argc, char **argv);
extern int system_main(int argc, char **argv);
extern int uchdir_main(int argc, char **argv);
#endif


struct cmdlist intcmdlist[] = {
	{"cd",		cmd_cd, 0},
	{"chdir",	cmd_cd, 0},
	{"goto",	cmd_goto, 0},
	{"echo",	cmd_echo, 0},
	{"@echo",	cmd_echo, 0},
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

#if 0
/* not yet implemented, but available as standalone programs:  */
	{"copy",	cmd_copy, 0},
	{"choice",	cmd_choice, 0},
#endif

	/* DOSEMU special support commands */

	{"exitemu",	exitemu_main, 1},
	{"speed",	speed_main, 1},
	{"bootoff",	bootoff_main, 1},
	{"booton",	booton_main, 1},
	{"ecpuoff",	ecpuoff_main, 1},
	{"ecpuon",	ecpuon_main, 1},
	{"eject",	eject_main, 1},
	{"emumouse",	emumouse_main, 1},
	{"ugetcwd",	ugetcwd_main, 1},
	{"vgaoff",	vgaoff_main, 1},
	{"vgaon",	vgaon_main, 1},

#ifdef USE_HEAP_EATING_BULTINS
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

	if (isalpha(arg0[0]) && arg0[1] && arg0[1] == ':') {
		com_dossetdrive(toupper(arg0[0]) -'A');
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
	rdta->cannotexit = saved_cannotexit;

	/* re-enlarge the environment to maximum */
	com_dosreallocmem(ctcb->envir_frame, 0x8000);
	if (ret == -1) setexitcode(com_errno & 255, -1);
	else setexitcode(ret, -1);
	return ret;
}

static int dopath_exec(int argc, char **argv)
{
	CMD_LINKAGE;
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


	/* check for explicite redirections and remove them from argv */
	for (i=1; i <argc; i++) {
	    switch (argv[i][0]) {
		case '>':
			if (argv[i][1]) argv[i]++;
			if (argv[i][0] == '>') {
				redir_out_append = 1;
				argv[i]++;
			}
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
			if (argv[i][1]) argv[i]++;
			if (!argv[i][0]) argc = remove_arg(i, argc, argv);
			if (!redir_in) redir_in =
				com_redirect_stdxx(argv[i], &redir_stdin, 0);
			break;
	    }
	}	

	switch (scan_path_env(name, argv)) {
	    case 1:
		argv[0] = name;
		ret = command_inter_preter_loop(1, argc, argv);
		break;
	    case 11:
		argv[1] = name;
		ret = command_inter_preter_loop(2, argc-1, argv+1);
		break;
	    case 0:
		ret = do_internal_command(argc, argv);
		if (ret != -2) break;
		/* else fallthrough */
	    default: {
		cmdline[0] = 0;
		if (argv[1]) {
			concat_args(cmdline, 1, argc-1, argv);
		}
		ret =  launch_child_program(name, cmdline);
		break;
	    }
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


static int command_inter_preter_loop(int batchmode, int argc, char **argv)
{
	CMD_LINKAGE;
	struct batchdata bdta;	/* resident here for inner recursion */
	struct dos0a_buffer *buf0a = &bdta.buf0a;

	char argbuf[256];	/* need this to keep argv[] over recursion */
	int ret = 0;

	bdta.parent = rdta->current_bdta;
	rdta->current_bdta = &bdta;
	if (bdta.parent) bdta.stdoutredir = bdta.parent->stdoutredir;
	else bdta.stdoutredir = 0;

	if (argc) {
		/* batchmode */
		com_doscanonicalize(bdta.filename, argv[0]);
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
		char *argv[32];
		int argc;

		if (!bdta.mode) print_prompt(2);
		ret = read_next_command();
		if (ret <= 0) {
			rdta->exitcode = 0;
			break;
		}
		if (!bdta.mode) com_doswrite(2, "\r\n", 2);

		memcpy(argbuf, &LEN0A, LEN0A+1); /* save contents */
		argc = com_argparse(argbuf, argv, 31);
		bdta.argc = argc;	/* save positional variables */
		bdta.argv = argv;
		if (!bdta.mode && !rdta->cannotexit
				&& !strcasecmp(argv[0], "exit")) {
			rdta->exitcode = 0;
			ret = 0;
			break;
		}
		ret = dopath_exec(argc, argv);
	};

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
	key = com_bioskbd(0);
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
	struct com_starter_seg  *ctcb = CTCB;
	struct com_MCB *mcb = (void *)((ctcb->envir_frame-1) << 4);
	char *newenv;
	int oldsize = mcb->size << 4;

	size = (size+15) & -16;
	if (size <= oldsize) size = oldsize;
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

	UDATAPTR = rdta;
	rdta->current_bdta = 0;
	rdta->break_pending = 0;
	rdta->fatal_handler_return_possible = 0;
	rdta->cannotexit = 0;
	rdta->option_p = 0;
	rdta->need_errprinting = 0;

	EXITCODE = 0;
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
		rdta->cannotexit =1;

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
			if (!(*p == '/' && strchr("pcey", tolower(p[1])))) {
				comspec = p;
				break;
			}
		}			
		if (!comspec) {
			comspec = comspec_;
			sprintf(comspec, "%c:\\COMMAND.COM", drive);
		}
		com_msetenv("COMSPEC", comspec, 1);
		argc = remove_arg(i, argc, argv);

		/* we now try to exec autoexec.bat */
		sprintf(buf, "%c:\\AUTOEXEC.BAT", drive);
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
	if (i) {
		/* we just start one program and exit after it terminates */
		if (!argv[++i]) return 0;
		dopath_exec(argc-i, argv+i);
		/* NOTE: there is no way to return the exitcode of the program,
		 *	 we need to return _our_ exitcode.
		 */
		if (!rdta->option_p) return 0;
		/* else fall through and start interactively */
	}

	/* signon */
	printf("\nDOSEMU built-in command.com version %4.2g\n\n",comcomversion);

	CLEAR_EXITCODE;
	if (command_inter_preter_loop(1, 0, argvX+1 /*pointer to NULL*/)) {
		com_fprintf(1, "%s\n", decode_DOS_error(rdta->exitcode));
	}

	/* in case of option_p we never should come here */
	return 0;
}

