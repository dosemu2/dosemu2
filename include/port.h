/*
 * include/port.h - contains INLINE-Functions for port access
 * 
 * $Id: port.h,v 2.2 1995/02/25 22:38:46 root Exp root $
 */

#ifndef PORT_H
#define PORT_H

#define inline __inline__

__inline__ static void port_out(char value, unsigned short port)
{
  __asm__ volatile ("outb %0,%1"
		    ::"a" ((char) value), "d"((unsigned short) port));
}

__inline__ static char port_in(unsigned short port)
{
  char _v;
  __asm__ volatile ("inb %1,%0"
		    :"=a" (_v):"d"((unsigned short) port));

  return _v;
}

static __inline__ void port_out_w(short value, u_short port)
{
  __asm__("outw %0,%1" :: "a" ((short) value),
		"d" ((unsigned short) port));
}

static __inline__ short port_in_w(unsigned short port)
{
  short _v;
  __asm__("inw %1,%0":"=a" (_v) : "d" ((unsigned short) port));
  return _v;
}

#if 0
/* write to port if we are root, optionally printing a 
 * diagnostic message
 */
static void inline safe_port_out_byte(const short port, const char byte)
{
	if(i_am_root)  {
		int result;

		result = ioperm(port, 1, 1);
		port_out(byte, port);
		result = ioperm(port, 1, 0);
	}	else i_printf("want to ");
	i_printf("out(%x, %x)\n", port, byte);
}


static inline char safe_port_in_byte(const short port)
{
	char value;

	if(i_am_root) {
		int result;
	
		result = ioperm(port, 1, 1);
		value = port_in(port);
		result = ioperm(port, 1, 0);
	}	else i_printf("want to ");
	i_printf("in(%x)");
	if(i_am_root)
		i_printf(" = %x", value);
	i_printf("\n");
	return value;
}
#endif		
#endif
