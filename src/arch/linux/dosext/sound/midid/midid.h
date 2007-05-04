/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef _MIDID_H
#define _MIDID_H

typedef unsigned char byte;
typedef enum {FALSE, TRUE} bool;
typedef enum {EMUMODE_MT32, EMUMODE_GM} Emumode;

/* Configuration */
typedef struct Config {
  Emumode mode;		/* Default emulation mode */
  bool resident;	/* TRUE if resident */
  int timeout;          /* timeout to turn off output after */
  int verbosity;        /* bitmask with outputoptions */
  int card;             /* GUS card */
  bool opl3;            /* TRUE if OPL3; FALSE if OPL2 */
  char *inputfile;      /* "" == stdin */
  char *midifile;       /* "" == midid.mid */
  int tempo;		/* "" == 120 beats per minute */
  int ticks_per_quarter_note;	/* "" == 144 */
  char *timid_host;	/* timidity server host name */
  int timid_port;	/* timidity server control port */
  char *timid_bin;	/* timidity binary name */
  char *timid_args;	/* timidity command-line args */
  int timid_mono;
  int timid_8bit;
  int timid_uns;
  int timid_freq;
  int timid_capture;
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
