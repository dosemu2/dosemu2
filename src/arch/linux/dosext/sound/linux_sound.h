/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * This sets the number & size of the buffers in the Linux sound driver
 *
 * 0x00020008  - The value Hannu notes in his explaination
 * 0x00FF0008  - Try more fragments
 *
 * See /usr/src/linux/drivers/sound/experimental.txt for full details 
 */

#define MAX_NUM_FRAGMENTS       0x20
#define MIN_NUM_FRAGMENTS       0x4

/* 
 * milliseconds to buffer sound. It should be high enough to avoid
 * clicking, but low enough to not delay the sound to much 
 */
#define BUFFER_MSECS		60

void linux_sb_mixer_write_setting (int ch, uint8_t val);
uint8_t linux_sb_mixer_read_setting(int ch);

int linux_sb_get_version(void);

void linux_sb_disable_speaker(void);
void linux_sb_enable_speaker (void);

int linux_sb_set_speed (uint16_t speed, uint8_t stereo_mode, uint8_t is_16bit, uint8_t is_signed);

int linux_sb_dma_start_init(int read);

size_t linux_sb_do_read(void *ptr, size_t size);
size_t linux_sb_do_write(void *ptr, size_t size);

int  linux_sb_dma_complete_test(void);
int  linux_sb_dma_is_empty(void);
int  linux_sb_dma_get_free_space(void);

void linux_sb_dma_complete(void);

int linux_mpu401_data_read(uint8_t data[], int max_len);
void linux_mpu401_data_write(uint8_t data);
void linux_mpu401_register_callback(void (*io_callback)(void));

int linux_sb_get_free_fragments(int *total, int *free, int *bytes);
