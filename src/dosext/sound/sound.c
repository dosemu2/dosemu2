/* Copyright 1995  Joel N. Weber II
   See the file README.sound in this directory for more information */

/* 
 * DANG_BEGIN_MODULE
 *
 * Soundblaster emulation. Very incomplete.
 *
 * maintainer:
 * Alistair MacDonald <alistair@slitesys.demon.co.uk>
 *
 * DANG_END_MODULE
 */

/*
 * modified 11/05/95 by Michael Beck
 *  added some more (undocumented ?) SB commands, version detection
 */
#include "emu.h" /* For S_printf */

#include <fcntl.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <linux/soundcard.h>
#endif
#ifdef __NetBSD__
#include <sys/audioio.h>
#endif
#include "pic.h"
#include "dma.h"
#include "sound.h"

#ifdef __linux__
#ifndef SOUND_FRAG
#error SOUND_FRAG not defined!
#endif
#endif

/* Ok, let's emulate a SB depending on the capabilities of actual /dev/dsp */
static int sb_version;

/*
 * These two are used by dos programs to read data from the dsp. The dsp
 * code in this file will write data to the variable dsp_out and then set
 * dsp_avail to 0x80. I think that right now this is only used to help
 * programs think they've reset the chip. However it can also be used
 * as an inefficent way to do A/D; it probably wouldn't be a great idea
 * to support it due to timing problems when you're using a multitasking
 * environment like Linux.
 * It probably is also involved in the MIDI port; if you have some info on
 * how this works, please add the appropriate code
 */

/* must be power of 2 */
#define DSP_QUEUE_SIZE 16

static unsigned char dsp_out[DSP_QUEUE_SIZE];
static int dsp_queue_start     = 0;
static int dsp_queue_end       = 0;
static int dsp_holds           = 0;
static unsigned char dsp_avail = 0;

unsigned char dsp_read_output(void);
void dsp_write_output(unsigned char );


/*
 * The way the dsp works involves writing a few bytes. However, my procedure
 * gets called once for each out instruction, so this variable stores the last
 * command written.
 */
static unsigned char dsp_last_write = 0;

/* Similarily, this is the index register for the mixer */
static unsigned char mixer_index = 0;

/*
 * When you give the command to do a dma transfer, you have to write a two
 * byte size so the SB knows hoch much bytes to transfer (and how longs 
 * DREQ must be activated.
 * This variable keeps track of whether you are writing the high
 * or low byte.
 */
static unsigned char dsp_write_size_mode = 0;

/* The dsp rate and data width params and speaker status */
static unsigned char dsp_stereo = 0;
static unsigned char dsp_rate = 0;
static unsigned char dsp_speaker_on = 1;

/*
 * These are the file desciptors used to communicate with Hannu's sound
 * driver. At the moment only dsp_fd is used and mixer_fd is opened for
 * detection.
 */
static int mixer_fd = -1, dsp_fd = -1;

static void dsp_clear_output(void);

/*
 * This will do everything prepare to output data...
 */
#ifdef __linux__
inline void start_dsp_dma(void)
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
#endif __linux__

/* 
 * The NetBSD version is completely broken at the moment
 */
#ifdef __NetBSD__
inline void start_dsp_dma(void)
/* This will open the fd and do everything else to prepare to output data... */
{
  int real_speed, trash;
  struct audio_info ainfo;

  S_printf ("Starting to open DMA access to DSP\n");

  AUDIO_INITINFO(&ainfo);
  ainfo.play.sample_rate = dsp_rate;
  ainfo.play.channels = dsp_stereo ? 2 : 1;
  ainfo.play.precision = 8;
  ainfo.play.encoding = AUDIO_ENCODING_PCM8; /* XXX unsigned linear 8-bit */

  ioctl(dsp_fd, AUDIO_SETINFO, &ainfo);
  dma_ch[sound_dma_ch].fd = dsp_fd;
  dma_ch[sound_dma_ch].dreq = DREQ_COUNTED;
}
#endif /* __NetBSD__ */

/* This undoes everything the previous function does ... */
inline void stop_dsp_dma(void)
{
  S_printf ("Stopping DMA access to DSP\n");

  dma_ch[sound_dma_ch].dreq = DREQ_OFF;
}

#ifdef __linux__
inline void mixer_write_setting(unsigned char ch, unsigned char val)
{
  int newsetting;

  S_printf ("Writing to the Mixer (%u, %u)\n", ch, val);

  newsetting = ((val & 0x0F0) * 0x0006) + ((val & 0xF0) * 0x0060);
  ioctl(mixer_fd, MIXER_WRITE(ch), &newsetting);
}

inline void mixer_write_mic(unsigned char val)
{
  int newsetting;

  S_printf ("Writing to the Mixer microphone (%u)\n", val);

  newsetting = ((val & 0x7) * 0x0C0C);
  ioctl(mixer_fd, MIXER_WRITE(SOUND_MIXER_MIC), &newsetting);
}
#endif /* __linux__ */


