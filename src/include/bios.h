/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* miscellaneous BIOS stuff */

#ifndef BIOS_H
#define BIOS_H

#include "config.h"
#include "extern.h"

extern void bios_f000(void);		/* BIOS start at 0xf0000 */
/* these two addresses are needed to avoid overwriting e.g. font
 * tables copied from VBIOS */
extern void bios_f000_endpart1(void);
extern void bios_f000_part2(void);
extern void bios_f000_end(void);	/* BIOS end at 0xfffff */
extern void bios_f000_int10ptr(void);
extern void bios_f000_bootdrive(void);
extern void bios_f000_int10_old(void);

extern char LFN_string[];

#define INT2F_IDLE_MAGIC	0x1680

/*
 * symbols to access BIOS-data with meaningful names, not just addresses,
 * which are only numbers. The names are retranslatios from an old german
 * book :-(
 */

#define bios_base_address_com1          (*(unsigned short *) 0x400)
#define bios_base_address_com2          (*(unsigned short *) 0x402)
#define bios_base_address_com3          (*(unsigned short *) 0x404)
#define bios_base_address_com4          (*(unsigned short *) 0x406)
#define bios_address_lpt1               (*(unsigned short *) 0x408)
#define bios_address_lpt2               (*(unsigned short *) 0x40a)
#define bios_address_lpt3               (*(unsigned short *) 0x40c)
/* 0x40e is reserved */
#define bios_configuration              (*(unsigned short *) 0x410)
/* 0x412 is reserved */
#define bios_memory_size                (*(unsigned short *) 0x413)
/* #define bios_expansion_memory_size      (*(unsigned int   *) 0x415) */
#define bios_keyboard_state             (*(unsigned short *) 0x417)
#define bios_keyboard_leds              (*(unsigned char  *) 0x418)
#define bios_keyboard_token             (*(unsigned short *) 0x419)
/* used for keyboard input with Alt-Number */
#define bios_keyboard_buffer_head       (*(unsigned short *) 0x41a)
#define bios_keyboard_buffer_tail       (*(unsigned short *) 0x41c)
/* #define bios_keyboard_buffer            (*(unsigned int   *) 0x41e) */
#define bios_drive_active               (*(unsigned char  *) 0x43e)
#define bios_drive_running              (*(unsigned char  *) 0x43f)
#define bios_motor_nachlaufzeit         (*(unsigned char  *) 0x440)
#define bios_disk_status                (*(unsigned char  *) 0x441)
/* #define bios_fdc_result_buffer          (*(unsigned short *) 0x442) */
#define bios_video_mode                 (*(unsigned char  *) 0x449)
#define bios_screen_columns             (*(unsigned short *) 0x44a)
#define bios_video_memory_used          (*(unsigned short *) 0x44c)
#define bios_video_memory_address       (*(unsigned short *) 0x44e)

#define bios_cursor_x_position(screen) \
                        (*(unsigned char *)(0x450 + 2*(screen)))
#define bios_cursor_y_position(screen) \
                        (*(unsigned char *)(0x451 + 2*(screen)))

#define bios_cursor_shape               (*(unsigned short *) 0x460)
#define bios_cursor_last_line           (*(unsigned char  *) 0x460)
#define bios_cursor_first_line          (*(unsigned char  *) 0x461)
#define bios_current_screen_page        (*(unsigned char  *) 0x462)
#define bios_video_port                 (*(unsigned short *) 0x463)
#define bios_vdu_control                (*(         char  *) 0x465)
#define bios_vdu_color_register         (*(unsigned short *) 0x466)
/* 0x467-0x468 is reserved */
#define bios_timer                      (*(unsigned long  *) 0x46c)
#define bios_24_hours_flag              (*(unsigned char  *) 0x470)
#define bios_keyboard_flags             (*(unsigned char  *) 0x471)
#define bios_ctrl_alt_del_flag          (*(unsigned short *) 0x472)
#define bios_harddisk_count		(*(unsigned short *) 0x475)
/* 0x474, 0x476, 0x477 is reserved */
#define bios_lpt1_timeout               (*(unsigned char  *) 0x478)
#define bios_lpt2_timeout               (*(unsigned char  *) 0x479)
#define bios_lpt3_timeout               (*(unsigned char  *) 0x47a)
/* 0x47b is reserved */
#define bios_com1_timeout               (*(unsigned char  *) 0x47c)
#define bios_com2_timeout               (*(unsigned char  *) 0x47d)
/* 0x47e is reserved */
/* 0x47f-0x4ff is unknow for me */
#define bios_keyboard_buffer_start      (*(unsigned short *) 0x480)
#define bios_keyboard_buffer_end        (*(unsigned short *) 0x482)

