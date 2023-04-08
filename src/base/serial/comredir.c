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
#include "dos2linux.h"
#include "serial.h"
#include "ser_defs.h"
#include "comredir.h"

static int com_num;
static int com_num_wr;
static far_t old_ivec;
static u_short irq_hlt;
static int redir_tid;
static void comredir_thr(void *arg);
static far_t old_int15;
static u_short int15_hlt;
static int int15_tid;
static void int15_thr(void *arg);
static void int15_irq(Bit16u idx, HLT_ARG(arg));
static far_t old_int10;
static void int10_irq(Bit16u idx, HLT_ARG(arg));
static u_short int10_hlt;
static unsigned tflags;
// suppress output
#define TFLG_SUPPR 1
// append NL to CR on output
#define TFLG_OANL 2
// prepend CR to NL on output
#define TFLG_OPCR 4
// append NL to CR on input
#define TFLG_IANL 8
// prepend CR to NL on input
#define TFLG_IPCR 0x10

void comredir_init(void)
{
  emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
  hlt_hdlr.name       = "comint15 isr";
  hlt_hdlr.func       = int15_irq;
  hlt_hdlr.len        = 1;
  int15_hlt = hlt_register_handler_vm86(hlt_hdlr);
  hlt_hdlr.name       = "comint10 isr";
  hlt_hdlr.func       = int10_irq;
  hlt_hdlr.len        = 1;
  int10_hlt = hlt_register_handler_vm86(hlt_hdlr);

  redir_tid = coopth_create_vm86("comredir thr", comredir_thr, do_eoi_iret,
                &irq_hlt);
  int15_tid = coopth_create("comint15 thr", int15_thr);
}

void comredir_setup(int num, int num_wr, unsigned flags)
{
  int i = -1, j = -1;
  if (num > 0 && num <= 4) {
    struct vm86_regs saved_regs = REGS;
    int intr;
    unsigned char imr, imr1;

    if (com_num) {
      com_printf("comredir is already active\n");
      return;
    }
    i = get_com_idx(num);
    if (i == -1) {
      com_printf("comredir: com port %i not configured\n", num);
      return;
    }
    j = get_com_idx(num_wr);
    if (j == -1) {
      com_printf("comredir: com port %i not configured\n", num_wr);
      return;
    }
    intr = COM_INTERRUPT(i);
    old_ivec.segment = ISEG(intr);
    old_ivec.offset = IOFF(intr);
    SETIVEC(intr, BIOS_HLT_BLK_SEG, irq_hlt);

    _AX = 0b11100011;  // 8N1
    _DX = i;
    do_int_call_back(0x14);
    if (num_wr != num) {
      _AX = 0b11100011;  // 8N1
      _DX = num_wr - 1;
      do_int_call_back(0x14);
    }
    REGS = saved_regs;
    write_MCR(i, com[i].MCR | UART_MCR_OUT2);
    write_IER(i, UART_IER_RDI);

    imr = imr1 = port_inb(0x21);
    imr &= ~(1 << com_cfg[i].irq);
    if (imr != imr1)
      port_outb(0x21, imr);

    old_int15.segment = ISEG(0x15);
    old_int15.offset = IOFF(0x15);
    if (num_wr)
      SETIVEC(0x15, BIOS_HLT_BLK_SEG, int15_hlt);

    old_int10.segment = ISEG(0x10);
    old_int10.offset = IOFF(0x10);
    if (flags & TFLG_SUPPR)
      SETIVEC(0x10, BIOS_HLT_BLK_SEG, int10_hlt);
  } else {
    if (!com_num)
      return;
    int intr = COM_INTERRUPT(com_num - 1);
    write_IER(com_num - 1, 0);
    SETIVEC(intr, old_ivec.segment, old_ivec.offset);
    SETIVEC(0x15, old_int15.segment, old_int15.offset);
    SETIVEC(0x10, old_int10.segment, old_int10.offset);
  }
  com_num = i + 1;
  com_num_wr = j + 1;
  tflags = flags;
}

void comredir_reset(void)
{
  com_num = 0;
}

static void do_int10(void)
{
  unsigned int ssp, sp;

  ssp = SEGOFF2LINEAR(SREG(ss), 0);
  sp = LWORD(esp);
  pushw(ssp, sp, LWORD(eflags));
  LWORD(esp) -= 2;
  do_call_back(old_int10.segment, old_int10.offset);
}

static void do_char_out(char out)
{
  _AH = 0x0e;
  _AL = out;
  _BX = 0;
  do_int10();
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
      if (c == 0x1a) {  // ^Z, exit
        comredir_setup(0, 0, 0);
        break;
      }
      if ((tflags & TFLG_IPCR) && c == '\n')
        do_char_out('\r');
      do_char_out(c);
      if ((tflags & TFLG_IANL) && c == '\r')
        do_char_out('\n');
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
    if (!c) {
      set_CF();
      return;
    }
    if ((tflags & TFLG_OPCR) && c == '\n')
      write_char(com_num_wr - 1, '\r');
    write_char(com_num_wr - 1, c);
    if ((tflags & TFLG_OANL) && c == '\r')
      write_char(com_num_wr - 1, '\n');
    if (c == 0x1a) {  // ^Z, exit
      do_char_out(c);
      comredir_setup(0, 0, 0);
    }
  } else {
    set_CF();
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

static void int10_irq(Bit16u idx, HLT_ARG(arg))
{
  /* suppress console output */
  if (_AH == 9 || _AH == 0xe || _AH == 0x13 || _AH == 2) {
    fake_iret();
    return;
  }
  jmp_to(old_int10.segment, old_int10.offset);
}
