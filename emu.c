#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <termio.h>
#include <termcap.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include <linux/fd.h>
#include "emu.h"

/* make sure that emulate() is the first function */

#define CONFIGURATION 0x002c;   /* without disk information */
#define	KEYBOARD	"/dev/tty"
#define KeyWait		(void)fcntl(kbd_fd, F_SETFL, O_RDONLY)
#define KeyNoWait	(void)fcntl(kbd_fd, F_SETFL, O_RDONLY | O_NDELAY)
#define NOWAIT	0
#define WAIT	1
#define TEST	2
#define SCREEN_ADR(s)	(us *)(0xb8000 + s * 0x1000)
#define UPDATE	400000		/* screen update time in usec */
#define OUTBUFSIZE	2048
#define CHOUT(c)	if (outp == &outbuf[OUTBUFSIZE]) { CHFLUSH } \
			else *outp++ = (trans[c] ? trans[c] : '_');
#define CHFLUSH		if (outp > outbuf) { write(2, outbuf, outp - outbuf); \
						outp = outbuf; }

struct disk {
	char *dev_name;
	int rdonly;
	int sectors, heads, tracks;
	int fdesc;
	int removeable;
	};


/* floppy disks, dos partitions or their images (files) (maximum 8 heads) */
#define DISKS 2    /* maximum 4 */
struct disk disktab[DISKS] = {
		{"/dev/fd0", 0, 0, 0, 0},
		{"/dev/fd1", 0, 0, 0, 0},
	};
/* whole hard disks, dos extented partitions (containing one ore more partitions)
   or their images (files) */
#define HDISKS 1	/* maximum 2 */
struct disk hdisktab[HDISKS] = {
		{"hdimage", 0, 17, 4, 6},
		/* {"/dev/hda", 1, 17, 9, 900}, */
	};


struct vm86_struct vm86s;
int error;
struct timeval scr_tv;
int kbd_fd;
unsigned char outbuf[OUTBUFSIZE], *outp = outbuf;
int iflag;
int akt_keycode, kbcount;
char kbbuf[50], *kbp, akt_key, erasekey;
int hdiskboot =0;
int do_screen_updates;
long start_time;
static	struct termio	oldtermio;	/* original terminal modes */
int screen_bitfield;
int screen, xpos[8], ypos[8];

char tc[200], termcap[1024],
     *cl,	/* clear screen */
     *le,	/* cursor left */
     *cm,	/* goto */
     *ce,	/* clear to end */
     *sr,	/* scroll reverse */
     *so,	/* stand out start */
     *se,	/* stand out end */
     *tp;
int   li, co;   /* lines, columns */     

void startemu(void);     

int emulate(int argc, char **argv)
{

	printf("EMULATE\n");
	sync(); /* for safety */

	if (argc >= 2 && toupper(argv[1][0]) == 'A') hdiskboot = 0;
	if (argc >= 2 && toupper(argv[1][0]) == 'C') hdiskboot = 1;
	if (argc >= 2 && toupper(argv[1][0]) == 'U') do_screen_updates = 1;
	if (argc >= 3 && toupper(argv[2][0]) == 'U') do_screen_updates = 1;
	startemu();
}


unsigned char trans[] = /* LATIN CHAR SET */
{	"\0\0\0\0\0\0\0\0\00\00\00\00\00\00\00\00"
	"\0\0\0\0\266\247\0\0\0\0\0\0\0\0\0\0"
	" !\"#$%&'()*+,-./"
	"0123456789:;<=>?"
	"@ABCDEFGHIJKLMNO"
	"PQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmno"
	"pqrstuvwxyz{|}~ "
	"\307\374\351\342\344\340\345\347\352\353\350\357\356\354\304\305"
	"\311\346\306\364\366\362\373\371\377\326\334\242\243\245\0\0"
	"\341\355\363\372\361\321\252\272\277\0\254\275\274\241\253\273"
	"\0\0\0|++++++|+++++"
	"++++-++++++++-++"
	"+++++++++++\0\0\0\0\0"
	"\0\337\0\0\0\0\265\0\0\0\0\0\0\0\0\0"
	"\0\261\0\0\0\0\367\0\260\267\0\0\0\262\244\0"
};

int outc(int c)
{
  write(2, (char *)&c, 1);
  return 1;
}

void gettermcap(void)
{
struct winsize ws;		/* buffer for TIOCSWINSZ */

  li = 0;
  co = 0;
  if (ioctl(2, TIOCGWINSZ, &ws) >= 0) {
    li = ws.ws_row;
    co = ws.ws_col;
  }
  if(tgetent(termcap, getenv("TERM")) != 1) {printf("no termcap \n"); exit(1);}
  if (li == 0 || co == 0) {
    li = tgetnum("li");
    co = tgetnum("co");
  }
  tp = tc;
  cl = tgetstr("cl", &tp);
  le = tgetstr("le", &tp);
  cm = tgetstr("cm", &tp);
  ce = tgetstr("ce", &tp);
  sr = tgetstr("sr", &tp);
  so = tgetstr("so", &tp);
  se = tgetstr("se", &tp);
  if (se == NULL) so = NULL;
  if (li == 0 || co == 0) {
    printf("unknown window sizes \n");
    exit(1);
  }
}

