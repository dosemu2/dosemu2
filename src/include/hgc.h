/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * video/hgc.h - Prototypes for HGC-card specifics functions
 */

#ifndef HGC_H
#define HGC_H

#define HGC_BASE0 ( (caddr_t) 0xb0000)
#define HGC_BASE1 ( (caddr_t) 0xb8000)
#define HGC_PLEN (32 * 1024)

extern void hgc_meminit(void);
extern void mda_initialize(void);
extern void mda_reinitialize(void);

extern char hgc_Mode;
extern char hgc_Konv;
extern int hgc_Page;
extern int hgc_ctrl;

extern void poshgacur(int x, int  y);
extern void set_hgc_page(int page);
extern void map_hgc_page( int fullmode );

#endif
/* End of video/hgc.h */
