/* 
 * DANG_BEGIN_MODULE
 * 
 * REMARK
 * CPU/V86 support for dosemu
 * /REMARK
 *
 * DANG_END_MODULE
 *
 * much of this code originally written by Matthias Lautner
 * taken over by:
 *          Robert Sanders, gt8134b@prism.gatech.edu
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>

#include "config.h"
#include "memory.h"
#include "termio.h"
#include "emu.h"
#include "port.h"
#include "int.h"

#include "vc.h"

#include "dpmi.h"
#include "priv.h"

extern void xms_control(void);

static unsigned long CRs[5] =
{
	0x00000010,	/*0x8003003f?*/
	0x00000000,	/* invalid */
	0x00000000,
	0x00000000,
	0x00000000
};

static unsigned long DRs[8] =
{
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0xffff0ff0,
	0x00000400,
	0xffff0ff0,
	0x00000400
};

static unsigned long TRs[2] =
{
	0x00000000,
	0x00000000
};

/* 
 * DANG_BEGIN_FUNCTION cpu_trap_0f
 *
 * process opcodes 0F xx xx trapped by GP_fault
 * returns 1 if handled, 0 otherwise
 * Main difference with previous version: bits in our pseudo-control
 * regs can now be written. This should make CPU detection pgms happy.
 *
 * DANG_END_FUNCTION
 *
 */
int cpu_trap_0f (unsigned char *csp, struct sigcontext_struct *scp)
{
  	g_printf("CPU: TRAP op 0F %02x %02x\n",csp[1],csp[2]);

  	if (in_dpmi && (scp==NULL)) return 0; /* for safety */

	if (csp[1] == 0x06) {
		(in_dpmi? scp->eip:LWORD(eip)) += 2;  /* CLTS - ignore */
		return 1;
	}
	else if (csp[1] == 0x31) {
		/* ref: Van Gilluwe, "The Undocumented PC". The program
		 * 'cpurdtsc.exe' traps here */
		if (vm86s.cpu_type >= CPU_586) {
		  __asm__ __volatile__ ("rdtsc" \
			:"=a" (REG(eax)),  \
			 "=d" (REG(edx)));
		}
		else {
		  struct timeval tv;
		  unsigned long long t;

		  /* try to do the best you can to return something
		   * meaningful, assume clk=100MHz */
		  gettimeofday(&tv, NULL);
		  t = (unsigned long long)tv.tv_sec*100000000 +
		      (unsigned long long)tv.tv_usec*100;
		  REG(eax)=((int *)&t)[0];
		  REG(edx)=((int *)&t)[1];
		}
		(in_dpmi? scp->eip:LWORD(eip)) += 2;
		return 1;
	}
	else if ((((csp[1] & 0xfc)==0x20)||(csp[1]==0x24)||(csp[1]==0x26)) &&
		((csp[2] & 0xc0) == 0xc0)) {
		unsigned long *cdt, *srg;
		int idx;
		boolean rnotw;

		rnotw = ((csp[1]&2)==0);
		idx = (csp[2]>>3)&7;
		switch (csp[1]&7) {
			case 0: case 2: cdt=CRs;
					if (idx>4) return 0;
					break;
			case 1: case 3: cdt=DRs; break;
			case 4: case 6: cdt=TRs;
					if (idx<6) return 0;
					idx -= 6;
					break;
			default: return 0;
		}

		switch (csp[2]&7) {
			case 0: srg = (in_dpmi? &(scp->eax):(unsigned long *)&(REGS.eax));
				break;
			case 1: srg = (in_dpmi? &(scp->ecx):(unsigned long *)&(REGS.ecx));
				break;
			case 2: srg = (in_dpmi? &(scp->edx):(unsigned long *)&(REGS.edx));
				break;
			case 3: srg = (in_dpmi? &(scp->ebx):(unsigned long *)&(REGS.ebx));
				break;
			case 6: srg = (in_dpmi? &(scp->esi):(unsigned long *)&(REGS.esi));
				break;
			case 7: srg = (in_dpmi? &(scp->edi):(unsigned long *)&(REGS.edi));
				break;
			default:	/* ESP(4),EBP(5) */
				return 0;
		}
		if (rnotw) {
		  *srg = cdt[idx];
		  g_printf("CPU: read =%08lx\n",*srg);
		} else {
		  g_printf("CPU: write=%08lx\n",*srg);
		  if (cdt==CRs) {
		    /* special cases... they are too many, I hope this
		     * will suffice for all */
		    if (idx==0) cdt[idx] = (*srg&0xe005002f)|0x10;
		     else if ((idx==1)||(idx==4)) return 0;
		  }
		  else cdt[idx] = *srg;
		}
		(in_dpmi? scp->eip:LWORD(eip)) += 3;
		return 1;
	}
	/* all other trapped combinations:
	 *	0f 0a, 0c..0f, 27, 29, 2b..2f, c2..c6, d0..fe, etc.
	 */
	return 0;
}

