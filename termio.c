#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <termio.h>
#include <sys/time.h>
#include <termcap.h>
#include "emu.h"

#define us unsigned short
#define	KEYBOARD	"/dev/tty"

int kbd_fd;
int akt_keycode, kbcount;
char kbbuf[50], *kbp, akt_key, erasekey;
static  struct termio   oldtermio;      /* original terminal modes */

char tc[1024], termcap[1024],
     *cl,	/* clear screen */
     *le,	/* cursor left */
     *cm,	/* goto */
     *ce,	/* clear to end */
     *sr,	/* scroll reverse */
     *so,	/* stand out start */
     *se,	/* stand out end */
     *md,	/* hilighted */
     *mr,	/* reverse */
     *me,	/* normal */
     *ti,	/* terminal init */
     *te,	/* terminal exit */
     *ks,	/* init keys */
     *ke,	/* ens keys */
     *tp;
int   li, co;   /* lines, columns */     

struct funkeystruct {
	char *esc;
	char *tce;
	us code;
};


#define FUNKEYS 20
static struct funkeystruct funkey[FUNKEYS] = {
	{NULL, "kI", 0x5200}, /* Ins */
	{NULL, "kD", 127},    /* Del */
	{NULL, "kh", 0x5c00}, /* Ho */
	{NULL, "kH", 0x6100}, /* End */
	{NULL, "ku", 0x4800}, /* Up */
	{NULL, "kd", 0x5000}, /* Dn */
	{NULL, "kr", 0x4d00}, /* Ri */
	{NULL, "kl", 0x4b00}, /* Le */
	{NULL, "kP", 0x4900}, /* PgUp */
	{NULL, "kN", 0x5100}, /* PgDn */
	{NULL, "k1", 0x3b00}, /* F1 */
	{NULL, "k2", 0x3c00}, /* F2 */
	{NULL, "k3", 0x3d00}, /* F3 */
	{NULL, "k4", 0x3e00}, /* F4 */
	{NULL, "k5", 0x3f00}, /* F5 */
	{NULL, "k6", 0x4000}, /* F6 */
	{NULL, "k7", 0x4100}, /* F7 */
	{NULL, "k8", 0x4200}, /* F8 */
	{NULL, "k9", 0x4300}, /* F9 */
	{NULL, "k0", 0x4400}, /* F10 */
};

int outc(int c)
{
  write(2, (char *)&c, 1);
  return 1;
}

static void gettermcap(void)
{
struct winsize ws;		/* buffer for TIOCSWINSZ */
struct funkeystruct *fkp;

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
  ti = tgetstr("ti", &tp);
  te = tgetstr("te", &tp);
  ks = tgetstr("ks", &tp);
  ke = tgetstr("ke", &tp);
  mr = tgetstr("mr", &tp);
  md = tgetstr("md", &tp);
  me = tgetstr("me", &tp);
  if (se == NULL) so = NULL;
  if (md == NULL || mr == NULL) me = NULL;
  if (li == 0 || co == 0) {
    printf("unknown window sizes \n");
    exit(1);
  }
  for (fkp=funkey; fkp < &funkey[FUNKEYS]; fkp++) {
	fkp->esc = tgetstr(fkp->tce, &tp);
	if (!fkp->esc) printf("cann't get termcap %s\n", fkp->tce);
  }
}

static void CloseKeyboard(void)
{
	ioctl(kbd_fd, TCSETAF, &oldtermio);
	close(kbd_fd);
	kbd_fd = -1;
}

static int OpenKeyboard(void)
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

static us alt_keys[] = { /* <ALT>-A ... <ALT>-Z */
	0x1e00, 0x3000, 0x2e00, 0x2000, 0x1200, 0x2100,
	0x2200, 0x2300, 0x1700, 0x2400, 0x2500, 0x2600,
	0x3200, 0x3100, 0x1800, 0x1900, 0x1000, 0x1300,
	0x1f00, 0x1400, 0x1600, 0x2f00, 0x1100, 0x2d00,
	0x2c00, 0x1500};

static us alt_nums[] = { /* <ALT>-0 ... <ALT>-9 */
	0x8100, 0x7800, 0x7900, 0x7a00, 0x7b00, 0x7c00,
	0x7d00, 0x7e00, 0x7f00, 0x8000};

