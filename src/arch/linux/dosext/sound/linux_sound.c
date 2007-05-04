/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* Copyright 1995  Joel N. Weber II
   See the file README.sound in this directory for more information */

/*
 * This Driver has extensively modified from the original driver.
 */

/* 
 * DANG_BEGIN_MODULE
 *
 * Soundblaster emulation. Very incomplete. Linux Driver.
 *
 * maintainer:
 * Alistair MacDonald <alistair@slitesys.demon.co.uk>
 * David Brauman <crisk@netvision.net.il>
 * Michael Karcher <Michael.Karcher@writeme.com>
 *
 * DANG_END_MODULE
 */

/*
 * modified 11/05/95 by Michael Beck
 *  added some more (undocumented ?) SB commands, version detection
 */

#include <unistd.h>

#include "emu.h" /* For S_printf */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
/* #include <sys/audioio.h> */

#include <errno.h>

#include "pic.h"
#include "dma.h"
#include "sound.h"
#include "linux_sound.h"

/* SB static vars */
static int mixer_fd = -1;
static int dsp_fd = -1;
static int dsp_fd_read = -1;
static int dsp_fd_write = -1;

/* Old variables - Obselete - AM */
static long int block_size = 0;
/*static long int sound_frag = 0xc;*/

/* New fragment control - AM */
static int sound_frag_size = 0x9; /* ie MAX_DMA_TRANSFERSIZE (== 512) */
static int num_sound_frag  = MAX_NUM_FRAGMENTS;
static int bits_per_samp = 0;
static int sample_rate = 0;
static int num_channels = 0;
static int oss_block_size = 0;

/* MPU static vars */
static int mpu_fd = -1;	             /* -1 = closed */
static int mpu_in_fd = -1;	             /* -1 = closed */
static boolean mpu_disabled = FALSE; /* TRUE if MIDI output disabled */

static void linux_sb_dma_set_blocksize(int blocksize, int fragsize)
{
  int blockbits, oss_fragsize;
  
  if(fragsize > MAX_DMA_TRANSFERSIZE) {
    oss_fragsize = MAX_DMA_TRANSFERSIZE;
  } else {
    oss_fragsize = fragsize;
  }
  block_size = oss_fragsize;

  for (blockbits = 0; oss_fragsize > 1; 
       blockbits++, oss_fragsize = oss_fragsize >> 1);
  /* blockbits now contains floor(log2(oss_fragsize)), and oss_fragsize is
      destroyed */

  sound_frag_size = blockbits;
  if (sound_frag_size < 4)	/* not all drivers supports size < 16b */
    sound_frag_size = 4;

  num_sound_frag = blocksize / (1 << sound_frag_size);
  
  if (num_sound_frag > config.oss_max_frags) {
    num_sound_frag = config.oss_max_frags;
  }
  if (num_sound_frag < config.oss_min_frags) {
    num_sound_frag = config.oss_min_frags;
  }

  S_printf ("SB:[Linux] DMA blocksize set to %u (%u,%u)\n",
    num_sound_frag * (1 << sound_frag_size), sound_frag_size, num_sound_frag);
}

static void linux_sb_write_mixer(int ch, uint8_t val)
{
  int newsetting, real_mixer_val;
  uint8_t driver_channel = -1;

  if (strcmp(config.sb_mixer, "") == 0) {
    S_printf("SB [Linux]: Warning: Mixer is not configured.\n");
    return;
  }

  switch (ch) {
  case SB_MIXER_VOLUME: 
    driver_channel = SOUND_MIXER_VOLUME;
    break;
  case SB_MIXER_PCM: 
    driver_channel = SOUND_MIXER_PCM;
    break;
  case SB_MIXER_SYNTH: 
    driver_channel = SOUND_MIXER_SYNTH;
    break;
  case SB_MIXER_CD: 
    driver_channel = SOUND_MIXER_CD;
    break;
  case SB_MIXER_LINE: 
    driver_channel = SOUND_MIXER_LINE;
    break;
  case SB_MIXER_MIC: 
    /* Special Case: No value */
    break;
  }

  if (mixer_fd == -1)
    if ((mixer_fd = open(config.sb_mixer, O_RDWR)) < 0) {
      S_printf("SB [Linux]: Warning: can't open mixer device\n");
      return;
    }

  if (ioctl(mixer_fd, MIXER_READ(driver_channel), &real_mixer_val)<0)
    real_mixer_val = 0;

  if (ch != SB_MIXER_MIC) {
    newsetting = ((val & 0xF0) >> 2) | ((val & 0x0F) << 10) |
      (real_mixer_val & 0xC3C3);	/* magic mixer code :) */
  }
  else {
    newsetting = ((val & 0x7) * 0x0C0C);
  }
  S_printf ("SB:[Linux] Writing to the Mixer (%u, 0x%X)\n", ch, newsetting);
    if (ioctl(mixer_fd, MIXER_WRITE(driver_channel), &newsetting)<0)
       S_printf ("SB:[Linux] Warning: ioctl() failed: %s\n", strerror(errno));

  close(mixer_fd);
  mixer_fd = -1;
}

