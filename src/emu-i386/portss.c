#if 0
#define CONFIG_PORT_TRACE 1
#endif
/* 
 * DANG_BEGIN_MODULE
 *
 * Description: Port handling code for DOSEMU
 * 
 * Maintainers: Volunteers?
 *
 * REMARK
 * This is the code that allows and disallows port access within DOSEMU.
 * The BOCHS port IO code was actually very cleverly done.  So I stole
 * the idea.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 */

#include <unistd.h>		/* for iopl() */

#include "config.h"
#include "emu.h"
#include "port.h"
#include "priv.h"

#ifndef __min
#define __min(x,y)   (((x)<(y))?(x):(y))
#endif

#define MAX_IO_DEVICES  EMU_MAX_IO_DEVICES

static struct {
  Bit8u      (*read_portb)(Bit32u port_addr);
  void       (*write_portb)(Bit32u port_addr, Bit8u byte);
  Bit16u     (*read_portw)(Bit32u port_addr);
  void       (*write_portw)(Bit32u port_addr, Bit16u word);
  Bit32u     (*read_portd)(Bit32u port_addr);
  void       (*write_portd)(Bit32u port_addr, Bit32u word);
  const char  *handler_name;
  int          irq;
} port_handler[EMU_MAX_IO_DEVICES];

static Bit8u port_handler_id[0x10000];     /* 64K ports */
static Bit8u port_handles;                 /* number of io_handler's */
static const char *irq_handler_name[EMU_MAX_IRQS];

#define emu_handler(port)    port_handler[port_handler_id[port]]

/* 
 * DANG_BEGIN_FUNCTION port_inb(Bit32u port)
 *
 * description:
 *   Handles/simulates an inb() port IO read
 *
 * DANG_END_FUNCTION
 */
Bit8u port_inb(Bit32u port)
{
  Bit8u res;
  port &= 0xffff;
  res   = emu_handler(port).read_portb(port);

#if CONFIG_PORT_TRACE > 0
#if CONFIG_PORT_TRACE == 1
  if (port < 0x100)
#endif
    i_printf("PORT: \"%s\" inb [0x%04lx] -> 0x%02hx\n", 
	     emu_handler(port).handler_name, port, res);
#endif
  return res;
}

/* 
 * DANG_BEGIN_FUNCTION port_outb(Bit32u port, Bit8u byte)
 *
 * description:
 *   Handles/simulates an outb() port IO write
 *
 * DANG_END_FUNCTION
 */
void port_outb(Bit32u port, Bit8u byte)
{
  port &= 0xffff;
  emu_handler(port).write_portb(port, byte);

#if CONFIG_PORT_TRACE > 0
#if CONFIG_PORT_TRACE == 1
  if (port < 0x100)
#endif
    i_printf("PORT: \"%s\" outb [0x%04lx] <- 0x%02hx\n",
	     emu_handler(port).handler_name, port, byte);
#endif
}

/* 
 * DANG_BEGIN_FUNCTION port_inw(Bit32u port)
 *
 * description:
 *   Handles/simulates an inw() port IO read.  Usually this invokes
 *   port_inb() twice, but it may be necessary to do full word i/o for
 *   some video boards.
 *
 * DANG_END_FUNCTION
 */
Bit16u port_inw(Bit32u port)
{
  Bit16u res;
  port &= 0xffff;

  if (emu_handler(port).read_portw != NULL) {
    res = emu_handler(port).read_portw(port);
#if CONFIG_PORT_TRACE > 0
#if CONFIG_PORT_TRACE == 1
    if (port < 0x100)
#endif
      i_printf("PORT: \"%s\" inw [0x%04lx] -> 0x%04hx\n",
	       emu_handler(port).handler_name, port, res);
#endif
  }
  else {
    res = (Bit16u)port_inb(port) | (((Bit16u)port_inb(port+1)) << 8);
  }

  return res;
}

/* 
 * DANG_BEGIN_FUNCTION port_outw(Bit32u port, Bit16u word)
 *
 * description:
 *   Handles/simulates an outw() port IO write
 *
 * DANG_END_FUNCTION
 */
void port_outw(Bit32u port, Bit16u word)
{
  port &= 0xffff;

  if (emu_handler(port).write_portw != NULL) {
    emu_handler(port).write_portw(port, word);
#if CONFIG_PORT_TRACE > 0
#if CONFIG_PORT_TRACE == 1
    if (port < 0x100)
#endif
      i_printf("PORT: \"%s\" outw [0x%04lx] <- 0x%04x\n",
	       emu_handler(port).handler_name, port, word);
#endif
  }
  else {
    port_outb(port, word & 0xff);
    port_outb(port+1, (word >> 8) & 0xff);
  }
}

