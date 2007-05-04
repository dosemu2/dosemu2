/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * trident.c
 *
 * This file contains support for VGA-cards with an Trident chip.
 *
 */

#include <stdio.h>
#include <unistd.h>

#include "emu.h"             /* nur für v_printf */
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "trident.h"

void trident_set_old_regs(void)
{
  port_out(0x0b, SEQ_I);
  port_out(0x00, SEQ_D);
  return;
}

void trident_set_new_regs(void)
{
  u_char dummy;

  port_out(0x0b, SEQ_I);
  port_out(0x00, SEQ_D);
  dummy = port_in(SEQ_D);
  return;
}

void trident_allow_svga(void)
{
  u_char dummy;

  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  dummy = port_in(SEQ_D);
  v_printf("Dummy then = 0x%02x\n", dummy);
  port_out(((dummy & ~0x20) | 0x10), SEQ_D);
  dummy = port_in(SEQ_D);
  v_printf("Dummy  now = 0x%02x\n", dummy);
  trident_set_new_regs();
  return;
}

void trident_disallow_svga(void)
{
  u_char dummy;

  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  dummy = port_in(SEQ_D);
  port_out(((dummy | 0x20) & ~0x10), SEQ_D);
  trident_set_new_regs();
  v_printf("Disallow SVGA Modes = 0x%02x\n", dummy & ~0x10);
  return;
}

u_char trident_get_svga(void)
{
  u_char dummy;

  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  dummy = port_in(SEQ_D) & 0x10;
  trident_set_new_regs();
  v_printf("Trident get SVGA, dummy = 0x%02x\n", dummy);
  return (dummy);
}

/* These set banks only work on 256k boundaries */
void trident_set_bank_read(u_char bank)
{
  if (trident_get_svga()) {
    trident_allow_svga();
    port_out(0x0e, SEQ_I);
    port_out(bank, SEQ_D);
  }
  else {
    port_out(0x0e, SEQ_I);
    port_out(0x02, SEQ_D);
  }
  v_printf("Trident set bank read = 0x%02x\n", bank);
  return;
}

void trident_set_bank_write(u_char bank)
{
  if (trident_get_svga()) {
    trident_allow_svga();
    port_out(0x0e, SEQ_I);
    port_out(bank, SEQ_D);
  }
  else {
    port_out(0x0e, SEQ_I);
    port_out(0x02, SEQ_D);
  }
  v_printf("Trident set bank write= 0x%02x\n", bank);
  return;
}

void trident_save_ext_regs(u_char xregs[], u_short xregs16[])
{
  port_out(0x0c, SEQ_I);
  xregs[0] = port_in(SEQ_D) & 0xff;
  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  xregs[1] = port_in(SEQ_D) & 0xff;
  port_out(0x0e, SEQ_I);
  xregs[2] = port_in(SEQ_D) & 0xff;
  trident_set_new_regs();
  port_out(0x0d, SEQ_I);
  xregs[3] = port_in(SEQ_D) & 0xff;
  port_out(0x0e, SEQ_I);
  xregs[4] = port_in(SEQ_D) & 0xff;
  port_out(0x0f, SEQ_I);
  xregs[5] = port_in(SEQ_D) & 0xff;
  port_out(0x1e, CRT_I);
  xregs[6] = port_in(CRT_D) & 0xff;
  port_out(0x1f, CRT_I);
  xregs[7] = port_in(CRT_D) & 0xff;
  port_out(0x0f, GRA_I);
  xregs[8] = port_in(GRA_D) & 0xff;
  return;
}

void trident_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
  trident_set_old_regs();
  port_out(0x0d, SEQ_I);
  port_out(xregs[1], SEQ_D);
  port_out(0x0e, SEQ_I);
  port_out(xregs[2], SEQ_D);
  trident_set_new_regs();
  port_out(0x0e, SEQ_I);
  port_out(0x82, SEQ_D);
  port_out(0x0c, SEQ_I);
  port_out(xregs[0], SEQ_D);
  v_printf("Check for change in 0x%02x\n", port_in(SEQ_D));
  port_out(0x0d, SEQ_I);
  port_out(xregs[3], SEQ_D);
  port_out(0x0e, SEQ_I);
  port_out(xregs[4] ^ 0x02, SEQ_D);
  port_out(0x0f, SEQ_I);
  port_out(xregs[5], SEQ_D);
  port_out(0x1e, CRT_I);
  port_out(xregs[6], CRT_D);
  port_out(0x1f, CRT_I);
  port_out(xregs[7], CRT_D);
  port_out(0x0f, GRA_I);
  port_out(xregs[8], GRA_D);
  port_out(0x0c, SEQ_I);
  port_out(xregs[0], SEQ_D);
  return;
}

/* These are trident Specific calls to get at banks */
void vga_init_trident(void)
{
  port_out(0x0b, SEQ_I);
  port_out(0x00, SEQ_D);
  if (port_in(SEQ_D) >= 0x03) {
    save_ext_regs = trident_save_ext_regs;
    restore_ext_regs = trident_restore_ext_regs;
    set_bank_read = trident_set_bank_read;
    set_bank_write = trident_set_bank_write;
    ext_video_port_in = trident_ext_video_port_in;
    ext_video_port_out = trident_ext_video_port_out;
    v_printf("Trident is 8900+\n");
  }
  else {
    save_ext_regs = trident_save_ext_regs;
    restore_ext_regs = trident_restore_ext_regs;
    set_bank_read = trident_set_bank_read;
    set_bank_write = trident_set_bank_write;
    ext_video_port_in = trident_ext_video_port_in;
    ext_video_port_out = trident_ext_video_port_out;
    v_printf("Trident is 8800\n");
  }
  return;
}