#define bios_rows_on_screen_minus_1     (*(unsigned char  *) 0x484)
#define bios_font_height                (*(unsigned short *) 0x485)

#define bios_video_info_0               (*(unsigned char  *) 0x487)
#define bios_video_info_1               (*(unsigned char  *) 0x488)
#define bios_video_info_2               (*(unsigned char  *) 0x489)
#define bios_video_combo                (*(unsigned char  *) 0x48a)

#define bios_keyboard_flags2            (*(unsigned short *) 0x496)
#define bios_print_screen_flag          (*(unsigned short *) 0x500)

#define bios_video_saveptr              (*(unsigned long  *) 0x4a8)




#define READ_BYTE(addr)                 (*(Bit8u *) (addr))
#define WRITE_BYTE(addr, val)           (*(Bit8u *) (addr) = (val) )
#define READ_WORD(addr)                 (*(Bit16u *) (addr))
#define WRITE_WORD(addr, val)           (*(Bit16u *) (addr) = (val) )
#define READ_DWORD(addr)                (*(Bit32u *) (addr))
#define WRITE_DWORD(addr, val)          (*(Bit32u *) (addr) = (val) )

#define MEMCPY_2UNIX(unix_addr, dos_addr, n) \
	memcpy((unix_addr), (Bit8u *)(dos_addr), (n))

#define MEMCPY_2DOS(dos_addr, unix_addr, n) \
	memcpy((Bit8u *)(dos_addr), (unix_addr), (n))

#define MEMCPY_DOS2DOS(dos_addr, unix_addr, n) \
	memcpy((Bit8u *)(dos_addr), (Bit8u *)(unix_addr), (n))

#define MEMMOVE_DOS2DOS(dos_addr1, dos_addr2, n) \
        memmove((Bit8u *)(dos_addr1), (Bit8u *)(dos_addr2), (n))

#define MEMCMP_DOS_VS_UNIX(dos_addr, unix_addr, n) \
	memcmp((Bit8u *)(dos_addr), (Bit8u *)(unix_addr), (n))

#define MEMSET_DOS(dos_addr, val, n) \
        memset((Bit8u *)(dos_addr), (val), (n))



#define BIOS_BASE_ADDRESS_COM1          0x400
#define BIOS_BASE_ADDRESS_COM2          0x402
#define BIOS_BASE_ADDRESS_COM3          0x404
#define BIOS_BASE_ADDRESS_COM4          0x406
#define BIOS_ADDRESS_LPT1               0x408
#define BIOS_ADDRESS_LPT2               0x40a
#define BIOS_ADDRESS_LPT3               0x40c
/* 0x40e is reserved */
#define BIOS_CONFIGURATION              0x410
/* 0x412 is reserved */
#define BIOS_MEMORY_SIZE                0x413
/* #define bios_expansion_memory_size      (*(unsigned int   *) 0x415) */
#define BIOS_KEYBOARD_STATE             0x417
#define BIOS_KEYBOARD_FLAGS1            BIOS_KEYBOARD_STATE
#define BIOS_KEYBOARD_FLAGS2            0x418
#define BIOS_KEYBOARD_TOKEN             0x419
/* used for keyboard input with Alt-Number */
#define BIOS_KEYBOARD_BUFFER_HEAD       0x41a
#define BIOS_KEYBOARD_BUFFER_TAIL       0x41c
#define BIOS_KEYBOARD_BUFFER            0x41e
/* #define bios_keyboard_buffer            (*(unsigned int   *) 0x41e) */
#define BIOS_DRIVE_ACTIVE               0x43e
#define BIOS_DRIVE_RUNNING              0x43f
#define BIOS_MOTOR_NACHLAUFZEIT         0x440
#define BIOS_DISK_STATUS                0x441
/* #define bios_fdc_result_buffer          (*(unsigned short *) 0x442) */
#define BIOS_VIDEO_MODE                 0x449
#define BIOS_SCREEN_COLUMNS             0x44a
#define BIOS_VIDEO_MEMORY_USED          0x44c
#define BIOS_VIDEO_MEMORY_ADDRESS       0x44e

#define set_bios_cursor_x_position(screen, val) \
                        WRITE_BYTE(0x450 + 2*(screen), (val))
#define get_bios_cursor_x_position(screen) \
                        READ_BYTE(0x450 + 2*(screen))


#define set_bios_cursor_y_position(screen, val) \
                        WRITE_BYTE(0x451 + 2*(screen), (val))
#define get_bios_cursor_y_position(screen) \
                        READ_BYTE(0x451 + 2*(screen))


