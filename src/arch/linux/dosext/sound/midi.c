/*

   Reads STDIN and interprets it as raw MIDI data according to
   the General Midi standard.

   Compile with:
   gcc -O2 -Wall midi.c -lgus -omidid

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
   
   TODO (in appearance of importance):
     * MT32 emulation support (== another set of instruments)
     * interface to OSS/Lite driver
     * add 'all notes off' functionality independant of driver
     * look if ultradriver version works (...instead of only compiling)
     * make it compilable without the ultradriver on a system :)
     * each driver in its own file
     * parameter support (device select, etc.)
     * drum channel support
     * driver for realtime recording of MIDI files
     * instrument caching / preloading
     * GS Standard. Anyone got documentation on this one?
     * preloading of instruments

 */

/*
  Ideas for parameters (NYI):

  midid [-h] [-ld] [-d#] [-mt] [-gm] [<input file>]

  -h  : help
  -ld : list drivers
  -d# : use driver number #
  -mt : select MT32 mode [default]
  -gm : select GM mode
  -r  : be resident; don't stop on end of file (only when input file != stdin)
  input file defaults to stdin
  
  for UltraDriver:

  -c# : Use card number #

 */

#include "midi.h"

Device *devices = NULL;         /* list of all drivers */

const int fd = 0;		/* stdin file descriptor */
Device *dev;			/* Current playback driver */

/* default instrument mapping */
int imap_default[128] =
{
   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,
  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,
  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,
  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,
  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101, 102, 103,
 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127
};
/* MT32 -> GM instrument mapping (from playmidi-2.3/emumidi.h) */
int imap_mt2gm[128] =
{
   0,   1,   2,   4,   4,   5,   5,   3,  16,  16,  16,  16,  19,
  19,  19,  21,   6,   6,   6,   7,   7,   7,   8,   8,  62,  57,
  63,  58,  38,  38,  39,  39,  88,  33,  52,  35,  97, 100,  38,
  39,  14, 102,  68, 103,  44,  92,  46,  80,  48,  49,  51,  45,
  40,  40,  42,  42,  43,  46,  46,  24,  25,  28,  27, 104,  32,
  32,  34,  33,  36,  37,  39,  35,  79,  73,  76,  72,  74,  75,
  64,  65,  66,  67,  71,  71,  69,  70,  60,  22,  56,  59,  57,
  63,  60,  60,  58,  61,  61,  11,  11,  99, 100,   9,  14,  13,
  12, 107, 106,  77,  78,  78,  76, 111,  47, 117, 127, 115, 118,
 116, 118, 126, 121, 121,  55, 124, 120, 125, 126, 127
};

/* Current instrument mapping */
int *imap=imap_mt2gm;

/* Options */
const int debugall = 1; 	/* 1 for all read bytes */
const int debug = 1;		/* 1 for interpretation of each byte */
const int ignored = 1;  	/* 1 for ignored but recognised messages */
const int comments = 1; 	/* 1 for status report */
const int warning = 1;		/* 1 for warnings during interpretation*/

const int statistics = 1;	/* 1 for statistics */

/***********************************************************************
  Output
 ***********************************************************************/

void error(char *err)
{
	fprintf(stderr, "Fatal error: %s\n", err);
	exit(1);
}

/***********************************************************************
  Input from stdin with 1 byte buffer
 ***********************************************************************/

/* Special return values; must be smaller then 0 */
#define MAGIC_EOF (-1)    /* End Of File */
#define MAGIC_INV (-2)    /* invalid byte */

int getbyte_buffer=MAGIC_EOF;

void getbyte_next(void)
/* Read next symbol into buffer */
{
	byte ch;
	bool is_eof;
	is_eof=(!read(fd, &ch, 1));
	getbyte_buffer=ch;
	if (is_eof) getbyte_buffer=MAGIC_EOF;
	if (debugall) fprintf(stderr,"getbyte_next=0x%02x (%i)\n",
			      getbyte_buffer,getbyte_buffer);
}

int getbyte(void)
/* Return current contents of buffer */
{
	return getbyte_buffer;
}