static int trident_old_regs = 0;

u_char trident_ext_video_port_in(ioport_t port)
{
  switch (port) {
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] == 0x0b) {
      trident_old_regs = !trident_old_regs;
      v_printf("old=1/new=0 = 0x%02x\n", trident_old_regs);
      return (0x04);
    }
    if (trident_old_regs) {
      if (dosemu_regs.regs[SEQI] == 0x0d) {
	v_printf("OLD Read on SEQI 0x0d got 0x%02x\n", dosemu_regs.xregs[1]);
	return (dosemu_regs.xregs[1]);
      }
      else if (dosemu_regs.regs[SEQI] == 0x0e) {
	v_printf("OLD Read on SEQI 0x0e got 0x%02x\n", dosemu_regs.xregs[2]);
	return (dosemu_regs.xregs[2]);
      }
    }
    else {
      if (dosemu_regs.regs[SEQI] == 0x0d) {
	v_printf("NEW Read on SEQI 0x0d got 0x%02x\n", dosemu_regs.xregs[3]);
	return (dosemu_regs.xregs[3]);
      }
      else if (dosemu_regs.regs[SEQI] == 0x0e) {
	v_printf("NEW Read on SEQI 0x0e got 0x%02x\n", dosemu_regs.xregs[4]);
	return (dosemu_regs.xregs[4]);
      }
    }
    if (dosemu_regs.regs[SEQI] == 0x0f) {
      v_printf("Read on SEQI 0x0f got 0x%02x\n", dosemu_regs.xregs[5]);
      return (dosemu_regs.xregs[5]);
    }
    if (dosemu_regs.regs[SEQI] == 0x0c) {
      v_printf("Read on SEQI 0x0c got 0x%02x\n", dosemu_regs.xregs[0]);
      return (dosemu_regs.xregs[0]);
    }
    break;
  case CRT_DC:
  case CRT_DM:
    if (dosemu_regs.regs[CRTI] == 0x1e) {
      v_printf("Read on CRT_D 0x1e got 0x%02x\n", dosemu_regs.xregs[6]);
      return (dosemu_regs.xregs[6]);
    }
    else if (dosemu_regs.regs[CRTI] == 0x1f) {
      v_printf("Read on CRT_D 0x1f got 0x%02x\n", dosemu_regs.xregs[7]);
      return (dosemu_regs.xregs[7]);
    }
    break;
  case GRA_D:
    if (dosemu_regs.regs[GRAI] == 0x0f) {
      v_printf("Read on GRA_D 0x0f got 0x%02x\n", dosemu_regs.xregs[8]);
      return (dosemu_regs.xregs[8]);
    }
    break;
  }
  v_printf("Bad Read on port 0x%04x\n", port);
  return (0);
}

void trident_ext_video_port_out(ioport_t port, u_char value)
{
  switch (port) {
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] == 0x0b) {
      trident_old_regs = 1;
      v_printf("Old_regs reset to OLD = 0x%02x\n", trident_old_regs);
      return;
    }
    if (trident_old_regs) {
      if (dosemu_regs.regs[SEQI] == 0x0d) {
	v_printf("OLD Write on SEQ_D 0x0d sent 0x%02x\n", value);
	dosemu_regs.xregs[1] = value;
	return;
      }
      else if (dosemu_regs.regs[SEQI] == 0x0e) {
	v_printf("OLD Write to SEQI at 0x0e put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[2]);
	return;
	dosemu_regs.xregs[2] = value;
	return;
      }
    }
    else {
      if (dosemu_regs.regs[SEQI] == 0x0d) {
	v_printf("NEW Write to SEQI at 0x0d put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[3]);
	dosemu_regs.xregs[3] = value;
	return;
      }
      else if (dosemu_regs.regs[SEQI] == 0x0e) {
	v_printf("NEW Write to SEQI at 0x0e put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[4]);
	dosemu_regs.xregs[4] = value;
	return;
      }
    }
    if (dosemu_regs.regs[SEQI] == 0x0c) {
      v_printf("Write to SEQI at 0x0c put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[0]);
      dosemu_regs.xregs[0] = value;
      return;
    }
    if (dosemu_regs.regs[SEQI] == 0x0f) {
      v_printf("Write to SEQI at 0x0f put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[5]);
      dosemu_regs.xregs[5] = value;
      return;
    }
    break;
  case CRT_DC:
  case CRT_DM:
    if (dosemu_regs.regs[CRTI] == 0x1e) {
      v_printf("Write to CRTI at 0x1e put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[6]);
      dosemu_regs.xregs[6] = value;
      return;
    }
    else if (dosemu_regs.regs[CRTI] == 0x1f) {
      v_printf("Write to CRTI at 0x1f put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[7]);
      dosemu_regs.xregs[7] = value;
      return;
    }
    break;
  case GRA_D:
    if (dosemu_regs.regs[GRAI] == 0x0f) {
      v_printf("Write to GRAD at 0x0f put 0x%02x->0x%02x\n", value, dosemu_regs.xregs[8]);
      return;
    }
    break;
  }
  v_printf("Bad Write on port 0x%04x with value 0x%02x\n", port, value);
}

/* End of video/trident.c */