static uint8_t linux_sb_read_mixer(int ch)
{
  int x;
  uint8_t driver_channel = -1;
  uint8_t sb_mixer_value;

  if (strcmp(config.sb_mixer, "") == 0) {
    S_printf("SB [Linux]: Warning: Mixer is not configured.\n");
    return 0;
  }

  switch (ch) {
  case SB_MIXER_VOLUME: 
    driver_channel = SOUND_MIXER_VOLUME;
    break;
  case SB_MIXER_PCM: 
    driver_channel = SOUND_MIXER_PCM;
    break;
  case SB_MIXER_SYNTH: 
    driver_channel = SOUND_MIXER_SYNTH;
    break;
  case SB_MIXER_CD: 
    driver_channel = SOUND_MIXER_CD;
    break;
  case SB_MIXER_LINE: 
    driver_channel = SOUND_MIXER_LINE;
    break;
  case SB_MIXER_MIC: 
    driver_channel = SOUND_MIXER_MIC;
    break;
  }

  if (mixer_fd == -1)
    if ((mixer_fd = open(config.sb_mixer, O_RDWR)) < 0) {
      S_printf("SB [Linux]: Warning: can't open mixer device\n");
      return 0;
    }

  if (ioctl(mixer_fd, MIXER_READ(driver_channel), &x)<0)
    S_printf ("SB:[Linux] Warning: ioctl() failed: %s\n", strerror(errno));
  S_printf ("SB:[Linux] Reading from the Mixer (%u -> 0x%X)\n", ch, x);

  close(mixer_fd);
  mixer_fd = -1;

  if (ch != SB_MIXER_MIC)
    sb_mixer_value = (((x & 0x003C) << 2) | ((x & 0x3C00) >> 10));
  else
    sb_mixer_value = x / 7; /* This isn't the right value.  Anyone care to tell me
                    		how to do division in C???? (- Joel)*/
  return sb_mixer_value;
}

/*
 * Speaker Functions
 */

void linux_sb_disable_speaker(void)
{
  if (dsp_fd_write != -1)
  {
    S_printf ("SB:[Linux] Syncing DSP\n");
    /* don't use blocking ioctl's like DSP_SYNC!!! */
    ioctl(dsp_fd_write, SNDCTL_DSP_SYNC);
    close (dsp_fd_write);
    dsp_fd_write = -1;
  } else {
    S_printf ("SB:[Linux] DSP already closed\n");
  }
  dsp_fd = dsp_fd_write;
}

void linux_sb_enable_speaker (void)
{
  oss_block_size = 0;
  bits_per_samp = 0;
  sample_rate = 0;
  num_channels = 0;
  if (dsp_fd_read != -1) {
    close(dsp_fd_read);
    dsp_fd_read = -1;
  }
  if (dsp_fd_write == -1)
    if ((dsp_fd_write = RPT_SYSCALL(open(config.sb_dsp, O_WRONLY | O_NONBLOCK))) < 0)
      S_printf ("SB:[Linux] Failed to initiate connection to %s (%s)\n",
        config.sb_dsp, strerror(errno));
  dsp_fd = dsp_fd_write;
}