#ifdef __NetBSD__
/*
 * should assemble a mixer structure for dealing with SB-type devices.
 * not all audio devices will have a good mixer, though :(
 * For now, just punt.
 */
#endif

/* addr must be a offset from the SB base */
void sb_write(unsigned char addr, unsigned char value)
{
  static unsigned char value_E4;
  static unsigned long tmp;

  S_printf ("Write to SB (0x%x, 0x%x)\n", addr, value);
  switch (addr) {
    case 0x06: /* reset */
	       S_printf (" - resetting SB\n");
	       dsp_clear_output();
               dsp_write_output(0xAA);
               break;
    case 0x0C: /* dsp write register */
	       S_printf (" - writing to the DSP on the SB\n");
               switch (dsp_last_write) {
                 case 0x40: dsp_rate = value;
                            dsp_last_write = 0;
                            break;
                 case 0x14: if ((dsp_write_size_mode = !dsp_write_size_mode))
                              tmp = value;
                            else {
			      tmp |= value << 8;
			      dma_ch[sound_dma_ch].dreq_count = tmp + 1;
                              dsp_last_write = 0;
                              start_dsp_dma();
                            }
			    break;
		 case 0xE0: /* Negate the following value ? */
		            dsp_write_output(~value);
			    dsp_last_write = 0;
                            break;
		 case 0xE4: /* Save the value and return it on 0xE8 */
		 	    value_E4 = value;
			    dsp_last_write = 0;
			    break;
                 default: /* figure out what command */
                            switch (value) {
                              case 0xD0: stop_dsp_dma(); break;
                              case 0xD4: start_dsp_dma(); break;
                              case 0x14: dsp_write_size_mode = 0; 
				         /* start_dsp_dma(); */
                                         break;
			      case 0xD1: /* Enable Speaker */
			      		 S_printf("Speaker on\n");
					 dsp_speaker_on = 1;
					 break;
			      case 0xD3: /* Disable Speaker */
			      		 S_printf("Speaker off\n");
					 /* 
					  * in this case we shouldn't emulate
					  * dma, 'cause this is used to check
					  * the used dma-channel
					  * just generate the appropriate IRQ
					  * and the software should be happy
					  * XXX -- next time
					  */
					 dsp_speaker_on = 0;
					 break;
			      case 0x40: /* Set sample rate */
			      case 0xE0: /* Negate value */
			      case 0xE4: /* save next value and return on E8 */
				         break;
			      case 0xE1: /* Identify version */
			      		 S_printf("Query version\n");
				         dsp_write_output(sb_version >> 8);
				         dsp_write_output(sb_version & 0xFF);
				         break;
		              case 0xE8: /* restore the E4 value */
		 	                 dsp_write_output(value_E4);
			                 break;
			      default:   S_printf ("Ignored instruction (0x%X)\n", value);
                              };
                            dsp_last_write = value;
               }
               break;
    /* 0x00 -- 0x03 are the fm registers */
    case 0x04: mixer_index = value; break;
    case 0x05: if (sb_version > SB_20)
                 switch (mixer_index) {
                 /* 0 is the reset register, but we'll ignore it */
                 /* 0x0C is ignored; it sets record source and a filter */
                 case 0x0E: dsp_stereo = value & 2;
                            break; /* I ignored the output filter */
#ifdef __linux__
			    /* XXX ignore these on NetBSD for now */
                 case 0x22: mixer_write_setting(SOUND_MIXER_VOLUME, value);
                            break;
                 case 0x04: mixer_write_setting(SOUND_MIXER_PCM, value);
                            break;
                 case 0x26: mixer_write_setting(SOUND_MIXER_SYNTH, value);
                            break;
                 case 0x28: mixer_write_setting(SOUND_MIXER_CD, value);
                            break;
                 case 0x2E: mixer_write_setting(SOUND_MIXER_LINE, value);
                            break;
                 case 0x0A: mixer_write_mic(value);
                            break;
                            /* This is special because the mic has a different
                               param set */
#endif
                 }
  }
}


#ifdef __linux__
inline unsigned char mixer_read_setting(unsigned char ch)
{
  int x;

  ioctl(mixer_fd, MIXER_READ(ch), &x);
  
  S_printf ("Reading from the Mixer (%u -> %u)\n", ch, x);

  return (((x & 0x00FF) / 6) | ((x & 0xFF00) / 0x600));
}

/*
 * The following routine is broken :-(
 * However, at the moment we emulate SB 1.5 only, so no mixer.
 */
inline unsigned char mixer_read_mic(void)
{
  int x;

  ioctl(mixer_fd, MIXER_READ(SOUND_MIXER_MIC), &x);
  
  S_printf ("Reading from the Mixer Microphone (%u)\n", x);

  return x / 7; /* This isn't the right value.  Anyone care to tell me
                   how to do division in C???? */
}
#endif

