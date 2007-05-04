/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * video/et4000.c
 *
 * This file contains support for VGA-cards with an et4000 chip.
 *
 */

#define ET4000_C

#include <stdio.h>
#include <unistd.h>

#include "emu.h"        /* for v_printf only */
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "et4000.h"

/* These are et4000 specific functions */

void et4000_save_ext_regs(u_char xregs[], u_short xregs16[])
{
  emu_video_retrace_off();
  xregs[12] = port_in(0x3bf) & 0xff;
  xregs[11] = port_in(0x3d8) & 0xff;
  port_out(0x03, 0x3bf);
  port_out(0xa0, 0x3d8);
  xregs[0] = port_in(0x3cd) & 0xff;
  xregs[18] = port_in(MIS_R);
  port_out(0x24, CRT_I);
  xregs[1] = port_in(CRT_D) & 0xff;
  port_out(0x32, CRT_I);
  xregs[2] = port_in(CRT_D) & 0xff;
  port_out(0x34, CRT_I);
  xregs[15] = port_in(CRT_D) & 0xff;
  port_out(xregs[15] & 0x0f, CRT_D);
  port_out(0x00, 0x3cd);
  port_out(0x33, CRT_I);
  xregs[3] = port_in(CRT_D) & 0xff;
  port_out(0x35, CRT_I);
  xregs[4] = port_in(CRT_D) & 0xff;
  port_out(0x36, CRT_I);
  xregs[5] = port_in(CRT_D) & 0xff;
  port_out(0x37, CRT_I);
  xregs[6] = port_in(CRT_D) & 0xff;
  port_out(0x3f, CRT_I);
  xregs[7] = port_in(CRT_D) & 0xff;
  port_out(0x6, SEQ_I);
  xregs[16] = port_in(SEQ_D) & 0xff;
  port_out(0x7, SEQ_I);
  xregs[8] = port_in(SEQ_D) & 0xff;
  port_in(IS1_R);
  port_out(0x36, ATT_IW);
  xregs[9] = port_in(ATT_R) & 0xff;
  port_out(xregs[9], ATT_IW);
  xregs[17] = port_in(0x3c3) & 0xff;
  xregs[10] = port_in(0x3cb) & 0xff;
  port_out(0xd, GRA_I);
  xregs[13] = port_in(GRA_D) & 0xff;
  port_out(0xe, GRA_I);
  xregs[14] = port_in(GRA_D) & 0xff;
  emu_video_retrace_on();
}

void et4000_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
  emu_video_retrace_off();
  port_out(0x03, 0x3bf);
  port_out(0xa0, 0x3d8);
  port_out(0x00, 0x3cd);
  port_out(0x6, SEQ_I);
  port_out(xregs[16], SEQ_D);
  port_out(0x7, SEQ_I);
  port_out(xregs[8], SEQ_D);
  port_in(IS1_R);
  port_out(0x36, ATT_IW);
  port_out(xregs[9], ATT_IW);
  port_out(0x24, CRT_I);
  port_out(xregs[1], CRT_D);
  port_out(0x32, CRT_I);
  port_out(xregs[2], CRT_D);
  port_out(0x33, CRT_I);
  port_out(xregs[3], CRT_D);
  port_out(0xe3, MIS_W);
  port_out(0x34, CRT_I);
  /* First time through leave as is. Second time, remove the #define JESX 1 line */
#define JESX 1
#ifdef JESX
  port_out(0x34, CRT_I);
  port_out(xregs[15], CRT_D);
  port_out(xregs[18], MIS_W);
#else
  temp = port_in(MIS_R);
  port_out((temp & 0xf3) | ((xregs[15] & 0x10) >> 1) | (xregs[15] & 0x04), MIS_W);
  port_out(0x34, CRT_I);
  port_out(0x00, CRT_D);
  port_out(0x02, CRT_D);
  port_out((xregs[15] & 0x08) >> 2, CRT_D);
  port_out(xregs[18], MIS_W);
