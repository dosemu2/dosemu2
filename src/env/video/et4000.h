extern void vga_init_et4000(void);

extern u_char et4000_ext_video_port_in(int port);
extern void et4000_ext_video_port_out(u_char value, int port); 

extern void et4000_set_bank_read(u_char bank);
extern void et4000_set_bank_write(u_char bank);

extern void et4000_save_ext_regs(u_char xregs[], u_short xregs16[]);
extern void et4000_restore_ext_regs(u_char xregs[], u_short xregs16[]);

