/* cmos.c, for DOSEMU
 *   by Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1995/01/14 15:29:17 $
 * $Source: /home/src/dosemu0.60/dosemu/RCS/cmos.c,v $
 * $Revision: 2.5 $
 * $State: Exp $
 */

#include <time.h>
#include <sys/time.h>

#include "emu.h"
#include "cmos.h"
#include "disks.h"


#define EXTMEM_SIZE ((config.xms_size>config.ems_size)?config.xms_size : \
		     config.ems_size)
#define SET_CMOS(byte,val)  do { cmos.subst[byte] = (val); cmos.flag[byte] = 1; } while(0)

int cmos_date(int);
static struct CMOS cmos;

void
cmos_init(void)
{
  int i;

  for (i = 0; i < 64; i++)
    cmos.subst[i] = cmos.flag[i] = 0;

  /* CMOS floppies...is this correct? */
  SET_CMOS(CMOS_DISKTYPE, (config.fdisks ? (disktab[0].default_cmos << 4) : 0) |
	   ((config.fdisks > 1) ? disktab[1].default_cmos & 0xf : 0));

  /* CMOS equipment byte..top 2 bits are 01 for 2 drives, 00 for 1
   * bit 1 is 1 for math coprocessor installed
   * bit 0 is 1 for floppies installed, 0 for none */

  cmos.subst[0x14] = ((config.fdisks ? config.fdisks - 1 : 0) << 5) + (config.fdisks ? 1 : 0);
  if (config.mathco)
    cmos.subst[0x14] |= 2;
  cmos.flag[0x14] = 1;

  /* CMOS hard disks...type 47 for both. */
  SET_CMOS(CMOS_HDTYPE, (config.hdisks ? 0xf0 : 0) + ((config.hdisks - 1) ? 0xf : 0));
  SET_CMOS(CMOS_HD1EXT, 47);
  if (config.hdisks == 2)
    SET_CMOS(CMOS_HD2EXT, 47);
  else
    SET_CMOS(CMOS_HD2EXT, 0);

  /* this is the CMOS status */
  SET_CMOS(CMOS_STATUSA, 0x26);
  SET_CMOS(CMOS_STATUSB, 2);

  /* 0xc and 0xd are read only */
  SET_CMOS(CMOS_STATUSC, 0x50);
  SET_CMOS(CMOS_STATUSD, 0x80);

  SET_CMOS(CMOS_DIAG, 0);

  /* memory counts */
  SET_CMOS(CMOS_BASEMEML, config.mem_size & 0xff);	/* base mem LSB */
  SET_CMOS(CMOS_BASEMEMM, config.mem_size >> 8);	/* base mem MSB */

  SET_CMOS(CMOS_EXTMEML, EXTMEM_SIZE & 0xff);
  SET_CMOS(CMOS_EXTMEMM, EXTMEM_SIZE >> 8);

  /* say protected mode test 7 passed (?) */
  SET_CMOS(CMOS_SHUTDOWN, 6);

  /* information flags...my CMOS returns this */
  SET_CMOS(CMOS_INFO, 0xe1);

  g_printf("CMOS initialized: \n$Header: /home/src/dosemu0.60/dosemu/RCS/cmos.c,v 2.5 1995/01/14 15:29:17 root Exp $\n");
}

static int
cmos_chksum(void)
{
  int i, sum = 0;

  /* return the checksum over bytes 0x10-0x20. These are static values,
   * so no need to call cmos_read()
   */

  for (i = 0x10; i < 0x21; i++)
    sum += cmos.subst[i];

  return sum;
}