#endif
  port_out(0x35, CRT_I);
  port_out(xregs[4], CRT_D);
  port_out(xregs[17], 0x3c3);
  port_out(0x36, CRT_I);
  port_out(xregs[5], CRT_D);
  port_out(0x37, CRT_I);
  port_out(xregs[6], CRT_D);
  port_out(0x3f, CRT_I);
  port_out(xregs[7], CRT_D);
  port_out(xregs[10], 0x3cb);
  port_out(0xd, GRA_I);
  port_out(xregs[13], GRA_D);
  port_out(0xe, GRA_I);
  port_out(xregs[14], GRA_D);
  port_out(xregs[0], 0x3cd);
  port_out(xregs[12], 0x3bf);
  port_out(xregs[11], 0x3d8);
  port_out(0x0, 0x3c4);
  port_out(0x3, 0x3c5);
  emu_video_retrace_on();
}

void et4000_set_bank_read(u_char bank)
{
  u_char dummy;

  port_out(0x03, 0x3bf);
  port_out(0xa0, 0x3d8);
  dummy = port_in(0x3cd) & 0x0f;
  port_out(dummy | (bank << 4), 0x3cd);
}

void et4000_set_bank_write(u_char bank)
{
  u_char dummy;

  port_out(0x03, 0x3bf);
  port_out(0xa0, 0x3d8);
  dummy = port_in(0x3cd) & 0xf0;
  port_out(dummy | (bank), 0x3cd);
}

/*
 * Initialize the function pointer to point to the et4000-specific
 * functions implemented here.
 */

void vga_init_et4000(void)
{
  save_ext_regs = et4000_save_ext_regs;
  restore_ext_regs = et4000_restore_ext_regs;
  set_bank_read = et4000_set_bank_read;
  set_bank_write = et4000_set_bank_write;
  ext_video_port_in = et4000_ext_video_port_in;
  ext_video_port_out = et4000_ext_video_port_out;
  return;
}

u_char et4000_ext_video_port_in(ioport_t port)
{
  switch (port) {
  case 0x3c3:
    v_printf("et4000 read of port 0x3c3 = 0x%02x\n", dosemu_regs.xregs[17]);
    return (dosemu_regs.xregs[17]);
  case 0x3bf:
    v_printf("et4000 read of port 0x3bf = 0x%02x\n", dosemu_regs.xregs[12]);
    return (dosemu_regs.xregs[12]);
  case 0x3cb:
    v_printf("et4000 read of port 0x3cb = 0x%02x\n", dosemu_regs.xregs[10]);
    return (dosemu_regs.xregs[10]);
  case 0x3cd:
    v_printf("et4000 read of port 0x3cd = 0x%02x\n", dosemu_regs.xregs[0]);
    return (dosemu_regs.xregs[0]);
  case 0x3d8:
    v_printf("et4000 read of port 0x3d8 = 0x%02x\n", dosemu_regs.xregs[11]);
    return (dosemu_regs.xregs[11]);
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] == 0x07) {
      v_printf("et4000 SEQ_D read at 0x07 of 0x%02x\n", dosemu_regs.xregs[8]);
      return (dosemu_regs.xregs[8]);
    }
    else if (dosemu_regs.regs[SEQI] == 0x06) {
      v_printf("et4000 SEQ_D read at 0x06 of 0x%02x\n", dosemu_regs.xregs[16]);
      return (dosemu_regs.xregs[16]);
    }
    break;
  case CRT_DC:
  case CRT_DM:
    switch (dosemu_regs.regs[CRTI]) {
    case 0x24:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[1]);
    case 0x32:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[2]);
      return (dosemu_regs.xregs[2]);
    case 0x33:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[3]);
      return (dosemu_regs.xregs[3]);
    case 0x34:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[15]);
      return (dosemu_regs.xregs[15]);
    case 0x35:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[4]);
      return (dosemu_regs.xregs[4]);
    case 0x36:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[5]);
      return (dosemu_regs.xregs[5]);
    case 0x37:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[6]);
      return (dosemu_regs.xregs[6]);
    case 0x3f:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[7]);
      return (dosemu_regs.xregs[7]);
    default:
      break;
    }
    break;
  case GRA_D:
    if (dosemu_regs.regs[GRAI] == 0x0d) {
      v_printf("ExtRead on GRA_D 0x0d got 0x%02x\n", dosemu_regs.xregs[13]);
      return (dosemu_regs.xregs[13]);
    }
    else if (dosemu_regs.regs[GRAI] == 0x0e) {
      v_printf("ExtRead on GRA_D 0x0e got 0x%02x\n", dosemu_regs.xregs[14]);
      return (dosemu_regs.xregs[14]);
    }
    break;
  case ATT_R:
    if (att_d_index == 0x16) {
      v_printf("ExtRead on ATT_R 0x16 got 0x%02x\n", dosemu_regs.xregs[9]);
      return (dosemu_regs.xregs[9]);
    }
    break;
  }
  v_printf("Bad Read on port 0x%04x\n", port);
  return (0);
}

