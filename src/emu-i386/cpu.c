/* 
 * DANG_BEGIN_MODULE
 * 
 * CPU/V86 support for dosemu
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * much of this code originally written by Matthias Lautner
 * taken over by:
 *          Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1995/04/08 22:30:40 $
 * $Source: /usr/src/dosemu0.60/dosemu/RCS/cpu.c,v $
 * $Revision: 2.14 $
 * $State: Exp $
 *
 * $Log: cpu.c,v $
 *
 * DANG_END_CHANGELOG
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "memory.h"
#include "termio.h"
#include "emu.h"
#include "port.h"
#include "int.h"

#include "vc.h"

#include "../dpmi/dpmi.h"

extern void xms_control(void);

#if 0
/* don't use this anymore */
static struct vec_t orig[256];		/* "original" interrupt vectors */
static struct vec_t snapshot[256];	/* vectors from last snapshot */

/* this is the array of interrupt vectors */
struct vec_t {
  unsigned short offset;
  unsigned short segment;
};
 
static struct vec_t *ivecs;    


#endif


/* 
 * DANG_BEGIN_FUNCTION cpu_init
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
}



int
do_hard_int(int intno)
{
  queue_hard_int(intno, NULL, NULL);
  return (1);
}

int
do_soft_int(int intno)
{
  do_int(intno);
  return 1;
}

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
      priv_on();
      opendevice = open( device,O_RDWR );
      priv_off();
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
      priv_on();
      set_ioperm(start, size, 1);
      priv_off();
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
      priv_on();
      set_ioperm(start, 0x400 - start, 1);
      priv_off();
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

unsigned char
read_port(unsigned short port)
{
  unsigned char r;
  int i = find_port(port, IO_READ);

  if (i == -1)
    return (0xff);

  if (!video_port_io) priv_on();
  if (port <= 0x3ff)
    set_ioperm(port, 1, 1);
  else
    iopl(3);
  if (!video_port_io) priv_off();

  r = port_in(port);

  if (!video_port_io) priv_on();
  if (port <= 0x3ff)
    set_ioperm(port, 1, 0);
  else
    iopl(0);
  if (!video_port_io) priv_off();

  if (!video_port_io) {
    r &= ports[i].andmask;
    r |= ports[i].ormask;
  }
  else
    video_port_io = 0;

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

  if (!video_port_io) priv_on();
  if(port <= 0x3fe) {
     set_ioperm(port  ,2,1);
  }
  else
     iopl(3);
  if (!video_port_io) priv_off();

  r = port_in_w(port);
  if (!video_port_io) priv_on();
  if(port <= 0x3fe) 
     set_ioperm(port,2,0);
  else
    iopl(0);
  if (!video_port_io) priv_off();

  if(!video_port_io) {
    r &= ( ports[i].andmask | 0xff00 );
    r &= ( (ports[j].andmask << 8) | 0xff);
    r |= ( ports[i].ormask & 0xff ); 
    r |= ( (ports[j].ormask << 8) & 0xff00 );
  }
  else
    video_port_io = 0;

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

  if (!video_port_io) priv_on();
  if (port <= 0x3ff)
    set_ioperm(port, 1, 1);
  else
    iopl(3);
  if (!video_port_io) priv_off();

  port_out(value, port);

  if (!video_port_io) priv_on();
  if (port <= 0x3ff)
    set_ioperm(port, 1, 0);
  else
    iopl(0);
  if (!video_port_io) priv_off();
  video_port_io = 0;

  return (1);
}

int
write_port_w(unsigned int value,unsigned short port)
{
  int i = find_port(port, IO_WRITE);
  int j = find_port(port + 1, IO_WRITE);

  if( (i == -1) || (j == -1) ) {
    i_printf("can't write to 16 bit port 0x%04x\n",port);
    return 0;
  } 
  if(!video_port_io) {
    value &= ( ports[i].andmask | 0xff00 );
    value &= ( (ports[j].andmask << 8) | 0xff );
    value |= ( ports[i].ormask & 0xff );
    value |= ( (ports[j].ormask << 8) & 0xff00);
  }

  i_printf("write 16 bit port 0x%x value %04x at %04x:%04x\n",
           port, (value & 0xffff), LWORD(cs), LWORD(eip));

  if (!video_port_io) priv_on();
  if (port <= 0x3fe) 
    set_ioperm(port  ,2,1);
  else
    iopl(3);
  if (!video_port_io) priv_off();

  port_out_w(value, port);

  if (!video_port_io) priv_on();
  if (port <= 0x3fe) 
    set_ioperm(port, 2, 0);
  else
    iopl(0);
  if (!video_port_io) priv_off();
  video_port_io = 0;

  return (1);
}

void safe_port_out_byte(const unsigned short port, const unsigned char byte)
{
        if(i_am_root)  {
                int result;

		priv_on();
                result = ioperm(port, 1, 1);
		priv_off();
		if (result) error("failed to enable port %x\n", port);
                port_out(byte, port);
		priv_on();
                result = ioperm(port, 1, 0);
		priv_off();
		if (result) error("failed to disable port %x\n", port);
		priv_off();

        }       else i_printf("want to ");
        i_printf("out(%x, %x)\n", port, byte);
}


char safe_port_in_byte(const unsigned short port)
{
        unsigned char value=0;

        if(i_am_root) {
                int result;

		priv_on();
                result = ioperm(port, 1, 1);
		priv_off();
		if (result) error("failed to enable port %x\n", port);
                value = port_in(port);
		priv_on();
                result = ioperm(port, 1, 0);
		priv_off();
		if (result) error("failed to disable port %x\n", port);
        }       else i_printf("want to ");
        i_printf("in(%x)", port);
        if(i_am_root)
                i_printf(" = %x", value);
        i_printf("\n");
        return value;
}


