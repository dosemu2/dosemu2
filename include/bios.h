/* miscellaneous BIOS stuff */

#ifndef BIOS_H
#define BIOS_H

#define INT2F_IDLE_MAGIC	0x1680

/* symbols to access BIOS-data with meaningful names, not just
 * addresses, which are only numbers.
 * The names are retranslatios from an old german book :-(
 */

#ifdef X86_EMULATOR
#include "x86_emulator.h"
#endif

#ifndef X86_EMULATOR_CHECK
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
                                      /* 0x474-0x477 is reserved */
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
#endif /* X86_EMULATOR_CHECK */




#ifndef X86_EMULATOR

typedef unsigned char  ubyte_t;
typedef unsigned short uword_t;
typedef unsigned long  udword_t;

#define READ_BYTE(addr)                 (*(ubyte_t *) (addr))
#define WRITE_BYTE(addr, val)           (*(ubyte_t *) (addr) = (val) )
#define READ_WORD(addr)                 (*(uword_t *) (addr))
#define WRITE_WORD(addr, val)           (*(uword_t *) (addr) = (val) )
#define READ_DWORD(addr)                (*(udword_t *) (addr))
#define WRITE_DWORD(addr, val)          (*(udword_t *) (addr) = (val) )

#define MEMCPY_2UNIX(unix_addr, dos_addr, n) \
	memcpy(unix_addr, dos_addr, n)

#define MEMCPY_2DOS(dos_addr, unix_addr, n) \
	memcpy(dos_addr, unix_addr, n)

#define MEMCPY_DOS2DOS(dos_addr, unix_addr, n) \
	memcpy(dos_addr, unix_addr, n)

#define MEMCMP_DOS_VS_UNIX(dos_addr, unix_addr, n) \
	memcmp(dos_addr, unix_addr, n)

#endif


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
#define BIOS_KEYBOARD_LEDS              0x418
#define BIOS_KEYBOARD_TOKEN             0x419
      /* used for keyboard input with Alt-Number */
#define BIOS_KEYBOARD_BUFFER_HEAD       0x41a
#define BIOS_KEYBOARD_BUFFER_TAIL       0x41c
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
                                      /* 0x474-0x477 is reserved */
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

#define BIOS_KEYBOARD_FLAGS2            0x496
#define BIOS_PRINT_SCREEN_FLAG          0x500

#define BIOS_VIDEO_SAVEPTR              0x4a8
    

#define BIT(x)  	(1<<x)

/* int 11h config single bit tests
 */

#define CONF_FLOP	BIT(0)
#define CONF_MATHCO	BIT(1)
#define CONF_MOUSE	BIT(2)
#define CONF_DMA	BIT(8)
#define CONF_GAME	BIT(12)

/* don't use CONF_NSER with num > 4, CONF_NLPT with num > 3, CONF_NFLOP
 * with num > 4
 */
#define CONF_NSER(c,num)	{c&=~(BIT(9)|BIT(10)|BIT(11)); c|=(num<<9);}
#define CONF_NLPT(c,num) 	{c&=~(BIT(14)|BIT(14)); c|=(num<<14);}
#define CONF_NFLOP(c,num) 	{c&=~(CONF_FLOP|BIT(6)|BIT(7)); \
				   if (num) c|=((num-1)<<6)|CONF_FLOP;}

extern unsigned int configuration;  /* The virtual BIOS-configuration */

void setup_rom_bios(void);
void bios_configuration_init(void);        /* init BIOS-configuration */
void bios_data_init(void);                 /* init BIOS-data-areas */

void INT16_dummy_start();
void INT16_dummy_end();
void INT09_dummy_start();
void INT09_dummy_end();
void INT08_dummy_start();
void INT08_dummy_end();
void DPMI_dummy_start();
void DPMI_dummy_end();
void DPMI_dpmi_init();
void DPMI_return_from_dosint();
void DPMI_return_from_realmode();
void DPMI_raw_mode_switch();
void DPMI_save_restore();
void DPMI_API_extension();
void DPMI_return_from_pm();
void DPMI_exception();
void DPMI_interrupt();

#endif /* BIOS_H */