void et4000_ext_video_port_out(ioport_t port, u_char value)
{
  switch (port) {
  case 0x3c3:
    v_printf("et4000 Ext write on port 0x3c3 = 0x%02x\n", value);
    dosemu_regs.xregs[17] = value;
    return;
  case 0x3bf:
    v_printf("et4000 Ext write on port 0x3bf = 0x%02x\n", value);
    dosemu_regs.xregs[12] = value;
    return;
  case 0x3cb:
    v_printf("et4000 Ext write on port 0x3cb = 0x%02x\n", value);
    dosemu_regs.xregs[10] = value;
    return;
  case 0x3cd:
    v_printf("et4000 Ext write on port 0x3cd = 0x%02x\n", value);
    dosemu_regs.xregs[0] = value;
    return;
  case 0x3d8:
    v_printf("et4000 Ext write on port 0x3d8 = 0x%02x\n", value);
    dosemu_regs.xregs[11] = value;
    return;
  case SEQ_D:
    if (dosemu_regs.regs[SEQI] == 0x07) {
      v_printf("et4000 SEQ_D Ext write at 0x07 of 0x%02x\n", value);
      dosemu_regs.xregs[8] = value;
      return;
    }
    else if (dosemu_regs.regs[SEQI] == 0x06) {
      v_printf("et4000 SEQ_D Ext write at 0x06 of 0x%02x\n", value);
      dosemu_regs.xregs[16] = value;
      return;
    }
    break;
  case CRT_DC:
  case CRT_DM:
    switch (dosemu_regs.regs[CRTI]) {
    case 0x24:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[1] = value;
      return;
    case 0x32:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[2] = value;
      return;
    case 0x33:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[3] = value;
      return;
    case 0x34:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[15] = value;
      return;
    case 0x35:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[4] = value;
      return;
    case 0x36:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[5] = value;
      return;
    case 0x37:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[6] = value;
      return;
    case 0x3f:
      v_printf("ExtWrite on CRT_D 0x%02x of 0x%02x\n", dosemu_regs.regs[CRTI], value);
      dosemu_regs.xregs[7] = value;
      return;
    default:
      break;
    }
    break;
  case GRA_D:
    if (dosemu_regs.regs[GRAI] == 0x0d) {
      v_printf("ExtWrite on GRA_D 0x0d of 0x%02x\n", value);
      dosemu_regs.xregs[13] = value;
      return;
    }
    else if (dosemu_regs.regs[GRAI] == 0x0e) {
      v_printf("ExtWrite on GRA_D 0x0e of 0x%02x\n", value);
      dosemu_regs.xregs[14] = value;
      return;
    }
    break;
  case ATT_IW:
    if (att_d_index == 0x16) {
      v_printf("ExtWrite on ATT_IW 0x16 of 0x%02x\n", value);
      dosemu_regs.xregs[9] = value;
      return;
    }
    break;
  default:
    break;
  }
  v_printf("Bad Write on port 0x%04x with value 0x%02x\n", port, value);
}

/* End of video/et4000.c */
