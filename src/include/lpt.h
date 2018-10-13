#ifndef LPT_H
#define LPT_H 1

#include "utilities.h"

struct p_fops {
  int (*open) (int prtnum);
  int (*write) (int prtnum, Bit8u data);
  int (*close) (int prtnum);
};

struct printer {
  char *dev;
  char *prtcmd;
  int delay;
  ioport_t base_port;		/* Base port address handled by device */

  /* end of user-set options */
  struct popen2 file;
  int dev_fd;
  struct p_fops fops;
  int initialized;
  int opened;
  int remaining;

  Bit8u data, control, status;
};

/* public functions */
int printer_open(int prnum);
int printer_close(int prnum);
int printer_flush(int prnum);
int printer_write(int prnum, Bit8u outchar);
ioport_t get_lpt_base(int lptnum);
void printer_config(int prnum, struct printer *pptr);
void printer_print_config(int prnum, void (*print)(const char *, ...));
int lpt_get_max(void);
int lpt_is_configured(int num);

/* status bits */
#define LPT_STAT_TIMEOUT	0x1
#define LPT_STAT_NOT_IRQ	0x4
#define LPT_STAT_NOIOERR	0x8
#define LPT_STAT_ONLINE		0x10
#define LPT_STAT_NOPAPER	0x20
#define LPT_STAT_NOT_ACK	0x40
#define LPT_STAT_NOT_BUSY	0x80

/* control bits */
#define LPT_CTRL_INPUT		0x20
#define LPT_CTRL_IRQ_EN		0x10
#define LPT_CTRL_SELECT		0x8
#define LPT_CTRL_NOT_INIT	0x4
#define LPT_CTRL_AUTOLF		0x2
#define LPT_CTRL_STROBE		0x1

#define NUM_LPTS 3

#endif /* LPT_H */
