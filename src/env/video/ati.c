/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * video/ati.c
 *
 * This file contains support for VGA-cards with an ati chip.
 *
 */

#define ATI_C

#include <stdio.h>
#include <unistd.h>

#include "emu.h"        /* for v_printf only */
#include "port.h"
#include "video.h"
#include "vc.h"
#include "vga.h"
#include "ati.h"

/* These are ati specific functions */

unsigned char ati_extregs[]=
{                   0xa3,             0xa6, 0xa7,                   0xab, 0xac, 0xad, 0xae, 
  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,       0xb8, 0xb9, 0xba,             0xbd, 0xbe, 0xbf
};

#define ATI_EXTREGS sizeof(ati_extregs)

void ati_save_ext_regs(u_char xregs[], u_short xregs16[])
{
  int i;

  emu_video_retrace_off();
  for (i=0; i<ATI_EXTREGS; i++)
  { 
    port_out(ati_extregs[i], 0x1ce);
    xregs[1+i] = port_in(0x1cf) & 0xff;
  }
  emu_video_retrace_on();
}

void ati_restore_ext_regs(u_char xregs[], u_short xregs16[])
{
  int i;

  emu_video_retrace_off();
  for (i=0; i<ATI_EXTREGS; i++)
  { 
    port_out_w(ati_extregs[i]+((int)xregs[1+i]<<8), 0x1ce);
  }
  emu_video_retrace_on();
}

void ati_set_bank_read(u_char bank)
{
  /* FIXME */
}

void ati_set_bank_write(u_char bank)
{
  /* FIXME */
}

/*
 * Initialize the function pointer to point to the et4000-specific
 * functions implemented here.
 */

void vga_init_ati(void)
{
  save_ext_regs = ati_save_ext_regs;
  restore_ext_regs = ati_restore_ext_regs;
  set_bank_read = ati_set_bank_read;
  set_bank_write = ati_set_bank_write;
  ext_video_port_in = ati_ext_video_port_in;
  ext_video_port_out = ati_ext_video_port_out;
  return;
}

u_char ati_ext_video_port_in(ioport_t port)
{
/* FIXME - bad v_print messages */

  switch (port) {
  case 0x1ce:
      return (dosemu_regs.xregs[0]); 
  case 0x1cf:
    switch (dosemu_regs.xregs[0]) {
    case 0xa3:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[1]);
    case 0xa6:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[2]);
    case 0xa7:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[3]);
    case 0xab:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[4]);
    case 0xac:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[5]);
    case 0xad:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[6]);
    case 0xae:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[7]);
    case 0xb0:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[8]);
    case 0xb1:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[9]);
    case 0xb2:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[10]);
    case 0xb3:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[11]);
    case 0xb4:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[12]);
    case 0xb5:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[13]);
    case 0xb6:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[14]);
    case 0xb8:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[15]);
    case 0xb9:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[16]);
    case 0xba:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[17]);
    case 0xbd:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[18]);
    case 0xbe:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[19]);
    case 0xbf:
      v_printf("ExtRead on CRT_D 0x%02x got 0x%02x\n", dosemu_regs.regs[CRTI], dosemu_regs.xregs[1]);
      return (dosemu_regs.xregs[20]);
    default:
      break;
    }
    break;
  }
  v_printf("Bad Read on port 0x%04x\n", port);
  return (0);
}

void ati_ext_video_port_out(ioport_t port, u_char value)
{
  switch (port) {
  case 0x1ce:
    v_printf("ati Ext write on port 0x1ce = 0x%02x\n", value);
    dosemu_regs.xregs[0] = value;
    return;
  case 0x1cf:
    switch (dosemu_regs.xregs[0]) {
    case 0xa3:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[1] = value;
      return;
    case 0xa6:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[2] = value;
      return;
    case 0xa7:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[3] = value;
      return;
    case 0xab:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[4] = value;
      return;
    case 0xac:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[5] = value;
      return;
    case 0xad:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[6] = value;
      return;
    case 0xae:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[7] = value;
      return;
    case 0xb0:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[8] = value;
      return;
    case 0xb1:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[9] = value;
      return;
    case 0xb2:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[10] = value;
      return;
    case 0xb3:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[11] = value;
      return;
    case 0xb4:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[12] = value;
      return;
    case 0xb5:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[13] = value;
      return;
    case 0xb6:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[14] = value;
      return;
    case 0xb8:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[15] = value;
      return;
    case 0xb9:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[16] = value;
      return;
    case 0xba:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[17] = value;
      return;
    case 0xbd:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[18] = value;
      return;
    case 0xbe:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[19] = value;
      return;
    case 0xbf:
      v_printf("ExtWrite on 0x1ce: 0x%02x of 0x%02x\n", dosemu_regs.xregs[0], value);
      dosemu_regs.xregs[20] = value;
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

/* End of video/ati.c */
