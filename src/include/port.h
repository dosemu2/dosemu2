/*
 * include/port.h - contains INLINE-Functions for port access
 * 
 */

#ifndef _PORT_H
#define _PORT_H

#include "types.h"

/* port i/o privileges */
#define IO_READ  1
#define IO_WRITE 2
#define IO_RDWR	 (IO_READ | IO_WRITE)

/*
 * maximum number of emulated devices allowed.  floppy, mda, etc...
 * you can increase this to anything below 256 since an 8-bit handle
 * is used for each device
 */
#define EMU_MAX_IO_DEVICES 0x20

/*
 * number of IRQ lines supported.  In an ISA PC there are two
 * PIC chips cascaded together.  each has 8 IRQ lines, so there
 * should be 16 IRQ's total
 */
#define EMU_MAX_IRQS 16

#define EMU_NO_IRQ   0xffff

typedef struct {
  Bit8u         (* read_portb)(Bit32u port);
  void          (* write_portb)(Bit32u port, Bit8u byte);
  Bit16u        (* read_portw)(Bit32u port);
  void          (* write_portw)(Bit32u port, Bit16u word);
  Bit32u        (* read_portd)(Bit32u port);
  void          (* write_portd)(Bit32u port, Bit32u word);
  const char    *handler_name;
  Bit32u        start_addr;
  Bit32u        end_addr;
  int           irq;
  } emu_iodev_t;

static __inline__ void port_real_outb(Bit16u port, Bit8u value)
{
  __asm__ volatile ("outb %0,%1"
		    ::"a" ((Bit8u) value), "d"((Bit16u) port));
}

static __inline__ Bit8u port_real_inb(Bit16u port)
{
  Bit8u _v;
  __asm__ volatile ("inb %1,%0"
		    :"=a" (_v):"d"((Bit16u) port));
  return _v;
}

static __inline__ void port_real_outw(Bit16u port, Bit16u value)
{
  __asm__("outw %0,%1" :: "a" ((Bit16u) value),
		"d" ((Bit16u) port));
}

static __inline__ Bit16u port_real_inw(Bit16u port)
{
  Bit16u _v;
  __asm__("inw %1,%0":"=a" (_v) : "d" ((Bit16u) port));
  return _v;
}

static __inline__ void port_real_outd(Bit16u port, Bit32u value)
{
  __asm__ __volatile__ ("outl %0,%1" : : "a" (value),
		"d" (port));
}

static __inline__ Bit32u port_real_ind(Bit16u port)
{
  unsigned int _v;
  __asm__ __volatile__ ("inl %1,%0":"=a" (_v) : "d" (port));
  return _v;
}


static __inline__ void port_out(Bit8u value, Bit32u port)
{
  __asm__ volatile ("outb %0,%1"
		    ::"a" ((char) value), "d"((Bit16u) port));
}

static __inline__ Bit8u port_in(Bit32u port)
{
  Bit8u _v;
  __asm__ volatile ("inb %1,%0"
		    :"=a" (_v):"d"((Bit16u) port));
  return _v;
}

static __inline__ void port_out_w(Bit16u value, Bit32u port)
{
  __asm__("outw %0,%1" :: "a" ((Bit16u) value),
		"d" ((Bit16u) port));
}

static __inline__ Bit16u port_in_w(Bit32u port)
{
  Bit16u _v;
  __asm__("inw %1,%0":"=a" (_v) : "d" ((Bit16u) port));
  return _v;
}

extern Bit8u   port_inb(Bit32u port);
extern Bit16u  port_inw(Bit32u port);
extern Bit32u  port_ind(Bit32u port);
extern void    port_outb(Bit32u port, Bit8u byte);
extern void    port_outw(Bit32u port, Bit16u word);
extern void    port_outd(Bit32u port, Bit32u word);

extern Bit8u   port_safe_inb(Bit32u port);
extern void    port_safe_outb(Bit32u port, Bit8u byte);
extern Bit16u  port_safe_inw(Bit32u port);
extern void    port_safe_outw(Bit32u port, Bit16u word);

extern void    port_init(void);
extern void    port_register_handler(emu_iodev_t info);
extern Boolean port_allow_io(Bit16u, Bit16u, int, Bit8u, Bit8u);

extern int     set_ioperm(int, int, int);


extern char safe_port_in_byte(const unsigned short port);
extern void safe_port_out_byte(const unsigned short port, const unsigned char byte);

extern unsigned char inb(unsigned int port);
extern void outb(unsigned int port, unsigned int byte);
extern unsigned int inw(int port);
extern unsigned int ind(int port);
extern void outw(unsigned int port, unsigned int value);
extern void outd(unsigned int port, unsigned int value);

#endif /* _PORT_H */