static int linux_set_OSS_fragsize (int frag_value)
{
    if (ioctl (dsp_fd, SNDCTL_DSP_SETFRAGMENT, &frag_value)<0) {
      S_printf ("SB:[Linux] Warning: ioctl() (SETFRAGMENT) failed: %s\n", strerror(errno));
      if (linux_sb_dma_is_empty()) {
      /* OSS manual states that the device must be reopened */
        linux_sb_disable_speaker();
	linux_sb_enable_speaker();
	if (ioctl (dsp_fd, SNDCTL_DSP_SETFRAGMENT, &frag_value)<0)
	  return DMA_HANDLER_NOT_OK;
      }
      else return DMA_HANDLER_NOT_OK;
    }
    S_printf ("SB:[Linux] ioctl() (SETFRAGMENT) success\n");
    return DMA_HANDLER_OK;
}

static void linux_sb_DAC_write (int bits, uint8_t value)
{
#define BUF_LEN 4096
  static int sound_frag = 0x0200007;
  static uint8_t buffer[BUF_LEN];
  static size_t buffer_count = 0;
  static int overflow_flag = 0;
  int result;

  buffer[buffer_count] = value;
  if (buffer_count < BUF_LEN - 1) {
    buffer_count ++;
  } else if (!overflow_flag) {
    error("SB: direct write buffer overflowed!\n");
    overflow_flag = 1;
  }

  if (buffer_count >= 128)
  {
    if (bits != bits_per_samp)
    {
      S_printf ("SB:[Linux] Initialising Direct DAC write (%u bits)\n", bits);
      if (linux_sb_dma_is_empty() == DMA_HANDLER_OK) {
        result = 0;
        if (ioctl (dsp_fd_write, SNDCTL_DSP_SAMPLESIZE, &bits)<0) {
           S_printf ("SB:[Linux] Warning: ioctl() (SAMPLESIZE) failed: %s\n", strerror(errno));
           result = -1;
        }
        if (linux_set_OSS_fragsize(sound_frag) == DMA_HANDLER_NOT_OK) {
           S_printf ("SB:[Linux] Warning: failed to change sound fragment size.\n");
           result = -1;
        }
        linux_sb_set_speed(config.oss_dac_freq, 0, 0, 0);
        /* reset DMA settings */
        sample_rate = 0;
        num_channels = 0;
        oss_block_size = 0;
        if (result == -1) {
          bits_per_samp = 0;
          return;
        }
        bits_per_samp = bits;
      }
      else {
        S_printf("SB:[Linux] Sorry, can't change OSS settings now...\n");
      }
    }

    if (linux_sb_dma_get_free_space() < buffer_count)
      return;
    buffer_count -= write (dsp_fd_write, buffer, buffer_count);
    if (config.oss_do_post)
      ioctl (dsp_fd_write, SNDCTL_DSP_POST);
  } else {
    overflow_flag = 0;
  }
}

/*
 * this function tries to detect the possible Soundblaster revision
 * we can emulate
 * one possible way is to read /dev/sndstat and check for the Soundblaster
 * version, however this didn't work for other cards so we try to check
 * whether stereo and 16 bits output is supported - Michael
 */
/*
 * BTW This was a brilliant idea and I wish I'd thought of it. I don't have
 * as much experience with the sound code though, eh Michael! - Alistair
 */

