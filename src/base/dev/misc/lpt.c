/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* for the Linux dos emulator versions 0.49 and newer
 *
 */
#define LPT_C 1

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "emu.h"
#include "dosio.h"
#include "lpt.h"
#include "utilities.h"
#include "dos2linux.h"

static int stub_printer_write(int, int);

struct p_fops def_pfops =
{
  printer_open,
  stub_printer_write,
  printer_flush,
  printer_close,
  printer_write
};

static struct printer lpt[NUM_PRINTERS] =
{
  {NULL, "lpr", "", 5, 0x378},
  {NULL, "lpr", "", 5, 0x278},
  {NULL, NULL, NULL, 10, 0x3bc}
};

int int17(void)
{
  int num;

  if (LWORD(edx) + 1 > config.num_lpt) {
    p_printf("LPT: print to non-defined printer LPT%d\n",
	     LWORD(edx) + 1);
    CARRY;
    return 1;
  }

  switch (HI(ax)) {
  case 0:			/* write char */
    /* p_printf("print character on lpt%d : %c (%d)\n",
			       LO(dx), LO(ax), LO(ax)); */
    HI(ax) = (lpt[LO(dx)].fops.write) (LO(dx), LO(ax));
    break;

  case 1:			/* init */
    HI(ax) = LPT_NOTBUSY | LPT_ACK | LPT_ONLINE;
    num = LWORD(edx);
    p_printf("LPT: init printer %d\n", num);
    break;

  case 2:			/* get status */
    HI(ax) = LPT_NOTBUSY | LPT_ONLINE;
    /* dbug_printf("printer 0x%x status: 0x%x\n", LO(dx), HI(ax)); */
    break;

  default:
    error("printer int17 bad call ax=0x%x\n", LWORD(eax));
    show_regs(__FILE__, __LINE__);
    /* fatalerr = 8; */
    break;
  }
  return 1;
}

int
printer_open(int prnum)
{
  int um;

  um = umask(026);
  if (lpt[prnum].file == NULL) {
    if (!lpt[prnum].dev) {
      lpt[prnum].file = tmpfile();
      p_printf("LPT: opened tmpfile\n");
    }
    else {
      lpt[prnum].file = fopen(lpt[prnum].dev, "a");
    }
  }
  umask(um);

  p_printf("LPT: opened printer %d to %s, file %p\n", prnum,
	   lpt[prnum].dev ? lpt[prnum].dev : "<<NODEV>>",
           (void *) lpt[prnum].file);
  return 0;
}

int
printer_close(int prnum)
{
  p_printf("LPT: closing printer %d, %s\n", prnum,
	   lpt[prnum].dev ? lpt[prnum].dev : "<<NODEV>>");
  if (lpt[prnum].file != NULL)
    fclose(lpt[prnum].file);
  lpt[prnum].file = NULL;

  /* delete any temporary files */
  if (lpt[prnum].prtcmd && lpt[prnum].dev) {
    unlink(lpt[prnum].dev);
    free(lpt[prnum].dev);
    lpt[prnum].dev = NULL;
  }

  lpt[prnum].remaining = -1;
  return 0;
}