int getbyte_status(void)
/* Tries to read a status byte (==bit 7 set) */
{
	bool accept=(getbyte_buffer & 0x80);
	int ch=getbyte_buffer;
	if (!accept) {
		ch=MAGIC_INV;
		if (warning) fprintf(stderr,"Warning: got an illegal status byte=%i\n",getbyte_buffer);
	}
	return(ch);
}

int getbyte_data(void)
/* Tries to read a data byte (==bit 7 reset) */
{
	bool accept=(!(getbyte_buffer & 0x80));
	int ch=getbyte_buffer;
	if (!accept) {
		ch=MAGIC_INV;
		if (warning) fprintf(stderr,"Warning: got an illegal data byte=%i\n",getbyte_buffer);
	}
		return (ch);
}

/***********************************************************************
  GUS device
 ***********************************************************************/

int gusdevice;

bool gus_detect(void)
{
	return (gus_cards() > 0);
}

bool gus_init(void)
{
	gus_midi_device_t *device;
  	if ( gus_midi_open_intelligent( GUS_MIDI_OUT, NULL, 2048, 0 ) < 0 )
		return (FALSE);

	device = gus_midi_output_devices();
	assert(device);
	gusdevice=device->device;

	gus_midi_reset();
     	if (device -> cap & GUS_MIDI_CAP_MEMORY)
        	gus_midi_memory_reset(gusdevice);

	/* We set the timer to a random value since we don't use it.
	   But when we don't start the timer, the program blocks. Why? */
	gus_midi_timer_base(100);
	gus_midi_timer_tempo(100);
	gus_midi_timer_start();
	return (TRUE);
}

void gus_done(void)
{
	gus_midi_close();
}

void gus_flush(void)
{
	gus_midi_flush();
}

void gus_noteon(int chn, int note, int vel)
{
	gus_midi_note_on(gusdevice, chn, note, vel);
}

void gus_noteoff(int chn, int note, int vel)
{
	gus_midi_note_off(gusdevice, chn, note, vel);
}

void gus_control(int chn, int control, int value)
{
	gus_midi_control(gusdevice, chn, control, value);
}

void gus_notepressure(int chn, int note, int vel)
{
	gus_midi_note_pressure(gusdevice, chn, note, vel);
}

void gus_channelpressure(int chn, int vel)
{
	gus_midi_channel_pressure(gusdevice, chn, vel);
}

void gus_bender(int chn, int pitch)
{
	gus_midi_bender(gusdevice, chn, pitch);
}

void gus_program(int chn, int pgm)
{
	/* FIXME: This isn't nice, but it works :) */
	gus_midi_timer_stop();
	gus_midi_preload_program(gusdevice,&pgm,1);
	gus_midi_timer_continue();
	gus_midi_program_change(gusdevice, chn, pgm);
}

bool gus_setmode(Emumode new_mode)
{
	int emulation;
        switch(new_mode) {
        case EMUMODE_MT32:
		emulation=GUS_MIDI_EMUL_MT32;
		break;
	case EMUMODE_GM:
		emulation=GUS_MIDI_EMUL_GM;
		break;
	default:
		if (warning)
			fprintf(stderr,"Warning: unknown emulation mode (%i)\n",new_mode);
		return FALSE;
        }
        return(gus_midi_emulation_set(gusdevice,emulation)!=-1);
}

/***********************************************************************
  null (flush) device -- mainly for testing
 ***********************************************************************/

bool null_detect(void)
{
	return(TRUE);
}

bool null_init(void)
{
	return(TRUE);
}

void null_doall(void)
{
}

void null_doall2(int a,int b)
{
}

void null_doall3(int a,int b,int c)
{
}

bool null_setmode(Emumode new_mode)
{
  /* Our NULL driver has all emulations in the world :) */
  return TRUE;
}

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

void register_gus(Device * dev)
{
	dev->name = "UltraDriver";
	dev->version = 10;
	dev->detect = gus_detect;
	dev->init = gus_init;
	dev->done = gus_done;
	dev->flush = gus_flush;
	dev->noteon = gus_noteon;
	dev->noteoff = gus_noteoff;
	dev->control = gus_control;
	dev->notepressure = gus_notepressure;
	dev->channelpressure = gus_channelpressure;
	dev->bender = gus_bender;
	dev->program = gus_program;
	dev->setmode = gus_setmode;
}