#define BIOS_CURSOR_SHAPE               0x460
#define BIOS_CURSOR_LAST_LINE           0x460
#define BIOS_CURSOR_FIRST_LINE          0x461
#define BIOS_CURRENT_SCREEN_PAGE        0x462
#define BIOS_VIDEO_PORT                 0x463
#define BIOS_VDU_CONTROL                0x465
#define BIOS_VDU_COLOR_REGISTER         0x466
/* 0x467-0x468 is reserved */
#define BIOS_TIMER                      0x46c
#define BIOS_24_HOURS_FLAG              0x470
#define BIOS_KEYBOARD_FLAGS             0x471
#define BIOS_CTRL_ALT_DEL_FLAG          0x472
#define BIOS_HARDDISK_COUNT		0x475
/* 0x474, 0x476, 0x477 is reserved */
#define BIOS_LPT1_TIMEOUT               0x478
#define BIOS_LPT2_TIMEOUT               0x479
#define BIOS_LPT3_TIMEOUT               0x47a
/* 0x47b is reserved */
#define BIOS_COM1_TIMEOUT               0x47c
#define BIOS_COM2_TIMEOUT               0x47d
/* 0x47e is reserved */
/* 0x47f-0x4ff is unknow for me */
#define BIOS_KEYBOARD_BUFFER_START      0x480
#define BIOS_KEYBOARD_BUFFER_END        0x482

#define BIOS_ROWS_ON_SCREEN_MINUS_1     0x484
#define BIOS_FONT_HEIGHT                0x485

#define BIOS_VIDEO_INFO_0               0x487
#define BIOS_VIDEO_INFO_1               0x488
#define BIOS_VIDEO_INFO_2               0x489
#define BIOS_VIDEO_COMBO                0x48a

#define BIOS_KEYBOARD_FLAGS3            0x496
#define BIOS_KEYBOARD_LEDS              0x497
#define BIOS_PRINT_SCREEN_FLAG          0x500

#define BIOS_VIDEO_SAVEPTR              0x4a8


#define BIT(x)  	(1<<x)

/*
 * int 11h config single bit tests
 */

#define CONF_FLOP	BIT(0)
#define CONF_MATHCO	BIT(1)
#define CONF_MOUSE	BIT(2)
#define CONF_DMA	BIT(8)
#define CONF_GAME	BIT(12)

/*
 * don't use CONF_NSER with num > 4, CONF_NLPT with num > 3, CONF_NFLOP
 * with num > 4
 */
#define CONF_NSER(c,num)	{c&=~(BIT(9)|BIT(10)|BIT(11)); c|=(num<<9);}
#define CONF_NLPT(c,num) 	{c&=~(BIT(14)|BIT(15)); c|=(num<<14);}
#define CONF_NFLOP(c,num) 	{c&=~(CONF_FLOP|BIT(6)|BIT(7)); \
				   if (num) c|=((num-1)<<6)|CONF_FLOP;}

EXTERN unsigned int configuration INIT(0);	/* The virtual
						 * BIOS-configuration */

void            setup_rom_bios(void);
void            bios_configuration_init(void);	/* init BIOS-configuration */
void            bios_data_init(void);	/* init BIOS-data-areas */

void            INT16_dummy_start(void);
void            INT16_dummy_end(void);
#if 0
void            INT09_dummy_start(void);
void            INT09_dummy_end(void);
#endif
void            INT08_dummy_start(void);
void            INT08_dummy_end(void);
void            INT70_dummy_start(void);
void            INT70_dummy_end(void);
void            DPMI_dummy_start(void);
void            DPMI_dummy_end(void);
void            DPMI_dpmi_init(void);
void            DPMI_return_from_dos(void);
void            DPMI_return_from_dos_exec(void);
void            DPMI_return_from_dosint(void);
void            DPMI_return_from_realmode(void);
void            DPMI_return_from_dos_memory(void);
void            DPMI_realmode_callback(void);
void            DPMI_mouse_callback(void);
void            DPMI_raw_mode_switch(void);
void            DPMI_save_restore(void);
void            DPMI_API_extension(void);
void            DPMI_return_from_pm(void);
void            DPMI_return_from_exception(void);
void            DPMI_return_from_rm_callback(void);
void            DPMI_return_from_mouse_callback(void);
void            DPMI_exception(void);
void            DPMI_interrupt(void);

void		bios_IPX_PopRegistersReturn(void);
void		bios_IPX_PopRegistersIRet(void);
void		bios_IPX_FarCall(void);

/* various declarations for interfacing with the packet driver code in
   bios.S */

void		PKTDRV_driver_name(void);
void		PKTDRV_param(void);
void		PKTDRV_stats(void);
void		PKTDRV_start(void);

/*
 * HLT block
 */

#define BIOS_HLT_BLK       0xfc000
#define BIOS_HLT_BLK_SIZE  0x01000

#endif				/* BIOS_H */
