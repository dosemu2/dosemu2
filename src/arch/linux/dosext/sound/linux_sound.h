/*
 * This sets the number & size of the buffers in the Linux sound driver
 *
 * 0x00020008  - The value Hannu notes in his explaination
 * 0x00FF0008  - Try more fragments
 *
 * See /usr/src/linux/drivers/sound/experimental.txt for full details 
 */

/* #define SOUND_FRAG 0x0002000F */
#define SOUND_FRAG 0x0002000c
#define SOUND_SIZE 1<<0xc

void linux_sb_mixer_write_setting (int ch, __u8 val);
__u8 linux_sb_mixer_read_setting(int ch);

int linux_sb_get_version(void);

void linux_sb_disable_speaker(void);
void linux_sb_enable_speaker (void);

void linux_sb_set_speed (__u16 speed, __u8 stereo_mode);

void linux_sb_dma_start_init(__u32 command);
void linux_sb_dma_start_complete(void);

void linux_sb_dma_complete(void);

void linux_mpu401_data_write(__u8 data);