void register_null(Device * dev)
{
	dev->name = "null";
	dev->version = 100;
	dev->detect = null_detect;
	dev->init = null_init;
	dev->done = null_doall;
	dev->flush = null_doall;
	dev->noteon = null_doall3;
	dev->noteoff = null_doall3;
	dev->control = null_doall3;
	dev->notepressure = null_doall3;
	dev->channelpressure = null_doall2;
	dev->bender = null_doall2;
	dev->program = null_doall2;
}

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

void device_register_all(void)
{
	device_add(register_null);
	device_add(register_gus);
}

Device *device_findfirst(void)
/* Return the first working device of the list;
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

/***********************************************************************
  Statistics
 ***********************************************************************/

typedef struct Stats {
	int command[256];	/* # of calls to this command */
	int program[256];	/* # of references to this program */
} Stats;
Stats stats;			/* Statistics */

void stats_reset(void)
{
	int i;
	for (i = 0; i < 256; i++)
		stats.command[i] = 0;
	for (i = 0; i < 256; i++)
		stats.program[i] = 0;
}

void stats_addcommand(int command)
{
	if ((command >= 0) && (command < 256))
		stats.command[command]++;
}

void stats_addprogram(int program)
{
	if ((program >= 0) && (program < 256))
		stats.program[program]++;
}

void stats_print(void)
{
	int i, chn;
	printf("Command/channel times:\n");
	printf("      ");
	for (chn = 0; chn < 16; chn++)
		printf("%3i:", chn);
	printf("\n");
	for (i = 8; i < 16; i++) {
		printf("0x%2x: ", i << 4);
		for (chn = 0; chn < 16; chn++) {
			if (stats.command[(i << 4) + chn])
				printf("%4i", stats.command[(i << 4) + chn]);
			else
				printf("%4c", '.');
		}
		printf("\n");
	}
	printf("\nProgram:\n");
	for (i = 0; i < 256; i++) {
		if (stats.program[i])
			printf("  program %4i : %6i times\n", i, stats.program[i]);
	}
	printf("\n");
}

/***********************************************************************
  event handling: read parameters and put event in queue
 ***********************************************************************/

#define EOX 0xf7

void do_noteoff(int chn)
{
	int note, vel;
	note = getbyte_data();
	if (note<0) return; /* Return at error */
	getbyte_next();
	vel = getbyte_data();
	if (vel<0) return;
	getbyte_next();
				if (debug)
					fprintf(stderr, "note_off(chn=%i,note=%i,vel=%i)\n", chn, note, vel);
				dev->noteoff(chn, note, vel);
}

void do_noteon(int chn)
{
	int note, vel;
	note = getbyte_data();
	if (note<0) return;
	getbyte_next();
	vel = getbyte_data();
	if (vel<0) return;
	getbyte_next();
				if (debug)
					fprintf(stderr, "note_on(chn=%i,note=%i,vel=%i)\n", chn, note, vel);
	if (vel) {
				dev->noteon(chn, note, vel);
	} else {
		/* No velocity */
		dev->noteoff(chn, note, vel);
	}
}

void do_notepressure(int chn)
{
				/* Polyphonic key pressure, note pressure */
	int note, vel;
	note = getbyte_data();
	if (note<0) return;
	getbyte_next();
	vel = getbyte_data();
	if (vel<0) return;
	getbyte_next();
				if (debug)
					fprintf(stderr, "note_pressure(chn=%i,note=%i,vel=%i)\n", chn, note, vel);
				dev->notepressure(chn, note, vel);
}

void do_program(int chn)
{
				/* Program change */
	int pgm;
	pgm = getbyte_data();
	if (pgm<0) return;
	getbyte_next();
				if (statistics)
					stats_addprogram(pgm);
				if (debug)
					fprintf(stderr, "change_program(chn=%i,pgm=%i)\n", chn, pgm);
	/* Use mapping for MT32 <-> GM emulation (imap[]) */
	dev->program(chn, imap[pgm]);
}