int
printer_flush(int prnum)
{
  p_printf("LPT: flushing printer %d\n", prnum);

  fflush(lpt[prnum].file);

  if (lpt[prnum].prtcmd) {
    size_t cmdbuflen, bufsize;
    FILE *pipe;
    char *buf, *cmdbuf;
    
    cmdbuflen = strlen(lpt[prnum].prtcmd) + 1 +
                strlen(lpt[prnum].prtopt) + 1;

    cmdbuf = malloc(cmdbuflen);
    if (!cmdbuf) {
      fprintf(stderr, "out of memory, giving up\n");
      longjmp(NotJEnv, 0x4d);
    }

    strcpy(cmdbuf, lpt[prnum].prtcmd);
    strcat(cmdbuf, " ");
    strcat(cmdbuf, lpt[prnum].prtopt);
    p_printf("LPT: doing printer command ..%s..\n",
	     cmdbuf);

    pipe = popen(cmdbuf, "w");
    if (pipe == NULL)
      error("system(\"%s\") in lpt.c failed, cannot print!\
  Command returned error %s\n", cmdbuf, strerror(errno));

    bufsize = ftell(lpt[prnum].file);
    buf = malloc(bufsize);
    if (!buf) {
      fprintf(stderr, "out of memory, giving up\n");
      longjmp(NotJEnv, 0x4d);
    }
    fseek(lpt[prnum].file, 0, SEEK_SET);
    fread(buf, 1, bufsize, lpt[prnum].file);
    fwrite(buf, 1, bufsize, pipe);
    pclose(pipe);
    free(buf);
    fseek(lpt[prnum].file, 0, SEEK_SET);
  }

  /* mark not accessed */
  lpt[prnum].remaining = -1;
  return 0;
}

int
stub_printer_write(int prnum, int outchar)
{
  (lpt[prnum].fops.open) (prnum);

  /* from now on, use real write */
  lpt[prnum].fops.write = lpt[prnum].fops.realwrite;

  return ((lpt[prnum].fops.write) (prnum, outchar));
}

int
printer_write(int prnum, int outchar)
{
  lpt[prnum].remaining = lpt[prnum].delay;

  fputc(outchar, lpt[prnum].file);
  return (LPT_NOTBUSY | LPT_ACK | LPT_ONLINE);
}

/* DANG_BEGIN_FUNCTION printer_init
 *
 * description:
 *  Initialize printer control structures
 *
 * DANG_END_FUNCTIONS
 */
void
printer_init(void)
{
  int i;

  for (i = 0; i < 3; i++) {
    p_printf("LPT: initializing printer %s\n", lpt[i].dev ? lpt[i].dev : "<<NODEV>>");
    lpt[i].file = NULL;
    lpt[i].remaining = -1;	/* mark not accessed yet */
    lpt[i].fops = def_pfops;
    if (i >= config.num_lpt) lpt[i].base_port = 0;
    /* set the port address for each printer in bios */
    *((u_short *)(0x408) + i) = lpt[i].base_port;
  }
}

void
close_all_printers(void)
{
  int loop;

  for (loop = 0; loop < NUM_PRINTERS; loop++) {
    p_printf("LPT: closing printer %d (%s)\n", loop,
	     lpt[loop].dev ? lpt[loop].dev : "<<NODEV>>");
    if (lpt[loop].fops.close)
       (lpt[loop].fops.close) (loop);
  }
}

int
printer_tick(u_long secno)
{
  int i;

  for (i = 0; i < NUM_PRINTERS; i++) {
    if (lpt[i].remaining >= 0) {
      p_printf("LPT: doing real tick for %d\n", i);
      if (lpt[i].remaining) {
	lpt[i].remaining--;
	if (!lpt[i].remaining)
	  (lpt[i].fops.flush) (i);
      }
    }
  }
  return 0;
}

void printer_config(int prnum, struct printer *pptr)
{
  struct printer *destptr;

  if (prnum < NUM_PRINTERS) {
    destptr = &lpt[prnum];
    destptr->prtcmd = pptr->prtcmd;
    destptr->prtopt = pptr->prtopt;
    destptr->dev = pptr->dev;
    destptr->file = pptr->file;
    destptr->remaining = pptr->remaining;
    destptr->delay = pptr->delay;
  }
}

void printer_print_config(int prnum, void (*print)(char *, ...))
{
  struct printer *pptr = &lpt[prnum];
  (*print)("LPT%d command \"%s  %s\"  timeout %d  device \"%s\"  baseport 0x%03x\n",
	  prnum+1, pptr->prtcmd, pptr->prtopt, pptr->delay, (pptr->dev ? pptr->dev : ""), pptr->base_port); 
}

#undef LPT_C
