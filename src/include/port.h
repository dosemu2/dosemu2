/*
 * include/port.h - contains INLINE-Functions for port access
 *
 */

#ifndef _PORT_H
#define _PORT_H

#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif
#include "types.h"
#include "priv.h"

/* port i/o privileges */
#define IO_READ  1
#define IO_WRITE 2
#define IO_RDWR	 (IO_READ | IO_WRITE)

/* Hey, stranger, there is one too many of us in this town! */
typedef struct {
  Bit8u         (* read_portb)(ioport_t port, void *arg);
  void          (* write_portb)(ioport_t port, Bit8u byte, void *arg);
  Bit16u        (* read_portw)(ioport_t port, void *arg);
  void          (* write_portw)(ioport_t port, Bit16u word, void *arg);
  Bit32u        (* read_portd)(ioport_t port, void *arg);
  void          (* write_portd)(ioport_t port, Bit32u word, void *arg);
  const char   *handler_name;
  ioport_t      start_addr;
  ioport_t      end_addr;
  void         *arg;
} emu_iodev_t, _port_handler;

extern int in_crit_section;

static __inline__ void port_real_outb(ioport_t port, Bit8u value)
{
#ifdef HAVE_SYS_IO_H
  outb(value, port);
#endif
}

static __inline__ Bit8u port_real_inb(ioport_t port)
{
  Bit8u _v = 0;
#ifdef HAVE_SYS_IO_H
  _v = inb(port);
#endif
  return _v;
}

static __inline__ void port_real_outw(ioport_t port, Bit16u value)
{
#ifdef HAVE_SYS_IO_H
  outw(value, port);
#endif
}

static __inline__ Bit16u port_real_inw(ioport_t port)
{
  Bit16u _v = 0;
#ifdef HAVE_SYS_IO_H
  _v = inw(port);
#endif
  return _v;
}

static __inline__ void port_real_outd(ioport_t port, Bit32u value)
{
#ifdef HAVE_SYS_IO_H
  outl(value, port);
#endif
}

static __inline__ Bit32u port_real_ind(ioport_t port)
{
  Bit32u _v = 0;
#ifdef HAVE_SYS_IO_H
  _v = inl(port);
#endif
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

extern unsigned char emu_io_bitmap[];

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


#define PORT_FAST	1
#define PORT_DEV_RD	2
#define PORT_DEV_WR	4
#define PORT_FORCE_FAST	8


extern int     port_init(void);
extern int     port_register_handler(emu_iodev_t info, int);
extern Boolean port_allow_io(ioport_t, Bit16u, int, Bit8u, Bit8u, int);
extern int     set_ioperm(int start, int size, int flag);

extern void init_port_traceing(void);
extern void register_port_traceing(ioport_t firstport, ioport_t lastport);
extern void clear_port_traceing(void);

extern void do_r3da_pending (void);

void port_enter_critical_section(const char *caller);
void port_leave_critical_section(void);

#endif /* _PORT_H */
