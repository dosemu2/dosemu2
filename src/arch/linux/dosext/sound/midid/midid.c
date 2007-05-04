/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*

   Reads STDIN and interprets it as raw MIDI data according to
   the General Midi standard.

   For use with DOSEMU0.65+, read the dosemu/doc/README.sound .
   
   (c) 1997 R.Nijlunsing <rutger@null.net>
   (c) 2002 Robert Komar <rkomar@telus.net>
   (c) 2002-2005 Stas Sergeev <stsp@users.sourceforge.net>

   #include GNU copyright

      12-1996: Initial version
      01-1997: GUS gives sound with UltraDriver 2.20
   ??-02     : v1.00
	       Compiles again; even with UltraDriver 2.60a-devel6
               Added null-driver
   27-02     : v1.01
	       Compilable with devel8
	       Added instrument mappings (for MT32 emulation on GM)
   30-02     : v1.02
               Added stubs for OSS support
	       UltraDriver even works
   02-03     : v1.03
               Added parameter support
	       Splitup in alot of files :)
      05-2002: v1.04
               Added MIDI file writer	 	(Robert Komar)
	       Added timidity server driver 	(Stas Sergeev)
	       Added ability to use more
	    	    than one device at a time 	(Stas Sergeev)
	       Misc cleanups and fixes 		(Stas Sergeev, Rob Komar)
	       The whole Rutger's TODO list is now covered and deleted:)
 */

#define VER_MAJOR 1
#define VER_MINOR 5

#include "midid.h"
#include "events.h"
#include "io.h"
#include "stats.h"
#include "device.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define _GNU_SOURCE		/* for getopt_long(), see man */
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/soundcard.h>

int debugall;			/* 1 for all read bytes */
int debug;			/* 1 for interpretation of each byte */
int ignored;			/* 1 for ignored but recognised messages */
int comments;			/* 1 for status report */
int warning;			/* 1 for warnings during interpretation */
int statistics;			/* 1 for statistics */
int initialised = 0;
int fd;				/* file descriptor */


/***********************************************************************
  Options handling
 ***********************************************************************/

Config config = { EMUMODE_GM, FALSE, 0, 0, 0, FALSE,
    "", "midid.mid", 120, 144, "localhost", 0,
    "timidity", "-EFreverb=0 -EFchorus=0 -EFresamp=1 -EFvlpf=0 -EFns=0",
    0, 0, 0, 0, 0 };

static void usage(void)
{
#define NEW_TIMIDITY 1
  printf("midid [parameters] [input file]\n\n"
	 "-h,  --help         : help\n"
	 "-l,  --list-devices : list devices\n"
	 "-d#, --device=#     : comma-separated list of devices to use\n"
	 "-m,  --emulate-mt   : select MT32 mode\n"
	 "-g,  --emulate-gm   : select GM mode (default)\n"
	 "-r,  --resident     : be resident; don't stop on end of file\n"
	 "                      (only when input file != stdin)\n"
	 "-o#,  --timeout=#   : turn output off after # seconds of inactivity.\n"
	 "                      (default: 0 - no timeout)\n"
	 "input file defaults to stdin\n\n"
	 "Options for .MID file output device:\n"
	 "-f, --file          : Set .MID output file name. Default: %s\n"
	 "-t#, --tempo=#      : Set tempo (beats/minute). Default: %i\n"
	 "-q#, --tick-rate=#  : Set # ticks/quarter note. Default: %i\n"
	 "\n"
	 "Options for timidity client:\n"
	 "-s, --server-name             : timidity server host name. Default: %s\n"
	 "-p#, --port=#                 : timidity server control port. Default: %i\n"
#if NEW_TIMIDITY
	 "                                Specifying 0 enables the standalone mode:\n"
	 "                                midid will attempt to start and configure\n"
	 "                                timidity itself.\n"
	 "\tIn standalone mode the following options are available:\n"
	 "-B path, --timidity-bin=path  : timidity binary path or name\n"
	 "-A args, --timidity-args=args : extra arguments to pass to timidity\n"
	 "                                default: %s\n"
	 "-M, --timidity-mono           : tells timidity to produce mono sound\n"
	 "                                instead of stereo\n"
	 "-L, --timidity-low            : tells timidity to produce the low-q\n"
	 "                                8bit sound instead of the 16bit\n"
	 "-U, --timidity-uns            : tells timidity to produce the unsigned\n"
	 "                                samples instead of the signed ones\n"
	 "-F #, --timidity-freq=#       : set the sampling rate for timidity\n"
	 "-C, --timidity-capture        : enables the capturing mode: the data produced\n"
	 "                                by timidity gets captured by midid and\n"
	 "                                forwarded to stdout.\n"
	 "                                You won't hear any sound in that mode.\n"
#endif
	 "\n",
	 config.midifile, config.tempo, config.ticks_per_quarter_note,
	 config.timid_host, config.timid_port
#if NEW_TIMIDITY
	 , config.timid_args
#endif
	 );
}

