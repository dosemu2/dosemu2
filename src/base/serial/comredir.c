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
 * Purpose: com port redirect to console
 *
 * Author: stsp
 */
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <assert.h>

#include "emu.h"
#include "hlt.h"
#include "int.h"
#include "coopth.h"
#include "port.h"
#include "keyboard/keyb_server.h"
#include "serial.h"
#include "ser_defs.h"
#include "comredir.h"

static int com_num;
static far_t old_ivec;
static u_short irq_hlt;
static int redir_tid;
static void comredir_thr(void *arg);
static far_t old_int15;
static u_short int15_hlt;
static int int15_tid;
static void int15_thr(void *arg);
static void int15_irq(Bit16u idx, HLT_ARG(arg));

void comredir_init(void)
{
  emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
  hlt_hdlr.name       = "comint15 isr";
  hlt_hdlr.func       = int15_irq;
  hlt_hdlr.len        = 1;
  int15_hlt = hlt_register_handler_vm86(hlt_hdlr);

  redir_tid = coopth_create_vm86("comredir thr", comredir_thr, do_eoi_iret,
                &irq_hlt);
  int15_tid = coopth_create("comint15 thr", int15_thr);
}

void comredir_setup(int num)
{
  if (num > 0 && num <= 4) {
    struct vm86_regs saved_regs = REGS;
    int i = num - 1;
    int intr = COM_INTERRUPT(i);
    unsigned char imr, imr1;
    old_ivec.segment = ISEG(intr);
    old_ivec.offset = IOFF(intr);
    SETIVEC(intr, BIOS_HLT_BLK_SEG, irq_hlt);

    _AX = 0b11100011;  // 8N1
    _DX = i;
    do_int_call_back(0x14);
    REGS = saved_regs;
    write_MCR(i, com[i].MCR | UART_MCR_OUT2);
    write_IER(i, UART_IER_RDI);

    imr = imr1 = port_inb(0x21);
    imr &= ~(1 << com_cfg[i].irq);
    if (imr != imr1)
      port_outb(0x21, imr);

    old_int15.segment = ISEG(0x15);
    old_int15.offset = IOFF(0x15);
    SETIVEC(0x15, BIOS_HLT_BLK_SEG, int15_hlt);
  } else {
    if (!com_num)
      return;
    int intr = COM_INTERRUPT(com_num - 1);
    write_IER(com_num - 1, 0);
    SETIVEC(intr, old_ivec.segment, old_ivec.offset);
    SETIVEC(0x15, old_int15.segment, old_int15.offset);
  }
  com_num = num;
}

void comredir_reset(void)
{
  com_num = 0;
}

static void comredir_thr(void *arg)
{
  int i = com_num - 1;
  uint8_t iir;

  s_printf("comredir: got irq\n");
  iir = read_IIR(i);
  switch (iir & UART_IIR_CND_MASK) {
  case UART_IIR_NO_INT:
    break;
  case UART_IIR_RDI: {
    struct vm86_regs saved_regs = REGS;
    while (read_LSR(i) & UART_LSR_DR) {
      unsigned char c = read_char(i);
      if (c == '\n') {
        _AH = 0x0e;
        _AL = '\r';
        _BX = 0;
        do_int_call_back(0x10);
      }
      _AH = 0x0e;
      _AL = c;
      _BX = 0;
      do_int_call_back(0x10);
      if (c == '\r') {
        _AH = 0x0e;
        _AL = '\n';
        _BX = 0;
        do_int_call_back(0x10);
      }
    }
    REGS = saved_regs;
    break;
  }
  default:
    s_printf("comredir: unexpected interrupt cond %#x\n", iir);
    break;
  }
}

static void int15_thr(void *arg)
{
  unsigned int ssp, sp;

  ssp = SEGOFF2LINEAR(SREG(ss), 0);
  sp = LWORD(esp);
  pushw(ssp, sp, LWORD(eflags));
  LWORD(esp) -= 2;
  do_call_back(old_int15.segment, old_int15.offset);
  if (!isset_CF())
    return;
  clear_CF();
  if (!(_AL & 0x80)) {
    unsigned char c = get_bios_key(_AL);
    write_char(com_num - 1, c);
    if (c == '\r')
      write_char(com_num - 1, '\n');
  }
}

static void int15_irq(Bit16u idx, HLT_ARG(arg))
{
  if (_AH != 0x4f) {
    jmp_to(old_int15.segment, old_int15.offset);
    return;
  }
  fake_iret();
  coopth_start(int15_tid, NULL);
}
