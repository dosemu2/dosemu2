#include <stdio.h>

static void inline port_out(char value, unsigned short port)
{
__asm__ volatile ("outb %0,%1"
		::"a" ((char) value),"d" ((unsigned short) port));
}

static char inline port_in(unsigned short port)
{
	char _v;
__asm__ volatile ("inb %1,%0"
		:"=a" (_v):"d" ((unsigned short) port));
	return _v;
}

main(int argc, char *argv[])
{
  int a,b,c,d,e,f;

  if (argc != 3)
    {
      printf("usage: curset LOW HIGH\n");
      exit(1);
    }

  a=atoi(argv[1]);
  b=atoi(argv[2]);

  ioperm(0x3c0,0x1b,1);  /* get all video ports */
  port_in(0x3da);	 /* reset address/data flipflop */

  /* make big cursor */
  port_out(0xa, 0x3d4);
  port_out(a, 0x3d5);
  port_out(0xb, 0x3d4);
  port_out(b, 0x3d5);
}