void poscur(int x, int y)
{
  if ((unsigned)x >= co || (unsigned)y >= li) return;
  tputs(tgoto(cm, x, y), 1, outc);
}

void scrollup(int x0, int y0 , int x1, int y1, int l, int att)
{
	int dx, dy, x, y, ofs;
	us *sadr, *p, *q, blank = ' ' | (att << 8);


	sadr = SCREEN_ADR(screen);
	sadr += x0 + co * (y0 + l);
	dx = x1 - x0 +1;
	dy = y1 - y0 - l +1;
	ofs = -co * l;
	for (y=0; y<dy; y++) {
		p = sadr;
		if (l != 0) for (x=0; x<dx; x++, p++) p[ofs] = p[0];
		else        for (x=0; x<dx; x++, p++) p[0] = blank;
		sadr += co;
	}
	for (y=0; y<l; y++) {
		sadr -= co;
		p = sadr;
		for (x=0; x<dx; x++, p++) *p = blank;
	}
}

void scrolldn(int x0, int y0 , int x1, int y1, int l, int att)
{
	int dx, dy, x, y, ofs;
	us *sadr, *p, blank = ' ' | (att << 8);


	sadr = SCREEN_ADR(screen);
	sadr += x0 + co * (y1 -l);
	dx = x1 - x0 +1;
	dy = y1 - y0 - l +1;
	ofs = co * l;
	for (y=0; y<dy; y++) {
		p = sadr;
		if (l != 0) for (x=0; x<dx; x++, p++) p[ofs] = p[0];
		else        for (x=0; x<dx; x++, p++) p[0] = blank;
		sadr -= co;
	}
	for (y=0; y<l; y++) {
		sadr += co;
		p = sadr;
		for (x=0; x<dx; x++, p++) *p = blank;
	}
}

void char_out(unsigned char ch, int s)
{
	us *sadr, *p;

	if (s > 7) return;
	if (ch >= ' ') {
		sadr = SCREEN_ADR(s);
		sadr[ypos[s]*co + xpos[s]++] = ch | (7 << 8);
		if (s == screen) outc(trans[ch]);
	} else if (ch == '\r') {
		xpos[s] = 0;
		if (s == screen) write(2, &ch, 1);
	} else if (ch == '\n') {
		ypos[s]++;
		if (s == screen) write(2, &ch, 1);
	} else if (ch == '\010' && xpos[s] > 0) {
		xpos[s]--;
		if (s == screen) write(2, le, 1);
	}
	if (xpos[s] == co) {
		xpos[s] = 0;
		ypos[s]++;
	}
	if (ypos[s] == li) {
		ypos[s]--;
		scrollup(0, 0, co-1, li-1, 1, 7);
	}
}

void clear_screen(int s, int att)
{
	us *sadr, *p, blank = ' ' | (att << 8);

	if (s > 7) return;
	if (s == screen) tputs(cl, 1, outc);
	xpos[s] = ypos[s] = 0;
	sadr = SCREEN_ADR(s);
	for (p = sadr; p < sadr+2000; *p++ = blank);
	screen_bitfield = 0;
}

void restore_screen(int bitmap)
{
	us *sadr, *p; 
	unsigned char c;

	if ((bitmap & (1 << (24 + screen))) == 0) return;
	printf("RESTORE SCREEN 0x%x\n", screen_bitfield);
	poscur(0,0);
	sadr = SCREEN_ADR(screen);
	for (p = sadr; p < sadr+2000; p++) {
		c = *(unsigned char *)p;
		CHOUT(c);
	}
	CHFLUSH;
	poscur(xpos[screen],ypos[screen]);
	screen_bitfield = 0;
}


void CloseKeyboard(void)
{
	ioctl(kbd_fd, TCSETAF, &oldtermio);
	close(kbd_fd);
	kbd_fd = -1;
}

int OpenKeyboard(void)
{
	struct termio	newtermio;	/* new terminal modes */

	kbd_fd = open(KEYBOARD, O_RDONLY | O_NDELAY );
	if (kbd_fd < 0)
		return -1;
	if (ioctl(kbd_fd, TCGETA, &oldtermio) < 0) {
		close(kbd_fd);
		kbd_fd = -1;
		return -1;
	}

	newtermio = oldtermio;
	newtermio.c_iflag &= (ISTRIP|IGNBRK); /* (IXON|IXOFF|IXANY|ISTRIP|IGNBRK);*/
	/* newtermio.c_oflag &= ~OPOST; */
	newtermio.c_lflag &= /* ISIG */ 0;
	newtermio.c_cc[VMIN] = 1;
	newtermio.c_cc[VTIME] = 0;
	erasekey = newtermio.c_cc[VERASE];
	if (ioctl(kbd_fd, TCSETAF, &newtermio) < 0) {
		close(kbd_fd);
		kbd_fd = -1;
		return -1;
	}
	return 0;
}