/* 
 * DANG_BEGIN_FUNCTION cpu_setup
 *
 * description:
 *  Setup initial interrupts which can be revectored so that the kernel
 * does not need to return to DOSEMU if such an interrupt occurs.
 *
 * DANG_END_FUNCTION
 *
 */
void cpu_setup(void)
{
  extern void int_vector_setup(void);
  extern void init_cpu (void);

  int_vector_setup();

  /* ax,bx,cx,dx,si,di,bp,fs,gs can probably can be anything */
  REG(eax) = 0;
  REG(ebx) = 0;
  REG(ecx) = 0;
  REG(edx) = 0;
  REG(esi) = 0;
  REG(edi) = 0;
  REG(ebp) = 0;
  REG(eip) = 0x7c00;
  REG(cs) = 0;			/* Some boot sectors require cs=0 */
  REG(esp) = 0x100;
  REG(ss) = 0x30;		/* This is the standard pc bios stack */
  REG(es) = 0;			/* standard pc es */
  REG(ds) = 0x40;		/* standard pc ds */
  REG(fs) = 0;
  REG(gs) = 0;
#if 0
  REG(eflags) |= (IF | VIF | VIP);
#else
  REG(eflags) |= (VIF | VIP);
#endif

#ifdef X86_EMULATOR
  if (config.cpuemu) {
    init_cpu();
  }
#endif
}



#ifdef USE_INT_QUEUE
int
do_hard_int(int intno)
{
  queue_hard_int(intno, NULL, NULL);
  return (1);
}
#endif

int
do_soft_int(int intno)
{
  do_int(intno);
  return 1;
}

#ifndef NEW_PORT_CODE

static struct port_struct {
  int start;
  int size;
  int permission;
  int ormask, andmask;
  int fp;
} *ports = NULL;

static int num_ports = 0;

static u_char video_port_io = 0;

/* find a matching entry in the ports array */
static int
find_port(unsigned short port, int permission)
{
  static int last_found = 0;
  unsigned int i;

  for (i = last_found; i < num_ports; i++)
    if (port >= ports[i].start &&
	port < (ports[i].start + ports[i].size) &&
	((ports[i].permission & permission) == permission)) {
      last_found = i;
      return (i);
    }

  for (i = 0; i < last_found; i++)
    if (port >= ports[i].start &&
	port < (ports[i].start + ports[i].size) &&
	((ports[i].permission & permission) == permission)) {
      last_found = i;
      return (i);
    }

/*
   for now, video_port_io flag allows special access for video regs
*/
  if (   ((LWORD(cs) & 0xf000) == config.vbios_seg) 
      && config.allowvideoportaccess 
      && scr_state.current) {
#if 0
    v_printf("VID: checking port %x\n", port);
#endif
    if (port > 0x4000)
      return (-1);
    port &= 0xfff;
    if (    (port > 0x300 && port < 0x3f8) 
         ||  port > 0x400
         || port == 0x42 || port == 0x1ce || port == 0x1cf) {
      video_port_io = 1;
      return 1;
    }
  }

  return (-1);
}

