/* cmos.c, for DOSEMU
 *   by Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1993/02/16 00:21:29 $ 
 * $Source: /usr/src/dos/RCS/cmos.c,v $
 * $Revision: 1.2 $
 * $State: Exp $
 */

#define CMOS_C
#include <time.h>
#include <sys/time.h>
#include "emu.h"
#include "cmos.h"

extern int mem_size, extmem_size, hdisks, fdisks;
extern struct disk disktab[];

int cmos_date(int);
static struct CMOS cmos;

void cmos_init(void)
{
  int i;

  for (i=0;i<64;i++)
    cmos.subst[i]=cmos.flag[i]=0;

  /* CMOS floppies...is this correct? */
  cmos.subst[0x10]=(fdisks ?  (disktab[0].default_cmos << 4) : 0) | 
    ((fdisks > 1) ? disktab[1].default_cmos&0xf : 0);
  cmos.flag[0x10]=1;

  /* CMOS equipment byte..top 2 bits are 01 for 2 drives, 00 for 1 
   * bit 1 is 1 for math coprocessor installed
   * bit 0 is 1 for floppies installed, 0 for none */
  cmos.subst[0x14]=((fdisks ? fdisks-1 : 0)<<5)+(fdisks ? 1 : 0);
#if MATHCO
  cmos.subst[0x14]|=2;
#endif
  cmos.flag[0x14]=1;

  /* CMOS hard disks...type 47 for both. */
  cmos.subst[0x12]=(hdisks ? 0xf0 : 0) + ((hdisks - 1) ? 0xf : 0);
  cmos.subst[0x19]=47;
  cmos.subst[0x1a]=47;
  cmos.flag[0x12]=cmos.flag[0x19]=cmos.flag[0x1a]=1;

  /* this is the CMOS status */
  cmos.subst[0xa]=0x26;
  cmos.flag[0xa]=1;
  cmos.subst[0xb]=2;
  cmos.flag[0xb]=1;
  cmos.subst[0xc]=0x50;  /* 0xc and 0xd are read only */
  cmos.flag[0xc]=1;
  cmos.subst[0xd]=0x80;  /* CMOS has power */
  cmos.flag[0xd]=1;
  cmos.subst[0xe]=0;
  cmos.flag[0xe]=1;

  /* memory counts */
  cmos.subst[0x15]=mem_size & 0xff;   /* base mem LSB */
  cmos.subst[0x16]=mem_size >> 8;
  cmos.subst[0x17]=extmem_size & 0xff;    /* extended mem LSB */
  cmos.subst[0x18]=extmem_size >> 8;
  cmos.flag[0x15]=cmos.flag[0x16]=cmos.flag[0x17]=cmos.flag[0x18]=1;

  /* information flags...my CMOS returns this */
  cmos.subst[0x33] = 0xe1;
  cmos.flag[0x33] = 1;

  warn("CMOS initialized: \n$Header: /usr/src/dos/RCS/cmos.c,v 1.2 1993/02/16 00:21:29 root Exp $\n");
}


int cmos_read(int port)
{
  unsigned char holder;

  h_printf("CMOS read. from add: 0x%02x\n", cmos.address);

  switch(cmos.address)
    {
      case 0:  /* RTC seconds */
      case 2:  /* minutes */
      case 4:  /* hours */
      case 6:  /* day of week */
      case 7:  /* day of month */
      case 8:  /* month */
      case 9:  /* year */
        return (cmos_date(cmos.address)); 
      case 1:  /* RTC seconds alarm */
      case 3:  /* minutes alarm */
      case 5:  /* hours alarm */
	h_printf("CMOS alarm read %d...UNIMPLEMENTED!\n", cmos.address);
	return cmos.subst[cmos.address];
    }

  /* date functions return, so hereafter all values should be static
   * after boot time...
   */

  if (cmos.flag[cmos.address])  /* this reg has been written to */
    {
      holder=cmos.subst[cmos.address];
      h_printf("CMOS: substituting written value 0x%02x for read\n",holder);
    }
#ifdef DANGEROUS_CMOS
  else if (!ioperm(0x70,2,1))
    {
      h_printf("CMOS: really reading!\n");
      port_out((cmos.address & ~0xc0)|0x80, 0x70);
      holder=port_in(0x71);
      ioperm(0x70,2,0);
    }
#endif
  else error("CMOS: Can't get permissions for true I/O to 0x70, 0x71\n");

  h_printf("CMOS read. add: 0x%02x = 0x%02x\n", cmos.address, holder);
  return holder;
}


void cmos_write(int port, int byte)
{
  if (port == 0x70)
      cmos.address=byte & ~0xc0;  /* get true address */
  else
    {
      if ((cmos.address != 0xc) && (cmos.address != 0xd))
	{
	  h_printf("CMOS: set address 0x%02x to 0x%02x\n", cmos.address,byte);
	  cmos.subst[cmos.address]=byte;
	  cmos.flag[cmos.address]=1;
	}
      else h_printf("CMOS: write to ref 0x%x blocked\n", cmos.address);
    }
}


int cmos_date(int reg)
{
  unsigned long ticks;
  struct timeval tp;
  struct timezone tzp;
  struct tm *tm;

  /* get the time */
  gettimeofday(&tp, &tzp);
  ticks = tp.tv_sec - (tzp.tz_minuteswest*60);
  tm = localtime((time_t *)&ticks);

#if 0
  h_printf("CMOS: get time %d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
  h_printf("CMOS: get date %d.%d.%d\n", tm->tm_mday, tm->tm_mon, tm->tm_year);
#endif

  /* is any of this correct?? */
  switch(reg)
    {
    case 0:  /* RTC seconds */
      return tm->tm_sec;
    case 2:  /* RTC minutes */
      return tm->tm_min;
    case 4:  /* RTC hour */
      return tm->tm_hour;
    case 6:  /* RTC weekday */
      return tm->tm_wday;
    case 7:  /* RTC day of month */
      return tm->tm_mday; /* day of month */
    case 8: /* RTC month */
      if (cmos.flag[8]) return cmos.subst[8];
      else return tm->tm_mon;
    case 9: /* RTC year */
      if (cmos.flag[9]) return cmos.subst[9];
      else return tm->tm_year;
    default:
      h_printf("CMOS: cmos_time() register 0x%02x defaulted to 0\n",reg);
      return 0;
    }

  /* the reason for month and year I return the substituted valus is this:
   * Norton Sysinfo checks the CMOS operation by reading the year, writing
   * a new year, reading THAT year, and then rewriting the old year,
   * apparently assuming that the CMOS year can be written to, and only
   * changes if the year changes, which is not likely between the 2 writes.
   * Since I personally know that dosemu won't stay uncrashed for 2 hours,
   * much less a year, I let it work that way.
   */

}

#undef CMOS_C
