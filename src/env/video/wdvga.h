/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

extern void vga_init_wd(void);

extern u_char wd_ext_video_port_in(ioport_t port);
extern void wd_ext_video_port_out(ioport_t port, u_char value);

extern void wd_set_bank_read(u_char bank);
extern void wd_set_bank_write(u_char bank);

extern void wd_save_ext_regs(u_char xregs[], u_short xregs16[]);
extern void wd_restore_ext_regs(u_char xregs[], u_short xregs16[]);

extern void wd_set_old_regs(void);
extern void wd_set_new_regs(void);
extern void wd_allow_svga(void);
extern void wd_disallow_svga(void);
extern u_char wd_get_svga(void);


