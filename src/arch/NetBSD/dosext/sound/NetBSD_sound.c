/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Copyright 1995  Joel N. Weber II
   See the file README.sound in this directory for more information */

/* 
 * DANG_BEGIN_MODULE
 *
 * Soundblaster emulation. Very incomplete. NetBSD version.
 *
 * maintainer:
 * 
 *
 * DANG_END_MODULE
 */

/*
 * modified 11/05/95 by Michael Beck
 *  added some more (undocumented ?) SB commands, version detection
 */
#include "emu.h" /* For S_printf */

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include "pic.h"
#include "dma.h"
#include "sound.h"
#include "NetBSD_sound.h"

/*
 * This will do everything prepare to output data...
 */
/* 
 * The NetBSD version is completely broken at the moment
 */
void start_dsp_dma(void)
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

/* This undoes everything the previous function does ... */
void stop_dsp_dma(void)
{
  S_printf ("Stopping DMA access to DSP\n");

  dma_ch[sound_dma_ch].dreq = DREQ_OFF;
}

/*
 * should assemble a mixer structure for dealing with SB-type devices.
 * not all audio devices will have a good mixer, though :(
 * For now, just punt.
 */

void mixer_write_setting(__u8 ch, __u8 val)
{
}

void mixer_write_mic(__u8 val)
{
}

__u8 mixer_read_setting (__u8 ch)
{
  return (0xff / 6 ) | (0xffff / 0x600);
}

__u8 mixer_read_mic(void)
{
  return 0xff / 7;
}

/*
 * this function tries to detect the possible Soundblaster revision
 * we can emulate
 * one possible way is to read /dev/sndstat and check for the Soundblaster
 * version, however this didn't work for other cards so we try to check
 * whether stereo and 16 bits output is supported
 */
int dsp_get_capability(void)
{
/* DANG_FIXME dsp_get_capability is completely broken for NetBSD */
    S_printf("No sound device for SoundBlaster emulation found.\n");
  return 0;
}
/* Copyright 1995  Joel N. Weber II
   See the file README.sound in this directory for more information */

/* 
 * DANG_BEGIN_MODULE
 *
 * Soundblaster emulation. Very incomplete. NetBSD version.
 *
 * maintainer:
 * 
 *
 * DANG_END_MODULE
 */

/*
 * modified 11/05/95 by Michael Beck
 *  added some more (undocumented ?) SB commands, version detection
 */
#include "emu.h" /* For S_printf */

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include "pic.h"
#include "dma.h"
#include "sound.h"
#include "NetBSD_sound.h"

/*
 * This will do everything prepare to output data...
 */
/* 
 * The NetBSD version is completely broken at the moment
 */
void start_dsp_dma(void)
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

/* This undoes everything the previous function does ... */
void stop_dsp_dma(void)
{
  S_printf ("Stopping DMA access to DSP\n");

  dma_ch[sound_dma_ch].dreq = DREQ_OFF;
}

/*
 * should assemble a mixer structure for dealing with SB-type devices.
 * not all audio devices will have a good mixer, though :(
 * For now, just punt.
 */

void mixer_write_setting(__u8 ch, __u8 val)
{
}

void mixer_write_mic(__u8 val)
{
}

__u8 mixer_read_setting (__u8 ch)
{
  return (0xff / 6 ) | (0xffff / 0x600);
}

__u8 mixer_read_mic(void)
{
  return 0xff / 7;
}

/*
 * this function tries to detect the possible Soundblaster revision
 * we can emulate
 * one possible way is to read /dev/sndstat and check for the Soundblaster
 * version, however this didn't work for other cards so we try to check
 * whether stereo and 16 bits output is supported
 */
int dsp_get_capability(void)
{
/* DANG_FIXME dsp_get_capability is completely broken for NetBSD */
    S_printf("No sound device for SoundBlaster emulation found.\n");
  return 0;
}
