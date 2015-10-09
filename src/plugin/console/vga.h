/*
 * video/vga.h - prototypes for VGA-card specific functions
 */

#ifndef DOSEMU_VGA_H
#define DOSEMU_VGA_H

extern void (*save_ext_regs) (u_char xregs[], u_short xregs16[]);
extern void (*restore_ext_regs) (u_char xregs[], u_short xregs16[]);
extern void (*set_bank_read) (unsigned char bank);
extern void (*set_bank_write) (unsigned char bank);
extern void (*ext_video_port_out) (ioport_t port, unsigned char value);
extern u_char(*ext_video_port_in) (ioport_t port);

void save_vga_state(struct video_save_struct *save_regs);
void restore_vga_state(struct video_save_struct *save_regs);

int emu_video_retrace_on(void);
int emu_video_retrace_off(void);

#endif
/* End of video/vga.h */
