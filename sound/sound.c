/* Copyright 1995  Joel N. Weber II
   See the file README.sound in this directory for more information */

#include "emu.h" /* For S_printf */

#include <fcntl.h>
#ifdef __linux__
#include <linux/soundcard.h>
#endif
#ifdef __NetBSD__
#include <sys/ioctl.h>
#include <sys/audioio.h>
#endif
#include "pic.h"
#include "dma.h"
#include "sound.h"

/* These two are used by dos programs to read data from the dsp.  The dsp
code in this file will write data to the variable dsp_out and then set
dsp_avail to 1.  I think that right now this is only used to help programs
think they've reset the chip.  However it can also be used as an inefficent
way to do A/D; it probably wouldn't be a great idea to support it due to
timing problems when you're using a multitasking environment like Linux.
It probably is also involved in the MIDI port; if you have some info on
how this works, please add the appropriate code */
#define DSP_QUEUE_SIZE 10
static unsigned char dsp_out[DSP_QUEUE_SIZE];
int dsp_queue_start = 0;
int dsp_queue_end = 0;
unsigned char dsp_read_output(void);
void dsp_write_output(unsigned char );

static unsigned char dsp_avail = 0;

/* The way the dsp works involves writing a few bytes.  However, my procedure
gets called once for each out instruction, so this variable stores the last
command written. */
static unsigned char dsp_last_write = 0;

/* Similarily, this is the index register for the mixer: */
static unsigned char mixer_index = 0;

/* When you give the command to do a dma transfer, you have to write a two
byte size.  I'm not sure why when you consider that the dma controller also
has a count.  This variable keeps track of whether you are writing the high
or low byte. */
static unsigned char dsp_write_size_mode = 0;

/* The dsp rate and data width params */
static unsigned char dsp_stereo = 0;
static unsigned char dsp_rate = 0;

/* These are the file desciptors used to communicate with Hannu's sound
driver */
static int mixer_fd = -1, dsp_fd = -1, synth_fd = -1;

static void dsp_clear_output(void);

#ifdef __linux__
inline void start_dsp_dma(void)

/* This will open the fd and do everything else to prepare to output data... */

{
#ifndef SOUND_FRAG
#error SOUND_FRAG not defined!
#endif
  int real_speed, trash;
  long int sound_frag = SOUND_FRAG;
 
  S_printf ("Starting to open DMA access to DSP\n");
 
  if (dsp_stereo)
    {real_speed = dsp_rate >> 1;}
  else
    {real_speed = dsp_rate;};

  dsp_fd = open(DSP_PATH, O_WRONLY | O_NONBLOCK);
  ioctl(dsp_fd, SNDCTL_DSP_SETFRAGMENT, &sound_frag);
  ioctl(dsp_fd, SOUND_PCM_WRITE_RATE, &real_speed);
  /*ioctl(dsp_fd, SNDCTL_PCM_WRITE_RATE, &real_speed);*/
  if (dsp_stereo)
    trash = 1;
  else
    trash = 2;
  /*ioctl(dsp_fd, SNDCTL_PCM_WRITE_CHANNELS, &trash);*/
  ioctl(dsp_fd, SOUND_PCM_WRITE_CHANNELS, &trash);
  dma_ch[sound_dma_ch].fd = dsp_fd;
  dma_ch[sound_dma_ch].dreq = DREQ_COUNTED;
};
#endif __linux__
#ifdef __NetBSD__
inline void start_dsp_dma(void)
/* This will open the fd and do everything else to prepare to output data... */
{
  int real_speed, trash;
  struct audio_info ainfo;

  S_printf ("Starting to open DMA access to DSP\n");

  dsp_fd = open(DSP_PATH, O_WRONLY | O_NONBLOCK);
  if (dsp_fd == -1)
      return;				/* XXX */

  AUDIO_INITINFO(&ainfo);
  ainfo.play.sample_rate = dsp_rate;
  ainfo.play.channels = dsp_stereo ? 2 : 1;
  ainfo.play.precision = 8;
  ainfo.play.encoding = AUDIO_ENCODING_PCM8; /* XXX unsigned linear 8-bit */

  ioctl(dsp_fd, AUDIO_SETINFO, &ainfo);
  dma_ch[sound_dma_ch].fd = dsp_fd;
  dma_ch[sound_dma_ch].dreq = DREQ_COUNTED;
};
#endif /* __NetBSD__ */

inline void stop_dsp_dma(void)

/* This undos everything the previous function does ... */

