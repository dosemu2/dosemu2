/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
#include <linux/soundcard.h>
/* #include <sys/audioio.h> */

#include <errno.h>

#include "pic.h"
#include "dma.h"
#include "sound.h"
#include "linux_sound.h"

/*#ifndef SOUND_FRAG
#error SOUND_FRAG not defined!
#endif*/

/* SB static vars */
static int mixer_fd = -1;
static int dsp_fd   = -1;

/* Old variables - Obselete - AM */
static long int block_size = 0;
static long int sound_frag = 0xc;

/* New fragment control - AM */
static int sound_frag_size = 0x9; /* ie MAX_DMA_TRANSFERSIZE (== 512) */
static int num_sound_frag  = MAX_NUM_FRAGMENTS;
static int extra_buffer    = 8000*BUFFER_MSECS/1000;

/* MPU static vars */
static int mpu_fd = -1;	             /* -1 = closed */
static boolean mpu_disabled = FALSE; /* TRUE if MIDI output disabled */

extern void sb_set_speed (void);   /* From sound.c */

void linux_sb_dma_set_blocksize(__u16 sb_blocksize)
{
  __u16 blockbits, oss_blocksize, oss_blocknum;

  /* 
   * The blocksize we are passed is the actual number of bytes to include in a
   * DMA transfer. The OSS/Free driver uses a number of fragments within the 
   * buffer to try to control the output, and the DOSEmu sound driver places an
   * upper limit on the size of the transfers (to avoid confusing applications
   * which monitor the DMA transfer)
   * Ideally, the size of a fragment should match that of the transfer. This
   * will not be possible in all cases as the fragment size must be a power of
   * ---
   * New by MK:
   * We provide more space than sb_blocksize for buffering, as the IRQ-timing
   * in dosemu is lousy. I hardly get 100 Hz on the SB-irq. The extra space
   * is depending on the actual data rate. The default is to buffer 10 msec,
   * but it can be changed in linux_sound.h. The calculation is done in
   * linux_sb_set_speed. There will be a problem when we start implementing
   * SB16, as stereo and 16-bit flags are not known till the sound really
   * starts.
   */
  
  if(sb_blocksize > MAX_DMA_TRANSFERSIZE) {
    oss_blocksize = MAX_DMA_TRANSFERSIZE;
  } else {
    oss_blocksize = sb_blocksize;
  }
  block_size = oss_blocksize;

  for (blockbits = 0; oss_blocksize > 1; 
       blockbits++, oss_blocksize = oss_blocksize >> 1)
    ;
  /* blockbits now contains floor(log2(oss_blocksize)), and oss_blocksize is
      destroyed */

  sound_frag_size = blockbits;

  /* This is intentionally here, because fragments should not be greater
     than the SB block size */
  sb_blocksize += extra_buffer;

  oss_blocknum = sb_blocksize / (1 << sound_frag_size);
  
  if (sb_blocksize - ( oss_blocknum * (1 << sound_frag_size)) > 0 ) {
    num_sound_frag = oss_blocknum + 1;
  } else {
    num_sound_frag = oss_blocknum;
  }

  if (num_sound_frag > MAX_NUM_FRAGMENTS) {
    num_sound_frag = MAX_NUM_FRAGMENTS;
  }

  S_printf ("SB:[Linux] DMA blocksize set to %u (%u,%u)\n", sb_blocksize, 
	    sound_frag_size, num_sound_frag);
}

void linux_sb_write_mixer(int ch, __u8 val)
{
  int newsetting;
  __u8 driver_channel = -1;

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

  S_printf ("SB:[Linux] Writing to the Mixer (%u, %u)\n", ch, val);

  if (ch != SB_MIXER_MIC) {
    newsetting = ((val & 0x0F0) * 0x0006) + ((val & 0xF0) * 0x0060);
    ioctl(mixer_fd, MIXER_WRITE(driver_channel), &newsetting);
  }
  else {
    newsetting = ((val & 0x7) * 0x0C0C);
    ioctl(mixer_fd, MIXER_WRITE(SOUND_MIXER_MIC), &newsetting);
  }
}

__u8 linux_sb_read_mixer(int ch)
{
  int x;
  __u8 driver_channel = -1;

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

  ioctl(mixer_fd, MIXER_READ(driver_channel), &x);
  S_printf ("SB:[Linux] Reading from the Mixer (%u -> %u)\n", ch, x);

  if (ch != SB_MIXER_MIC) {
    return (((x & 0x00FF) / 6) | ((x & 0xFF00) / 0x600));
  }
  else {
    return x / 7; /* This isn't the right value.  Anyone care to tell me
                     how to do division in C???? (- Joel)*/
  }
}

/*
 * Speaker Functions
 */

void linux_sb_disable_speaker(void)
{
  if (dsp_fd != -1)
  {
    S_printf ("SB:[Linux] Syncing DSP\n");
    ioctl(dsp_fd, SNDCTL_DSP_SYNC);
    close (dsp_fd);
    dsp_fd = -1;
  } else {
    S_printf ("SB:[Linux] DSP already closed\n");
  }
}

