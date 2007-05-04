/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * include/port.h - contains INLINE-Functions for port access
 * 
 */

#ifndef _PORT_H
#define _PORT_H

#include "config.h"
#include "types.h"
#include "priv.h"

/* port i/o privileges */
#define IO_READ  1
#define IO_WRITE 2
#define IO_RDWR	 (IO_READ | IO_WRITE)


/*
 * number of IRQ lines supported.  In an ISA PC there are two
 * PIC chips cascaded together.  each has 8 IRQ lines, so there
 * should be 16 IRQ's total
 */
#define EMU_MAX_IRQS 16

#define EMU_NO_IRQ   0xffff

/* Hey, stranger, there is one too many of us in this town! */
typedef struct {
  Bit8u         (* read_portb)(ioport_t port);
  void          (* write_portb)(ioport_t port, Bit8u byte);
  Bit16u        (* read_portw)(ioport_t port);
  void          (* write_portw)(ioport_t port, Bit16u word);
  Bit32u        (* read_portd)(ioport_t port);
  void          (* write_portd)(ioport_t port, Bit32u word);
  char          *handler_name;
  ioport_t      start_addr;
  ioport_t      end_addr;
  int           irq, fd;
} emu_iodev_t;

typedef struct {
  Bit8u  (*read_portb)  (ioport_t port_addr);
  void   (*write_portb) (ioport_t port_addr, Bit8u byte);
  Bit16u (*read_portw) (ioport_t port_addr);
  void   (*write_portw) (ioport_t port_addr, Bit16u word);
  Bit32u (*read_portd) (ioport_t port_addr);
  void   (*write_portd) (ioport_t port_addr, Bit32u dword);
  char   *handler_name;
  int    irq, fd;
} _port_handler;

extern int in_crit_section;

static __inline__ void port_real_outb(ioport_t port, Bit8u value)
{
  __asm__ __volatile__ ("outb %0,%1"
		    ::"a" ((Bit8u) value), "d"((Bit16u) port));
}

static __inline__ Bit8u port_real_inb(ioport_t port)
{
  Bit8u _v;
  __asm__ __volatile__ ("inb %1,%0"
		    :"=a" (_v):"d"((Bit16u) port));
  return _v;
}

static __inline__ void port_real_outw(ioport_t port, Bit16u value)
{
  __asm__ __volatile__ ("outw %0,%1" :: "a" ((Bit16u) value),
		"d" ((Bit16u) port));
}

static __inline__ Bit16u port_real_inw(ioport_t port)
{
  Bit16u _v;
  __asm__ __volatile__ ("inw %1,%0":"=a" (_v) : "d" ((Bit16u) port));
  return _v;
}

static __inline__ void port_real_outd(ioport_t port, Bit32u value)
{
  __asm__ __volatile__ ("outl %0,%1" : : "a" (value),
		"d" ((Bit16u) port));
}

static __inline__ Bit32u port_real_ind(ioport_t port)
{
  Bit32u _v;
  __asm__ __volatile__ ("inl %1,%0":"=a" (_v) : "d" ((Bit16u) port));
  return _v;
}


/* These are for compatibility */
#define port_in(p)	port_real_inb((p))
#define port_out(v,p)	port_real_outb((p),(v))
#define port_in_w(p)	port_real_inw((p))
#define port_out_w(v,p)	port_real_outw((p),(v))
#define port_in_d(p)	port_real_inw((p))
#define port_out_d(v,p)	port_real_outw((p),(v))


extern Bit8u   port_inb(ioport_t port);
extern Bit16u  port_inw(ioport_t port);
extern Bit32u  port_ind(ioport_t port);
extern void    port_outb(ioport_t port, Bit8u byte);
extern void    port_outw(ioport_t port, Bit16u word);
extern void    port_outd(ioport_t port, Bit32u word);

extern Bit8u  std_port_inb(ioport_t port);
extern void   std_port_outb(ioport_t port, Bit8u byte);
extern Bit16u std_port_inw(ioport_t port);
extern void   std_port_outw(ioport_t port, Bit16u word);
extern Bit32u std_port_ind(ioport_t port);
extern void   std_port_outd(ioport_t port, Bit32u word);
extern void   pci_port_outd(ioport_t port, Bit32u word);

extern int port_rep_inb(ioport_t port, Bit8u *dest, int df, Bit32u count);
extern int port_rep_outb(ioport_t port, Bit8u *dest, int df, Bit32u count);
extern int port_rep_inw(ioport_t port, Bit16u *dest, int df, Bit32u count);
extern int port_rep_outw(ioport_t port, Bit16u *dest, int df, Bit32u count);
extern int port_rep_ind(ioport_t port, Bit32u *dest, int df, Bit32u count);
extern int port_rep_outd(ioport_t port, Bit32u *dest, int df, Bit32u count);

extern void port_exit(void);

/* avoid potential clashes with <sys/io.h> */
#undef inb
#undef inw
#undef ind
#undef outb
#undef outw
#undef outd
#define inb			port_inb
#define inw			port_inw
#define ind			port_ind
#define outb			port_outb
#define outw			port_outw
#define outd			port_outd
#define safe_port_in_byte	std_port_inb
#define safe_port_out_byte	std_port_outb
#define port_safe_inb		std_port_inb
#define port_safe_outb		std_port_outb
#define port_safe_inw		std_port_inw
#define port_safe_outw		std_port_outw

extern int extra_port_init(void);
extern void release_ports(void);

/*
 * maximum number of emulated devices allowed.  floppy, mda, etc...
 * an 8-bit handle is used for each device:
 */
#define EMU_MAX_IO_DEVICES 254

#define NO_HANDLE	0x00
#define HANDLE_STD_IO	0x01
#define HANDLE_STD_RD	0x02
#define HANDLE_STD_WR	0x03
#define HANDLE_VID_IO	0x04
#define HANDLE_SPECIAL	0x05		/* catch-all */
#define STD_HANDLES	6

extern _port_handler port_handler[];
/*
 * Codes:
 *	00    = no device attached, port undefined
 *	01-FD = index into handle
 *	FE,FF = reserved
 */
extern unsigned char port_handle_table[];
extern unsigned char port_andmask[];
extern unsigned char port_ormask[];

extern pid_t portserver_pid;

#define PORT_FAST	1
#define PORT_DEV_RD	2
#define PORT_DEV_WR	4
#define PORT_FORCE_FAST	8


extern int     port_init(void);
extern int     port_register_handler(emu_iodev_t info, int);
extern Boolean port_allow_io(ioport_t, Bit16u, int, Bit8u, Bit8u, int, char *);
extern int     set_ioperm(int start, int size, int flag);

extern void init_port_traceing(void);
extern void register_port_traceing(ioport_t firstport, ioport_t lastport);
extern void clear_port_traceing(void);

extern void do_r3da_pending (void);

void port_enter_critical_section(const char *caller);
void port_leave_critical_section(void);

#endif /* _PORT_H */