/* 
 * DANG_BEGIN_FUNCTION port_ind(Bit32u port)
 *
 * description:
 *   Handles/simulates an ind() port IO read.  For the moment, this just
 *   does port_inw() twice, but it may be necessary to do full dword i/o for
 *   some video boards.
 *
 * DANG_END_FUNCTION
 */
Bit32u port_ind(Bit32u port)
{
  Bit32u res;
  port &= 0xffff;

  res = port_inw(port);
  res |= ((Bit32u)port_inw(port+2)) << 16;
  return res;
}

/* 
 * DANG_BEGIN_FUNCTION port_outd(Bit32u port, Bit32u dword)
 *
 * description:
 *   Handles/simulates an outd() port IO write
 *
 * DANG_END_FUNCTION
 */
void port_outd(Bit32u port, Bit32u dword)
{
  port &= 0xffff;

  port_outw(port, dword & 0xffff);
  port_outw(port+2, (dword >> 16) & 0xffff);
}

static Bit8u port_default_inb(Bit32u port)
{
  return 0;
}

static void port_default_outb(Bit32u port, Bit8u byte)
{
}

Bit8u port_safe_inb(Bit32u port)
{
  Bit8u res = 0;

  i_printf("PORT: safe_inb ");
  if (i_am_root) {
    enter_priv_on();
    iopl(3);
    res = port_in(port);
    iopl(0);

    leave_priv_setting();
  }
  else
    i_printf("want to ");
  i_printf("in(%lx)", port);
  if (i_am_root)
    i_printf(" = %hx", res);
  i_printf("\n");
  return res;
}

void port_safe_outb(Bit32u port, Bit8u byte)
{
  i_printf("PORT: safe_outb ");
  if (i_am_root) {
    enter_priv_on();
    iopl(3);
    port_out(byte, port);
    iopl(0);
    leave_priv_setting();
  }
  else
    i_printf("want to ");
  i_printf("outb(%lx, %hx)\n", port, byte);
}

Bit16u port_safe_inw(Bit32u port)
{
  Bit16u res = 0;

  i_printf("PORT: safe_inw ");
  if (i_am_root) {
    enter_priv_on();
    iopl(3);
    res = port_in_w(port);
    iopl(0);
    leave_priv_setting();
  }
  else
    i_printf("want to ");
  i_printf("inw(%lx)", port);
  if (i_am_root)
    i_printf(" = %x", res);
  i_printf("\n");
  return res;
}

void port_safe_outw(Bit32u port, Bit16u word)
{
  i_printf("PORT: safe_outw ");
  if (i_am_root) {
    enter_priv_on();
    iopl(3);
    port_out_w(word, port);
    iopl(0);
    leave_priv_setting();
  }
  else
    i_printf("want to ");
  i_printf("outw(%lx, %x)\n", port, word);
}

/* 
 * DANG_BEGIN_FUNCTION port_init(void)
 *
 * description:
 *   Resets all the port port_handler information
 *
 * DANG_END_FUNCTION
 */
void port_init(void)
{
  int i;

  /* set all elements to default values */
  for (i=0; i < MAX_IO_DEVICES; i++) {
    port_handler[i].read_portb   = NULL;
    port_handler[i].write_portb  = NULL;
    port_handler[i].read_portw   = NULL;
    port_handler[i].write_portw  = NULL;
    port_handler[i].read_portd   = NULL;
    port_handler[i].write_portd  = NULL;
  }

  /* handle 0 maps to the unmapped IO device handler.  Basically any
     ports which don't map to any other device get mapped to this
     handler which does absolutely nothing.
   */
  port_handler[0].read_portb     = port_default_inb;
  port_handler[0].write_portb    = port_default_outb;
  port_handler[0].read_portw     = NULL;
  port_handler[0].write_portw    = NULL;
  port_handler[0].handler_name   = "unknown port";
  port_handler[0].irq            = EMU_NO_IRQ;

  port_handler[1].read_portb     = port_safe_inb;
  port_handler[1].write_portb    = port_safe_outb;
  port_handler[1].read_portw     = NULL;
  port_handler[1].write_portw    = NULL;
  port_handler[1].handler_name   = "safe port io";
  port_handler[1].irq            = EMU_NO_IRQ;

  port_handler[2].read_portb     = port_safe_inb;
  port_handler[2].write_portb    = port_default_outb;
  port_handler[2].read_portw     = NULL;
  port_handler[2].write_portw    = NULL;
  port_handler[2].handler_name   = "safe port read";
  port_handler[2].irq            = EMU_NO_IRQ;

  port_handler[3].read_portb     = port_default_inb;
  port_handler[3].write_portb    = port_safe_outb;
  port_handler[3].read_portw     = NULL;
  port_handler[3].write_portw    = NULL;
  port_handler[3].handler_name   = "safe port write";
  port_handler[3].irq            = EMU_NO_IRQ;

  port_handles = 4;

  for (i=0; i < 0x10000; i++)
    port_handler_id[i] = 0;  /* unmapped IO handle */
}