void do_channelpressure(int chn)
{
				/* Channel pressure */
	int vel;
	vel = getbyte_data();
	if (vel<0) return;
	getbyte_next();
				if (debug)
		fprintf(stderr, "control_change(chn=%i,vel=%i)\n", chn, vel);
	dev->channelpressure(chn, vel);
}

void do_bender(int chn)
{
				/* Pitch wheel, bender */
	int pitch;
	pitch=getbyte_data();
	if (pitch<0) return;
	getbyte_next();
	if (getbyte_data()<0) return;
	pitch += getbyte_data() << 7;
	getbyte_next();
				if (debug)
		fprintf(stderr, "bender(chn=%i,pitch=%i)\n", chn, pitch);
				dev->bender(chn, pitch);
}

void do_sysex(void)
{
	int ch; /* Last read char */
	ch = getbyte_data();
	if (ch<0) return;
	getbyte_next();
	if (debug)
		fprintf(stderr,"Sysex for vendor ID#%i\n",ch);
	while(1) {
		ch = getbyte_data();
		if (ch<0) break;
		getbyte_next();
	}
	if (getbyte()==EOX) getbyte_next();
	else
		if (debug)
			fprintf(stderr,"Sysex event wasn't terminated by EOX\n");
}

void do_allnotesoff(void)
{
	/* NYI */
	if (warning)
		fprintf(stderr,"do_allnotesoff NYI\n");
}

void do_modemessage(int chn,int control)
{
	int value;
	if (debug)
		fprintf(stderr,"mode message (chn=%i,control=%i)\n",
                        chn,control);
	switch(control) {
	case 122:
		value=getbyte_data();
		if (value<0) return;
		getbyte_next();
		if (ignored)
			fprintf(stderr,
                                "Ignored: Local control (chn=%i,val=%i\n",
                                chn,value);
		break;
	case 123:
		/* all notes off */
		do_allnotesoff();
		break;
	case 124:
		if (ignored) fprintf(stderr,"Ignored: omni mode off\n");
		do_allnotesoff();
		break;
	case 125:
		if (ignored) fprintf(stderr,"Ignored: omni mode on\n");
		do_allnotesoff();
		break;
	case 126:
		value = getbyte_data();
		if (value<0) return;
		getbyte_next();
		if (ignored) fprintf(stderr,"Ignored: monophonic mode, %i channels\n",value);
		do_allnotesoff();
		break;
	case 127:
		if (ignored) fprintf(stderr,"Ignored: polyphonic mode\n");
		do_allnotesoff();
		break;
	default:
		assert(FALSE);
	}
}

void do_controlchange(int chn)
{
	int control;
	int value;
	control = getbyte_data();
	if (control<0) return;
	getbyte_next();
	if ((control>=122)&&(control<=127)) do_modemessage(chn,control);
	/* Unhandled controller; let the driver decide */
	value = getbyte();
	if (value<0) return;
	getbyte_next();
	if (debug)
		fprintf(stderr, "control_change(chn=%i,control=%i,val=%i)\n",
                        chn, control, value);
	dev->control(chn, control, value);
}

/***********************************************************************
  Main
 ***********************************************************************/

int main(int argc, char **argv)
{
	int ch;				/* Current status (==command) */
	int last_status=MAGIC_EOF;     	/* Last performed status */
	if (comments)
		printf("Midi interpreter v1.01\n"
	               "(c)1997 R.Nijlunsing <rutger@null.net>\n");
	/* Select first working device */
	device_register_all();
	dev = device_findfirst();
	if (!dev) error("No supported music device detected!");
	if (comments)
		printf("Using %s driver v%i.%02i\n", dev->name,
                       dev->version/100, dev->version % 100);
	assert(dev->detect());
	if (!dev->init()) error("Error initializing driver.");
	if (statistics)	stats_reset();

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
		if (ch==MAGIC_EOF) break;
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
	sleep(2);
	dev->done();
	return(0);
}