static void getKeys(void)
{
        int     cc;

        if (kbcount == 0) {
                kbp = kbbuf;
        } else if (kbp > &kbbuf[30]) {
                memmove(kbbuf, kbp, kbcount);
                kbp = kbbuf;
        }
        cc = read(kbd_fd, &kbp[kbcount], &kbbuf[50] - kbp);
        if (cc > 0) {
                kbcount += cc;
        }
}


static int convKey()
{
	int i;
	struct timeval scr_tv;
	struct funkeystruct *fkp;
	fd_set fds;

	if (kbcount == 0) return 0;
	if (*kbp == '\033') {
		if (kbcount == 1) {
			scr_tv.tv_sec = 0;
			scr_tv.tv_usec = 500000;
			FD_ZERO(&fds);
			FD_SET(kbd_fd, &fds);
			select(kbd_fd+1, &fds, NULL, NULL, &scr_tv);
			getKeys();
			if (kbcount == 1) {
				kbcount--;
				return (unsigned char)*kbp++;
			}
		}
#ifdef LATIN1
		if (islower((unsigned char)kbp[1])) {
			kbcount -= 2;
			kbp++;
			return alt_keys[*kbp++ - 'a'];
		} else if (isdigit((unsigned char)kbp[1])) {
			kbcount -= 2;
			kbp++;
			return alt_nums[*kbp++ - '0'];
		}
#endif
		fkp = funkey;
		for (i=1;;) {
			if (fkp->esc == NULL || 
			    fkp->esc[i] < kbp[i]) {
				if (++fkp >= &funkey[FUNKEYS])
					break;
			} else if (fkp->esc[i] == kbp[i]) {
				if (fkp->esc[++i] == '\0') {
					kbcount -= i;
					kbp += i;
					return fkp->code;
				}
				if (kbcount <= i) {
					scr_tv.tv_sec = 0;
					scr_tv.tv_usec = 800000;
					FD_ZERO(&fds);
					FD_SET(kbd_fd, &fds);
					select(kbd_fd+1, &fds, NULL, NULL, &scr_tv);
					getKeys();
					if (kbcount <= i) {
						break;
					}
				}
			} else {
				break;
			}
		}
	} else if (*kbp == erasekey) {
		kbcount--;
		kbp++;
		return 8;
#ifndef LATIN1
	} else if ((unsigned char)*kbp >= ('a'|0x80) && 
		   (unsigned char)*kbp <= ('z'|0x80)) {
		kbcount--;
		return alt_keys[(unsigned char)*kbp++ - ('a'|0x80)];
	} else if ((unsigned char)*kbp >= ('0'|0x80) && 
		   (unsigned char)*kbp <= ('9'|0x80)) {
		kbcount--;
		return alt_nums[(unsigned char)*kbp++ - ('0'|0x80)];
#endif
	}
	kbcount--;
	return (unsigned char)*kbp++;
}

/* ReadKeyboard
   returns 1 if a character could be read in buf 
   */
int ReadKeyboard(int *buf, int wait)
{
	fd_set fds;
	int r;
	static int aktkey;

	while (!aktkey) {
		if (kbcount == 0 && wait == WAIT) {
			FD_ZERO(&fds);
			FD_SET(kbd_fd, &fds);
			r = select(kbd_fd+1, &fds, NULL, NULL, NULL);
		}
		getKeys();
		if (kbcount == 0 && wait != WAIT) return 0;
		aktkey = convKey();
	}
	*buf = aktkey;
	if (wait != TEST) aktkey = 0;
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

static int fkcmp(const void *a, const void *b)
{
	return strcmp(((struct funkeystruct *)a)->esc, ((struct funkeystruct *)b)->esc);
}

void termioInit()
{
	if (OpenKeyboard() != 0) {
		printf("cann't open keyboard\n");
		exit(1);
	}
	gettermcap();
	qsort(funkey, FUNKEYS, sizeof(struct funkeystruct), &fkcmp);
	if (ks) tputs(ks, 1, outc);
}

void termioClose()
{
	CloseKeyboard();
	if (ke) tputs(ke, 1, outc);
}

