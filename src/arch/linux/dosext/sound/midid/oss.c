/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/***********************************************************************
  OSS Lite device
 ***********************************************************************/

#include <stdio.h>
#include <stdlib.h>

/* You shouldn't have to change this since it's pretty standard */
#define SEQUENCER_DEV   "/dev/sequencer"  /* Used device */
#define PATH_FM_DEFAULT "/etc/"           /* Location of FM instrument files */
#define O3MELODIC       "std.o3"          /* OPL3 instruments file */
#define O3DRUMS         "drums.o3"        /* OPL3 drums file */
#define SBMELODIC       "std.sb"          /* OPL2 instruments file */
#define SBDRUMS         "drums.sb"        /* OPL3 drums file */

#include "device.h"
#include <sys/types.h>     /* for open(2) */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

#define seqbuf_dump oss_seqbuf_dump
#include <sys/soundcard.h>

SEQ_USE_EXTBUF();

int seqfd;               /* Sequencer file handle */
char *path_fm;           /* Location of FM instrument files */
char *o3melodic, *o3drums, *sbmelodic, *sbdrums;
struct synth_info card_info; /* OSS synthesizer device info */
int device;              /* OSS synthesizer device number */
int curvoc;              /* Current voice to be allocated */
int timestamp;           /* Timestamp for voice aging */
typedef struct Voice {
  bool active;           /* TRUE if note is active */
  int time;              /* Timestamp when note started */
  int chn;               /* Channel */
  int note;              /* Note */
} Voice;
Voice *voices;
int chn2prg[16];         /* The program on each channel */

void oss_seqbuf_dump (void)
{
  if (_seqbufptr) {
    if (write (seqfd, _seqbuf, _seqbufptr) == -1) {
      perror ("write " SEQUENCER_DEV);
      exit (-1);
    }
    _seqbufptr = 0;
  }
}

/* Load all FM instruments; routine from playmidi-2.3/patchload.c
   Return TRUE iff successful */
bool loadfm(void)
{
    int sbfd, i, n, voice_size, data_size;
    char buf[60];
    struct sbi_instrument instr;

    if (config.opl3) {
	voice_size = 60;
	sbfd = open(o3melodic, O_RDONLY, 0);
    } else {
	voice_size = 52;
	sbfd = open(sbmelodic, O_RDONLY, 0);
    }
    if (sbfd == -1) {
      perror("open instrument file");
      return(FALSE);
    }
    instr.device = device;

    for (i = 0; i < 128; i++) {
        if (read(sbfd, buf, voice_size) != voice_size) {
	  perror("reading instrument file");
	  return(FALSE);
	}
	instr.channel = i;

	if (strncmp(buf, "4OP", 3) == 0) {
	    instr.key = OPL3_PATCH;
	    data_size = 22;
	} else {
	    instr.key = FM_PATCH;
	    data_size = 11;
	}

	/* adjustfm(buf, instr.key); */
	for (n = 0; n < 32; n++)
	    instr.operators[n] = (n < data_size) ? buf[36 + n] : 0;

	SEQ_WRPATCH(&instr, sizeof(instr));
    }
    close(sbfd);

    if (config.opl3)
	sbfd = open(o3drums, O_RDONLY, 0);
    else
	sbfd = open(sbdrums, O_RDONLY, 0);

    for (i = 128; i < 175; i++) {
        if (read(sbfd, buf, voice_size) != voice_size) {
	  perror("reading drumset file");
	  return(FALSE);
	}
	instr.channel = i;

	if (strncmp(buf, "4OP", 3) == 0) {
	    instr.key = OPL3_PATCH;
	    data_size = 22;
	} else {
	    instr.key = FM_PATCH;
	    data_size = 11;
	}

	/* adjustfm(buf, instr.key);  */
	for (n = 0; n < 32; n++)
	    instr.operators[n] = (n < data_size) ? buf[n + 36] : 0;

	SEQ_WRPATCH(&instr, sizeof(instr));
    }
    close(sbfd);
    return(TRUE);
}

bool oss_detect(void)
{
  int sdevs;     /* Number of synthesizers */
  seqfd=open(SEQUENCER_DEV,O_WRONLY);
  if (seqfd==-1) return(FALSE);
  if (ioctl(seqfd, SNDCTL_SEQ_NRSYNTHS,&sdevs)==-1) {
    close(seqfd);
    return(FALSE);
  }
  close(seqfd);
  if (!sdevs) return(FALSE);
  return(TRUE);
}

