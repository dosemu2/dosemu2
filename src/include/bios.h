/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
extern char bios_in_int10_callback;

extern char LFN_string[];

#define INT2F_IDLE_MAGIC	0x1680

/*
 * symbols to access BIOS-data with meaningful names, not just addresses,
 * which are only numbers. The names are retranslatios from an old german
 * book :-(
 */

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
#define BIOS_MOTOR_TIMEOUT              0x440
#define BIOS_DISK_STATUS                0x441
/* #define bios_fdc_result_buffer          (*(unsigned short *) 0x442) */
#define BIOS_VIDEO_MODE                 0x449
#define BIOS_SCREEN_COLUMNS             0x44a
#define BIOS_VIDEO_MEMORY_USED          0x44c
#define BIOS_VIDEO_MEMORY_ADDRESS       0x44e

#define set_bios_cursor_x_position(screen, val) \
                        LOWMEM_WRITE_BYTE(0x450 + 2*(screen), (val))
#define get_bios_cursor_x_position(screen) \
                        LOWMEM_READ_BYTE(0x450 + 2*(screen))


#define set_bios_cursor_y_position(screen, val) \
                        LOWMEM_WRITE_BYTE(0x451 + 2*(screen), (val))
#define get_bios_cursor_y_position(screen) \
                        LOWMEM_READ_BYTE(0x451 + 2*(screen))


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

void            bios_setup_init(void);

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
void            DPMI_return_from_dosint(void);
void            DPMI_return_from_realmode(void);
void            DPMI_return_from_dos_memory(void);
void            DPMI_raw_mode_switch_rm(void);
void            DPMI_save_restore_rm(void);

void            MSDOS_srm_start(void);
void            MSDOS_mouse_callback(void);
void            MSDOS_PS2_mouse_callback(void);
void            MSDOS_srm_end(void);
void            MSDOS_rpm_start(void);
void            MSDOS_return_from_rm(void);
void            MSDOS_rpm_end(void);

/* various declarations for interfacing with the packet driver code in
   bios.S */

void		PKTDRV_driver_name(void);
void		PKTDRV_param(void);
void		PKTDRV_stats(void);
void		PKTDRV_start(void);

#endif				/* BIOS_H */