/* control the io permissions for ports */
int
allow_io(unsigned int start, int size, int permission, int ormask, int andmask,
unsigned int portspeed, char *device)
{
  FILE *fp;
  int opendevice= -1;
  char line[100];
  char portname[50];
  unsigned int beg, end;
    
  /* find out whether the port address request is available 
   * this way, try to deny uncoordinated acces
   *
   * If it is not listed in /proc/ioports, register them 
   * ( we need some syscall to do so bo 960609)
   * if it is registered, we need the name of a device to open
   * if we can open it, we disallow to that port
   */
  if ((fp = fopen("/proc/ioports", "r")) == NULL) {
    c_printf("Ports can't open /proc/ioports\n");
    return (0);
  }
  while(fgets(line, 100, fp)) {
    sscanf(line, "%x-%x : %s", &beg, &end, portname);
    if ((start >= beg) && ( start + size -1 <= end)) {
      /* ports are occupied, try to open the according device */
      c_printf("PORT already registered as %s 0x%04x-0x%04x\n",
	       portname,beg,end);
      enter_priv_on();
      opendevice = open( device,O_RDWR );
      leave_priv_setting();
      if (opendevice == -1)
	switch (errno) {
	case EBUSY : 
	  c_printf("PORT Device %s already in use\n",device);
	  return (0);
	case EACCES :
	  c_printf("PORT Device %s , access not allowed\n",device);
	  return (0);
	case ENOENT :
	  c_printf("PORT No such Device %s\n",device);
	  return (0);
	}
      c_printf("PORT Device %s opened successfully\n",device);
    }
  }
  if (opendevice == -1){
    /* register it
     */
    c_printf("PORT Syscall to register ports not yet implemented\n");
  }
#if 0
  if ((base + size) < beg ||  base >= end)
      continue;
    else
      return NULL;        /* overlap */
#endif

      

  /* first the simplest case...however, this simplest case does
   * not apply to ports above 0x3ff--the maximum ioperm()able
   * port under Linux--so we must handle some of that ourselves.
   *
   * if we want to log port access below 0x400, don't enter these 
   * ports here with set_ioperm but add them to the list of allowed
   * ports. That way we get an exception to get out of vm86(0)
   */
  if (permission == IO_RDWR && (ormask == 0 && andmask == 0xFFFF) 
      && portspeed) {
    if ((start + size - 1) <= 0x3ff) {
      c_printf("giving fast hardware access to ports 0x%x -> 0x%x\n",
	       start, start + size - 1);
      set_ioperm(start, size, 1);
      /* Don't return, but enter it in the list to keep track of the file 
       * descriptor
       * return (1);
       */ 
    }

    /* allow direct I/O to all the ports that *are* below 0x3ff
       * and add the rest to our list
       */
    else if (start <= 0x3ff) {
      warn("PORT: s-s: %d %d\n", start, size);
      c_printf("giving fast hardware access only to ports 0x%x -> 0x3ff\n",
	       start);
      set_ioperm(start, 0x400 - start, 1);
      size = start + size - 0x400;
      start = 0x400;
    }
  }

  /* we'll need to add an entry */
  if (ports)
    ports = (struct port_struct *)
      realloc(ports, sizeof(struct port_struct) * (num_ports + 1));

  else
    ports = (struct port_struct *)
      malloc(sizeof(struct port_struct) * (num_ports + 1));

  num_ports++;
  if (!ports) {
    error("allocation error for ports!\n");
    num_ports = 0;
    return (0);
  }

  ports[num_ports - 1].start = start;
  ports[num_ports - 1].size = size;
  ports[num_ports - 1].permission = permission;
  ports[num_ports - 1].ormask = ormask;
  ports[num_ports - 1].andmask = andmask;
  ports[num_ports - 1].fp = opendevice;

  c_printf("added port %x size=%d permission=%d ormask=%x andmask=%x\n",
	   start, size, permission, ormask, andmask);

  return (1);
}

/* release registered ports or blocked devices */
void
release_ports()
{
  unsigned int i;

  for (i = 0; i < num_ports; i++)
    if (ports[i].fp == -1) {
      c_printf("PORT Syscall to release ports not yet implemented\n");
      c_printf("PORT releasing registered ports 0x%04x-0x%04x \n",
	       ports[i].start,ports[i].start + ports[i].size -1);
    }
    else
      if ( close(ports[i].fp)  == -1)
	c_printf("PORT Closeing device-filedescriptor %d failed with %s\n",
		 ports[i].fp, strerror(errno));
      else
	c_printf("PORT Closeing device succeeded\n");
}