int
cmos_read(int port)
{
  unsigned char holder = 0;

  h_printf("CMOS read. from add: 0x%02x\n", cmos.address);

  switch (cmos.address) {
  case CMOS_SEC:
  case CMOS_MIN:
  case CMOS_HOUR:
  case CMOS_DOW:		/* day of week */
  case CMOS_DOM:		/* day of month */
  case CMOS_MONTH:
  case CMOS_YEAR:
    return (cmos_date(cmos.address));

  case CMOS_SECALRM:
  case CMOS_MINALRM:
  case CMOS_HOURALRM:
    h_printf("CMOS alarm read %d...UNIMPLEMENTED!\n", cmos.address);
    return cmos.subst[cmos.address];

  case CMOS_CHKSUML:
    return (cmos_chksum() & 0xff);

  case CMOS_CHKSUMM:
    return (cmos_chksum() >> 8);
  }

  /* date functions return, so hereafter all values should be those set
   * either at boot time or changed by DOS programs...
   */

  if (cmos.flag[cmos.address]) {/* this reg has been written to */
    holder = cmos.subst[cmos.address];
    h_printf("CMOS: substituting written value 0x%02x for read\n", holder);
  }
#ifdef DANGEROUS_CMOS
  else if (!set_ioperm(0x70, 2, 1)) {
    h_printf("CMOS: really reading 0x%x!\n", cmos.address);
    port_out((cmos.address & ~0xc0), 0x70);
    holder = port_in(0x71);
    set_ioperm(0x70, 2, 0);
  }
#endif
  else {
    error("CMOS: unknown CMOS read 0x%x\n", cmos.address);
    holder = cmos.subst[cmos.address];
  }

  h_printf("CMOS read. add: 0x%02x = 0x%02x\n", cmos.address, holder);
  return holder;
}

void
cmos_write(int port, int byte)
{
  if (port == 0x70)
    cmos.address = byte & ~0xc0;/* get true address */
  else {
    if ((cmos.address != 0xc) && (cmos.address != 0xd)) {
      h_printf("CMOS: set address 0x%02x to 0x%02x\n", cmos.address, byte);
      SET_CMOS(cmos.address, byte);
    }
    else
      h_printf("CMOS: write to ref 0x%x blocked\n", cmos.address);
  }
}

static u_short
BCD(int binval)
{
  unsigned short tmp1, tmp2;

  /* bit 2 of register 0xb set=binary mode, clear=BCD mode */
  if (cmos.subst[CMOS_STATUSB] & 4)
    return binval;

  if (binval > 99)
    binval = 99;

  tmp1 = binval / 10;
  tmp2 = binval % 10;
  return ((tmp1 << 4) | tmp2);
}

static int
cmos_date(int reg)
{
  unsigned long ticks;
  struct timeval tp;
  struct timezone tzp;
  struct tm *tm;
  int tmp;
  time_t this_time;

  /* get the time */
#if 0
  gettimeofday(&tp, &tzp);
  ticks = tp.tv_sec - (tzp.tz_minuteswest * 60);
  tm = localtime((time_t *) & ticks);
#else
  time(&this_time);
  tm = localtime((time_t *) &this_time);
#endif

  switch (reg) {
  case CMOS_SEC:
    return BCD(tm->tm_sec);

  case CMOS_MIN:
    return BCD(tm->tm_min);

  case CMOS_HOUR:		/* RTC hour...bit 1 of 0xb set=24 hour mode, clear 12 hour */
    tmp = BCD(tm->tm_hour);
    if (!(cmos.subst[CMOS_STATUSB] & 2)) {
      if (tmp == 0)
	return 12;
      else if (tmp > 12)
	return tmp - 12;
    }
    return tmp;

  case CMOS_DOW:
    return BCD(tm->tm_wday);

  case CMOS_DOM:
    return BCD(tm->tm_mday);

  case CMOS_MONTH:
    if (cmos.flag[8])
      return cmos.subst[8];
    else
      return BCD(1 + tm->tm_mon);

  case CMOS_YEAR:
    if (cmos.flag[9])
      return cmos.subst[9];
    else
      return BCD(tm->tm_year);

  default:
    h_printf("CMOS: cmos_time() register 0x%02x defaulted to 0\n", reg);
    return 0;
  }

  /* XXX - the reason for month and year I return the substituted values is this:
   * Norton Sysinfo checks the CMOS operation by reading the year, writing
   * a new year, reading THAT year, and then rewriting the old year,
   * apparently assuming that the CMOS year can be written to, and only
   * changes if the year changes, which is not likely between the 2 writes.
   * Since I personally know that dosemu won't stay uncrashed for 2 hours,
   * much less a year, I let it work that way.
   */

}