static void options_read(int argc, char **argv)
/* Read and evaluate command line options */
{
  int c, need_printall = FALSE, option_index = 0, need_detect = TRUE;
  char *ptr;
  static struct option long_options[] = {
    {"help", 0, 0, 'h'},
    {"list-devices", 0, 0, 'l'},
    {"device", 1, 0, 'd'},
    {"emulate-mt", 0, 0, 'm'},
    {"emulate-gm", 0, 0, 'g'},
    {"resident", 0, 0, 'r'},
    {"timeout", 1, 0, 'o'},
    {"verbosity", 1, 0, 'v'},
    /* for UltraDriver */
    {"card", 1, 0, 'c'},
    /* for OSS */
    {"opl2", 0, 0, '2'},
    {"opl3", 0, 0, '4'},
    /* for midout */
    {"file", 1, 0, 'f'},
    {"tempo", 1, 0, 't'},
    {"tick-rate", 1, 0, 'q'},
    /* for timid */
    {"server-name", 1, 0, 's'},
    {"port", 1, 0, 'p'},
    {"timidity-bin", 1, 0, 'B'},
    {"timidity-args", 1, 0, 'A'},
    {"timidity-mono", 0, 0, 'M'},
    {"timidity-low", 0, 0, 'L'},
    {"timidity-uns", 0, 0, 'U'},
    {"timidity-freq", 1, 0, 'F'},
    {"timidity-capture", 0, 0, 'C'},
    {0, 0, 0, 0}
  };
  /* Set options to default values */
  debugall = 0;
  debug = 1;
  ignored = 1;
  warning = 1;
  statistics = 1;
  /* Read all command line options */
  while (1) {
    c = getopt_long(argc, argv, "hld:mgro:v:c:24f:t:q:s:p:B:A:MLUF:C",
		    long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
    case '?':
    case 'h':
      usage();
      exit(1);
    case 'l':
      need_printall = TRUE;
      break;
    case 'd': {
        int num = 0;
        while ((ptr = strsep(&optarg, ","))) {
          num = atoi(ptr);
          if (!num)
            break;
          device_activate(num);
        }
        if (!num)
          break;
        need_detect = FALSE;
      }
      break;
    case 'm':
      config.mode = EMUMODE_MT32;
      break;
    case 'g':
      config.mode = EMUMODE_GM;
      break;
    case 'r':
      config.resident = TRUE;
      break;
    case 'o':
      config.timeout = atoi(optarg);
      break;
    case 'v':
      config.verbosity = atoi(optarg);
      break;
    case 'c':
      config.card = atoi(optarg);
      break;
    case '2':
      config.opl3 = FALSE;
      break;
    case '4':
      config.opl3 = TRUE;
      break;
    case 'f':
      config.midifile = optarg;
      break;
    case 't':
      config.tempo = atoi(optarg);
      break;
    case 'q':
      config.ticks_per_quarter_note = atoi(optarg);
      break;
    case 's':
      config.timid_host = optarg;
      break;
    case 'p':
      config.timid_port = atoi(optarg);
      break;
    case 'B':
      config.timid_bin = optarg;
      break;
    case 'A':
      config.timid_args = optarg;
      break;
    case 'C':
      config.timid_capture = 1;
      break;
    case 'M':
      config.timid_mono = 1;
      break;
    case 'L':
      config.timid_8bit = 1;
      break;
    case 'U':
      config.timid_uns = 1;
      break;
    case 'F':
      config.timid_freq = atoi(optarg);
      break;
    }
  }
  if (optind < argc)
    config.inputfile = argv[optind];
  if (config.resident && (!strlen(config.inputfile))) {
    fprintf(stderr, "Error: Can't use resident option without inputfile!\n"
	    "       Resident option disabled.\n");
    config.resident = FALSE;
  }
  /* detection results may depend on options so we have to show devices
     after parsing is finished */
  if (need_detect)
    device_detect_all();
  if (need_printall) {
    device_printall();
    exit(1);
  }
}

/***********************************************************************
  Main
 ***********************************************************************/

