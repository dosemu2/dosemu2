/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _MIDID_H
#define _MIDID_H

typedef unsigned char byte;
typedef enum {FALSE, TRUE} bool;
typedef enum {EMUMODE_MT32, EMUMODE_GM} Emumode;

/* Linked list of output devices */
typedef struct Device {
	struct Device *next;	/* Next device */
	char *name;
	int version;		/* v1.00 = 100 */
	int detected;
	int active;
	bool (*detect) (void);	/* returns TRUE if detected */
	bool (*init) (void);	/* returns TRUE if init was succesful */
	void (*done) (void);
	void (*flush) (void);   /* Flush all commands sent */
        bool (*setmode) (Emumode new_mode);
         /* Set (emulation) mode to new_mode; returns TRUE iff possible */
        /* MIDI commands */
	void (*noteon) (int chn, int note, int vel);             /* 0x90 */
	void (*noteoff) (int chn, int note, int vel);            /* 0x80 */
	void (*notepressure) (int chn, int control, int value);  /* 0xA0 */
	void (*channelpressure) (int chn, int vel);              /* 0xD0 */
	void (*control) (int chn, int control, int value);       /* 0xB0 */
	void (*program) (int chn, int pgm);                      /* 0xC0 */
	void (*bender) (int chn, int pitch);                     /* 0xE0 */
	void (*sysex) (char buf[], int len);                     /* 0xF0 */
} Device;

/* Configuration */
typedef struct Config {
  Emumode mode;		/* Default emulation mode */
  bool resident;	/* TRUE if resident */
  int timeout;          /* timeout to turn off output after */
  int verbosity;        /* bitmask with outputoptions */
  char *file;           /* Input filename; empty=stdin */
  int card;             /* GUS card */
  bool opl3;            /* TRUE if OPL3; FALSE if OPL2 */
  char *inputfile;      /* "" == stdin */
  char *midifile;       /* "" == midid.mid */
  int tempo;		/* "" == 120 beats per minute */
  int ticks_per_quarter_note;	/* "" == 144 */
  char *timid_host;	/* timidity server host name */
  int timid_port;	/* timidity server control port */
} Config;

extern Config config;

/* Verbosity level */
extern int debugall; 	/* 1 for all read bytes */
extern int debug;	/* 1 for interpretation of each byte */
extern int ignored;  	/* 1 for ignored but recognised messages */
extern int comments; 	/* 1 for status report */
extern int warning;	/* 1 for warnings during interpretation*/
extern int statistics;	/* 1 for statistics */

void error(char *err);

#endif