us alt_keys[] = { /* <ALT>-A ... <ALT>-Z */
	0x1e00, 0x3000, 0x2e00, 0x2000, 0x1200, 0x2100,
	0x2200, 0x2300, 0x1700, 0x2400, 0x2500, 0x2600,
	0x3200, 0x3100, 0x1800, 0x1900, 0x1000, 0x1300,
	0x1f00, 0x1400, 0x1600, 0x2f00, 0x1100, 0x2d00,
	0x2c00, 0x1500};

/* ReadKeyboard
   returns 1 if a character could be read in buf 
   */
int ReadKeyboard(int *buf, int wait)
{
	int	cc;
	char *savep;
	int savecnt;

	if (kbcount == 0) {
		if (wait == WAIT) KeyWait;
		for (;;) {
			cc = read(kbd_fd, kbbuf, 50);
			if (cc > 0) {
				kbcount = cc;
				kbp = kbbuf;
				break;
			}
			if (wait != WAIT) break;
		}
		if (wait) KeyNoWait;
		if (kbcount == 0) return 0;
	}
	savep = kbp;
	savecnt = kbcount;
	*buf = (unsigned char)*kbp++;
	kbcount--;
	if (*buf == erasekey) {
		*buf = 8;
	} else if (*buf == '\033' && kbcount > 1 && *kbp == '[') {
		switch (kbp[1]) {
			case 'A':
				*buf = 0x4800;
				kbcount -= 2;
				break;
			case 'B':
				*buf = 0x5000;
				kbcount -= 2;
				break;
			case 'C':
				*buf = 0x4d00;
				kbcount -= 2;
				break;
			case 'D':
				*buf = 0x4b00;
				kbcount -= 2;
				break;
		}
	} else if (*buf >= ('a'|0x80) && *buf <= ('z'|0x80)) {
		*buf = alt_keys[*buf - ('a'|0x80)];
	}
	if (wait == TEST) { /* restore variables (test only) */
		kbp = savep;
		kbcount = savecnt;
	}
	return 1;
}


/* ReadString
   reads a string into a buffer
   buf[0] ... length of string
   buf +1 ... string
   */
void ReadString(int max, char *buf)
{
	char ch, *cp = buf +1, *ce = buf + max;
	int c;

	for (;;) {
		if (ReadKeyboard(&c, WAIT) != 1) continue;
		if ((unsigned)c >= 128) continue;
		ch = (char)c;
		if (ch >= ' ' && ch <= '~' && cp < ce) {
			*cp++ = ch;
			char_out(ch, screen);
			continue;
		}
		if (ch == '\010' && cp > buf +1) { /* BS */
			cp--;
			char_out('\010', screen);
			char_out(' ', screen);
			char_out('\010', screen);
			continue;
		}
		if (ch == 13) {
			*cp = ch;
			break;
		}
	}
	*buf = (cp - buf) -1; /* length of string */
}

void process_key(char key)
{
	int code;

	akt_keycode = (int)key;
	akt_key = key;
}

void disk_close(void) {
	struct disk * dp;

	for (dp = disktab; dp < &disktab[DISKS]; dp++) {
		if (dp->removeable && dp->fdesc >= 0) {
			(void)close(dp->fdesc);
			dp->fdesc = -1;
		}
	}
}

void disk_open(struct disk *dp)
{
struct floppy_struct fl;

	if (dp == NULL || dp->fdesc >= 0) return;
	dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
	if (dp->fdesc < 0) {
		printf("cann't open %s\n", dp->dev_name);
		error = 5;
		return;
	}
	if (ioctl(dp->fdesc, FDGETPRM, &fl) == -1) {
		if (errno == ENODEV) { /* no disk available */
			dp->sectors = 0;
			dp->heads = 0;
			dp->tracks = 0;
			return;
		}
		printf("cann't get floppy parameter of %s (%s)\n", dp->dev_name, sys_errlist[errno]);
		error = 5;
		return;
	}
	printf("FLOPPY %s h=%d, s=%d, t=%d\n", dp->dev_name, fl.head, fl.sect, fl.track);
	dp->sectors = fl.sect;
	dp->heads = fl.head;
	dp->tracks = fl.track;
}