{
  S_printf ("Stopping DMA access to DSP\n");

  dma_ch[sound_dma_ch].dreq = DREQ_OFF;
  close(dsp_fd);
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
/* addr must be offset from base */
void sb_write(unsigned char addr, unsigned char value)
{
  S_printf ("Write to SB (0x%x, 0x%x)", addr, value);
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
                 case 0x14: if (dsp_write_size_mode = !dsp_write_size_mode)
                                {dma_ch[sound_dma_ch].dreq_count = value;}
                              else
                                {dma_ch[sound_dma_ch].dreq_count += 256 * value;
                                 dsp_last_write = 0;
                                 start_dsp_dma();
                                 };
                              break;
                 default: /* figure out what command */
                            switch (value) {
                              case 0xD0: stop_dsp_dma(); break;
                              case 0xD4: start_dsp_dma(); break;
                              case 0x14: dsp_write_size_mode = 0; 
				         /* start_dsp_dma(); */
                                         break;
			      case 0xE1: /* SB Pro 
				         dsp_write_output(3);
				         dsp_write_output(0); */
				         /* SB */
				         dsp_write_output(1);
				         dsp_write_output(5);
				         break;
			      case 0xD1: /* Enable Speaker */
			      case 0xD3: /* Disable Speaker */
				         break;
			      default:   S_printf ("Ignored instruction (%x)\n", value);
                              };
                            dsp_last_write = value;
               }
               break;
    /* 0x00 -- 0x03 are the fm registers */
    case 0x04: mixer_index = value; break;
    case 0x05: switch (mixer_index) {
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
               };
  };
}


#ifdef __linux__
inline unsigned char mixer_read_setting(unsigned char ch)
{
  int x;

  ioctl(mixer_fd, MIXER_READ(ch), &x);
  
  S_printf ("Reading from the Mixer (%u -> %u)\n", ch, x);

  return (((x & 0x00FF) / 6) | ((x & 0xFF00) / 0x600));
}

/* The following routine is broken :-( */
inline unsigned char mixer_read_mic(void)
{
  int x;

  ioctl(mixer_fd, MIXER_READ(SOUND_MIXER_MIC), &x);
  
  S_printf ("Reading from the Mixer Microphne (%u)\n", x);

  return x / 7; /* This isn't the right value.  Anyone care to tell me
                   how to do division in C???? */
}
#endif

void dsp_write_output(unsigned char value)
{
  /* This implements a queue for output ... */
  /* These is no check for exceeding the length of the queue ... */

  S_printf ("Insert into SB output Queue ... (%u)\n", value);
  dsp_out[dsp_queue_end] = value;
  dsp_queue_end = (dsp_queue_end < DSP_QUEUE_SIZE) ? dsp_queue_end+1 : 0;

  dsp_avail = 0x80;
}

void dsp_clear_output(void)
{
  S_printf ("Clearing the SB output Queue");
  dsp_queue_end = 0;
  dsp_queue_start = 0;
  dsp_out[0] = 0;

  dsp_avail = 0;
}


unsigned char dsp_read_output()
{
  unsigned char r;

  /* this implementes a queue for output ... */
  r = dsp_out[dsp_queue_start];
  S_printf ("Remove from SB output Queue ... (%u)\n", r);
  dsp_out[dsp_queue_start] = 0x00;
  dsp_queue_start = (dsp_queue_start < DSP_QUEUE_SIZE) ? dsp_queue_start+1 : 0;

  if (dsp_queue_start != dsp_queue_end)
  {
    dsp_avail = 0x80;
  } 
  else
  {
    dsp_avail = 0x00;
  }

  return r;
}

unsigned char sb_read(unsigned char addr)

{
  S_printf ("Read from SB (%u)\n", addr);
  switch (addr){
    /* case 0x0A: return dsp_out; dsp_avail = 0x00; break; */
    case 0x0A: return dsp_read_output(); break;
    case 0x0C: 
    case 0x0E: return dsp_avail; break;
    case 0x05: switch (mixer_index){
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
      };
    };
}

void fm_write(unsigned char addr, unsigned char value)
{
  S_printf ("Write to the FM sythesizer ... ignored ...\n");
}

/* Yes, you just saw another completely broken routine.  Don't just sit there
and think this is junk, FIX IT! */

void sound_init(void)
{
  S_printf ("Initiallising the SB emulation\n");

  mixer_fd = open(MIXER_PATH, O_RDWR);
  dma_ch[sound_dma_ch].tc_irq = sound_irq;
}

