/*
 * include/port.h - contains INLINE-Functions for port access
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

#endif
/* End of include/port.h */
