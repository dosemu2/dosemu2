/* 
 * video/vga.h - prototypes for VGA-card specific functions
 */

#ifndef VGA_H
#define VGA_H

void (*save_ext_regs) (u_char xregs[], u_short xregs16[]);
void (*restore_ext_regs) (u_char xregs[], u_short xregs16[]);
void (*set_bank_read) (unsigned char bank);
void (*set_bank_write) (unsigned char bank);
void (*ext_video_port_out) (unsigned char value, int port);

u_char(*ext_video_port_in) (int port);

int vga_screenoff(void);
int vga_screenon(void);
int vga_setpalvec(int start, int num, u_char * pal);
int vga_getpalvec(int start, int num, u_char * pal);
__inline__ void disable_vga_card(void);
__inline__ void enable_vga_card(void);
int store_vga_regs(char regs[]);
void store_vga_mem(u_char * mem, u_char mem_size[], u_char banks);
void restore_vga_mem(u_char * mem, u_char mem_size[], u_char banks);
int restore_vga_regs(char regs[], u_char xregs[], u_short xregs16[]);
void save_vga_state(struct video_save_struct *save_regs);
void restore_vga_state(struct video_save_struct *save_regs);
void vga_initialize(void);
void dump_video_regs(void);
void save_ext_regs_dummy(u_char xregs[], u_short xregs16[]);
void restore_ext_regs_dummy(u_char xregs[], u_short xregs16[]);
void set_bank_read_dummy(u_char bank);
void set_bank_write_dummy(u_char bank);
u_char dummy_ext_video_port_in(int port);
void dummy_ext_video_port_out(u_char value, int port);

#endif
/* End of video/vga.h */