int main(int argc, char **argv)
{
  int ch, chn;			/* Current status (==command) */
  int last_status = MAGIC_INV;	/* Last performed status */

  comments = 1;

  /* Register all devices */
  device_register_all();

  /* Evaluate command line options */
  options_read(argc, argv);

  /* Banner */
  if (comments)
    fprintf(stderr, "Midi Daemon / interpreter v%i.%02i\n"
	    "(c) 1997 R.Nijlunsing <rutger@null.net>\n"
	    "(c) 2002-2005 Stas Sergeev <stsp@users.sourceforge.net>\n"
	    "Use '%s --help' for options\n\n", VER_MAJOR, VER_MINOR, argv[0]);

  /* Open input */
  if (strlen(config.inputfile)) {
    /* open given inputfile */
    fd = open(config.inputfile, O_RDONLY);
    if (fd == -1) {
      perror(config.inputfile);
      exit(1);
    }
  } else {
    /* Use stdin file descriptor */
    fd = 0;
  }

  if (!device_activate(0))
    error("No supported music device detected!");

  if (!config.timeout) {
    if (! (initialised = device_init_all())) {
      error("Driver init failed!\n");
      exit(1);
    }
    fprintf(stderr, "Initialised %i devices.\n\n", initialised);
  }
  if (statistics)
    stats_reset();

  /* Read first byte */
  getbyte_next();
  while (1) {
    ch = getbyte_status();
    if (ch == MAGIC_EOF || ch == MAGIC_TIMEOUT) {
      last_status = MAGIC_INV;
      /* End of file? */
      if (ch == MAGIC_EOF && !config.resident) {
	/* Not resident; quit */
	break;
      }
      if (initialised)
        device_pause_all();
      do {
	usleep(100000);
	getbyte_next();
	ch = getbyte();
      } while (ch == MAGIC_TIMEOUT);
      if (initialised && ch != MAGIC_EOF)
        device_resume_all();
      continue;
    }
    /* Illegal for a status byte? */
    if (ch == MAGIC_INV) {
      /* This is not a status byte, but a data byte.
         Use previous status byte. */
      ch = last_status;
    } else {
      if ((ch & 0xf0) != MIDI_SYSTEM_PREFIX)
        last_status = ch;
      else
        last_status = MAGIC_INV;
      getbyte_next();
    }
    if (ch == MAGIC_INV) {
      fprintf(stderr, "Bad data, ignoring\n");
      getbyte_next();
      continue;
    }

    if (!initialised) {
      if (! (initialised = device_init_all())) {
	error("Driver init failed!\n");
	exit(1);
      }
      fprintf(stderr, "Initialised %i devices.\n\n", initialised);
    }

    if (statistics)
      stats_addcommand(ch);
    chn = (ch & 0xf);
    switch (ch & 0xf0) {
    case MIDI_NOTEOFF:
      do_noteoff(chn);
      break;
    case MIDI_NOTEON:
      do_noteon(chn);
      break;
    case MIDI_KEY_PRESSURE:
      do_notepressure(chn);
      break;
    case MIDI_CTL_CHANGE:
      do_controlchange(chn);
      break;
    case MIDI_PGM_CHANGE:
      do_program(chn);
      break;
    case MIDI_CHN_PRESSURE:
      do_channelpressure(chn);
      break;
    case MIDI_PITCH_BEND:
      do_bender(chn);
      break;
    case MIDI_SYSTEM_PREFIX:
      switch (chn) {
        /* Common System Messages */
        case 0x00:		/* SysEx */
	  if (debug)
	    fprintf(stderr, "SysEx\n");
	  do_sysex();
	  break;
	case 0x01:		/* Quarter Frame */
	  do_quarter_frame();
	  break;
	case 0x02:		/* Song Position */
	  do_song_position();
	  break;
	case 0x03:		/* Song Select */
	  do_song_select();
	  break;
	case 0x04:
	case 0x05:		/* Undef */
	  if (warning)
	    fprintf(stderr, "Undefined Common system message %x\n", ch);
	  getbyte_next();
	  break;
	case 0x06:		/* Tune Request */
	  do_tune_request();
	  break;
	case 0x07:		/* EOX */
	  if (warning)
	    fprintf(stderr, "Unexpected EOX\n");
	  getbyte_next();
	  break;

	  /* System Real Time Messages */
	case 0x08:		/* MIDI Clock */
	  do_midi_clock();
	  break;
	case 0x09:		/* MIDI Tick */
	  do_midi_tick();
	  break;
	case 0x0A:		/* MIDI Start */
	  do_midi_start();
	  break;
	case 0x0B:		/* MIDI Continue */
	  do_midi_continue();
	  break;
	case 0x0C:		/* MIDI Stop */
	  do_midi_stop();
	  break;
	case 0x0D:		/* Undef */
	  if (warning)
	    fprintf(stderr, "Undefined Real-Time system message %x\n", ch);
	  getbyte_next();
	  break;
	case 0x0E:		/* Active Sensing */
	  do_active_sensing();
	  break;
	case 0x0F:		/* Reset */
	  do_reset();
	  break;
      }
      break;
    default:
      if (warning)
	fprintf(stderr, "Warning: Unkown midi message (0x%02x)\n", ch);
      getbyte_next();
      break;
    }
    /* Make it happen _now_ */
    dev_flush();
  }

  if (statistics)
    stats_print();
  if (initialised) {
    device_stop_all();
    initialised = 0;
  }
  return 0;
}
