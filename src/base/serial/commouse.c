/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * mouse.com backend
 *
 * FIXME: only supporting M3 for now
 *
 * Author: Stas Sergeev
 */
#include <string.h>
#include "emu.h"
#include "init.h"
#include "hlt.h"
#include "int.h"
#include "port.h"
#include "timers.h"
#include "coopth.h"
#include "ser_defs.h"

#define read_reg(num, offset) do_serial_in((num), com_cfg[(num)].base_port + (offset))
#define read_char(num)        read_reg((num), UART_RX)
#define read_LCR(num)         read_reg((num), UART_LCR)
#define read_LSR(num)         read_reg((num), UART_LSR)
#define read_IIR(num)         read_reg((num), UART_IIR)
#define write_reg(num, offset, byte) do_serial_out((num), com_cfg[(num)].base_port + (offset), (byte))
#define write_char(num, byte) write_reg((num), UART_TX, (byte))
#define write_DLL(num, byte)  write_reg((num), UART_DLL, (byte))
#define write_DLM(num, byte)  write_reg((num), UART_DLM, (byte))
#define write_FCR(num, byte)  write_reg((num), UART_FCR, (byte))
#define write_LCR(num, byte)  write_reg((num), UART_LCR, (byte))
#define write_MCR(num, byte)  write_reg((num), UART_MCR, (byte))
#define write_IER(num, byte)  write_reg((num), UART_IER, (byte))

static int com_num = -1;
static u_short irq_hlt;
static void com_irq(Bit32u idx, void *arg);

static int com_mouse_init(void)
{
  emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
  int num;
  if (config.mouse.com == -1 || !config.mouse.intdrv)
    return 0;
  for (num = 0; num < config.num_ser; num++) {
    if (com_cfg[num].real_comport == config.mouse.com)
      break;
  }
  if (num >= config.num_ser)
    return 0;

  com_num = num;
  hlt_hdlr.name       = "commouse isr";
  hlt_hdlr.func       = com_irq;
  irq_hlt = hlt_register_handler(hlt_hdlr);

  return 1;
}

static void com_irq(Bit32u idx, void *arg)
{
  uint8_t iir, val;
  s_printf("COMMOUSE: got irq\n");
  iir = read_IIR(com_num);
  switch (iir & UART_IIR_CND_MASK) {
    case UART_IIR_NO_INT:
      break;
    case UART_IIR_RDI:
      while (read_LSR(com_num) & UART_LSR_DR) {
        val = read_char(com_num);
        DOSEMUMouseProtocol(&val, 1, MOUSE_MS3BUTTON);
      }
      break;
    default:
      s_printf("COMMOUSE: unexpected interrupt cond %#x\n", iir);
      break;
  }

  do_eoi_iret();
}

static int get_char(int num)
{
  const int timeout = 10;
  const int scale = 0x10000;
  hitimer_t end_time = GETtickTIME(0) + timeout * scale;
  int i = 1;
  while (GETtickTIME(0) < end_time) {
    if (read_LSR(num) & UART_LSR_DR)
      break;
    s_printf("COMMOUSE: Wait for recv, %i\n", i);
    i++;
    _set_IF();
    coopth_wait();
    clear_IF();
  }
  if (!(read_LSR(num) & UART_LSR_DR)) {
    s_printf("COMMOUSE: timeout, returning\n");
    return -1;
  }
  return read_char(num);
}

int com_mouse_post_init(void)
{
  int num = com_num, ch, i;
  uint8_t imr, imr1;
  char buf[2];

  if (com_num == -1)
    return 0;

  write_IER(num, 0);

  /* 1200, 8N1 */
  write_LCR(num, UART_LCR_DLAB);
  write_DLL(num, DIV_1200 & 0xff);
  write_DLM(num, DIV_1200 >> 8);
  write_LCR(num, UART_LCR_WLEN8);

  write_MCR(num, UART_MCR_DTR);
  _set_IF();
  coopth_wait();
  clear_IF();
  write_MCR(num, UART_MCR_DTR | UART_MCR_RTS);
  for (i = 0; i < 2; i++) {
    ch = get_char(num);
    if (ch == -1)
      return 0;
    buf[i] = ch;
  }
  if (strncmp(buf, "M3", 2) != 0) {
    s_printf("COMMOUSE: unsupported ID %s\n", buf);
    return 0;
  }

  com[num].ivec.segment = ISEG(com[num].interrupt);
  com[num].ivec.offset = IOFF(com[num].interrupt);
  SETIVEC(com[num].interrupt, BIOS_HLT_BLK_SEG, irq_hlt);
  write_MCR(num, com[num].MCR | UART_MCR_OUT2);
  imr = imr1 = port_inb(0x21);
  imr &= ~(1 << com_cfg[num].irq);
  if (imr != imr1)
    port_outb(0x21, imr);
  write_IER(num, UART_IER_RDI);
  return 1;
}

static struct mouse_client com_mouse =  {
  "com mouse",		/* name */
  com_mouse_init,	/* init */
  NULL,			/* close */
  NULL,			/* run */
  NULL
};

CONSTRUCTOR(static void com_mouse_register(void))
{
  register_mouse_client(&com_mouse);
}