int linux_sb_get_version(void)
{
#if 0
  long int sound_frag = SOUND_FRAG;
#endif
  int tmp, version = 0;
  char *s=NULL;

  if (dsp_fd_read != -1) {
    close(dsp_fd_read);
    dsp_fd_read = -1;
  }

  if (dsp_fd_write == -1)
    dsp_fd_write = RPT_SYSCALL(open(config.sb_dsp, O_WRONLY | O_NONBLOCK));
  
  if (dsp_fd_write > 0) {
    /* Ok, let's try to set stereo for output */
    tmp = 1;
    if (ioctl(dsp_fd_write, SNDCTL_DSP_STEREO, &tmp) < 0) {
      /*
       * we cannot set stereo, so it can be only SB 1.5 or 2.0,
       * the only difference as far as I know is that SB 1.5 cannot
       * sample in with more than 13 kHz; however at least one version of
       * VoxWare didn't return this error (or is this fixed now?)
       * on the other site sampling in is not the question yet, so
       * I return SB 1.5 (because have one) - Michael Beck
       * -----
       * But I need SB 2.0 for my MOD-Player to work on my soundcard!
       * An important difference is the high-speed feature (DSP-command 0x9X)
       * As you said, the SB 1.5 can sample slower than the SB 2.0, because the
       * SB 2.0 is using the high-speed command (DSP: 0x99). BUT: the high-
       * speed command is existing for playback, and without HS you can play
       * back up to 22 kHz, and with HS you can play back up to 44 kHz (!)
       *            (-Karch)       
       */

       tmp=44100;
       if (ioctl(dsp_fd_write, SNDCTL_DSP_SPEED,&tmp)<0)
	  S_printf ("SB:[Linux] Warning: ioctl() (SPEED) failed: %s\n", strerror(errno));
       if(tmp < 44100)
	 version = SB_OLD;
       else
	 version = SB_20;
    }
    else {
      /*
       * ok, we can set stereo, so try 16 bit output, if
       * this is possible, say it's a SB16 otherwise a SBPro
       */
      tmp = AFMT_S16_LE;
      if (ioctl(dsp_fd_write, SNDCTL_DSP_SAMPLESIZE, &tmp) < 0 || tmp != AFMT_S16_LE)
	version = SB_PRO;
      else 
        version = SB_16;
    }
  } else {
    S_printf("SB: %s open failed, %s\n", config.sb_dsp, strerror(errno));
  }
  if (version) {
    switch (version) {
      case SB_OLD : s = "1.5"; break;
      case SB_20  : s = "2.0"; break;
      case SB_PRO : s = "PRO"; break;
      case SB_16  : s = "16";
    }
    S_printf("SB:[Linux] SoundBlaster %s can be emulated.\n", s);
  }
  else 
    S_printf("SB:[Linux] No sounddevice for SoundBlaster emulation found.\n");

  /*
   * The devices are closed until we need them. This makes the driver more
   * OS friendly, but leaves the possibility that we can't access the driver 
   * when we want to. - Alistair
   */

  if (dsp_fd_write != -1) {
    close (dsp_fd_write);
    dsp_fd_write = -1;
  }

  return version;
}

int linux_sb_dma_start_init(int read)
{
  long int fragments = (num_sound_frag << 16) | sound_frag_size;
  long int new_block_size = num_sound_frag * sound_frag_size;
  long int frag_size;
  int result=0;

  if (read) {
    if (dsp_fd_read == -1)
      if ((dsp_fd_read = RPT_SYSCALL(open(config.sb_dsp, O_RDONLY))) < 0) {
        S_printf ("SB:[Linux] Failed to open %s for read (%s)\n",
          config.sb_dsp, strerror(errno));
	return 0;
      }
    dsp_fd = dsp_fd_read;
    dsp_fd_write = -1;
  }

  if (oss_block_size != new_block_size) {
    if (linux_sb_dma_is_empty() == DMA_HANDLER_OK) {
      if (ioctl(dsp_fd, SNDCTL_DSP_RESET)<0) {
	S_printf ("SB:[Linux] Warning: ioctl() (RESET) failed: %s\n", strerror(errno));
      }
      if (linux_set_OSS_fragsize(fragments) == DMA_HANDLER_NOT_OK) {
	result = -1;
      }
      ioctl(dsp_fd, SNDCTL_DSP_GETBLKSIZE, &frag_size);
      S_printf ("SB:[Linux] OSS fragsize set to: %lu\n", frag_size);
    }
    else {
      S_printf("SB:[Linux] Sorry, can't change OSS settings now...\n");
      S_printf("SB:[Linux] Trying to continue with current settings.\n");
    }
  }
  oss_block_size = (result < 0 ? 0 : new_block_size);
  
  return result;
}

size_t linux_sb_do_read(void *ptr, size_t size)
{
int amount_done;
  if (dsp_fd_read == -1) {
    S_printf("SB [Linux]: ERROR: Speaker not disabled\n");
    return 0;
  }
  amount_done = read(dsp_fd_read, ptr, size);
  S_printf("SB [Linux]: read() returned %d", amount_done);
  if (amount_done < 0)
    S_printf(": %s", strerror(errno));
  S_printf("\n");
  return (amount_done > 0 ? amount_done : 0);
}