void port_register_handler(emu_iodev_t device)
{
  int handle, i;

  i_printf("PORT: registering \"%s\" for port range [0x%04lx-0x%04lx]\n",
	   device.handler_name, device.start_addr, device.end_addr);

  if (device.irq != EMU_NO_IRQ  &&  device.irq >= EMU_MAX_IRQS) {
    error("PORT: IO device %s registered with IRQ=%d above %u\n",
	  device.handler_name, device.irq, EMU_MAX_IRQS-1);
    leavedos(1);
  }

  /* first find existing handle for function or create new one */
  for (handle=0; handle < (int) port_handles; handle++) {
    if (!strcmp(port_handler[handle].handler_name, device.handler_name))
      break;
  }

  if (handle >= (int) port_handles) {
    /* no existing handle found, create new one */
    if (port_handles >= EMU_MAX_IO_DEVICES) {
      error("PORT: too many IO devices, increase EMU_MAX_IO_DEVICES\n");
      leavedos(1);
    }

    if (device.irq != EMU_NO_IRQ  &&  irq_handler_name[device.irq]) {
      error("PORT: IRQ %d conflict.  IO devices %s & %s\n",
        device.irq, irq_handler_name[device.irq], device.handler_name);
      leavedos(1);
    }
    if (device.irq != EMU_NO_IRQ  &&  device.irq < EMU_MAX_IRQS)
      irq_handler_name[device.irq] = device.handler_name;
    port_handles++;
    port_handler[handle].read_portb     = device.read_portb;
    port_handler[handle].write_portb    = device.write_portb;
    port_handler[handle].read_portw     = device.read_portw;
    port_handler[handle].write_portw    = device.write_portw;
    port_handler[handle].read_portd     = device.read_portd;
    port_handler[handle].write_portd    = device.write_portd;
    port_handler[handle].handler_name   = device.handler_name;
    port_handler[handle].irq            = device.irq;
  }

  /* change table to reflect new handler id for that address */
  for (i=device.start_addr; i <= device.end_addr; i++) {
    if (port_handler_id[i] != 0) {
      error("PORT: IO device address conflict at IO address 0x%x\n", i);
      error("      conflicting devices: %s & %s\n",
	    port_handler[handle].handler_name, emu_handler(i).handler_name);
      leavedos(1);
    }
    port_handler_id[i] = handle;
  }
}

/*
 * We have several cases here:
 *   1. Simple case -- no ormask or andmask, read & write
 *      ports <= 0x3ff, map to SafePortIO & set_ioperm
 *      ports >  0x3ff, map to SafePortIO
 *   2. Silly case -- no ormask or andmask, read OR write
 *      map to SafePortRead/WriteIO
 *   3. Complex case -- ormask or andmask
 *      <no solution at present>
 */

Boolean port_allow_io(Bit16u start, Bit16u size, int permission,
		      Bit8u ormask, Bit8u andmask)
{
  emu_iodev_t  io_device;

  if ((ormask == 0) && (andmask == 0xff)) {
    if (permission == IO_RDWR) {
      if (start <= 0x3ff)
	set_ioperm(start, __min(size, 0x400-start), 1);
      io_device.handler_name = "safe port io";
      io_device.start_addr   = start;
      io_device.end_addr     = start + size - 1;
      io_device.irq          = EMU_NO_IRQ;
      port_register_handler(io_device);
      return 1;
    } else if (permission == IO_READ) {
      io_device.handler_name = "safe port read";
      io_device.start_addr   = start;
      io_device.end_addr     = start + size - 1;
      io_device.irq          = EMU_NO_IRQ;
      port_register_handler(io_device);
      return 1;
    } else {
      io_device.handler_name = "safe port write";
      io_device.start_addr   = start;
      io_device.end_addr     = start + size - 1;
      io_device.irq          = EMU_NO_IRQ;
      port_register_handler(io_device);
      return 1;
    }
  }
  else {
    error("CONFIG: { PORT andmask & ormask } are not currently supported\n");
    c_printf("PORT: { PORT andmask & ormask } are not currently supported\n");
    return 0;
  }
}

#if 0
/* return status of io_perm call */
int set_ioperm(int start, int size, int flag)
{
  int result;

  if (!i_am_root)
    return -1;		/* don't bother */

  enter_priv_on();
  result = ioperm(start, size, flag);
  leave_priv_setting();
  return result;
}
#endif
