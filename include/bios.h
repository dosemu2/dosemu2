/* miscellaneous BIOS stuff */

#ifndef BIOS_H
#define BIOS_H

#define INT2F_IDLE_MAGIC	0x1680

/* symbols to access BIOS-data with meaningful names, not just
 * addresses, which are only numbers.
 * The names are retranslatios from an old german book :-(
 */

#define bios_base_address_com1          (*(unsigned short *) 0x400)
#define bios_base_address_com2          (*(unsigned short *) 0x402)
                                      /* 0x404-0x407 is reserved */
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
#define bios_keyboard_flags2            (*(unsigned short *) 0x496)
#define bios_print_screen_flag          (*(unsigned short *) 0x500)


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

#endif
