/* 
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdio.h>
#include <sys/soundcard.h>
#include <sys/types.h>     /* for open(2) */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

SEQ_DEFINEBUF(1024);

int seqfd;               /* Sequencer file handle */

void seqbuf_dump ()
{
  if (_seqbufptr) {
    if (write (seqfd, _seqbuf, _seqbufptr) == -1) {
      perror ("write /dev/sequencer");
      exit (-1);
    }
    _seqbufptr = 0;
  }
}

void main(void)
{
  struct synth_info sinfo;
  struct midi_info minfo;
  int i;
  int sdevs,mdevs;
  seqfd=open("/dev/sequencer",O_WRONLY);
  if (!seqfd) {
    perror("oss_init");
  }
  ioctl(seqfd, SNDCTL_SEQ_NRSYNTHS,&sdevs);
  ioctl(seqfd, SNDCTL_SEQ_NRMIDIS,&mdevs);
  printf("%i synthesizers\n%i midis\n",sdevs,mdevs);
  SEQ_START_NOTE(1,1,60,127);
  SEQ_DUMPBUF();
  sleep(2);
  close(seqfd);
}