void disk_close_all(void)
{
	struct disk * dp;

	for (dp = disktab; dp < &disktab[DISKS]; dp++) {
		if (dp->fdesc >= 0) {
			(void)close(dp->fdesc);
			dp->fdesc = -1;
		}
	}
	for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
		if (dp->fdesc >= 0) {
			(void)close(dp->fdesc);
			dp->fdesc = -1;
		}
	}
}


void disk_init(void)
{
	int s;
	struct disk * dp;
	struct stat stbuf;
	char buf[30];

	for (dp = disktab; dp < &disktab[DISKS]; dp++) {
		if (stat(dp->dev_name, &stbuf) < 0) {
			printf("cann't stat %s\n", dp->dev_name);
			exit(1);
		}
		if (S_ISBLK(stbuf.st_mode)) printf("ISBLK\n");
		printf ("dev : %x\n", stbuf.st_rdev);
		if (S_ISBLK(stbuf.st_mode) && (stbuf.st_rdev & 0xff00) == 0x200) {
			printf("DISK %s removeable\n", dp->dev_name);
			dp->removeable = 1;
			dp->fdesc = -1;
			continue;
		}
		dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
		if (dp->fdesc < 0) {
			printf("cann't open %s\n", dp->dev_name);
			exit(1);
		}
	}
	for (dp = hdisktab; dp < &hdisktab[HDISKS]; dp++) {
		dp->fdesc = open(dp->dev_name, dp->rdonly ? O_RDONLY : O_RDWR, 0);
		dp->removeable = 0;
		if (dp->fdesc < 0) {
			printf("cann't open %s\n", dp->dev_name);
			exit(1);
		}
#ifdef 0
		if (read(dp->fdesc, buf, 30) != 30) {
			printf("cann't read disk info of %s\n", dp->dev_name);
			exit(1);
		}
		dp->sectors = *(us *)&buf[24];
		dp->heads = *(us *)&buf[26];
		s = *(us *)&buf[19] + *(us *)&buf[28];  /* used sectors + hidden sectors */
		dp->tracks = s / (dp->sectors * dp->heads);
		printf("disk %s; h=%d, s=%d, t=%d, sz=%d, hid=%d\n", dp->dev_name, 
			dp->heads, dp->sectors, dp->tracks, s, *(us *)&buf[28]);
		if (s % (dp->sectors * dp->heads) != 0) {
			printf("cann't read track number of %s\n", dp->dev_name);
			exit(1);
		}
#endif
	}
}


void show_regs(void)
{
	int i;
	unsigned char *cp = (unsigned char *)0 + (_regs.cs<<4) + _regs.eip;

	printf(" cs    eip     ss    esp      flags    ds   es   fs   gs \n");
	printf("%4x:%08x %4x:%08x %08x %4x %4x %4x %4x\n", _regs.cs, _regs.eip, 
		_regs.ss, _regs.esp, _regs.eflags, _regs.ds, _regs.es, _regs.fs, _regs.gs);
	printf("  eax      ebx      ecx      edx      edi      esi      ebp\n");
	printf("%08x %08x %08x %08x %08x %08x %08x \n", _regs.eax, _regs.ebx, 
		_regs.ecx, _regs.edx, _regs.edi, _regs.esi, _regs.ebp);
	for (i=0; i<10; i++)
		printf(" %02x", *cp++);
	printf("\n");
}

int inb(int port)
{
	port &= 0xffff;
	printf("inb [0x%x] \n", port);
	return 0;
}

void outb(int port, int byte)
{
	port &= 0xffff;
	byte &= 0xff;
	printf("outb [0x%x] 0x%x \n", port, byte);
}

void boot(struct disk *dp)
{
	char *buffer;
	int i;

	disk_close();
	disk_open(dp);
	buffer = (char *)0x7c00;
	memset(NULL, 0, 0x7c00); /* clear the first 32 k */
	lseek(dp->fdesc, 0, 0);
	i = read(dp->fdesc, buffer, 512);
	if (i != 512) {
		printf("cann't boot from disk, using harddisk\n");
		dp = hdisktab;
		lseek(dp->fdesc, 0, 0);
		i = read(dp->fdesc, buffer, 512);
		if (i != 512) {
			printf("cann't boot from disk\n");
			_exit(1);
		}
	}
	disk_close();
	_regs.eax = _regs.ebx = _regs.edx = 0;
	_regs.ecx = 0;
	_regs.ebp = _regs.esi = _regs.edi = _regs.esp = 0;
	_regs.cs = _regs.ss = _regs.ds = _regs.es = _regs.fs = _regs.gs = 0x7c0;
	_regs.eip = 0;
	_regs.eflags = 0;
	printf("booted\n");
}

