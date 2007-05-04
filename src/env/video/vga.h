/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 
 * video/vga.h - prototypes for VGA-card specific functions
 */

#ifndef DOSEMU_VGA_H
#define DOSEMU_VGA_H

EXTERN void (*save_ext_regs) (u_char xregs[], u_short xregs16[]);
EXTERN void (*restore_ext_regs) (u_char xregs[], u_short xregs16[]);
EXTERN void (*set_bank_read) (unsigned char bank);
EXTERN void (*set_bank_write) (unsigned char bank);
EXTERN void (*ext_video_port_out) (ioport_t port, unsigned char value);

EXTERN u_char(*ext_video_port_in) (ioport_t port);

void save_vga_state(struct video_save_struct *save_regs);
void restore_vga_state(struct video_save_struct *save_regs);
void do_int10_callback(struct vm86_regs *regs);

#endif
/* End of video/vga.h */