bool oss_init(void)
{
  int sdevs;     /* Number of synthesizers */
  int i;
  
  seqfd=open(SEQUENCER_DEV,O_WRONLY);
  if (seqfd==-1) {
    perror("oss_init");
    return(FALSE);
  }
  if (ioctl(seqfd, SNDCTL_SEQ_NRSYNTHS,&sdevs)==-1) {
    close(seqfd);
    perror("SNDCTL_SEQ_NRSYNTHS");
    exit(1);
  }
  if (!sdevs) {
    close(seqfd);
    fprintf(stderr,"No synthesizers devices available with OSS\n");
    return(FALSE);
  }

  /* Compose locating of instrument */
  path_fm=PATH_FM_DEFAULT;
  o3melodic=malloc(strlen(path_fm)+strlen(O3MELODIC)+1);
  strcpy(o3melodic,path_fm);
  strcat(o3melodic,O3MELODIC);
  o3drums=malloc(strlen(path_fm)+strlen(O3DRUMS)+1);
  strcpy(o3drums,path_fm);
  strcat(o3drums,O3DRUMS);
  sbmelodic=malloc(strlen(path_fm)+strlen(SBMELODIC)+1);
  strcpy(sbmelodic,path_fm);
  strcat(sbmelodic,SBMELODIC);
  sbdrums=malloc(strlen(path_fm)+strlen(SBDRUMS)+1);  
  strcpy(sbdrums,path_fm);
  strcat(sbdrums,SBDRUMS);

  /* Find first FM synthesizer */
  device=-1;
  for (i = 0; i < sdevs; i++) {
    card_info.device = i;
    if (ioctl(seqfd, SNDCTL_SYNTH_INFO, &card_info) == -1) {
      perror("SNDCTL_SYNTH_INFO");
      exit(-1);
    }
    card_info.device = i;
    if (card_info.synth_type == SYNTH_TYPE_FM) {
      device = i;
      if (config.opl3)
	card_info.nr_voices = 6;	/* we have 6 with 4-op */
    }
  }

  if (device==-1)
    error("No FM synthesizer found");
  
  if (!loadfm())
    error("Error while loading FM patches");

  curvoc=0;
  timestamp=0;
  voices=malloc(sizeof(Voice)*card_info.nr_voices);
  for(i=0;i<card_info.nr_voices;i++)
    /* Reset each voice */
    voices[i].active=FALSE;
  for(i=0;i<16;i++)
    /* Reset each channel */
    chn2prg[i]=0;
  
  SEQ_START_TIMER();
  
  return(TRUE);	
}

void oss_done(void)
{
  free(voices);
  close(seqfd);
}

void oss_flush(void)
{
  SEQ_DUMPBUF();
}

bool oss_setmode(Emumode new_mode)
{
  if (new_mode == EMUMODE_GM)
    return TRUE;
  return FALSE;
}

void oss_noteon(int chn, int note, int vel)
{
  int i;
  int mintime=timestamp+1;
  int minvoice=-1;
  /* Find a free voice or the oldest voice */
  for(i=0;i<card_info.nr_voices;i++) {
    if (!voices[i].active) break;
    if (voices[i].time<mintime) {
      mintime=voices[i].time;
      minvoice=i;
    }
  }
  if (voices[i].active) {
    /* Kill old voice */
    i=minvoice;
    SEQ_STOP_NOTE(device, i, voices[i].note, 0);
  }
    
  voices[i].active=TRUE;
  voices[i].chn=chn;
  voices[i].note=note;
  voices[i].time=timestamp++;
  SEQ_SET_PATCH(device, i, chn2prg[voices[i].chn]);
  SEQ_START_NOTE(device, i, note, vel);
}

void oss_noteoff(int chn, int note, int vel)
{}

void oss_control(int chn, int control, int value)
{}

void oss_notepressure(int chn, int control, int value)
{}

void oss_channelpressure(int chn, int vel)
{}

void oss_bender(int chn, int pitch)
{}

void oss_program(int chn, int pgm)
{}

void register_oss(Device * dev)
{
	dev->name = "OSS FM Synthesizer";
	dev->version = 10;
	dev->detect = oss_detect;
	dev->init = oss_init;
	dev->done = oss_done;
	dev->flush = oss_flush;
	dev->noteon = oss_noteon;
	dev->noteoff = oss_noteoff;
	dev->control = oss_control;
	dev->notepressure = oss_notepressure;
	dev->channelpressure = oss_channelpressure;
	dev->bender = oss_bender;
	dev->program = oss_program;
	dev->setmode = oss_setmode;
}