/* This implements a queue for output ... */
void dsp_write_output(unsigned char value)
{
  /* There is no check for exceeding the length of the queue ... */

  ++dsp_holds;
  dsp_out[dsp_queue_end++] = value;
  dsp_queue_end &= (DSP_QUEUE_SIZE - 1);
  dsp_avail = 0x80;

  S_printf ("Insert into SB output Queue [%u]... (0x%x)\n", dsp_holds, value);
}

void dsp_clear_output(void)
{
  S_printf ("Clearing the SB output Queue\n");

  dsp_holds       = 0;
  dsp_queue_end   = 0;
  dsp_queue_start = 0;
  dsp_avail       = 0;
}

/* this implementes a queue for output ... */
unsigned char dsp_read_output(void)
{
  unsigned char r = 0xFF;

  if (dsp_holds) {
    r = dsp_out[dsp_queue_start];
    dsp_out[dsp_queue_start++] = 0x00;
    dsp_queue_start &= (DSP_QUEUE_SIZE - 1);

    if (--dsp_holds)
      dsp_avail = 0x80;
    else
      dsp_avail = 0x00;

    S_printf ("Remove from SB output Queue [%u]... (0x%X)\n", dsp_holds, r);
  }
  return r;
}

unsigned char sb_read(unsigned char addr)
{
  S_printf ("Read from SB (0x%X)\n", addr);
  switch (addr) {
    case 0x0A: return dsp_read_output(); break;
    case 0x0C: 
    case 0x0E: return dsp_avail; break;
    case 0x05: if (sb_version > SB_20)
                 switch (mixer_index){
                   case 0x0E: return dsp_stereo;
#ifdef __linux__
                   case 0x22: return mixer_read_setting(SOUND_MIXER_VOLUME);
                   case 0x04: return mixer_read_setting(SOUND_MIXER_PCM);
                   case 0x26: return mixer_read_setting(SOUND_MIXER_SYNTH);
                   case 0x28: return mixer_read_setting(SOUND_MIXER_CD);
                   case 0x2E: return mixer_read_setting(SOUND_MIXER_LINE);
                   case 0x0A: return mixer_read_mic();
#endif
#ifdef __NetBSD__
	               /* gack koff bletch.  XXX punt for now. */
                   case 0x22:
                   case 0x04:
                   case 0x26:
                   case 0x28:
                   case 0x2E:
	               return (0xff / 6) | (0xffff / 0x600);
                   case 0x0A:
	               return 0xff / 7;
#endif
                 }
  }
  return 0xFF;
}

void fm_write(unsigned char addr, unsigned char value)
{
  S_printf ("Write to the FM sythesizer ... ignored ...\n");
}

/* Yes, you just saw another completely broken routine.  Don't just sit there
and think this is junk, FIX IT! */

/*
 * this function tries to detect the possible Soundblaster revision
 * we can emulate
 * one possible way is to read /dev/sndstat and check for the Soundblaster
 * version, however this didn't work for other cards so we try to check
 * whether stereo and 16 bits output is supported
 */
int dsp_get_capability(void)
{
#ifdef __linux__
  long int sound_frag = SOUND_FRAG;
  int tmp, version = 0;
  char *s=NULL;

  /* if we cannot open the mixer, it's not more than a SB 2.0 */
  mixer_fd = open(MIXER_PATH, O_RDWR);

  dsp_fd = open(DSP_PATH, O_WRONLY | O_NONBLOCK);
  
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
       * I return SB 1.5 (because have one)
       */
      version = SB_OLD;
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
    /* reset to 8 bit per sample and mono */
    tmp = 0;
    ioctl(dsp_fd, SNDCTL_DSP_STEREO, &tmp);
    tmp = AFMT_U8;
    ioctl(dsp_fd, SNDCTL_DSP_SAMPLESIZE, &tmp);

    /* Set the fragments and ignore any error yet */
    ioctl(dsp_fd, SNDCTL_DSP_SETFRAGMENT, &sound_frag);
  }
  if (version) {
    switch (version) {
      case SB_OLD : s = "1.5"; break;
      case SB_20  : s = "2.0"; break;
      case SB_PRO : s = "PRO"; break;
      case SB_16  : s = "16";
    }
    S_printf("SoundBlaster %s can be emulated.\n", s);
  }
  else 
    S_printf("No sounddevice for SoundBlaster emulation found.\n");
  return version;
#endif
#ifdef __NetBSD__
/* XXX */
    S_printf("No sounddevice for SoundBlaster emulation found.\n");
  return 0;
#endif  
}

void sound_init(void)
{
  S_printf ("Initialising the SB emulation\n");

  /* Check the DSP */
  sb_version = dsp_get_capability();
  
  if (sb_version) {
    /* emulation is possible, so allocate the IRQ */
    dma_ch[sound_dma_ch].tc_irq = PIC_IRQ5;

    /* for now, SB 1.5 is enough :-( */
    sb_version = SB_OLD;

    pic_seti(PIC_IRQ5, do_irq, 5);
    pic_unmaski(PIC_IRQ5);
  }
  else {
    /* switch sound off */
    /* config.sound = 0; */
  }
}

