/* 
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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

/* 
 * milliseconds to buffer sound. It should be high enough to avoid
 * clicking, but low enough to not delay the sound to much 
 */
#define BUFFER_MSECS		60

void linux_sb_mixer_write_setting (int ch, __u8 val);
__u8 linux_sb_mixer_read_setting(int ch);

int linux_sb_get_version(void);

void linux_sb_disable_speaker(void);
void linux_sb_enable_speaker (void);

void linux_sb_set_speed (__u16 speed, __u8 stereo_mode);

void linux_sb_dma_start_init(__u32 command);
void linux_sb_dma_start_complete(void);

int  linux_sb_dma_complete_test(void);

void linux_sb_dma_complete(void);

void linux_mpu401_data_write(__u8 data);

int linux_sb_get_free_fragments(int *total, int *free);
