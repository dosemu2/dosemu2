#include<libgus.h>
#include<stdio.h>
#include<assert.h>
#include<unistd.h>

/*

   Reads STDIN and interprets it as raw MIDI data

   Compile with:
   gcc -O2 -Wall midi.c -lgus -omidi

   (c)1997 R.Nijlunsing

 */

#define TRUE 1
#define FALSE 0
typedef int bool;

typedef unsigned char byte;

/* Linked list of output devices */
typedef struct Device {
	struct Device *next;	/* Next device */
	 bool(*detect) (void);	/* TRUE if detected */
	 bool(*init) (void);	/* TRUE if init was succesful */
	void (*done) (void);
	void (*flush) (void);
	void (*noteon) (int chn, int note, int vel);	/* note_on routine */
	void (*noteoff) (int chn, int note, int vel);	/* etc. */
	void (*control) (int chn, int control, int value);
	void (*notepressure) (int chn, int control, int value);
	void (*channelpressure) (int chn, int note);
	void (*bender) (int chn, int note, int pitch);
	void (*program) (int chn, int pgm);
} Device;
Device *devices = NULL;

const int fd = 0;		/* stdin file descriptor */

/***********************************************************************
  I/O
 ***********************************************************************/

void error(char *err)
{
	fprintf(stderr, "Fatal error: %s\n", err);
	exit(1);
}

#define MAGIC_EOF (-1)

int getbyte(void)
{
	byte ch;
	if (!read(fd, &ch, 1))
		return (MAGIC_EOF);
	else
		return (ch);
}

/***********************************************************************
  GUS device
 ***********************************************************************/

/*

   /dev/gusmidi

   <size high byte of data block><size low><device><cmd | channel><data>

   #define GUS_MIDID_UART               0x00
   #define GUS_MIDID_SYNTH              0x01
   #define GUS_MIDID_LAST               GUS_MIDID_SYNTH
   #define GUS_MIDID_COMMON             0xff

 */
int gusdevice = GUS_MIDID_COMMON;

bool gus_detect(void)
{
	return (TRUE);
}

bool gus_init(void)
{
	int init = gus_midi_open_intelligent(NULL, 2048);
	if (init)
		return (FALSE);
	gus_midi_reset();
	gus_midi_reset_dram();
	/* We set the timer to a random value since we don't use it.
	   But when we don't start the timer, the program blocks. Why? */
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

void gus_channelpressure(int chn, int note)
{
	/* ?? What is the velocity? */
	int vel = 63;
	gus_midi_channel_pressure(gusdevice, chn, note, vel);
}

void gus_bender(int chn, int pitch)
{
	gus_midi_bender(gusdevice, chn, pitch);
}

void gus_program(int chn, int pgm)
{
	gus_midi_pgm_change(gusdevice, chn, pgm);
}

/***********************************************************************
  Devices
 ***********************************************************************/

void register_gus(Device * dev)
{
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
}

void device_add(void (*register_func) (Device * dev))
{
	Device *newdev;
	newdev = malloc(sizeof(Device));
	assert(newdev);
	register_func(newdev);
	newdev->next = devices;
	devices = newdev;
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
	if ((command >= 0) && (command <= 255))
		stats.command[command]++;
}

void stats_addprogram(int program)
{
	if ((program >= 0) && (program <= 255))
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
	for (i = 0; i < 16; i++) {
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
  Main
 ***********************************************************************/

void main(void)
{
	const int debug = 0;	/* 1 for all messages */
	const int warning = 1;	/* 1 for warnings */
	const int statistics = 1;	/* 1 for statistics */
	int ch;			/* Last read character */
	Device *dev = NULL;	/* Current playback device */
	device_add(register_gus);
	dev = devices;		/* Select first device */
	assert(dev);
	assert(dev->detect());
	assert(dev->init());
	if (statistics)
		stats_reset();
	while ((ch = getbyte()) != MAGIC_EOF) {
		if (statistics)
			stats_addcommand(ch);
		if ((ch < 0x80) || (ch > 0xff)) {
			if (warning) {
				fprintf(stderr, "Warning: got an unexpected character (0x%02x); discarded\n", ch);
			}
		}
		else {
			int chn = (ch & 0xf);
			int note;	/* Note; 60=C3 */
			int vel;	/* Velocity; 0..127 */
			int value;	/* Value */
			int control;	/* Controller */
			int pgm;	/* Program */
			int pitch;	/* Pitch */
			switch (ch & 0xf0) {
			case 0x80:
				/* Note off */
				note = getbyte();
				vel = getbyte();
				if (debug)
					fprintf(stderr, "note_off(chn=%i,note=%i,vel=%i)\n", chn, note, vel);
				dev->noteoff(chn, note, vel);
				break;
			case 0x90:
				/* Note on */
				note = getbyte();
				vel = getbyte();
				if (debug)
					fprintf(stderr, "note_on(chn=%i,note=%i,vel=%i)\n", chn, note, vel);
				dev->noteon(chn, note, vel);
				break;
			case 0xa0:
				/* Polyphonic key pressure, note pressure */
				note = getbyte();
				vel = getbyte();
				if (debug)
					fprintf(stderr, "note_pressure(chn=%i,note=%i,vel=%i)\n", chn, note, vel);
				dev->notepressure(chn, note, vel);
				break;
			case 0xb0:
				/* Control change */
				control = getbyte();
				value = getbyte();
				if (debug)
					fprintf(stderr, "control_change(chn=%i,control=%i,val=%i)\n", chn, control, value);
				dev->control(chn, control, value);
				break;
			case 0xc0:
				/* Program change */
				pgm = getbyte();
				if (statistics)
					stats_addprogram(pgm);
				if (debug)
					fprintf(stderr, "change_program(chn=%i,pgm=%i)\n", chn, pgm);
				dev->program(chn, pgm);
				break;
			case 0xd0:
				/* Channel pressure */
				note = getbyte();
				if (debug)
					fprintf(stderr, "control_change(chn=%i,note=%i)\n", chn, note);
				dev->channelpressure(chn, note);
				break;
			case 0xe0:
				/* Pitch wheel, bender */
				pitch = getbyte() + (getbyte() << 7);
				if (debug)
					fprintf(stderr, "bender(pitch=%i)\n", chn, pitch);
				dev->bender(chn, pitch);
				break;
			case 0xf0:
				/* System exclusive */
				switch (ch) {
				case 0xf0:
					/* Sysex start */
					if (debug)
						fprintf(stderr, "sysex\n");
					while (getbyte() != 0xf7);
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
			dev->flush();
		}
	}
	stats_print();
	/* Allow some time for notes to end */
	sleep(2);
	dev->done();
}
