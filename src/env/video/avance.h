/* Derived from cirrus.h */

extern void vga_init_avance(void);

extern u_char avance_ext_video_port_in(int port);
extern void avance_ext_video_port_out(u_char value, int port);

extern void avance_set_bank_read(u_char bank);
extern void avance_set_bank_write(u_char bank);

extern void avance_save_ext_regs(u_char xregs[], u_short xregs16[]);
extern void avance_restore_ext_regs(u_char xregs[], u_short xregs16[]);
