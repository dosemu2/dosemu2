/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DOSEMU vbios checker,  Author: Bart Oldeman
 *
 * This is file vbioscheck.c
 *
 * As some people were not that sure about the location and size of their
 * video bios (this is necessary for graphics on console configuration),
 * I've written this small automatic detection program.
 * You must (normally!) be root to execute it, as it needs read access
 * to /dev/mem.
 *
 */

#include <stdio.h>

int main (void)
{
  FILE *f;
  int i;
  unsigned char c[0x21];

  f = fopen("/dev/mem","r");
  if (f==NULL) {
    printf("You must have read access to /dev/mem to execute this.\n");
    return 1;
  }
  for (i = 0xc0000; i < 0xf0000; i += 0x800) {
    fseek(f, i, SEEK_SET);
    fread(c, 0x21, 1, f);
    if (c[0]==0x55 && c[1]==0xaa &&
        c[0x1e]=='I' && c[0x1f]=='B' && c[0x20]=='M') {
      printf("$_vbios_seg = (0x%x)\n", i>>4);
      printf("$_vbios_size = (0x%x)\n", c[2]*0x200);
    }
  }
  fclose(f);
  return 0;
}
