/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

#ifndef _EMU86_PROTMODE_H
#define _EMU86_PROTMODE_H

#include "emu-ldt.h"

typedef struct {
	unsigned int BoundL,BoundH;
	unsigned short Oldsel, Attrib __attribute__ ((packed));
} SDTR;

typedef struct {
	unsigned int Base;
	unsigned int Limit;
	unsigned int Attrib;
} DTR;

/*
 *   segment selectors
 */
#define SELECTOR_GDT             0
#define SELECTOR_LDT             1
#define SELECTOR_RPL_SHIFT       0
#define SELECTOR_RPL_MASK        0x03
#define SELECTOR_TI_SHIFT        2
#define SELECTOR_TI_MASK         0x4
#define SELECTOR_INDEX_SHIFT     3
#define SELECTOR_INDEX_MASK      0xfff8

#define SELECTOR_RPL(_sel)       ((_sel) & SELECTOR_RPL_MASK)
#define SELECTOR_INDEX(_sel)     ((_sel) >> SELECTOR_INDEX_SHIFT)

#define NULL_SELECTOR(_sel)      (!((_sel) & ~SELECTOR_RPL_MASK))

/*
 *   segment descriptors - little endian
 *
 *  00-15	limit 15-0
 *  16-39	base  23-0
 *  40-43	type
 *  44		DT
 *  45-46	DPL
 *  47		P
 *  48-51	limit 19-16
 *  52		vf
 *  53		r
 *  54		DB
 *  55		G
 *  56-63	base 31-24
 *
 * limit mask 000h0000.0000llll
 * base  mask hh0000mm.llll0000
 * flags mask 00f0ff00.00000000
 *
 */
/* descriptor byte 6 */
#define	DF_PAGES	0x8000
#define	DF_32		0x4000

/* descriptor byte 5 */
#define	DF_PRESENT	0x80
#define	DF_DPL		0x60
#define	DF_USER		0x10
#define	DF_CODE		0x08
#define	DF_DATA		0x00
#define	DF_EXPANDDOWN	0x04
#define	DF_CREADABLE	0x02
#define	DF_DWRITEABLE	0x02
#define DF_ACCESSED	0x01

#define DT_CODE(dp)               ( ((dp)->S) && (((dp)->type & 0x8) == 0x8))
#define DT_CONFORMING_CODE(dp)    ( ((dp)->S) && (((dp)->type & 0xc) == 0xc))
#define DT_NONCONFORMING_CODE(dp) ( ((dp)->S) && (((dp)->type & 0xc) == 0x8))
#define DT_READABLE_CODE(dp)      ( ((dp)->S) && (((dp)->type & 0xa) == 0xa))
#define DT_DATA(dp)               ( ((dp)->S) && (((dp)->type & 0x8) == 0x0))
#define DT_WRITEABLE_DATA(dp)     ( ((dp)->S) && (((dp)->type & 0xa) == 0x2))
#define DT_EXPAND_DOWN(dp)        ( ((dp)->S) && (((dp)->type & 0xc) == 0x4))
#define DT_CALL_GATE(dp)          (!((dp)->S) && (((dp)->type & 0x7) == 0x4))
#define DT_LDT(dp)                (!((dp)->S) && (((dp)->type & 0xf) == 0x2))
#define DT_TASK_GATE(dp)          (!((dp)->S) && (((dp)->type & 0xf) == 0x5))
#define DT_TSS(dp)                (!((dp)->S) && (((dp)->type & 0x5) == 0x1))
#define DT_AVAIL_TSS(dp)          (!((dp)->S) && (((dp)->type & 0x7) == 0x1))

#define DT_ACCESS                    0x2
#define DT_32BIT                     0x8
#define DT_TSS_BUSY                  0x2

#define GT_TASK(gp)		(((gp)->type & 0x1f) == 0x05)
#define GT_INTR(gp)		(((gp)->type & 0x17) == 0x06)
#define GT_TRAP(gp)		(((gp)->type & 0x17) == 0x07)
#define GT_32BIT(gp)		((gp)->type & 0x08)
#define GT_CS(dp)		((dp)->seg)
#define GT_OFFS(dp)		(((dp)->offs_hi<<16) | ((dp)->offs_lo))

#define DATA_DESC           0x2
#define CODE_DESC           0xa
#define TASK_DESC           0x9  /* TSS available */
#define TASK_DESC_BUSY      0xb  /* TSS busy */

#define DT_NO_XFER		 0	/* error */

#define DT_XFER_DATA		0x10
#define DT_XFER_DATA16		0x10	/* 16bit data segment */
#define DT_XFER_DATA32		0x11	/* 32bit data segment */

#define DT_XFER_CODE		0x20
#define DT_RM_XFER		0x20	/* real/VM86 mode */
#define DT_XFER_CODE16		0x20	/* 16bit code segment */
#define DT_XFER_CODE32		0x21	/* 32bit code segment */