int
port_readable(unsigned short port)
{
  return (find_port(port, IO_READ) != -1);
}

int
port_writeable(unsigned short port)
{
  return (find_port(port, IO_WRITE) != -1);
}

/* Support for controllable port i/o tracing 		G. Guenther 96.07.20

   The "T" debug flag and LOG_IO macro below may be used to collect
   port i/o protocol traces.  These are the latest version of the patches
   that I used to collect the data required for deducing the parallel
   port communication protocols used by the ZIP drive, the Shuttle EPAT
   chip (in the EZ drives) and the backpack CDrom ...

   Uwe's patches to allow_io above, control whether or not "T" debugging
   will work:  If a port is listed in the ports section of the DOSemu
   config file, and *NOT* given the "fast" attribute, then all accesses
   to that port will trap through the following routines.  If the "T"
   flag is enabled (with -D+T or dosdbg +T), a very short record will
   be logged to the debug file for every access to a listed port.

   If you enable "h" *and* "i" debugging you will get the same information 
   logged in a much more verbose manner - along with a lot of other stuff.
   This verbosity turns out to be a serious problem, as it can slow the \
   emulator's response so much that external hardware gets confused.  

   (Can anybody explain *why* byte accesses are logged with the 'h' flag
   and word access are logged with the 'i' flag ? )

   If you are trying to use a slow system to trace a fast protocol, you
   may need to shorten the output further ...  (In one case, I had to 
   add suppression for repeated identical writes, as the drive being
   monitored read and wrote each byte 4 times ...)

   The logged records look like this:

	ppp f vvvv

   ppp is the hex address of the port, f is a flag, one of:

	>   byte read    inb
	<   byte write   outb
	}   word read    inw
	{   word write   outw

   vvvv is the value read or written.

*/

#define LOG_IO(p,r,f,m)	  if (d.io_trace) T_printf("%x %c %x\n",p&0xfff,f,r&m);

unsigned char
read_port(unsigned short port)
{
  unsigned char r;
  int i = find_port(port, IO_READ);

  if (i == -1)
    return (0xff);

  if (!video_port_io) enter_priv_on();
  if (port <= 0x3ff)
    set_ioperm(port, 1, 1);
  else
    priv_iopl(3);
  if (!video_port_io) leave_priv_setting();

  r = port_in(port);

  if (!video_port_io) enter_priv_on();
  if (port <= 0x3ff)
    set_ioperm(port, 1, 0);
  else
    priv_iopl(0);
  if (!video_port_io) leave_priv_setting();

  if (!video_port_io) {
    r &= ports[i].andmask;
    r |= ports[i].ormask;
  }
  else
    video_port_io = 0;

  LOG_IO(port,r,'>',0xff);

  h_printf("read port 0x%x gave %02x at %04x:%04x\n",
	   port, r, LWORD(cs), LWORD(eip));
  return (r);
}

unsigned int
read_port_w(unsigned short port)
{
  unsigned int r;
  int i = find_port(port,IO_READ);
  int j = find_port(port+1,IO_READ);

#ifdef GUSPNP
  if (port == 0x324)
    i = j = 0;
#endif
  if(i == -1) {
    i_printf("can't read low-byte of 16 bit port 0x%04x:trying to read high\n",
              port); 
    return((read_port(port+1) << 8 ) | 0xff);
  }
  if(j == -1) {
    i_printf("can't read hi-byte of 16 bit port 0x%04x:trying to read low\n",
              port);
  return(read_port(port) | 0xff00);
  }

  if (!video_port_io) enter_priv_on();
  if(port <= 0x3fe) {
     set_ioperm(port  ,2,1);
  }
  else
     priv_iopl(3);
  if (!video_port_io) leave_priv_setting();

  r = port_in_w(port);
  if (!video_port_io) enter_priv_on();
  if(port <= 0x3fe) 
     set_ioperm(port,2,0);
  else
    priv_iopl(0);
  if (!video_port_io) leave_priv_setting();

  if (
#ifdef GUSPNP
       i && j &&
#endif
       !video_port_io) {
    r &= ( ports[i].andmask | 0xff00 );
    r &= ( (ports[j].andmask << 8) | 0xff);
    r |= ( ports[i].ormask & 0xff ); 
    r |= ( (ports[j].ormask << 8) & 0xff00 );
  }
  else
    video_port_io = 0;

  LOG_IO(port,r,'}',0xffff);

  i_printf("read 16 bit port 0x%x gave %04x at %04x:%04x\n",
           port, r, LWORD(cs), LWORD(eip));
   return (r);
}