void linux_sb_enable_speaker (void)
{
  dsp_fd = open(config.sb_dsp, O_WRONLY | O_NONBLOCK);

  if (dsp_fd == -1)
  {
    S_printf ("SB:[Linux] Failed to initiate connection to %s (%s)\n", config.sb_dsp, 
	      strerror(errno));
    return;
  }
}

static void linux_sb_DAC_write (int bits, __u8 value)
{
  static int last_bits = 0;
  static int sound_frag = 0x0200007;
  static __u8 buffer[128];
  static __u8 buffer_count = 0;
  
  buffer[buffer_count] = value;
  buffer_count ++;

  if (buffer_count == 128)
  {
    if (bits != last_bits)
    {
      S_printf ("SB:[Linux] Intialising Direct DAC write (%u bits)\n", bits);
      last_bits = bits;
      ioctl (dsp_fd, SNDCTL_DSP_SAMPLESIZE, &last_bits);
      ioctl (dsp_fd, SNDCTL_DSP_SETFRAGMENT, &sound_frag);
      SB_dsp.time_constant = 193; /* Approx 16kHz */
      sb_set_speed();
    }

    write (dsp_fd, buffer, buffer_count);
    buffer_count = 0;
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

  /* if we cannot open the mixer, it's not more than a SB 2.0 */
  mixer_fd = open(config.sb_mixer, O_RDWR);

  dsp_fd = open(config.sb_dsp, O_WRONLY | O_NONBLOCK);
  
  if (dsp_fd > 0) {
    /* Ok, let's try to set stereo for output */
    tmp = 1;
    if (mixer_fd < 0 || ioctl(dsp_fd, SNDCTL_DSP_STEREO, &tmp) < 0 || !tmp) {
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
       ioctl(dsp_fd, SNDCTL_DSP_SPEED,&tmp);
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
      if (ioctl(dsp_fd, SNDCTL_DSP_SAMPLESIZE, &tmp) < 0 || tmp != AFMT_S16_LE)
	version = SB_PRO;
      else 
        version = SB_16;
    }

#if 0
    /* reset to 8 bit per sample and mono */
    tmp = 0;
    ioctl(dsp_fd, SNDCTL_DSP_STEREO, &tmp);
    tmp = AFMT_U8;
    ioctl(dsp_fd, SNDCTL_DSP_SAMPLESIZE, &tmp);

    /* Set the fragments and ignore any error yet */
    ioctl(dsp_fd, SNDCTL_DSP_SETFRAGMENT, &sound_frag);
#endif

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
  close (dsp_fd);
  close (mixer_fd);

  return version;
}

void linux_sb_dma_start_init(__u32 command)
{
  /*fndef SOUND_FRAG
#error SOUND_FRAG not defined!
#endif*/
/*  long int sound_frag = SOUND_FRAG; */
  long int samplesize = AFMT_U8;

  /*  extern long int sound_frag;
  extern long int block_size;
  */

  /*  long int fragments = 0x0020000 | sound_frag; */
  long int fragments = (num_sound_frag << 16) | sound_frag_size;

  switch(command)
  {
  case 0x14: /* 8-bit DMA */
    S_printf ("SB:[Linux] 8-bit DMA starting\n");
    break;
  case 0x1C: /* 8-bit DMA (Auto-Init) */
    S_printf ("SB:[Linux] 8-bit DMA (Auto-Init) starting\n");
    break;
  case 0x90: /* 8-bit DMA (Auto-Init, High Speed) */
    S_printf ("SB:[Linux] 8-bit DMA (High Speed, Auto-Init) starting\n");
    break;
  case 0x91: /* 8-bit DMA (High Speed) */
    S_printf ("SB:[Linux] 8-bit DMA (High Speed) starting\n");
    break;
  default:
    S_printf ("SB:[Linux] Unsupported DMA type (%x)\n", command);
    return;
    break;
  };

  ioctl(dsp_fd, SNDCTL_DSP_SETFRAGMENT, &fragments);
  ioctl(dsp_fd, SNDCTL_DSP_SAMPLESIZE, &samplesize);

  ioctl(dsp_fd, SNDCTL_DSP_GETBLKSIZE, &block_size);
  S_printf ("SB:[Linux] 8-bit DMA Blocksize set to: %lu\n", block_size);

}


void linux_sb_dma_start_complete (void) {
  /*
   * This is the important part:
   *
   * The first parameter is the DMA channel we are referring to.
   *
   * The WRITE_FD is used for writing to the DMA channel.
   *
   * The READ FD is used to read from the DMA channel (in this case, reads
   * are sent to /dev/dsp
   *
   * The handler responds to the data being written - it is called after each 
   * read/write to the DMA channel. It is expected to provide the DACK 
   * handshaking. Ideally it should be very short ....
   * 
   * The final value sets the number of bytes to try and transfer.
   */
   
   /* 
    * **CRISK**
    * 
    * Now using the "blocksize" as set by the user, when set.
    * Potential bug -> this may need resetting. No idea how the actual
    * thing works.
    *
    */
   
   if(!block_size)
   {
     S_printf("SB:[Linux] Using default blocksize\n");
     block_size = MAX_DMA_TRANSFERSIZE;
   }
   S_printf ("SB:[Linux] DMA start completed (block_size %lu)\n",block_size);

   dma_install_handler(config.sb_dma, -1, dsp_fd, sb_dma_handler, block_size);
}

int linux_sb_dma_complete_test(void)
{
  audio_buf_info data;

  if (ioctl(dsp_fd, SNDCTL_DSP_GETOSPACE, &data) != -1) {
    S_printf ("SB:[Linux] DMA completion test (%d, %d)\n", 
	      data.fragstotal, data.fragments);

    /* 
     * As we set fragments that way that all are used, we can just
     * look for space in the output buffer. If there is more than
     * one fragment left, we have still space, and we want it to be
     * filled as soon as possible. - MK
     */

    if(data.fragments > 2) {
      return DMA_HANDLER_OK;
    }

  } else {
    if (errno == EBADF) {
      return DMA_HANDLER_OK;
    } else {
      S_printf ("SB:[Linux] DMA completion test IOCTL error (%s)\n", 
		strerror(errno));
    }
  }

  return DMA_HANDLER_NOT_OK;
}

void linux_sb_dma_complete(void)
{
	S_printf ("SB:[Linux] DMA Completed\n");
}

#if 0
void start_dsp_dma(void)
{
  int real_speed, trash;
 
  S_printf ("Starting to open DMA access to DSP\n");
 
  if (dsp_stereo)
    real_speed = dsp_rate >> 1;
  else
    real_speed = dsp_rate;

  ioctl(dsp_fd, SOUND_PCM_WRITE_RATE, &real_speed);
  if (dsp_stereo)
    trash = 1;
  else
    trash = 2;
  ioctl(dsp_fd, SOUND_PCM_WRITE_CHANNELS, &trash);

  dma_ch[sound_dma_ch].fd = dsp_fd;
  dma_ch[sound_dma_ch].dreq = DREQ_COUNTED;

  /*
   * immediately start the dma-transfer, as some programs use a short
   * timeout to check for the used dma-channel
   */
  dma_trans();
}
#endif

void linux_sb_set_speed (__u16 speed, __u8 stereo_mode)
{
  int rate;
  int channels = 1;

  rate = speed;

  /* stereo_mode is actually 2 is stereo is requested - Karcher */
  if(stereo_mode)
    channels = 2;

  if (dsp_fd != -1)
  {
    ioctl(dsp_fd, SNDCTL_DSP_CHANNELS, &channels);
    ioctl(dsp_fd, SNDCTL_DSP_SPEED, &rate);
    
    S_printf ("SB:[Linux] (actual rate : %u)\n", rate);
  }
  else
  {
    S_printf ("SB:[Linux] Device not open - Can't set speed.\n");
  }
  extra_buffer = channels*rate*BUFFER_MSECS/1000;
}


/*
 * This is required to set up the driver for future use.
 */
int SB_driver_init (void) {
  extern struct SB_driver_t SB_driver;

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
  SB_driver.DMA_start_complete  = linux_sb_dma_start_complete;
  SB_driver.DMA_pause           = NULL;
  SB_driver.DMA_resume          = NULL;
  SB_driver.DMA_stop            = NULL;
  SB_driver.DMA_complete_test   = linux_sb_dma_complete_test;
  SB_driver.DMA_complete        = linux_sb_dma_complete;
  SB_driver.DMA_set_blocksize   = linux_sb_dma_set_blocksize;
   
  /* Miscellaneous Functions */
  SB_driver.set_speed           = linux_sb_set_speed;
  SB_driver.play_buffer         = NULL;

  /*
   * This determines a suitable value for the SB Version that we can 
   * emulate given the actual hardware capabilities, as indicated by 
   * various probes of the Linux Sound Driver(s).
   */
  return linux_sb_get_version();
}


void linux_mpu401_data_write(__u8 data)
{
	/* Output a MIDI byte to an external file;
	   'open on demand' strategy. */
	if (mpu_fd == -1) {
	  	if (mpu_disabled) return;
		/* Added NONBLOCK to prevent hanging - Karcher */
		mpu_fd = open(DOSEMU_MIDI_PATH,
			      O_WRONLY | O_CREAT | O_NONBLOCK, 0777);
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


int FM_driver_init()
{
  S_printf ("SB:[Linux] FM Driver Initialisation Called\n");

  return ADLIB_NONE;
}

int MPU_driver_init()
{
  S_printf("MPU:[Linux] MPU Driver Initialisation Called\n");
  mpu401_info.data_write = linux_mpu401_data_write;
  mpu_fd = -1;
  mpu_disabled = FALSE;
  return MPU_NONE;
}

void SB_driver_reset() {
  S_printf ("SB:[Linux] SB Driver Reset Called\n");

}

void FM_driver_reset() {
  S_printf ("SB:[Linux] FM Driver Reset Called\n");

}

void MPU_driver_reset()
{
	S_printf("MPU:[Linux] MPU Driver Reset Called\n");
	if (mpu_fd != -1) {
		close(mpu_fd);
		mpu_fd = -1;
	}
	mpu_disabled = FALSE;
}
