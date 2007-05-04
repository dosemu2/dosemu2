/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * wdvga.c
 *
 * This file contains support for VGA-cards with a WD(Paradise) chip.
 * This is very similar to trident.c, if needed we'll add more features
 * by looking at XFree sources.
 *
 */

#include <stdio.h>
#include <unistd.h>

#include "emu.h"             /* nur für v_printf */
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "wdvga.h"

void wd_set_old_regs(void)
{
  port_real_outb(SEQ_I, 0x0b);
  port_real_outb(SEQ_D, 0x00);
  return;
}

void wd_set_new_regs(void)
{
  u_char dummy;

  port_real_outb(SEQ_I, 0x0b);
  port_real_outb(SEQ_D, 0x00);
  dummy = port_real_inb(SEQ_D);
  return;
}

void wd_allow_svga(void)
{
  u_char dummy;

  wd_set_old_regs();
  port_real_outb(SEQ_I, 0x0d);
  dummy = port_real_inb(SEQ_D);
  v_printf("Dummy then = 0x%02x\n", dummy);
  port_real_outb(SEQ_D, ((dummy & ~0x20) | 0x10));
  dummy = port_real_inb(SEQ_D);
  v_printf("Dummy  now = 0x%02x\n", dummy);
  wd_set_new_regs();
  return;
}

void wd_disallow_svga(void)
{
  u_char dummy;

  wd_set_old_regs();
  port_real_outb(SEQ_I, 0x0d);
  dummy = port_real_inb(SEQ_D);
  port_real_outb(SEQ_D, ((dummy | 0x20) & ~0x10));
  wd_set_new_regs();
  v_printf("Disallow SVGA Modes = 0x%02x\n", dummy & ~0x10);
  return;
}

u_char wd_get_svga(void)
{
  u_char dummy;

  wd_set_old_regs();
  port_real_outb(SEQ_I, 0x0d);
  dummy = port_real_inb(SEQ_D) & 0x10;
  wd_set_new_regs();
  v_printf("Paradise get SVGA, dummy = 0x%02x\n", dummy);
  return (dummy);
}

/* These set banks only work on 256k boundaries */
void wd_set_bank_read(u_char bank)
{
  if (wd_get_svga()) {
    wd_allow_svga();
    port_real_outb(SEQ_I, 0x0e);
    port_real_outb(SEQ_D, bank);
  }
  else {
    port_real_outb(SEQ_I, 0x0e);
    port_real_outb(SEQ_D, 0x02);
  }
  v_printf("Paradise set bank read = 0x%02x\n", bank);
  return;
}

void wd_set_bank_write(u_char bank)
{
  if (wd_get_svga()) {
    wd_allow_svga();
    port_real_outb(SEQ_I, 0x0e);
    port_real_outb(SEQ_D, bank);
  }
  else {
    port_real_outb(SEQ_I, 0x0e);
    port_real_outb(SEQ_D, 0x02);
  }
  v_printf("Paradise set bank write= 0x%02x\n", bank);
  return;
}

void wd_save_ext_regs(u_char xregs[], u_short xregs16[])
{
  port_real_outb(SEQ_I, 0x0c);
  xregs[0] = port_real_inb(SEQ_D) & 0xff;
  wd_set_old_regs();
  port_real_outb(SEQ_I, 0x0d);
  xregs[1] = port_real_inb(SEQ_D) & 0xff;
  port_real_outb(SEQ_I, 0x0e);
  xregs[2] = port_real_inb(SEQ_D) & 0xff;
  wd_set_new_regs();
  port_real_outb(SEQ_I, 0x0d);
  xregs[3] = port_real_inb(SEQ_D) & 0xff;
  port_real_outb(SEQ_I, 0x0e);
  xregs[4] = port_real_inb(SEQ_D) & 0xff;
  port_real_outb(SEQ_I, 0x0f);
  xregs[5] = port_real_inb(SEQ_D) & 0xff;
  port_real_outb(CRT_I, 0x1e);
  xregs[6] = port_real_inb(CRT_D) & 0xff;
  port_real_outb(CRT_I, 0x1f);
  xregs[7] = port_real_inb(CRT_D) & 0xff;
  port_real_outb(GRA_I, 0x0f);
  xregs[8] = port_real_inb(GRA_D) & 0xff;
  return;
}

void wd_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
  wd_set_old_regs();
  port_real_outb(SEQ_I, 0x0d);
  port_real_outb(SEQ_D, xregs[1]);
  port_real_outb(SEQ_I, 0x0e);
  port_real_outb(SEQ_D, xregs[2]);
  wd_set_new_regs();
  port_real_outb(SEQ_I, 0x0e);
  port_real_outb(SEQ_D, 0x82);
  port_real_outb(SEQ_I, 0x0c);
  port_real_outb(SEQ_D, xregs[0]);
  v_printf("Check for change in 0x%02x\n", port_real_inb(SEQ_D));
  port_real_outb(SEQ_I, 0x0d);
  port_real_outb(SEQ_D, xregs[3]);
  port_real_outb(SEQ_I, 0x0e);
  port_real_outb(SEQ_D, xregs[4] ^ 0x02);
  port_real_outb(SEQ_I, 0x0f);
  port_real_outb(SEQ_D, xregs[5]);
  port_real_outb(CRT_I, 0x1e);
  port_real_outb(CRT_D, xregs[6]);
  port_real_outb(CRT_I, 0x1f);
  port_real_outb(CRT_D, xregs[7]);
  port_real_outb(GRA_I, 0x0f);
  port_real_outb(GRA_D, xregs[8]);
  port_real_outb(SEQ_I, 0x0c);
  port_real_outb(SEQ_D, xregs[0]);
  return;
}

void vga_init_wd(void)
{
  port_real_outb(SEQ_I, 0x0b);
  port_real_outb(SEQ_D, 0x00);
  save_ext_regs = wd_save_ext_regs;
  restore_ext_regs = wd_restore_ext_regs;
  set_bank_read = wd_set_bank_read;
  set_bank_write = wd_set_bank_write;
/*  ext_video_port_in = wd_ext_video_port_in; */
/*  ext_video_port_out = wd_ext_video_port_out; */
  return;
}

unsigned char wd_ext_video_port_in(ioport_t port)
{
  v_printf("Bad Read on port 0x%04x\n", port);
  return (0);
}

void wd_ext_video_port_out(ioport_t port, u_char value)
{
  v_printf("Bad Write on port 0x%04x with value 0x%02x\n", port, value);
}

/* End of video/wdvga.c */