int
write_port(unsigned int value, unsigned short port)
{
  int i = find_port(port, IO_WRITE);

  if (i == -1)
    return (0);

  if (!video_port_io) {
    value &= ports[i].andmask;
    value |= ports[i].ormask;
  }

  h_printf("write port 0x%x value %02x at %04x:%04x\n",
	   port, (value & 0xff), LWORD(cs), LWORD(eip));

  LOG_IO(port,value,'<',0xff);

  if (!video_port_io) enter_priv_on();
  if (port <= 0x3ff)
    set_ioperm(port, 1, 1);
  else
    priv_iopl(3);
  if (!video_port_io) leave_priv_setting();

  port_out(value, port);

  if (!video_port_io) enter_priv_on();
  if (port <= 0x3ff)
    set_ioperm(port, 1, 0);
  else
    priv_iopl(0);
  if (!video_port_io) leave_priv_setting();
  video_port_io = 0;

  return (1);
}

int
write_port_w(unsigned int value,unsigned short port)
{
  int i = find_port(port, IO_WRITE);
  int j = find_port(port + 1, IO_WRITE);

#ifdef GUSPNP
  if (port == 0x324)
    i = j = 0;
#endif
  if( (i == -1) || (j == -1) ) {
    i_printf("can't write to 16 bit port 0x%04x\n",port);
    return 0;
  } 
  if (
#ifdef GUSPNP
       i && j &&
#endif
       !video_port_io) {
    value &= ( ports[i].andmask | 0xff00 );
    value &= ( (ports[j].andmask << 8) | 0xff );
    value |= ( ports[i].ormask & 0xff );
    value |= ( (ports[j].ormask << 8) & 0xff00);
  }

  i_printf("write 16 bit port 0x%x value %04x at %04x:%04x\n",
           port, (value & 0xffff), LWORD(cs), LWORD(eip));

  LOG_IO(port,value,'{',0xffff);

  if (!video_port_io) enter_priv_on();
  if (port <= 0x3fe) 
    set_ioperm(port  ,2,1);
  else
    priv_iopl(3);
  if (!video_port_io) leave_priv_setting();

  port_out_w(value, port);

  if (!video_port_io) enter_priv_on();
  if (port <= 0x3fe) 
    set_ioperm(port, 2, 0);
  else
    priv_iopl(0);
  if (!video_port_io) leave_priv_setting();
  video_port_io = 0;

  return (1);
}

void safe_port_out_byte(const unsigned short port, const unsigned char byte)
{
        if(i_am_root)  {
                int result;

                result = set_ioperm(port, 1, 1);
		if (result) error("failed to enable port %x\n", port);
                port_out(byte, port);
                result = set_ioperm(port, 1, 0);
		if (result) error("failed to disable port %x\n", port);

        }       else i_printf("want to ");
        i_printf("out(%x, 0x%x)\n", port, byte);
}


char safe_port_in_byte(const unsigned short port)
{
        unsigned char value=0;

        if(i_am_root) {
                int result;

                result = set_ioperm(port, 1, 1);
		if (result) error("failed to enable port %x\n", port);
                value = port_in(port);
                result = set_ioperm(port, 1, 0);
		if (result) error("failed to disable port %x\n", port);
        }       else i_printf("want to ");
        i_printf("in(%x)", port);
        if(i_am_root)
                i_printf(" = 0x%x", value);
        i_printf("\n");
        return value;
}

#endif	/* NEW_PORT_CODE */
