/* cmos.h, for DOSEMU
 *   by Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1993/02/15 00:53:58 $ 
 * $Source: /usr/src/dos/RCS/cmos.h,v $
 * $Revision: 1.1 $
 * $State: Exp $
 */

#ifndef CMOS_H
#define CMOS_H

void cmos_write (int,int),
     cmos_init(void);
int cmos_read(int);

struct CMOS {
  unsigned char subst[64];
  unsigned char flag[64];
  int address;
};

#endif
