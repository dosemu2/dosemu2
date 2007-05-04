/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef LPT_H
#define LPT_H 1

struct p_fops {
  int (*open) (int prtnum);
  int (*write) (int prtnum, int data);
  int (*close) (int prtnum);
  int (*realwrite) (int prnum, int data);
};

struct printer {
  char *dev;
  char *prtcmd;
  int delay;
  ioport_t base_port;		/* Base port address handled by device */

  /* end of user-set options */
  FILE *file;
  int remaining;

  struct p_fops fops;

  Bit8u data, status, control;
};

/* public functions */
int printer_open(int prnum);
int printer_close(int prnum);
int printer_flush(int prnum);
int printer_write(int prnum, int outchar);
void printer_config(int prnum, struct printer *pptr);
void printer_print_config(int prnum, void (*print)(char *, ...));

/* status bits */
#define LPT_TIMEOUT	0x1
#define LPT_IOERR	0x8
#define LPT_ONLINE	0x10
#define LPT_NOPAPER	0x20
#define LPT_ACK		0x40
#define LPT_NOTBUSY	0x80

#define NUM_PRINTERS 3
extern struct printer lpt[NUM_PRINTERS];

#endif /* LPT_H */
