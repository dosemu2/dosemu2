/* dos emulator, Matthias Lautner */
#ifndef EMU_H
#define EMU_H
/* Extensions by Robert Sanders, 1992-93
 *
 * $Date: 1993/02/13 23:37:20 $
 * $Source: /usr/src/dos/RCS/emu.h,v $
 * $Revision: 1.8 $
 * $State: Exp $
 *
 * $Log: emu.h,v $
 * Revision 1.8  1993/02/13  23:37:20  root
 * latest version, no time to document!
 *
 * Revision 1.7  1993/02/10  20:56:45  root
 * for the circa-Wp dosemu
 *
 * Revision 1.6  1993/02/08  04:17:56  root
 * dosemu 0.47.7
 *
 * Revision 1.5  1993/02/05  02:54:24  root
 * this is for 0.47.6
 *
 * Revision 1.4  1993/02/04  01:16:57  root
 * version 0.47.5
 *
 * Revision 1.3  1993/01/28  02:19:59  root
 * for emu.c 1.12
 * THIS IS THE DOSEMU47 DISTRIBUTION EMU.H
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
#define VM 0x20000

extern struct vm86_struct vm86s;
extern int error, screen;

#ifndef TERMIO_C
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

extern int kbd_fd, mem_fd;
extern int in_readkeyboard;

extern int  li, co, li2, co2;	/* lines, columns */     
extern int lastscan;
#endif

void dos_ctrlc(void), dos_ctrl_alt_del(void);
void show_regs(void);
int ext_fs(int, char *, char *, int);
void char_out(unsigned char, int);
int outc(int c);
void termioInit();
void termioClose();
inline void run_vm86(void);

#define NOWAIT  0
#define WAIT    1
#define TEST    2
#define POLL    3

int ReadKeyboard(unsigned int *, int);
int InsKeyboard(unsigned short scancode);
int PollKeyboard(void);
void ReadString(int, unsigned char *);

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

struct debug_flags
{
  unsigned char
    disk,	/* disk msgs, "d" */
    read,	/* disk read "R" */
    write,	/* disk write "W" */
    dos,	/* unparsed int 21h, "D" */
    video,	/* video, "v" */
    keyb,	/* keyboard, "k" */
    debug,
    io,		/* port I/O, "i" */
    serial,	/* serial, "s" */
    defint,	/* default ints */
    printer,
    general,
    warning,
    all,	/* all non-classifiable messages */
    hardware,
    xms;
};

int ifprintf(unsigned char,const char *, ...),
    p_dos_str(char *, ...);

#define dbug_printf(f,a...)	ifprintf(1,f,##a)
#define k_printf(f,a...) 	ifprintf(d.keyb,f,##a)
#define h_printf(f,a...) 	ifprintf(d.hardware,f,##a)
#define v_printf(f,a...) 	ifprintf(d.video,f,##a)
#define s_printf(f,a...) 	ifprintf(d.serial,f,##a)
#define p_printf(f,a...) 	ifprintf(d.printer,f,##a)
#define d_printf(f,a...) 	ifprintf(d.disk,f,##a)
#define i_printf(f,a...) 	ifprintf(d.io,f,##a)
#define R_printf(f,a...) 	ifprintf(d.read,f,##a)
#define W_printf(f,a...) 	ifprintf(d.write,f,##a)
#define warn(f,a...)     	ifprintf(d.warning,f,##a)
#define g_printf(f,a...)	ifprintf(d.general,f,##a)
#define x_printf(f,a...)	ifprintf(d.xms,f,##a)
#define e_printf(f,a...) 	ifprintf(1,f,##a)
#define error(f,a...)	 	ifprintf(1,f,##a)

#define IOFF(i) ivecs[i].offset
#define ISEG(i) ivecs[i].segment
#define IVEC(i) ((ISEG(i)<<4) + IOFF(i))

#define WORD(i) (i&0xffff)

#define CARRY	_regs.eflags|=CF;
#define NOCARRY _regs.eflags&=~CF;

#define char_out(c,s)   char_out_att(c,7,s)

struct disk {
	char *dev_name;			/* disk file */
	int rdonly;			/* readonly flag */
	int sectors, heads, tracks;	/* geometry */
	int default_cmos;		/* default CMOS floppy type */
       	int fdesc;			/* not user settable */
	int removeable;			/* not user settable */
	};

/* CMOS types for the floppies */
#define THREE_INCH_FLOPPY   4		/* 3.5 in, 1.44 MB floppy */
#define FIVE_INCH_FLOPPY    2		/* 5.25 in, 1.2 MB floppy */
#define DISKS 3         /* size of disktab[] structure */
#define HDISKS 2	/* size of hdisktab[] structure */

struct ioctlq
{
  int fd,
      req,
      param3;
  int queued;
};

void do_queued_ioctl(void);
int queue_ioctl(int, int, int),
    do_ioctl(int, int, int);
void keybuf_clear(void);

#ifndef EMU_C
extern struct debug_flags d;
extern int gfx_mode;  /* flag for in gxf mode or not */
extern int in_sighandler, in_ioctl;  
extern struct ioctlq iq, curi;
#endif

static char RCSid[]="$Header: /usr/src/dos/RCS/emu.h,v 1.8 1993/02/13 23:37:20 root Exp $";
#endif /* EMU_H */
