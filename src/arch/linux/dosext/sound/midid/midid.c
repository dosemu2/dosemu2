/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*

   Reads STDIN and interprets it as raw MIDI data according to
   the General Midi standard.

   For use with DOSEMU0.65+, read the dosemu/doc/README.sound .
   
   (c)1997 R.Nijlunsing <rutger@null.net>

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
	       
	       
   TODO (in appearance of importance):
     * complete interface to OSS/Lite/Free driver
     * add 'all notes off' functionality independant of driver
     * driver for realtime recording of MIDI files
     * GS Standard. Anyone got documentation on this one?
     * remove some untrue warnings given
     * make Sysex events of:
       * Mode setting (MT32/GM)
       * Preloading of instruments

 */

#include"midid.h"
#include"events.h"
#include"io.h"
#include"stats.h"
#include"emulation.h"
#include"string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

Device *devices = NULL;         /* list of all drivers */

/***********************************************************************
  Unitialised device default routines
 ***********************************************************************/

bool default_setmode(Emumode new_mode)
{
  /* Can't set another emulation mode */
  return FALSE;
}

/***********************************************************************
  Devices handling
 ***********************************************************************/

void device_add(void (*register_func) (Device * dev))
/* Add device with register function dev to beginning of device list */
{
	Device *newdev;
	newdev = malloc(sizeof(Device));
	assert(newdev);
	/* Setting up some default values */
	newdev->name = "(noname)";
	newdev->version = 0;
	newdev->setmode = default_setmode;
	/* Fill in rest of the values */
	register_func(newdev);
	newdev->next = devices;
	devices = newdev;
}

/* The register function are all in their own device file */
#ifdef USE_ULTRA
extern void register_gus(Device * dev);
#endif
extern void register_oss(Device * dev);
extern void register_null(Device * dev);

void device_register_all(void)
{
	device_add(register_null);
	device_add(register_oss);
#ifdef USE_ULTRA
	device_add(register_gus);
#endif
}

Device *device_findfirst(void)
/* Return the first detected device of the list;
   NULL if none found */
{
	/* Select first device */
	Device *dev=devices;
	while (dev) {
		if (dev->detect()) return(dev);
		dev=dev->next;
	}
	return(dev);
}

void device_printall(void)
/* Prints a list of available drivers */
{
  int i=1;
  Device *dev=devices;
  while(dev) {
    printf("%i. %s (v%i.%02i) ",i,dev->name,dev->version/100,dev->version%100);
    if (dev->detect()) printf("[detected]");
    printf("\n");
    i++;
    dev=dev->next;
  }
}

Device *device_select(int number)
/* Select device number <number>; first one is 1.
   0 means first detected one */
{
  Device *dev=devices;
  if (!number)
    return(device_findfirst());
  else {
    number--;
    while((number--) && dev)
      dev=dev->next;
    return(dev);
  }
}
     
/***********************************************************************
  Options handling
 ***********************************************************************/

Config config={0,EMUMODE_MT32,FALSE,0,"",0,FALSE,""}; 	/* Current config */

void usage()
{
  printf("midid [parameters] [input file]\n\n"
	 "-h,  --help         : help\n"
	 "-l,  --list-devices : list devices\n"
	 "-d#, --device=#     : use device number #\n"
	 "-m,  --emulate-mt   : select MT32 mode\n"
	 "-g,  --emulate-gm   : select GM mode\n"
	 "-r,  --resident     : be resident; don't stop on end of file\n"
	 "                      (only when input file != stdin)\n"
	 "-v#, --verbosity=#  : Set verbosity to bitmask # [NYI]\n"
	 "input file defaults to stdin\n\n"
	 "for UltraDriver device:\n"
	 "-c#, --card=#       : Use card number # [NYI]\n\n"
	 "for OSS/Lite device:\n"
	 "-2, --opl2          : Use OPL-2 voices (2 operators per voice) [NYI]\n"
	 "-4, --opl3          : Use OPL-3 voices (4 operators per voice) [NYI]\n"
	 "\n"
	 "[NYI] indicates this option is not implemented yet.\n"
	 "\n");
}

