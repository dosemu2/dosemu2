#ifndef LPT_H
#define LPT_H 1


struct p_fops {
  int (*open) (int prtnum);  
  int (*write) (int prtnum, int data);  
  int (*flush) (int prnum);
  int (*close) (int prtnum);  
  int (*realwrite) (int prnum, int data);
};

struct printer {
  char *dev;
  char *prtcmd;
  char *prtopt;
  int delay;

  /* end of user-set options */
  FILE *file;
  int remaining;

  struct p_fops fops;

};

/* status bits */
#define LPT_TIMEOUT	0x1
#define LPT_IOERR	0x8
#define LPT_ONLINE	0x10
#define LPT_NOPAPER	0x20
#define LPT_ACK		0x40
#define LPT_NOTBUSY	0x80

#define NUM_PRINTERS 3

#endif LPT_H