void int10(void)
{
	int x, y, s, i;
	char c, m;

	switch((_regs.eax >> 8) & 0xff) {
		case 0x0: /* define mode */
			if (_regs.eax & 0xff != 2) {
				printf("undefined video mode %d\n", _regs.eax & 0xff);
				error = 1;
			}
			break;
		case 0x1: /* define cursor shape */
			printf("define cursor shape not implemented\n");
			break;
		case 0x2: /* set cursor pos */
			s = (_regs.ebx >> 8) & 0xff;
			x = _regs.edx & 0xff;
			y = (_regs.edx >> 8) & 0xff;
			if (s != 0) {
				printf("video error\n");
				show_regs();
				error = 1;
				return;
			}
			if (x >= co || y >= li) break;
			xpos[s] = x;
			ypos[s] = y;
			if (s == screen) poscur(x, y);
			break;
		case 0x3: /* get cursor pos */
			s = (_regs.ebx >> 8) & 0xff;
			if (s != 0) {
				printf("video error\n");
				show_regs();
				error = 1;
				return;
			}
			_regs.edx = (ypos[s] << 8) | xpos[s];
			break;
		case 0x5: /* change page */ 
			if ((_regs.eax & 0xff) == 0) break;
			printf("video error\n");
			show_regs();
			error = 1;
			return;
		case 0x6: /* scroll up */
			printf("scroll up %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
			show_regs();
			scrollup(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
			break;
		case 0x7: /* scroll down */
			printf("scroll dn %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
			show_regs();
			scrolldn(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
			break;
		case 0x9: /* set chars at cursor pos */
		case 0xA: /* set chars at cursor pos */
			if (_regs.eax & 0xff00 == 0x900)
				m = *(char *)&_regs.eax;
			else 
				m = '\007';
			c = *(char *)&_regs.eax;
			s = (_regs.ebx >> 8) & 0xff;
			if (s != 0) {
				printf("video error\n");
				show_regs();
				error = 1;
				return;
			}
			for (i=1; i < *(us *)&_regs.ecx; i++)
				char_out(c, s);
			break;
		case 0xe: /* print char */ 
			char_out(*(char *)&_regs.eax, screen);
			break;
		case 0x0f: /* get screen mode */
			_regs.eax = (co << 8) | 2; /* chrs per line, mode 2 */
			_regs.ebx &= ~0xff00;
			_regs.ebx |= screen << 8;
			break;
		case 0x8: /* get char */
		case 0xb: /* palette */
		case 0xc: /* set dot */
		case 0xd: /* get dot */
		case 0x4: /* get light pen */
			printf("video error\n");
			show_regs();
			error = 1;
			return;
		case 0x10: /* ega palette */
		case 0x11: /* ega character generator */
		case 0x12: /* get ega configuration */
		case 0x4f: /* vesa interrupt */
		default:
			printf("unknown video int 0x%x\n", _regs.eax);
			break;
	}
}

void int13(void)
{
	unsigned int disk, head, sect, track, number, pos, res;
	char *buffer;
	struct disk *dp;
	
	disk = LO(dx);
	if (disk < DISKS) {
		dp = &disktab[disk];
	} else if (disk >= 0x80 && disk < 0x80 + HDISKS) 
		dp = &hdisktab[disk - 0x80];
	else dp = NULL;
	switch((_regs.eax >> 8) & 0xff) {
		case 0: /* init */
			printf("DISK init %d\n", disk);
			break;
		case 1: /* read error code */	
			_regs.eax &= ~0xff;
			printf("DISK error code\n");
			break;
		case 2: /* read */
			disk_open(dp);
			head = (_regs.edx >> 8) & 0xff;
			sect = (_regs.ecx & 0x3f) -1;
			track = ((_regs.ecx >> 8) & 0xff) |
				((_regs.ecx & 0xc0) << 2);
			buffer = SEG_ADR((char *), es, bx);
			number = _regs.eax & 0xff;
			printf("DISK %d read [h%d,s%d,t%d](%d)->0x%x\n", disk, head, sect, track, number, buffer - (char *)0);
			if (dp == NULL || head >= dp->heads || 
			    sect >= dp->sectors || track >= dp->tracks) {
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    break;
			}
			pos = ((track * dp->heads + head) * dp->sectors + sect) << 9;
			if (pos != lseek(dp->fdesc, pos, 0)) {
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    break;
			}
			res = read(dp->fdesc, buffer, number << 9);
			if (res & 0x1ff != 0) { /* must read multiple of 512 bytes  and res != -1 */
			    _regs.eax = 0x200; /* sector corrrupt */
			    _regs.eflags |= CF;
			    break;
			}
			_regs.eax = res >> 9;
			_regs.eflags &= ~CF;
			printf("DISK read @%d (%d) OK.\n", pos, res >> 9);
			break;
		case 3: /* write */
			disk_open(dp);
			head = (_regs.edx >> 8) & 0xff;
			sect = (_regs.ecx & 0x3f) -1;
			track = ((_regs.ecx >> 8) & 0xff) |
				((_regs.ecx & 0xc0) << 2);
			buffer = SEG_ADR((char *), es, bx);
			number = _regs.eax & 0xff;
			printf("DISK write [h%d,s%d,t%d](%d)->0x%x\n", head, sect, track, number, buffer - (char *)0);
			if (dp == NULL || head >= dp->heads || 
			    sect >= dp->sectors || track >= dp->tracks) {
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    break;
			}
			if (dp->rdonly) {
			    _regs.eax = 0x300; /* write protect */
			    _regs.eflags |= CF;
			    break;
			}
			pos = ((track * dp->heads + head) * dp->sectors + sect) << 9;
			if (pos != lseek(dp->fdesc, pos, 0)) {
			    _regs.eax = 0x400; /* sector not found */
			    _regs.eflags |= CF;
			    break;
			}
			res = write(dp->fdesc, buffer, number << 9);
			if (res & 0x1ff != 0) { /* must read multiple of 512 bytes  and res != -1 */
			    _regs.eax = 0x200; /* sector corrrupt */
			    _regs.eflags |= CF;
			    break;
			}
			_regs.eax = res >> 9;
			_regs.eflags &= ~CF;
			printf("DISK write @%d (%d) OK.\n", pos, res >> 9);
			break;
		case 8: /* get disk drive parameters */
			printf("disk get parameters %d\n", disk);
			if (dp != NULL) {
				_regs.edx = ((dp->heads-((disk < 0x80)?0:1)) <<8) | 
					   ((disk < 0x80) ? DISKS : HDISKS);
				_regs.ecx = ((dp->tracks & 0xff) <<8) |
					   dp->sectors | ((dp->tracks & 0x300) >> 2);
			} else {
				_regs.edx = 0; /* no hard disks */
				_regs.ecx = 0;
			}	
			_regs.eflags &= ~CF; /* no error */
			break;
		case 0x15:
			printf("disk 0x15 ?????\n");
			break;
		default:
			printf("disk IO error\n");
			show_regs();
			error = 5;
			return;
	}
}

void int14(void)
{
	int num;

	switch((_regs.eax >> 8) & 0xff) {
		case 0: /* init */
			_regs.eax = 0;
			num = _regs.edx & 0xffff;
			printf("init serial %d\n", num);
			break;
		default:
			printf("serial error\n");
			show_regs();
			error = 5;
			return;
	}
}

void int16(void)
{
	int key;
	fd_set fds;

	switch((_regs.eax >> 8) & 0xff) {
		case 0: /* read key code, wait */
			/* printf("get key\n"); */
			for (;;) {
				if (ReadKeyboard(&key, WAIT)) break;
			}
			_regs.eax = key;
			break;
		case 1: /* test key code */
			/* printf("test key\n"); */
			if (ReadKeyboard(&key, TEST)) {
				_regs.eflags &= ~(ZF | CF); /* key pressed */
				_regs.eax = key;
			} else {
				_regs.eflags |= ZF | CF; /* no key */
			}
			break;
		case 2: /* read key state */
			/* printf("get key state not implemented\n"); */
			FD_ZERO(&fds);
			FD_SET(kbd_fd, &fds);
			_regs.eax &= ~0xff;
			select(kbd_fd+1, &fds, NULL, NULL, &scr_tv);
			if (!scr_tv.tv_sec && scr_tv.tv_usec < 100) {
				restore_screen(screen_bitfield);
				scr_tv.tv_sec = 0;
				scr_tv.tv_usec = UPDATE;
			}
			break;
		default:
			printf("keyboard error\n");
			show_regs();
			error = 7;
			return;
	}
}

void int17(void)
{
	int num;

	switch((_regs.eax >> 8) & 0xff) {
		case 1: /* init */
			_regs.eax &= ~0xff00;
			num = _regs.edx & 0xffff;
			printf("init printer %d\n", num);
			break;
		default:
			printf("printer error\n");
			show_regs();
			error = 8;
			return;
	}
}

void int1a(void)
{
	int num;
	static unsigned long last_ticks;
	unsigned long ticks;
	long akt_time, elapsed;

	switch((_regs.eax >> 8) & 0xff) {
		case 0: /* read time counter */
			time(&akt_time);
			elapsed = akt_time - start_time;
			ticks = (elapsed *182) / 10 + last_ticks;
			_regs.eax &= ~0xff; /* 0 hours */
			_regs.ecx = ticks >> 16;
			_regs.edx = ticks & 0xffff;
			/* printf("read timer %ud t=%d\n", ticks, akt_time); */
			break;
		case 1: /* write time counter */
			last_ticks = (_regs.ecx << 16) | (_regs.edx & 0xffff);
			time(&start_time);
			printf("set timer to %ud \n", last_ticks);
			break;
		case 2:
			printf("int 1a, 2 ?????\n");
			break;
		default:
			printf("timer error\n");
			show_regs();
			error = 9;
			return;
	}
}

int ms_dos(int nr) /* returns 1 if emulated, 0 if internal handling */
{
	char ch, *csp, *cp, *dp, *p, buf[80]; 
	int c, m, fd;
	long ofs;
	static char dosname[11];
	static int dosmode;
	struct stat stbuf;

Restart:
	/* printf("DOSINT 0x%x\n", nr); */
	/* emulate keyboard io to avoid DOS' busy wait */
	switch(nr) {
		case 2: /* char out */
			char_out(LO(dx), screen);
			break;
		case 1: /* read and echo char */
		case 8: /* read char */
		case 7: /* read char, do not check <ctrl> C */
			disk_close();
			while (ReadKeyboard(&c, WAIT) != 1);
			*(char *)&_regs.eax = c;
			if (nr == 1) char_out(c, screen);
			break;
		case 9: /* str out */
			csp = SEG_ADR((char *), ds, dx);
			for (p = csp; *p != '$';) char_out(*p++, screen);
			break;
		case 10: /* read string */
			disk_close();
			csp = SEG_ADR((char *), ds, dx);
			ReadString(((unsigned char *)csp)[0], csp +1);
			break;
		case 12: /* clear key buffer, do int AL */
			while (ReadKeyboard(&c, NOWAIT) == 1);
			nr = _regs.eax & 0xff;
			_regs.eax = (nr << 8) | nr;
			goto Restart;
		case 0xfa: /* unused by DOS */
			if ((_regs.ebx & 0xffff) == 0x1234) { /* MAGIC */
				_regs.eax = ext_fs(_regs.eax & 0xff, SEG_ADR((char *), fs, di), 
						SEG_ADR((char *), gs, si), _regs.ecx & 0xffff); 
				printf("RESULT %d\n", _regs.eax);
				break;
			} else
				return 0;
		default:
			/* printf(" dos interrupt 0x%x \n", nr); */
			return 0;
	}
	return 1;
}

void do_int(int i)
{
us *ssp;

	switch(i) {
		case 0x10 : /* VIDEO */
			int10();
			return;
		case 0x11 : /* CONFIGURATION */
			_regs.eax = CONFIGURATION;
			if (DISKS > 0) _regs.eax |= 1 | ((DISKS -1)<<6);
			printf("configuration read\n");
			return;
		case 0x12 : /* MEMORY */
			_regs.eax = 640;
			printf("memory tested\n");
			return;
		case 0x13 : /* DISK IO */
			int13();
			return;
		case 0x14 : /* COM IO */
			int14();
			return;
		case 0x15 : /* Cassette */
			printf(" cassette %d ???????????\n", (_regs.eax >>8) & 0xff);
			return;
		case 0x16 : /* KEYBOARD */
			int16();
			return;
		case 0x17 : /* PRINTER */
			int17();
			return;
		case 0x18 : /* BASIC */
			break;
		case 0x19 : /* LOAD SYSTEM */
			boot(hdiskboot? hdisktab : disktab);
			return;
		case 0x1a : /* CLOCK */
			int1a();
			return;
#ifdef 0
		case 0x1b : /* BREAK */
		case 0x1c : /* TIMER */
		case 0x1d : /* SCREEN INIT */
		case 0x1e : /* DEF DISK PARMS */
		case 0x1f : /* DEF GRAPHIC */
		case 0x20 : /* EXIT */
		case 0x27 : /* TSR */
#endif
		case 0x21 : /* MS-DOS */
			if (ms_dos((_regs.eax>>8) & 0xff)) return;
			/* else do default handling in vm86 mode */
			goto default_handling;

			/* else do default handling in vm86 mode */
		default :
default_handling:
			/* if (i != 0x21) printf("interrupt 0x%x default\n", i); */
			ssp = SEG_ADR((us *), ss, sp);
			*--ssp = _regs.eflags;
			*--ssp = _regs.cs;
			*--ssp = _regs.eip;
			_regs.esp -= 6;
			_regs.cs =  ((us *)0)[ (i<<1) +1];
			_regs.eip = ((us *)0)[  i<<1    ];
			_regs.eflags &= 0xfffffcff;
			return;
	}
	printf("\nint 0x%x not implemented\n", i);
	show_regs();
	error = 1;
	return;
}

void sigalrm(int sig)
{
	printf("SIGALRM received!!!!!\n");
	restore_screen(-1);
	scr_tv.tv_sec = 0;
	scr_tv.tv_usec = 500000;
	alarm(1);
}

void sigsegv(int sig)
{
	short d;
	us *ssp;
	unsigned char *csp;

	/* printf("SIGSEGV %d received\n", sig);
	show_regs(); */
	csp = SEG_ADR((unsigned char *), cs, ip);
	switch (*csp) {
		case 0xcd: /* int xx */
			_regs.eip += 2;
			do_int((int)*++csp);
			break;
		case 0xcc: /* int 3 */
			_regs.eip += 1;
			do_int(3);
			break;
		case 0xcf: /* iret */
			ssp = SEG_ADR((us *), ss, sp);
			_regs.eip = *ssp++;
			_regs.cs = *ssp++;
			_regs.eflags = (_regs.eflags & 0xffff0000) | *ssp++;
			_regs.esp += 6;
			break;
		case 0xe5: /* inw xx */
			_regs.eax &= ~0xff00;
			_regs.eax |= inb((int)csp[1] +1) << 8;
		case 0xe4: /* inb xx */
			_regs.eax &= ~0xff;
			_regs.eax |= inb((int)csp[1]);
			_regs.eip += 2;
			break;
		case 0xed: /* inw dx */
			_regs.eax &= ~0xff00;
			_regs.eax |= inb(_regs.edx +1) << 8;
		case 0xec: /* inb dx */
			_regs.eax &= ~0xff;
			_regs.eax |= inb(_regs.edx);
			_regs.eip += 1;
			break;
		case 0xe7: /* outw xx */
			outb((int)csp[1] +1, _regs.eax >>8);
		case 0xe6: /* outb xx */
			outb((int)csp[1], _regs.eax);
			_regs.eip += 2;
			break;
		case 0xef: /* outw dx */
			outb(_regs.edx +1, _regs.eax >>8);
		case 0xee: /* outb dx */
			outb(_regs.edx, _regs.eax);
			_regs.eip += 1;
			break;
		case 0xfa: /* cli */
			iflag = 0;
			_regs.eip += 1;
			break;
		case 0xfb: /* sti */
			iflag = 1;
			_regs.eip += 1;
			break;
		case 0x9c: /* pushf */
			_regs.eflags &= ~IF;
			if (iflag) _regs.eflags |= IF;
			ssp = SEG_ADR((us *), ss, sp);
			*--ssp = (us)_regs.eflags;
			_regs.esp -= 2;
			_regs.eip += 1;
			break;
		case 0x9d: /* popf */
			ssp = SEG_ADR((us *), ss, sp);
			_regs.eflags &= ~0xffff;
			_regs.eflags |= (int)*ssp;
			_regs.esp += 2;
			_regs.eip += 1;
			break;
		case 0xf0: /* lock */
		default:
			printf("general protection\n");
			show_regs();
			error = 4;
	}	
}

void sigill(int sig)
{
unsigned char *csp;
int i, d;

	printf("SIGILL %d received\n", sig);
	show_regs();
	csp = SEG_ADR((unsigned char *), cs, ip);
	i = (csp[0] << 8) + csp[1]; /* swapped */
	if ((i & 0xf800) != 0xd800) { /* no fpu instruction */
		error = 4;
		return;
	}
	switch(i & 0xc0) {
		case 0x00:
			if ((i & 0x7) == 0x6) {
				d = *(short *)(csp +2);
				_regs.eip += 4;
			} else {
				_regs.eip += 2;
				d = 0;
			}
			break;
		case 0x40:
			d = (signed)csp[2];
			_regs.eip += 3;
			break;
		case 0x80:
			d = *(short *)(csp +2);
			_regs.eip += 4;
			break;
		default:
			_regs.eip += 2;
			d = 0;
	}
	printf("math emulation %x d=%x\n", i, d);
}

void sigfpe(int sig)
{
	printf("SIGFPE %d received\n", sig);
	show_regs();
	error = 4;
}

void sigtrap(int sig)
{
	printf("SIGTRAP %d received\n", sig);
	show_regs();
	error = 4;
}

#define SETSIG(sig, fun)	sa.sa_handler = fun; \
				sa.sa_flags = 0; \
				sa.sa_mask = 0; \
				sigaction(sig, &sa, NULL);
void startemu()
{
	struct sigaction sa;

	SETSIG(SIGSEGV, sigsegv);
	SETSIG(SIGILL, sigill);
	SETSIG(SIGALRM, sigalrm);
	SETSIG(SIGFPE, sigfpe);
	SETSIG(SIGTRAP, sigtrap);
	time(&start_time);
	disk_init();
	if (OpenKeyboard() != 0) {
		printf("cann't open keyboard\n");
		exit(1);
	}
	gettermcap();
	clear_screen(screen, 7);
	boot(hdiskboot? hdisktab : disktab);
	scr_tv.tv_sec = 0;
	scr_tv.tv_usec = 500000;
	fflush(stdout);
	if (do_screen_updates) alarm(1);
	for(;!error;) {
		(void)vm86(&vm86s);
	}
	CloseKeyboard();
	disk_close_all();
	_exit(error);
}