void options_read(int argc, char **argv)
/* Read and evaluate command line options */
{
  int c;
  /* Set options to default values */
  debugall = 0;
  debug = 1;
  ignored = 1;
  comments = 1;
  warning = 1;
  statistics = 1;
  /* Read all command line options */
  while(1) {
    static struct option long_options[] = {
      {"help",0,0,'h'},
      {"list-devices",0,0,'l'},
      {"device",1,0,'d'},
      {"emulate-mt",0,0,'m'},
      {"emulate-gm",0,0,'g'},
      {"resident",0,0,'r'},
      {"verbosity",1,0,'v'},
      /* for UltraDriver */
      {"card",1,0,'c'},
      /* for OSS */
      {"opl2",0,0,'2'},
      {"opl3",0,0,'4'},
      {0,0,0,0}
    };
    int option_index=0;
    c=getopt_long(argc,argv,"hld:mgrv:c:23",long_options,&option_index);
    if (c==-1) break;
    switch(c) {
    case '?':
    case 'h':
      usage();
      exit(1);
    case 'l':
      device_printall();
      exit(1);
    case 'd':
      config.device=atoi(optarg);
      break;
    case 'm':
      config.mode=EMUMODE_MT32;
      break;
    case 'g':
      config.mode=EMUMODE_GM;
      break;
    case 'r':
      config.resident=TRUE;
      break;
    case 'v':
      config.verbosity=atoi(optarg);
      break;
    case 'c':
      config.card=atoi(optarg);
      break;
    case '2':
      config.opl3=FALSE;
      break;
    case '4':
      config.opl3=TRUE;
      break;
    }
  }
  if (optind<argc)
    config.inputfile=argv[optind];
  if (config.resident && (!strlen(config.inputfile))) {
    fprintf(stderr,"Error: Can't use resident option without inputfile!\n"
	           "       Resident option disabled.\n");
    config.resident=FALSE;
  }
}

/***********************************************************************
  Main
 ***********************************************************************/

int main(int argc, char **argv)
{
	int ch;				/* Current status (==command) */
	int last_status=MAGIC_EOF;     	/* Last performed status */

	/* Register all devices */
	device_register_all();

	/* Evaluate command line options */
	options_read(argc,argv);
	
	/* Banner */
	if (comments)
		printf("Midi Daemon / interpreter v1.04\n"
	               "(c)1997 R.Nijlunsing <rutger@null.net>\n"
		       "Use '%s --help' for options\n\n",
		       argv[0]);

	/* Select first working device */
	dev = device_select(config.device);
	if (!dev) error("No supported music device detected!");
	if (comments)
		printf("Using %s driver v%i.%02i\n", dev->name,
                       dev->version/100, dev->version % 100);
	if (!dev->init()) error("Error initializing driver.");
	if (statistics)	stats_reset();

	emulation_set(config.mode);

	/* Open input */
	if (strlen(config.inputfile)) {
	  /* open given inputfile */
	  fd=open(config.inputfile,O_RDONLY);
	  if (fd==-1) {
	    perror(config.inputfile);
	    exit(1);
	  }
	} else {
	  /* Use stdin file descriptor */
	  fd=0;
	}
	
	/* Read first byte */
	getbyte_next();
	while (1) {
		int chn;       	/* Channel (0..15) */
	        ch = getbyte_status();
		/* Illegal for a status byte? */
		if (ch==MAGIC_INV) {
			/* This is not a status byte, but a data byte.
			   Use previous status byte. */
			ch=last_status;
		} else {
			last_status=ch;
			getbyte_next();
		}
		/* End of file? */
		if (ch==MAGIC_EOF) {
		  if (!config.resident)
		    /* Not resident; quit */
		    break;
		  else {
		    /* Resident; reset soundcard */
		    dev->done();
		    dev->init();
		    continue;
		  }
		}
		if (statistics)	stats_addcommand(ch);
		chn=(ch & 0xf);
		switch (ch & 0xf0) {
		case 0x80:
			do_noteoff(chn);
			break;
		case 0x90:
			do_noteon(chn);
			break;
		case 0xa0:
			do_notepressure(chn);
			break;
		case 0xb0:
			do_controlchange(chn);
			break;
		case 0xc0:
			do_program(chn);
			break;
		case 0xd0:
			do_channelpressure(chn);
			break;
		case 0xe0:
			do_bender(chn);
			break;
		case 0xf0:
		  	/* 0xF0 - 0xF7 = system exclusive messages */
			/* 0XF8 - 0xFF = system real time messages */
			switch (ch) {
			case 0xf0:
				/* Sysex start */
				if (debug)
					fprintf(stderr, "sysex\n");
				do_sysex();
				break;
			case 0xfe:
				/* Active sensing */
				if (debug)
					fprintf(stderr, "active_sensing\n");
				break;
			default:
				if (warning) {
					fprintf(stderr, "Warning: Unkown realtime event (0x%02x)\n", ch);
				}
				break;
			}
		}
		/* Make it happen _now_ */
		dev->flush();
	}

	if (statistics)	stats_print();
	dev->done();
	return(0);
}