size_t linux_sb_do_write(void *ptr, size_t size)
{
int amount_done;
static int in_frag = 0;
  if (dsp_fd_write == -1) {
    S_printf("SB [Linux]: ERROR: Device is not opened for write!\n");
    return 0;
  }
  if (linux_sb_dma_get_free_space() < size) {
    if (config.oss_do_post)
      ioctl (dsp_fd_write, SNDCTL_DSP_POST);	/* some buggy drivers needs this */
    in_frag = 0;
    return 0;
  }
  amount_done = write(dsp_fd_write, ptr, size);
  S_printf("SB [Linux]: write() returned %d", amount_done);
  if (amount_done < 0) {
    S_printf(": %s", strerror(errno));
    amount_done = 0;
  }
  S_printf("\n");
  in_frag += amount_done;
  if (in_frag >= (1 << sound_frag_size)) {
    if (config.oss_do_post) {
      S_printf("SB [Linux]: ioctling POST\n");
      ioctl (dsp_fd_write, SNDCTL_DSP_POST);	/* some buggy drivers needs this */
    }
    in_frag = 0;
  }
  return amount_done;
}

int linux_sb_get_free_fragments(int *total, int *free, int *bytes)
{
  audio_buf_info data;

  *total = 0;
  *free = 0;
  *bytes = 0;

  if (dsp_fd_write == -1)
    return DMA_HANDLER_OK;

  if (ioctl(dsp_fd_write, SNDCTL_DSP_GETOSPACE, &data) > -1) {
    S_printf ("SB:[Linux] Get Free Fragments (%d, %d)\n", 
	      data.fragstotal, data.fragments);

    *total = data.fragstotal;
    *free = data.fragments;
    *bytes = data.bytes;

    return DMA_HANDLER_OK;
  } else {
    if (errno == EBADF) {
      return DMA_HANDLER_OK;
    } else {
      S_printf ("SB:[Linux] Get Free Fragments IOCTL error (%s)\n", 
		strerror(errno));
      return DMA_HANDLER_NOT_OK;
    }
  }
}

int linux_sb_dma_get_free_space(void)
{
  int free_fragments = 0, total_fragments = 0, bytes = 0;
  int result = DMA_HANDLER_OK;

  result = linux_sb_get_free_fragments(&total_fragments, &free_fragments, &bytes);

  return bytes;

}

int linux_sb_dma_complete_test(void)
{
  int extra_fragments, free_fragments = 0, total_fragments = 0, bytes;
  int result = DMA_HANDLER_OK;

  result = linux_sb_get_free_fragments(&total_fragments, &free_fragments, &bytes);

    /* 
     * As we set fragments that way that all are used, we can just
     * look for space in the output buffer. If there is more than
     * one fragment left, we have still space, and we want it to be
     * filled as soon as possible. - MK
     */

  extra_fragments = total_fragments / 5;
  if (extra_fragments < config.oss_min_extra_frags)
    extra_fragments = config.oss_min_extra_frags;
  if (extra_fragments < config.oss_stalled_frags)
    extra_fragments = config.oss_stalled_frags;
  if (result == DMA_HANDLER_OK &&
      free_fragments >= (total_fragments - extra_fragments)) {
    return DMA_HANDLER_OK;
  }

  return DMA_HANDLER_NOT_OK;
}

int linux_sb_dma_is_empty(void)
{
  int free_fragments = 0, total_fragments = 0, bytes;
  int result = DMA_HANDLER_OK;

  result = linux_sb_get_free_fragments(&total_fragments, &free_fragments, &bytes);

  if (result == DMA_HANDLER_OK && free_fragments >= total_fragments - config.oss_stalled_frags)
    return DMA_HANDLER_OK;

  return DMA_HANDLER_NOT_OK;
}

void linux_sb_dma_complete(void)
{
	S_printf ("SB:[Linux] DMA Completed\n");
}

