/* dos emulator, Matthias Lautner */

/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/01/28 02:19:59 $
 * $Source: /usr/src/dos/RCS/emu.h,v $
 * $Revision: 1.3 $
 * $State: Exp $
 *
 * $Log: emu.h,v $
 * Revision 1.3  1993/01/28  02:19:59  root
 * for emu.c 1.12
 * THIS IS THE DOSEMU47 DISTRIBUTION EMU.H
 *
 * Revision 1.2  1993/01/12  01:27:11  root
 * just added log RCS var.
 *
 */
 

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
	    *ke,	/* ens keys */
  	    *vi,	/* cursor invisible */
  	    *ve;	/* cursor normal */
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

int ReadKeyboard(unsigned int *, int);
void ReadString(int, char *);

static void inline port_out(char value, unsigned short port)
{
__asm__ volatile ("outb %0,%1"
		::"a" ((char) value),"d" ((unsigned short) port));
}

static char inline port_in(unsigned short port)
{
	char _v;
__asm__ volatile ("inb %1,%0"
		:"=a" (_v):"d" ((unsigned short) port));
	return _v;
}

static char RCSid[]="$Header: /usr/src/dos/RCS/emu.h,v 1.3 1993/01/28 02:19:59 root Exp $";

