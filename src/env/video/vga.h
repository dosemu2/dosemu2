/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 
 * video/vga.h - prototypes for VGA-card specific functions
 */

#ifndef VGA_H
#define VGA_H

#include "extern.h"

EXTERN void (*save_ext_regs) (u_char xregs[], u_short xregs16[]);
EXTERN void (*restore_ext_regs) (u_char xregs[], u_short xregs16[]);
EXTERN void (*set_bank_read) (unsigned char bank);
EXTERN void (*set_bank_write) (unsigned char bank);
EXTERN void (*ext_video_port_out) (ioport_t port, unsigned char value);

EXTERN u_char(*ext_video_port_in) (ioport_t port);

int dosemu_vga_screenoff(void);
int dosemu_vga_screenon(void);
int dosemu_vga_setpalvec(int start, int num, u_char * pal);
int dosemu_vga_getpalvec(int start, int num, u_char * pal);
__inline__ void disable_vga_card(void);
__inline__ void enable_vga_card(void);
int store_vga_regs(char regs[]);
void store_vga_mem(u_char * mem, u_char mem_size[], u_char banks);
void restore_vga_mem(u_char * mem, u_char mem_size[], u_char banks);
int restore_vga_regs(char regs[], u_char xregs[], u_short xregs16[]);
void save_vga_state(struct video_save_struct *save_regs);
void restore_vga_state(struct video_save_struct *save_regs);
int vga_initialize(void);
#if 0
void mda_initialize(void);
#endif
void dump_video_regs(void);
void save_ext_regs_dummy(u_char xregs[], u_short xregs16[]);
void restore_ext_regs_dummy(u_char xregs[], u_short xregs16[]);
void set_bank_read_dummy(u_char bank);
void set_bank_write_dummy(u_char bank);
unsigned char dummy_ext_video_port_in(ioport_t port);
void dummy_ext_video_port_out(ioport_t port, unsigned char value);

#endif
/* End of video/vga.h */