int linux_sb_set_speed (uint16_t speed, uint8_t stereo_mode, uint8_t is_16bit, uint8_t is_signed)
{
  int rate = speed;
  int channels, result = 0;
  long int samplesize = is_16bit ?
    (is_signed ? AFMT_S16_LE : AFMT_U16_LE) :
    (is_signed ? AFMT_S8 : AFMT_U8);

  /* stereo_mode is actually 2 is stereo is requested - Karcher */
  channels = stereo_mode ? 2 : 1;

  if (dsp_fd != -1)
  {
    if (sample_rate != rate || num_channels != channels) {
      if (linux_sb_dma_is_empty() == DMA_HANDLER_OK) {
	sample_rate = rate;
	num_channels = channels;
	if (ioctl(dsp_fd, SNDCTL_DSP_RESET)<0) {
	  S_printf ("SB:[Linux] Warning: ioctl() (RESET) failed: %s\n", strerror(errno));
	  sample_rate = 0;
	  num_channels = 0;
	}
	if (ioctl(dsp_fd, SNDCTL_DSP_SAMPLESIZE, &samplesize)<0) {
	  S_printf ("SB:[Linux] Warning: ioctl() (SAMPLESIZE) failed: %s\n", strerror(errno));
	  sample_rate = 0;
	  num_channels = 0;
	}
	if (ioctl(dsp_fd, SNDCTL_DSP_CHANNELS, &channels)<0) {
	  S_printf ("SB:[Linux] Warning: ioctl() (CHANNELS) failed: %s\n", strerror(errno));
	  sample_rate = 0;
	  num_channels = 0;
	}
	if (ioctl(dsp_fd, SNDCTL_DSP_SPEED, &rate)<0) {
	  S_printf ("SB:[Linux] Warning: ioctl() (SPEED) failed: %s\n", strerror(errno));
	  sample_rate = 0;
	  num_channels = 0;
	}
	S_printf ("SB:[Linux] (actual rate : %u)\n", rate);
      }
      else {
	S_printf("SB:[Linux] Sorry, can't change OSS settings now...\n");
        ioctl (dsp_fd, SNDCTL_DSP_POST);
	result = -1;
      }
    }
  }
  else
  {
    S_printf ("SB:[Linux] Device not open - Can't set speed.\n");
  }
  return result;
}


/*
 * This is required to set up the driver for future use.
 */
int SB_driver_init () {

  S_printf ("SB:[Linux] SB Driver Initialisation Called\n");

  /*
   * Here we set up the Driver array with all of the functions that we
   * can provide. At this point we don't know how many of these services
   * our actual hardware can support, but it doesn't harm to define extra
   * functions.
   */
  
  /* Mixer Functions */
  SB_driver.write_mixer         = linux_sb_write_mixer;
  SB_driver.read_mixer          = linux_sb_read_mixer;
  
  /* Speaker Functions */
  SB_driver.speaker_on          = linux_sb_enable_speaker;
  SB_driver.speaker_off         = linux_sb_disable_speaker;
  
  /* Direct DAC */
  SB_driver.DAC_write           = linux_sb_DAC_write;
  
  /* DMA Functions */
  SB_driver.DMA_start_init      = linux_sb_dma_start_init;
  SB_driver.DMA_do_read         = linux_sb_do_read;
  SB_driver.DMA_do_write        = linux_sb_do_write;
  SB_driver.DMA_pause           = NULL;
  SB_driver.DMA_resume          = NULL;
  SB_driver.DMA_stop            = NULL;
  SB_driver.DMA_complete_test   = linux_sb_dma_complete_test;
  SB_driver.DMA_can_change_speed= linux_sb_dma_is_empty;
  SB_driver.DMA_complete        = linux_sb_dma_complete;
  SB_driver.DMA_set_blocksize   = linux_sb_dma_set_blocksize;
   
  /* Miscellaneous Functions */
  SB_driver.set_speed           = linux_sb_set_speed;
  SB_driver.play_buffer         = NULL;

  /* Some sanity checks */
  if (config.oss_min_frags < MIN_NUM_FRAGMENTS) {
    error("SB: oss_min_frags is too small (%i, min %i)\n",
	config.oss_min_frags, MIN_NUM_FRAGMENTS);
    config.exitearly = 1;
    return 0;
  }
  if (config.oss_min_frags > config.oss_max_frags) {
    error("SB: oss_min_frags is larger than oss_max_frags (%i and %i)\n",
	config.oss_min_frags, config.oss_max_frags);
    config.exitearly = 1;
    return 0;
  }
  if (config.oss_max_frags > MAX_NUM_FRAGMENTS) {
    error("SB: oss_max_frags is too large (%i, max %i)\n",
	config.oss_max_frags, MAX_NUM_FRAGMENTS);
    config.exitearly = 1;
    return 0;
  }
  if (config.oss_stalled_frags > MAX_NUM_FRAGMENTS) {
    error("SB: oss_stalled_frags is too large (%i, max %i)\n",
	config.oss_stalled_frags, MAX_NUM_FRAGMENTS);
    config.exitearly = 1;
    return 0;
  }
  if (config.oss_min_extra_frags > MAX_NUM_FRAGMENTS) {
    error("SB: oss_min_extra_frags is too large (%i, max %i)\n",
	config.oss_min_extra_frags, MAX_NUM_FRAGMENTS);
    config.exitearly = 1;
    return 0;
  }

  /*
   * This determines a suitable value for the SB Version that we can 
   * emulate given the actual hardware capabilities, as indicated by 
   * various probes of the Linux Sound Driver(s).
   */

  return linux_sb_get_version();
}

