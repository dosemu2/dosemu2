/* dos emulator, Matthias Lautner */

#include <linux/vm86.h>

#define _regs	vm86s.regs

#define us unsigned short
#define SEG_ADR(type, seg, reg) type((int)(*(us *)&_regs.seg<<4)\
				 + *(us *)&_regs.e##reg)
#define LO(reg)		(int)(*(unsigned char *)&_regs.e##reg)
#define HI(reg)		(int)(*((unsigned char *)&_regs.e##reg +1))
#define CF 0x01
#define ZF 0x40
#define SF 0x80
#define TF 0x100
#define IF 0x200

extern struct vm86_struct vm86s;
extern int error, screen;


extern char *cl,	/* clear screen */
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
	    *ke;	/* ens keys */
extern int  li, co;	/* lines, columns */     
extern int kbd_fd;

void show_regs(void);
int ext_fs(int, char *, char *, int);
void char_out(unsigned char, int);
int outc(int c);
void termioInit();
void termioClose();
#define NOWAIT  0
#define WAIT    1
#define TEST    2
int ReadKeyboard(int *, int);
void ReadString(int, char *);

