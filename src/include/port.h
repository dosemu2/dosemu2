/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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

static __inline__ void port_real_outb(ioport_t port, Bit8u value)
{
  if (!can_do_root_stuff) return;
  __asm__ __volatile__ ("outb %0,%1"
		    ::"a" ((Bit8u) value), "d"((Bit16u) port));
}

static __inline__ Bit8u port_real_inb(ioport_t port)
{
  Bit8u _v;
  if (!can_do_root_stuff) return 0xff;
  __asm__ __volatile__ ("inb %1,%0"
		    :"=a" (_v):"d"((Bit16u) port));
  return _v;
}

static __inline__ void port_real_outw(ioport_t port, Bit16u value)
{
  if (!can_do_root_stuff) return;
  __asm__ __volatile__ ("outw %0,%1" :: "a" ((Bit16u) value),
		"d" ((Bit16u) port));
}

static __inline__ Bit16u port_real_inw(ioport_t port)
{
  Bit16u _v;
  if (!can_do_root_stuff) return 0xffff;
  __asm__ __volatile__ ("inw %1,%0":"=a" (_v) : "d" ((Bit16u) port));
  return _v;
}

static __inline__ void port_real_outd(ioport_t port, Bit32u value)
{
  if (!can_do_root_stuff) return;
  __asm__ __volatile__ ("outl %0,%1" : : "a" (value),
		"d" ((Bit16u) port));
}

static __inline__ Bit32u port_real_ind(ioport_t port)
{
  Bit32u _v;
  if (!can_do_root_stuff) return 0xffffffff;
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

extern int port_rep_inb(ioport_t port, Bit8u *dest, int df, Bit32u count);
extern int port_rep_outb(ioport_t port, Bit8u *dest, int df, Bit32u count);
extern int port_rep_inw(ioport_t port, Bit16u *dest, int df, Bit32u count);
extern int port_rep_outw(ioport_t port, Bit16u *dest, int df, Bit32u count);
extern int port_rep_ind(ioport_t port, Bit32u *dest, int df, Bit32u count);
extern int port_rep_outd(ioport_t port, Bit32u *dest, int df, Bit32u count);

#if GLIBC_VERSION_CODE >= 2000
  /* arrgh... we need this because <sys/io.h> in glibc includes asm/io.h :-( */
  #undef inb			port_inb
  #undef inw			port_inw
  #undef ind			port_ind
  #undef outb			port_outb
  #undef outw			port_outw
  #undef outd			port_outd
#endif
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

#define PORT_FAST	1
#define PORT_DEV_RD	2
#define PORT_DEV_WR	4


extern int     port_init(void);
extern int     port_register_handler(emu_iodev_t info, int);
extern Boolean port_allow_io(ioport_t, Bit16u, int, Bit8u, Bit8u, unsigned int,
	char *);
extern int     set_ioperm(int start, int size, int flag);

#endif /* _PORT_H */