void linux_mpu401_register_callback(void (*io_callback)(void))
{
  if (mpu_in_fd == -1) return;
  add_to_io_select(mpu_in_fd, 1, io_callback);
}

void linux_mpu401_data_write(uint8_t data)
{
	/* Output a MIDI byte to an external file;
	   'open on demand' strategy. */
	if (mpu_fd == -1) {
	  	if (mpu_disabled) return;
		/* Added NONBLOCK to prevent hanging - Karcher */
		mpu_fd = RPT_SYSCALL(open(DOSEMU_MIDI_PATH,
			      O_WRONLY | O_CREAT | O_NONBLOCK, 0777));
		if (mpu_fd == -1) {
			mpu_disabled = TRUE;
			S_printf("MPU401:[Linux] Failed to open file 'midi' (%s)\n",
			strerror(errno));
			return;
		}
	}
	if (write(mpu_fd,&data,1) != 1) {
			S_printf("MPU401:[Linux] Failed to write to file 'midi' (%s)\n",
			strerror(errno));
	}
}

int linux_mpu401_data_read(uint8_t data[], int max_len)
{
	int ret;
	if (mpu_in_fd == -1) return 0;
	if ((ret = read(mpu_in_fd,data,max_len)) == -1) {
			S_printf("MPU401:[Linux] Failed to write to file 'midi' (%s)\n",
			strerror(errno));
	}
	return ret;
}


int FM_driver_init(void)
{
  S_printf ("SB:[Linux] FM Driver Initialisation Called\n");

  return ADLIB_NONE;
}

int MPU_driver_init(void)
{
  S_printf("MPU:[Linux] MPU Driver Initialisation Called\n");

  /* MPU-401 Functions */
  mpu401_info.data_write = linux_mpu401_data_write;
  mpu401_info.data_read = linux_mpu401_data_read;
  mpu401_info.register_io_callback = linux_mpu401_register_callback;

  mpu_in_fd = RPT_SYSCALL(open(DOSEMU_MIDI_IN_PATH, O_RDONLY | O_NONBLOCK, 0777));

  mpu_disabled = FALSE;
  /* Output a MIDI byte to an external file */
  /* Added NONBLOCK to prevent hanging - Karcher */
  mpu_fd = RPT_SYSCALL(open(DOSEMU_MIDI_PATH, O_WRONLY | O_CREAT | O_NONBLOCK, 0777));
  if (mpu_fd == -1) {
    S_printf("MPU401:[Linux] Failed to open file 'midi' (%s)\n", strerror(errno));
  }
  return MPU_NONE;
}

void SB_driver_reset() {
  S_printf ("SB:[Linux] SB Driver Reset Called\n");
  bits_per_samp = 0; /* to reinitialize on direct DAC writes */
  sample_rate = 0;
  num_channels = 0;
  oss_block_size = 0;
  if (dsp_fd_write != -1)
    ioctl(dsp_fd, SNDCTL_DSP_RESET);
  if (dsp_fd_read != -1) {
    close(dsp_fd_read);
    dsp_fd = dsp_fd_read = -1;
  }
}

void FM_driver_reset() {
  S_printf ("SB:[Linux] FM Driver Reset Called\n");

}

void MPU_driver_reset()
{
	S_printf("MPU:[Linux] MPU Driver Reset Called\n");
}