#define DT_XFER_SYS		0x40	/* mask: bit 6 */
#define DT_XFER_TSS16		0x41	/* n.i.*/
#define DT_XFER_LDT		0x42
#define DT_XFER_CG16		0x43	/* n.i.*/
#define DT_XFER_TSKG		0x44
#define DT_XFER_IG16		0x45	/* n.i.*/
#define DT_XFER_TRP16		0x46	/* n.i.*/
#define DT_XFER_TSS32		0x47
#define DT_XFER_CG32		0x48
#define DT_XFER_IG32		0x49
#define DT_XFER_TRP32		0x4a

/* Segment types */
				/* 0x00F2 */
#define ST_DATA16	DF_PRESENT|DF_DPL|DF_USER|DF_DATA|DF_DWRITEABLE
				/* 0x00FA */
#define ST_CODE16	DF_PRESENT|DF_DPL|DF_USER|DF_CODE|DF_CREADABLE
				/* 0x00F6 */
#define ST_STACK16	DF_PRESENT|DF_DPL|DF_USER|DF_DATA|DF_EXPANDDOWN| \
			DF_DWRITEABLE
				/* 0x80F2 */
#define ST_DATA32	DF_32|DF_PRESENT|DF_DPL|DF_USER|DF_DATA|DF_DWRITEABLE
				/* 0x80FA */
#define ST_CODE32	DF_32|DF_PRESENT|DF_DPL|DF_USER|DF_CODE|DF_CREADABLE
				/* 0x80F6 */
#define ST_STACK32	DF_32|DF_PRESENT|DF_DPL|DF_USER|DF_DATA|DF_EXPANDDOWN| \
			DF_DWRITEABLE

#define IS_GDT(sel)	(((sel)&4)==0)
#define IS_LDT(sel)	(((sel)&4)!=0)
#define LDTorGDT(sel)	(IS_LDT(sel)? LDT:GDT)

/* Segment limit -- 64K */
#define SL_DEFAULT	(LPARAM)0xFFFF

/* Macros to access fields in LGDT tables	*/

#define DTgetSelBase(w)		({Descriptor *t=LDTorGDT(w); \
				  (t[w>>3].present? DT_BASE(&t[(w)>>3]):0); })
#define DTsetSelBase(w,d)	({Descriptor *t=LDTorGDT(w); MKBASE(&t[(w)>>3],d); })
#define DTgetSelLimit(w)	({Descriptor *t=LDTorGDT(w); DT_LIMIT(&t[(w)>>3]); })
#define DTsetSelLimit(w,d)	({Descriptor *t=LDTorGDT(w); MKLIMIT(&t[(w)>>3],d); })
#define DTgetFlags(w)		({Descriptor *t=LDTorGDT(w); DT_FLAGS(&t[(w)>>3]); })
#define GetFlagAccessed(w)	({Descriptor *t=LDTorGDT(w); t[(w)>>3].type&1; })
#define SetFlagAccessed(w)	({Descriptor *t=LDTorGDT(w); t[(w)>>3].type|=1; })

#define GetSelectorAddress(w)	(unsigned char *)(REALADDR()? (w<<4):DTgetSelBase(w))
#define	GetPhysicalAddress(w)	GetSelectorAddress(w)

#define SetPhysicalAddress(w,l) {if (PROTMODE()) DTsetSelBase(w,(unsigned long)l);}

#define GetSelectorLimit(w)	(REALADDR()? 0xffff:DTgetSelLimit(w))

#define GetSelectorByteLimit(w)	({Descriptor *t=LDTorGDT(w); \
				  (REALADDR()? 0xffff:\
				   (t[(w)>>3].gran?\
				    (DT_LIMIT(&t[(w)>>3])<<12)|0xfff :\
				    DT_LIMIT(&t[(w)>>3]))); })
#define GetSelectorAddrMax(w)	(GetSelectorAddress(w)+GetSelectorByteLimit(w))

#define SetSelectorLimit(w,d)	{if (PROTMODE()) DTsetSelLimit(w,(unsigned long)d);}

#define GetSelectorFlags(w)	(REALADDR()? 0x00f0:DTgetFlags(w))

#define GetSelectorType(w)	({Descriptor *t=LDTorGDT(w); \
				  (REALADDR()? 0:t[(w)>>3].type); })

#define	CopySelector(t,w1,w2)	memcpy((char *)&t[w1>>3], \
					(char *)&t[w2>>3], \
					sizeof(Descriptor));

/* Messages for DPMI_Notify() */
#if 0
/* unused? - Bart DN_MODIFY is also in <fcntl.h> */
#define DN_ASSIGN	1
#define DN_FREE		2
#define DN_INIT		3
#define DN_MODIFY	4
#define DN_EXIT		5
#endif

#define SELECTOR_PADDRESS(sel) GetPhysicalAddress(sel)
//
extern signed char e_ofsseg[];
//
int SetSegProt(int a16, int ofs, unsigned char *big, unsigned long sel);
int SetSegReal(unsigned short sel, int ofs);
int e_larlsl(int mode, unsigned short sel);
int hsw_verr(unsigned short sel);
int hsw_verw(unsigned short sel);
//

#endif
