/* NOTE:
 * This file was modified for DOSEMU by the DOSEMU-team.
 * The original is 'Copyright 1997 Willows Software, Inc.' and generously
 * was put under the GNU Library General Public License.
 * ( for more information see http://www.willows.com/ )
 *
 * We make use of section 3 of the GNU Library General Public License
 * ('...opt to apply the terms of the ordinary GNU General Public License...'),
 * because the resulting product is an integrated part of DOSEMU and
 * can not be considered to be a 'library' in the terms of Library License.
 * The (below) original copyright notice from Willows therefore was edited
 * conforming to section 3 of the GNU Library General Public License.
 *
 * Nov. 1 1997, The DOSEMU team.
 */


/*    
	fp87.c	1.8
    	Copyright 1997 Willows Software, Inc. 

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; see the file COPYING.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.


For more information about the Willows Twin Libraries.

	http://www.willows.com	

To send email to the maintainer of the Willows Twin Libraries.

	mailto:twin@willows.com 

changes for use with dosemu-0.67 1997/10/20 vignani@tin.it
changes for use with dosemu-0.99 1998/12/13 vignani@tin.it
 */

#include "emu-globv.h"
#include "hsw_interp.h"
#include "mod_rm.h"
#include <math.h>

#define DPLOG10_2 log10(2.0)
#define DPLOGE_2 0.69314718055994530942  /*  log(2.0)  */
#define DPPI 3.14159265358979323846      /* 3.141592654 */
#define DPLOG2_E 1.4426950408889634074  /* 1.0/log(2.0) */
#define DPLOG2_10 log(10.0)/log(2.0)
#define MAXTAN pow(2.0,63.0)

#ifdef __i386__
#define DBLCOPY(de,sr)	{*((QWORD *)(de))=*((QWORD *)(sr));}

#ifdef __GNUC__
static inline void TDBLCPY(const void *de, const void *sr)
{
	__asm__ ("
		movl	(%0),%%eax
		movl	%%eax,(%1)
		movl	4(%0),%%eax
		movl	%%eax,4(%1)
		movw	8(%0),%%ax
		movw	%%ax,8(%1)"
		:
		: "r"((char *)sr), "r"((char *)de)
		: "%eax","memory");
}

static inline int L_ISZERO(const void *sr)
{
	int _res;
	__asm__ ("
		movw	8(%1),%w0
		shll	$17,%0
		orl	(%1),%0
		orl	4(%1),%0"
		: "=&r"(_res)
		: "r"((char *)sr)
		: "memory");
	return _res;
}
#else
#define TDBLCPY(de,sr)	{*((QWORD *)(de))=*((QWORD *)(sr));\
				*((WORD *)(((char *)de)+8))=*((WORD *)(((char *)sr)+8));}

static __inline int L_ISZERO(const void *sr)
{
	unsigned int *wp = (unsigned int *)sr;
	return ((wp[0] | wp[1] | (wp[2]&0xffff))==0);
}
#endif

#define STORE32INT(addr,i32)	*((long *)(addr)) = (long)i32
#define STORE16INT(addr,i16)	*((short *)(addr)) = (short)i16
#define STORE64INT(addr,i64)	*((QWORD *)(addr)) = (QWORD)i64
#define GET32INT(addr)		(*((unsigned long *)(addr)))
#define GET16INT(addr)		(*((unsigned short *)(addr)))
#define GET64INT(addr)		(*((QLONG *)(addr)))
#define GET64REAL(dtmp,addr)	(*(QWORD *)&dtmp=*(QWORD *)(addr), dtmp)
#define STORE64REAL(addr,dtmp)	(*(QWORD *)(addr)=*(QWORD *)&dtmp)
#define GET32REAL(dtmp,addr)	(*(long *)&dtmp=*(long *)(addr), dtmp)
#define STORE32REAL(addr,dtmp)	(*(long *)(addr)=*(long *)&dtmp)
#define EXPONENT64(addr)	((((unsigned short *)(addr))[3]&0x7ff0)>>4)
#define EXPONENT80(addr)	(((unsigned short *)(addr))[4]&0x7fff)
#define BIASEXPONENT64(addr)	(((BYTE*)(addr))[7] &= (~0x40),  \
				 ((BYTE*)(addr))[7] |= 0x3f,  \
				 ((BYTE*)(addr))[6] |= 0xf0)
#define BIASEXPONENT80(addr)	(((BYTE*)(addr))[9] &= (~0x40),  \
				 ((BYTE*)(addr))[9] |= 0x3f,  \
				 ((BYTE*)(addr))[8] |= 0xff)
#define LD80R(fpreg,r80)	TDBLCPY(fpreg,r80)
#define ST80R(r80,fpreg)	TDBLCPY(r80,fpreg)

#else
#define STORE32INT(addr,i32) ( (addr)[0] = (i32), (addr)[1] = (i32)>>8,  \
                               (addr)[2] = (i32)>>16, (addr)[3] = (i32)>>24 )
#define STORE16INT(addr,i16) ( (addr)[0] = (i16), (addr)[1] = (i16)>>8 )
#define GET32INT(addr) ((int) ( (((DWORD)(addr)[3])<<24) | (((DWORD)(addr)[2])<<16) |  \
					   (((DWORD)(addr)[1])<<8) | (addr)[0]) )
#define GET16INT(addr) ((signed short) ((((WORD)(addr)[1])<<8) | (addr)[0]))
/* the following are given here in Big Endian (target) version only */
#define GET64REAL(dtmp,addr) (  \
		   ((BYTE*)&dtmp)[7] = (addr)[0], ((BYTE*)&dtmp)[6] = (addr)[1], \
		   ((BYTE*)&dtmp)[5] = (addr)[2], ((BYTE*)&dtmp)[4] = (addr)[3], \
		   ((BYTE*)&dtmp)[3] = (addr)[4], ((BYTE*)&dtmp)[2] = (addr)[5], \
		   ((BYTE*)&dtmp)[1] = (addr)[6], ((BYTE*)&dtmp)[0] = (addr)[7], \
		   dtmp )
#define STORE64REAL(addr,dtmp) (  \
		   (addr)[0] = ((BYTE*)&dtmp)[7], (addr)[1] = ((BYTE*)&dtmp)[6], \
		   (addr)[2] = ((BYTE*)&dtmp)[5], (addr)[3] = ((BYTE*)&dtmp)[4], \
		   (addr)[4] = ((BYTE*)&dtmp)[3], (addr)[5] = ((BYTE*)&dtmp)[2], \
		   (addr)[6] = ((BYTE*)&dtmp)[1], (addr)[7] = ((BYTE*)&dtmp)[0]  )
#define GET32REAL(ftmp,addr) (  \
		   ((BYTE*)&ftmp)[3] = (addr)[0], ((BYTE*)&ftmp)[2] = (addr)[1], \
		   ((BYTE*)&ftmp)[1] = (addr)[2], ((BYTE*)&ftmp)[0] = (addr)[3], \
		   ftmp )
#define STORE32REAL(addr,ftmp) (  \
		   (addr)[0] = ((BYTE*)&ftmp)[3], (addr)[1] = ((BYTE*)&ftmp)[2], \
		   (addr)[2] = ((BYTE*)&ftmp)[1], (addr)[3] = ((BYTE*)&ftmp)[0] )
#define EXPONENT64(addr) ( ( (((BYTE*)addr)[0] & 0x7f) << 4 ) |  \
						 ( (((BYTE*)addr)[1] & 0xf0) >> 4 )     )
#define BIASEXPONENT64(addr) ( ((BYTE*)addr)[0] &= (~0x40),  \
                             ((BYTE*)addr)[0] |= 0x3f,  \
                             ((BYTE*)addr)[1] |= 0xf0 )
#define LD80R(fpreg,r80)  {  \
if(!(*(r80+9)|*(r80+8)|*(r80+7)|*(r80+6)|*(r80+5)|*(r80+4)|*(r80+3)|*(r80+2)|\
*(r80+1)|*(r80))) *fpreg=0.0; else {\
	i16 = *(r80+9); i16<<=8; i16|= *(r80+8);  expdif= (i16&0x7fff) - 16383; \
        expdif += 1023 /*add DOUBLE bias*/;    \
        if ( i16 & 0x8000 )  expdif |= 0x800; \
        *((BYTE*)fpreg)     = (int)expdif >> 4;         /* <-- Se4..e10 */   \
        *(((BYTE*)fpreg)+1) = (expdif&0xf)<<4;         /*  <-- e0..e3 */    \
        *(((BYTE*)fpreg)+1) |= (((*(r80+7))<<1) >> 4)&0xf; /*  <-- m59..m62 */  \
        *(((BYTE*)fpreg)+2)  = ((*(r80+7)) << 5);    /*  <-- m56..m58 */  \
        *(((BYTE*)fpreg)+2) |= ((*(r80+6)) >> 3);    /*  <-- m51..m55 */  \
        *(((BYTE*)fpreg)+3)  = ((*(r80+6)) << 5);    /*  <-- m48..m50 */  \
        *(((BYTE*)fpreg)+3) |= ((*(r80+5)) >> 3);    /*  <-- m43..m47 */  \
        *(((BYTE*)fpreg)+4)  = ((*(r80+5)) << 5);    /*  <-- m40..m42 */  \
        *(((BYTE*)fpreg)+4) |= ((*(r80+4)) >> 3);    /*  <-- m35..m39 */  \
        *(((BYTE*)fpreg)+5)  = ((*(r80+4)) << 5);    /*  <-- m32..m34 */  \
        *(((BYTE*)fpreg)+5) |= ((*(r80+3)) >> 3);    /*  <-- m27..m31 */  \
        *(((BYTE*)fpreg)+6)  = ((*(r80+3)) << 5);    /*  <-- m24..m26 */  \
        *(((BYTE*)fpreg)+6) |= ((*(r80+2)) >> 3);    /*  <-- m19..m23 */  \
        *(((BYTE*)fpreg)+7)  = ((*(r80+2)) << 5);    /*  <-- m16..m18 */  \
        *(((BYTE*)fpreg)+7) |= ((*(r80+1)) >> 3);    /*  <-- m11..m15 */  \
        }}
#define ST80R(r80,fpreg)  {  \
        i16 = *((unsigned short *)fpreg);  \
        expdif = ((i16&0x7fff) >> 4) - 1023;  /* get un-biased exponent */  \
        expdif += 16383;  /* add EXT real bias */ \
        if ( i16 & 0x8000 )  expdif |= 0x8000;    \
        *(r80+9) = expdif >> 8;                   \
        *(r80+8) = expdif;                        \
        *(r80+7) = ((i16&0xf)|0x10) << 3;         \
        *(r80+7) |= *(((BYTE*)fpreg)+2) >> 5;     \
        *(r80+6)  = *(((BYTE*)fpreg)+2) << 3;     \
        *(r80+6) |= *(((BYTE*)fpreg)+3) >> 5;     \
        *(r80+5)  = *(((BYTE*)fpreg)+3) << 3;     \
        *(r80+5) |= *(((BYTE*)fpreg)+4) >> 5;     \
        *(r80+4)  = *(((BYTE*)fpreg)+4) << 3;     \
        *(r80+4) |= *(((BYTE*)fpreg)+5) >> 5;     \
        *(r80+3)  = *(((BYTE*)fpreg)+5) << 3;     \
        *(r80+3) |= *(((BYTE*)fpreg)+6) >> 5;     \
        *(r80+2)  = *(((BYTE*)fpreg)+6) << 3;     \
        *(r80+2) |= *(((BYTE*)fpreg)+7) >> 5;     \
        *(r80+1)  = *(((BYTE*)fpreg)+7) << 3;     \
        *(r80)   = 0x0;                           \
		}
#endif	/* DOSEMU on i386 */

#define MUL10(iv) ( iv + iv + (iv << 3) )

#define F_ST		hsw_env87.fpstt

#define FPR_ST0		hsw_env87.fpregs[F_ST]
#define FPR_ST1		hsw_env87.fpregs[(F_ST+1)&0x7]
#define FPR_ST(r)	hsw_env87.fpregs[(F_ST+(r))&0x7]
#define FPIRQMASK	(hsw_env87.fpuc&0x3f)

#ifdef __GNUC__
#define FP_INVFLG	({char v=(0x01 & FPIRQMASK); (v? v|0x80:v);})
#define FP_DENORMFLG	({char v=(0x02 & FPIRQMASK); (v? v|0x80:v);})
#define FP_ZEROFLG	({char v=(0x04 & FPIRQMASK); (v? v|0x80:v);})
#define FP_OVFLFLG	({char v=(0x08 & FPIRQMASK); (v? v|0x80:v);})
#define FP_UNDFLFLG	({char v=(0x10 & FPIRQMASK); (v? v|0x80:v);})
#define FP_PRECFLG	({char v=(0x20 & FPIRQMASK); (v? v|0x80:v);})
#else
#define FP_INVFLG	((FPIRQMASK&0x01)? (FPIRQMASK&0x01)|0x80:(FPIRQMASK&0x01))
#define FP_DENORMFLG	((FPIRQMASK&0x02)? (FPIRQMASK&0x02)|0x80:(FPIRQMASK&0x02))
#define FP_ZEROFLG	((FPIRQMASK&0x04)? (FPIRQMASK&0x04)|0x80:(FPIRQMASK&0x04))
#define FP_OVFLFLG	((FPIRQMASK&0x08)? (FPIRQMASK&0x08)|0x80:(FPIRQMASK&0x08))
#define FP_UNDFLFLG	((FPIRQMASK&0x10)? (FPIRQMASK&0x10)|0x80:(FPIRQMASK&0x10))
#define FP_PRECFLG	((FPIRQMASK&0x20)? (FPIRQMASK&0x20)|0x80:(FPIRQMASK&0x20))
#endif

#define FPUC_RC		(hsw_env87.fpuc&0xc00)
#define RC_NEAR		0x000
#define RC_DOWN		0x400
#define RC_UP		0x800
#define RC_CHOP		0xc00

#define FPUC_PC		(hsw_env87.fpuc&0x300)
#define PC_24		0x000
#define PC_53		0x200
#define PC_64		0x300

static unsigned short TagMask[8] =
{
	0x0003, 0x000c, 0x0030, 0x00c0,
	0x0300, 0x0c00, 0x3000, 0xc000
};

#define VALID_ST0	hsw_env87.fptag &= (~TagMask[F_ST])
#define EMPTY_ST0	hsw_env87.fptag |= TagMask[F_ST]
#define VALID_ST(r)	hsw_env87.fptag &= (~TagMask[(F_ST+(r))&0x7])
#define EMPTY_ST(r)	hsw_env87.fptag |= TagMask[(F_ST+(r))&0x7]

#define POPFSP		EMPTY_ST0; F_ST=(F_ST+1)&0x7
#define PUSHFSP		F_ST=(F_ST-1)&0x7


ENV87 hsw_env87;
extern Interp_ENV *envp_global;


static void illegal_op (char *s)
{
	dbug_printf("illegal FP opcode: %s\n",s);
	FatalAppExit(0, "FP");
	exit(1);
}

static void not_implemented (char *s)
{
	dbug_printf("unimplemented FP opcode: %s\n",s);
	FatalAppExit(0, "FP");
	exit(1);
}

void init_npu (void)
{
	hsw_env87.fpus = 0;
	hsw_env87.fpstt = 0;
	hsw_env87.fpuc = 0x37f;
	hsw_env87.fptag = 0xffff;	/* empty */
}

/*
 * These functions are coded as:
 *	ESCi mod/rm {addr}
 * ESC    = d8..df  i=0..7  ->hsw_fp87_0**..hsw_fp87_7**
 * mod>>3 = 00..07  n=0..7  ->hsw_fp87_*0*..hsw_fp87_*7*
 * mod>>6 =         n=0..3  ->hsw_fp87_**m..hsw_fp87_**r
 * rm -> reg
 *
 * Intel notation: ex. d8/4 = 04m
 */

void
hsw_fp87_00m(unsigned char *mem_ref)
{
/* FADDm32r_sti */
	float m32r;
	FPR_ST0 += GET32REAL(m32r, mem_ref);
}

void
hsw_fp87_00r(int reg_num)
{
/* FADDm32r_sti */
	FPR_ST0 += FPR_ST(reg_num);
}

void
hsw_fp87_01m(unsigned char *mem_ref)
{
/* FMULm32r_sti */
	float m32r;
	FPR_ST0 *= GET32REAL(m32r, mem_ref);
}

void
hsw_fp87_01r(int reg_num)
{
/* FMULm32r_sti */
	FPR_ST0 *= FPR_ST(reg_num);
}

void
hsw_fp87_02m(unsigned char *mem_ref)
{
/* FCOMm32r_sti */
	Ldouble fpsrcop;
	float m32r;

	fpsrcop = GET32REAL(m32r,mem_ref);
	hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
	if ( FPR_ST0 < fpsrcop )
	    hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
	else if ( FPR_ST0 == fpsrcop )
	    hsw_env87.fpus |= 0x4000; /* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_02r(int reg_num)
{
/* FCOMm32r_sti */
	Ldouble fpsrcop;
	TDBLCPY(&fpsrcop,&FPR_ST(reg_num));
	hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
	if ( FPR_ST0 < fpsrcop )
	    hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
	else if ( FPR_ST0 == fpsrcop )
	    hsw_env87.fpus |= 0x4000; /* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_03m(unsigned char *mem_ref)
{
/* FCOMPm32r_sti */
	Ldouble fpsrcop;
	float m32r;

	fpsrcop = GET32REAL(m32r, mem_ref);
	hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
	if ( FPR_ST0 < fpsrcop )
	    hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
	else if ( FPR_ST0 == fpsrcop )
	    hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
	POPFSP;
}

void
hsw_fp87_03r(int reg_num)
{
/* FCOMPm32r_sti */
	Ldouble fpsrcop;
	TDBLCPY(&fpsrcop,&FPR_ST(reg_num));
	hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
	if ( FPR_ST0 < fpsrcop )
	    hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
	else if ( FPR_ST0 == fpsrcop )
	    hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
	POPFSP;
}

void
hsw_fp87_04m(unsigned char *mem_ref)
{
/* FSUBm32r_sti */
	float m32r;

	FPR_ST0 -= GET32REAL(m32r, mem_ref);	/* dest - src -> dest */
}

void
hsw_fp87_04r(int reg_num)
{
/* FSUBm32r_sti */
	FPR_ST0 -= FPR_ST(reg_num);	/* dest - src -> dest */
}

void
hsw_fp87_05m(unsigned char *mem_ref)
{
/* FSUBRm32r_sti */
	Ldouble fpsrcop;
	float m32r;
	fpsrcop = GET32REAL(m32r, mem_ref);
	FPR_ST0 = fpsrcop - FPR_ST0;	/* src - dest -> dest */
}

void
hsw_fp87_05r(int reg_num)
{
/* FSUBRm32r_sti */
	FPR_ST0 = FPR_ST(reg_num) - FPR_ST0;	/* src - dest -> dest */
}

void
hsw_fp87_06m(unsigned char *mem_ref)
{
/* FDIVm32r_sti */
	Ldouble fpsrcop;
	float m32r;
	fpsrcop = GET32REAL(m32r, mem_ref);
	FPR_ST0 /= fpsrcop;
	if (!L_ISZERO(&fpsrcop)) { hsw_env87.fpus |= FP_ZEROFLG; }
}

void
hsw_fp87_06r(int reg_num)
{
/* FDIVm32r_sti */
	Ldouble fpsrcop;
	TDBLCPY(&fpsrcop,&FPR_ST(reg_num));
	FPR_ST0 /= fpsrcop;
	if (!L_ISZERO(&fpsrcop)) { hsw_env87.fpus |= FP_ZEROFLG; }
}

void
hsw_fp87_07m(unsigned char *mem_ref)
{
/* FDIVRm32r_sti */
	Ldouble fpsrcop;
	float m32r;
	fpsrcop = GET32REAL(m32r, mem_ref);
	if (!L_ISZERO(&FPR_ST0)) { hsw_env87.fpus |= FP_ZEROFLG; }
	FPR_ST0 = fpsrcop / FPR_ST0;
}

void
hsw_fp87_07r(int reg_num)
{
/* FDIVRm32r_sti */
	Ldouble fpsrcop;
	TDBLCPY(&fpsrcop,&FPR_ST(reg_num));
	if (!L_ISZERO(&FPR_ST0)) { hsw_env87.fpus |= FP_ZEROFLG; }
	FPR_ST0 = fpsrcop / FPR_ST0;
}

void
hsw_fp87_10m(unsigned char *mem_ref)
{
/* FLDm32r_sti */
	Ldouble fpsrcop;
	float m32r;

	fpsrcop = GET32REAL(m32r, mem_ref);
	PUSHFSP;
	TDBLCPY(&FPR_ST0, &fpsrcop);
	VALID_ST0;
}

void
hsw_fp87_10r(int reg_num)
{
/* FLDm32r_sti */
	Ldouble fpsrcop;
	TDBLCPY(&fpsrcop,&FPR_ST(reg_num));
	PUSHFSP;
	TDBLCPY(&FPR_ST0, &fpsrcop);
	VALID_ST0;
}

void
hsw_fp87_11m(unsigned char *mem_ref)
{
	illegal_op("011m");
}

void
hsw_fp87_11r(int reg_num)
{
/* FXCH */
	Ldouble fptemp;
	TDBLCPY(&fptemp, &FPR_ST(reg_num));
	TDBLCPY(&FPR_ST(reg_num), &FPR_ST0);
	TDBLCPY(&FPR_ST0, &fptemp);
}

void
hsw_fp87_12m(unsigned char *mem_ref)
{
/* FSTm32r_FNOP */
	float m32r;
	m32r = (float)FPR_ST0;
	STORE32REAL(mem_ref,m32r);
}

void
hsw_fp87_12r(int reg_num)
{
/* FSTm32r_FNOP */
/* FNOP do nothing */
	if (reg_num>0)
	    illegal_op("FPNOP r>0");
}

void
hsw_fp87_13m(unsigned char *mem_ref)
{
/* FSTPm32r */
	float m32r;
	m32r = (float)FPR_ST0;
	STORE32REAL(mem_ref,m32r);
	POPFSP;
}

void
hsw_fp87_13r(int reg_num)
{
	illegal_op("013r");
}

void
hsw_fp87_14m(unsigned char *mem_ref)
{
/* FLDENV */
	if (data32) {
	    hsw_env87.fpuc = GET16INT(mem_ref);
	    hsw_env87.fpus = GET16INT(mem_ref+4);
	    hsw_env87.fptag = GET16INT(mem_ref+8);
	    /* IP,OP,opcode: 8 long n.i. */
	}
	else {
	    hsw_env87.fpuc = GET16INT(mem_ref);
	    hsw_env87.fpus = GET16INT(mem_ref+2);
	    hsw_env87.fptag = GET16INT(mem_ref+4);
	    /* IP,OP,opcode: 8 word n.i. */
	}
	hsw_env87.fpstt = (hsw_env87.fpus>>11)&7;
}

void
hsw_fp87_14r(int reg_num)
{
	unsigned int expdif;

	switch (reg_num) {
		case 0: /* FCHS */
			((short *)&FPR_ST0)[4] ^= 0x8000;
			return;
		case 1: /* FABS */
			((short *)&FPR_ST0)[4] &= 0x7fff;
			return;
		case 2:
			illegal_op("014r r=2"); return;
		case 3:
			illegal_op("014r r=3"); return;
		case 4: /* FTST */ {
			Ldouble fpsrcop = 0.0;
			hsw_env87.fpus &= (~0x4500);   /* (C3,C2,C0) <-- 000 */
			if ( FPR_ST0 < fpsrcop )
				hsw_env87.fpus |= 0x100;   /* (C3,C2,C0) <-- 001 */
			else if ( FPR_ST0 == fpsrcop )
				hsw_env87.fpus |= 0x4000;  /* (C3,C2,C0) <-- 100 */
			/* else if ( FPR_ST0 > fpsrcop )  do nothing */
			/* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
			} return;
		case 5: /* FXAM */ {
			QLONG i64lh;
			double fptemp;
			hsw_env87.fpus &= (~0x4700);  /* (C3,C2,C1,C0) <-- 0000 */
			if ( FPR_ST0 < 0.0 )
			    hsw_env87.fpus |= 0x200; /* C1 <-- 1 */
			fptemp = FPR_ST0;
			expdif = EXPONENT64(&fptemp);
			if ( expdif == 0x7ff ) {
#ifdef __GNUC__
				i64lh = *((QLONG *)&fptemp) & 0xfffffffffffffLL;
#else
				i64lh = *((QLONG *)&fptemp) & 0xfffffffffffff;
#endif
			    hsw_env87.fpus |= (i64lh == 0?
				0x500 /*Infinity*/: 0x100 /*NaN*/);
			}
			else if ( expdif == 0x0 ) {
#ifdef __GNUC__
			    i64lh = *((QLONG *)&fptemp) & 0xfffffffffffffLL;
#else
				i64lh = *((QLONG *)&fptemp) & 0xfffffffffffff;
#endif
			    hsw_env87.fpus |= (i64lh == 0?
				0x4000 /*Zero*/: 0x4400 /*Denormal*/);
			}
			else hsw_env87.fpus |= 0x400;
			} return;
		case 6:
			illegal_op("014r r=6"); return;
		case 7:
			illegal_op("014r r=7"); return;
	}
}

void
hsw_fp87_15m(unsigned char *mem_ref)
{
/* FLDCW */
	hsw_env87.fpuc = GET16INT(mem_ref);
	/* NOT FULLY IMPLEMENTED YET!!! If an exception bit in the
	   status word is set... see manuals */
	if (FPUC_PC != PC_64) {
	    e_printf("FPEMU: set precision %x not supported!\n",(FPUC_PC>>8));
	}
}

static const unsigned short f15rk[] =
{
/*0*/	0x0000,0x0000,0x0000,0x0000,0x0000,
/*1*/	0x0000,0x0000,0x0000,0x8000,0x3fff,
/*pi*/	0xc235,0x2168,0xdaa2,0xc90f,0x4000,
/*lg2*/	0xf799,0xfbcf,0x9a84,0x9a20,0x3ffd,
/*ln2*/	0x79ac,0xd1cf,0x17f7,0xb172,0x3ffe,
/*l2e*/	0xf0bc,0x5c17,0x3b29,0xb8aa,0x3fff,
/*l2t*/	0x8afe,0xcd1b,0x784b,0xd49a,0x4000
};

void
hsw_fp87_15r(int reg_num)
{
/* FLDCONST */
	PUSHFSP;
	VALID_ST0;
	switch (reg_num) {
		case 0: /* FLD1 */
			TDBLCPY(&FPR_ST0, &f15rk[5]);
			return;
		case 1: /* FLDL2T */
			TDBLCPY(&FPR_ST0, &f15rk[30]);
			return;
		case 2: /* FLDL2E */
			TDBLCPY(&FPR_ST0, &f15rk[25]);
			return;
		case 3: /* FLDPI */
			TDBLCPY(&FPR_ST0, &f15rk[10]);
			return;
		case 4: /* FLDLG2 */
			TDBLCPY(&FPR_ST0, &f15rk[15]);
			return;
		case 5: /* FLDLN2 */
			TDBLCPY(&FPR_ST0, &f15rk[20]);
			return;
		case 6: /* FLDZ */
			TDBLCPY(&FPR_ST0, &f15rk[0]);
			return;
		case 7:
			EMPTY_ST0;
			illegal_op("015r r=7"); return;
	}
}

void
hsw_fp87_16m(unsigned char *mem_ref)
{
	int i, fptag, ntag;
/* FSTENV */
	hsw_env87.fpus &= (~0x3800);	/* set TOP field to 0 */
	hsw_env87.fpus |= (hsw_env87.fpstt&0x7)<<11;
	fptag = hsw_env87.fptag; ntag=0;
	for (i=7; i>=0; --i) {
	    unsigned short *sf = (unsigned short *)&(hsw_env87.fpregs[i]);
	    ntag <<= 2;
	    if ((fptag & 0xc000) == 0xc000) ntag |= 3;
	    else {
		if (!L_ISZERO(sf)) {
		    ntag |= 1;
		}
		else {
		    unsigned short sf2 = sf[4]&0x7fff;
		    if ((sf2==0)||(sf2==0x7fff)||((sf[3]&0x8000)==0)) {
			ntag |= 2;
		    }		/* else tag=00 valid */
		}
	    }
	    fptag <<= 2;
	}
	hsw_env87.fptag = ntag;
	if (data32) {
	    STORE32INT((mem_ref), hsw_env87.fpuc);
	    STORE32INT((mem_ref+4), hsw_env87.fpus);
	    STORE32INT((mem_ref+8), hsw_env87.fptag);
	    *((long *)(mem_ref+12)) = 0;
	    *((long *)(mem_ref+16)) = 0;
	    *((long *)(mem_ref+20)) = 0;
	    *((long *)(mem_ref+24)) = 0; /* IP,OP,opcode: n.i. */
	}
	else {
	    STORE16INT((mem_ref), hsw_env87.fpuc);
	    STORE16INT((mem_ref+2), hsw_env87.fpus);
	    STORE16INT((mem_ref+4), hsw_env87.fptag);
	    *((long *)(mem_ref+6)) = 0;
	    *((long *)(mem_ref+10)) = 0; /* IP,OP,opcode: n.i. */
	}
	hsw_env87.fpuc |= 0x3f;	/* set exception masks */
}

void
hsw_fp87_16r(int reg_num)
{
	Ldouble fptemp;
	Ldouble fpsrcop;
	switch (reg_num) {
		case 0: /* F2XM1 */
			FPR_ST0 = pow(2.0,FPR_ST0) - 1.0;
			return;
		case 1: /* FYL2X */
			fptemp = FPR_ST0;
			if(fptemp>0.0){
			fptemp = log(fptemp)/log(2.0);	 /* log2(ST) */
			FPR_ST1 *= fptemp;
			POPFSP;}
			else { hsw_env87.fpus &= (~0x4700);
				hsw_env87.fpus |= 0x400;
			}
			return;
		case 2: /* FPTAN */
			fptemp = FPR_ST0;
			if((fptemp > MAXTAN)||(fptemp < -MAXTAN))  hsw_env87.fpus |= 0x400;
			else {
			FPR_ST0 = tan(fptemp);
			PUSHFSP;
			FPR_ST0 = 1.0;
			VALID_ST0;
			hsw_env87.fpus &= (~0x400);  /* C2 <-- 0 */
			/* the above code is for  |arg| < 2**52 only */
			}
			return;
		case 3: /* FPATAN */
			fpsrcop = FPR_ST1;
			fptemp = FPR_ST0;
			FPR_ST1 = atan2(fpsrcop,fptemp);
			POPFSP;
			return;
		case 4: /* FXTRACT */ {
			unsigned int expdif;
			fpsrcop = FPR_ST0;
			expdif = EXPONENT80(&FPR_ST0) - 16383;
					/*DP exponent bias*/
			FPR_ST0 = expdif;
			PUSHFSP;
			BIASEXPONENT80(&fpsrcop);
			FPR_ST0 = fpsrcop;
			VALID_ST0;
			}
			return;
		case 5: /* FPREM1 */ {
			Ldouble dblq;
			int expdif;
			int q;

			fpsrcop = FPR_ST0;
			fptemp = FPR_ST1;
			expdif = EXPONENT80(&fpsrcop) - EXPONENT80(&fptemp);
			if ( expdif < 53 ) {
				dblq = fpsrcop / fptemp;
				dblq = (dblq < 0.0)? ceil(dblq): floor(dblq);
				FPR_ST0 = fpsrcop - fptemp*dblq;
				q = (int)dblq; /* cutting off top bits is assumed here */
				hsw_env87.fpus &= (~0x4700); /* (C3,C2,C1,C0) <-- 0000 */
				/* (C0,C1,C3) <-- (q2,q1,q0) */
				hsw_env87.fpus |= (q&0x4) << 6; /* (C0) <-- q2 */
				hsw_env87.fpus |= (q&0x2) << 8; /* (C1) <-- q1 */
				hsw_env87.fpus |= (q&0x1) << 14; /* (C3) <-- q0 */
			}
			else {
				hsw_env87.fpus |= 0x400;  /* C2 <-- 1 */
				fptemp = pow(2.0, ((Ldouble)(expdif-50)) );
				fpsrcop = (FPR_ST0 / FPR_ST1) / fptemp;
				/* fpsrcop = integer obtained by rounding to the nearest */
				fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
							floor(fpsrcop): ceil(fpsrcop);
				FPR_ST0 -= (FPR_ST1 * fpsrcop * fptemp);
			}
			}
			return;
		case 6: /* FDECSTP */
			PUSHFSP;
			hsw_env87.fpus &= (~0x4700);
			return;
		case 7: /* FINCSTP */
			hsw_env87.fpstt++; hsw_env87.fpstt &= 0x7;
			hsw_env87.fpus &= (~0x4700);
			return;
	}
}

void
hsw_fp87_17m(unsigned char *mem_ref)
{
/* FSTCW */
	STORE16INT(mem_ref,hsw_env87.fpuc);
}

void
hsw_fp87_17r(int reg_num)
{
/* FD9SLASH7 */
	Ldouble fptemp;
	Ldouble fpsrcop;
	switch (reg_num) {
		case 0: /* FPREM */ {
			Ldouble dblq;
			int expdif;
			int q;

			TDBLCPY(&fpsrcop,&FPR_ST0);
			TDBLCPY(&fptemp,&FPR_ST1);
			expdif = EXPONENT80(&fpsrcop) - EXPONENT80(&fptemp);
			if ( expdif < 53 ) {
				dblq = fpsrcop / fptemp;
				dblq = (dblq < 0.0)? ceil(dblq): floor(dblq);
				FPR_ST0 = fpsrcop - fptemp*dblq;
				q = (int)dblq; /* cutting off top bits is assumed here */
				hsw_env87.fpus &= (~0x4700); /* (C3,C2,C1,C0) <-- 0000 */
				/* (C0,C1,C3) <-- (q2,q1,q0) */
				hsw_env87.fpus |= (q&0x4) << 6; /* (C0) <-- q2 */
				hsw_env87.fpus |= (q&0x2) << 8; /* (C1) <-- q1 */
				hsw_env87.fpus |= (q&0x1) << 14; /* (C3) <-- q0 */
			}
			else {
				hsw_env87.fpus |= 0x400;  /* C2 <-- 1 */
	   	           	fptemp = pow(2.0, ((Ldouble)(expdif-50)) );
   		           	fpsrcop = (FPR_ST0 / FPR_ST1) / fptemp;
		                /* fpsrcop = integer obtained by chopping */
   	        		fpsrcop = (fpsrcop < 0.0)?
					-(floor(fabs(fpsrcop))): floor(fpsrcop);
				FPR_ST0 -= (FPR_ST1 * fpsrcop * fptemp);
			}
			}
	        	return;
		case 1: /* FYL2XP1 */
			TDBLCPY(&fptemp,&FPR_ST0);
			if((fptemp+1.0)>0.0){
			fptemp = log(fptemp+1.0) /
				log(2.0); /* log2(ST+1.0) */
			FPR_ST1 *= fptemp;
			POPFSP;}
			else { hsw_env87.fpus &= (~0x4700);
				hsw_env87.fpus |= 0x400;} 
			return;
		case 2: /* FSQRT */
			TDBLCPY(&fptemp,&FPR_ST0);
			if(fptemp<0.0){ hsw_env87.fpus &= (~0x4700);  /* (C3,C2,C1,C0) <-- 0000 */
			hsw_env87.fpus |= 0x400;}
			FPR_ST0 = sqrt(fptemp);
			return;
		case 3: /* FSINCOS */
			TDBLCPY(&fptemp,&FPR_ST0);
			if((fptemp > MAXTAN)||(fptemp < -MAXTAN))  hsw_env87.fpus |= 0x400;
			else {
			FPR_ST0 = sin(fptemp);
			PUSHFSP;
			FPR_ST0 = cos(fptemp);
			VALID_ST0;
			hsw_env87.fpus &= (~0x400);  /* C2 <-- 0 */
			/* the above code is for  |arg| < 2**63 only */
			}
			return;
		case 4: /* FRNDINT */
			TDBLCPY(&fpsrcop,&FPR_ST0);
			if (FPUC_RC == RC_CHOP) /*Chop towards zero*/ {
				if ( fpsrcop < 0.0 )
					FPR_ST0 = -(floor(-fpsrcop));
				else FPR_ST0 = floor(fpsrcop);
			}
			else
			if (FPUC_RC == RC_NEAR) /*Round to the nearest*/ {
				if ( (fpsrcop-floor(fpsrcop)) < (ceil(fpsrcop)-fpsrcop) )
					FPR_ST0 = floor(fpsrcop);
				else FPR_ST0 = ceil(fpsrcop);
			}
			else
			if (FPUC_RC == RC_DOWN) /*Chop towards -INFI*/
				FPR_ST0 = floor(fpsrcop);
			else  /*Round towards +INFI*/
				FPR_ST0 = ceil(fpsrcop);
			return;
		case 5: /* FSCALE */
			fpsrcop = 2.0;
			fptemp = pow(fpsrcop,FPR_ST1);
			FPR_ST0 *= fptemp;
			return;
		case 6: /* FSIN */
			TDBLCPY(&fptemp,&FPR_ST0);
			if((fptemp > MAXTAN)||(fptemp < -MAXTAN))  hsw_env87.fpus |= 0x400;
			else {
			FPR_ST0 = sin(fptemp);
			hsw_env87.fpus &= (~0x400);  /* C2 <-- 0 */
			/* the above code is for  |arg| < 2**53 only */
			}
			return;
		case 7: /* FCOS */
			TDBLCPY(&fptemp,&FPR_ST0);
			if((fptemp > MAXTAN)||(fptemp < -MAXTAN))  hsw_env87.fpus |= 0x400;
			else {
			FPR_ST0 = cos(fptemp);
			hsw_env87.fpus &= (~0x400);  /* C2 <-- 0 */
			/* the above code is for  |arg5 < 2**63 only */
			}
			return;
	}
}

void
hsw_fp87_20m(unsigned char *mem_ref)
{
/* FADDm32i */
	FPR_ST0 += GET32INT(mem_ref);
}

void
hsw_fp87_20r(int reg_num)
{
/* FCMOVB */
    not_implemented("020r");
}

void
hsw_fp87_21m(unsigned char *mem_ref)
{
/* FMULm32i */
	FPR_ST0 *= GET32INT(mem_ref);
}

void
hsw_fp87_21r(int reg_num)
{
/* FCMOVE */
    not_implemented("021r");
}

void
hsw_fp87_22m(unsigned char *mem_ref)
{
/* FICOMm32i */
	Ldouble fpsrcop;
    fpsrcop = GET32INT(mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_22r(int reg_num)
{
/* FCMOVBE */
    not_implemented("022r");
}

void
hsw_fp87_23m(unsigned char *mem_ref)
{
/* FICOMPm32i */
	Ldouble fpsrcop;
    fpsrcop = GET32INT(mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    POPFSP;
}

void
hsw_fp87_23r(int reg_num)
{
/* FCMOVU */
    not_implemented("023r");
}

void
hsw_fp87_24m(unsigned char *mem_ref)
{
/* FISUBm32i */
	Ldouble fpsrcop;
	fpsrcop = GET32INT(mem_ref);
	FPR_ST0 -= fpsrcop;
}

void
hsw_fp87_24r(int reg_num)
{
/* FISUBm32i */
    illegal_op("024r");
}

void
hsw_fp87_25m(unsigned char *mem_ref)
{
/* FISUBRm32i_FUCOMPPst1 */
	Ldouble fpsrcop;
	fpsrcop = GET32INT(mem_ref);
	FPR_ST0 = fpsrcop - FPR_ST0;
}

void
hsw_fp87_25r(int reg_num)
{
/* FISUBRm32i_FUCOMPPst1 */
    Ldouble fpsrcop;
    if (reg_num!=1) {
	illegal_op("025r r!=1"); return;
    }
    TDBLCPY(&fpsrcop,&FPR_ST1);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    POPFSP;
    POPFSP;
}

void
hsw_fp87_26m(unsigned char *mem_ref)
{
/* FIDIVm32i */
	Ldouble fpsrcop;
	fpsrcop = GET32INT(mem_ref);
	FPR_ST0 /= fpsrcop;
	if (!L_ISZERO(&fpsrcop)) { hsw_env87.fpus |= FP_ZEROFLG; }
}

void
hsw_fp87_26r(int reg_num)
{
/* FIDIVm32i */
    illegal_op("026r");
}

void
hsw_fp87_27m(unsigned char *mem_ref)
{
/* FIDIVRm32i */
	Ldouble fpsrcop;
	fpsrcop = GET32INT(mem_ref);
	if (!L_ISZERO(&FPR_ST0)) { hsw_env87.fpus |= FP_ZEROFLG; }
	FPR_ST0 = fpsrcop / FPR_ST0;
}

void
hsw_fp87_27r(int reg_num)
{
/* FIDIVRm32i */
    illegal_op("027r");
}

void
hsw_fp87_30m(unsigned char *mem_ref)
{
/* FILDm32i */
	PUSHFSP;
	FPR_ST0 = GET32INT(mem_ref);
	VALID_ST0;
}

void
hsw_fp87_30r(int reg_num)
{
/* FCMOVNB */
    not_implemented("030r");
}

void
hsw_fp87_31m(unsigned char *mem_ref)
{
    illegal_op("031m");
}

void
hsw_fp87_31r(int reg_num)
{
/* FCMOVNE */
    not_implemented("031r");
}

void
hsw_fp87_32m(unsigned char *mem_ref)
{
/* FISTm32i */
	Ldouble fpsrcop;
	int m32i;
    TDBLCPY(&fpsrcop,&FPR_ST0);
    if (FPUC_RC == RC_CHOP) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if (FPUC_RC == RC_NEAR) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
              floor(fpsrcop): ceil(fpsrcop);
    else if (FPUC_RC == RC_DOWN) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    m32i = (int)fpsrcop;
    STORE32INT(mem_ref,m32i);
}

void
hsw_fp87_32r(int reg_num)
{
/* FCMOVNBE */
    not_implemented("032r");
}

void
hsw_fp87_33m(unsigned char *mem_ref)
{
/* FISTPm32i */
    Ldouble fpsrcop;
    int m32i;
    TDBLCPY(&fpsrcop,&FPR_ST0);
    if (FPUC_RC == RC_CHOP) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if (FPUC_RC == RC_NEAR) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
              floor(fpsrcop): ceil(fpsrcop);
    else if (FPUC_RC == RC_DOWN) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    m32i = (int)fpsrcop;
#ifdef DEBUG
    e_printf("FP: storing %#x\n",m32i);
#endif
    STORE32INT(mem_ref,m32i);
    POPFSP;
}

void
hsw_fp87_33r(int reg_num)
{
/* FCMOVNU */
    not_implemented("033r");
}

void
hsw_fp87_34m(unsigned char *mem_ref)
{
    illegal_op("034m");
}

void
hsw_fp87_34r(int reg_num)
{
/* FINIT_FCLEX */
	switch (reg_num) {
	    case 0: case 1:	/* FNENI,FNDISI: 8087 */
	    case 4: case 5:	/* FSETPM,FRSTPM: 80287 */
		return;
	    case 3:		/* FINIT */
		hsw_env87.fpus = 0;
		hsw_env87.fpstt = 0;
		hsw_env87.fpuc = 0x37f;
		hsw_env87.fptag = 0xffff;	/* empty */
		return;
	    case 2:		/* FCLEX */
		hsw_env87.fpus &= 0x7f00;
		return;
	    default:
		illegal_op("034r d=6,7");
	}
}

void
hsw_fp87_35m(unsigned char *mem_ref)
{
/* FLDm80r */
#ifndef __i386__
	int expdif;
	signed short i16;
#endif
	PUSHFSP;
	LD80R(&FPR_ST0,(mem_ref));
	VALID_ST0;
}

void
hsw_fp87_35r(int reg_num)
{
/* FUCOMI */
    not_implemented("035r");
}

void
hsw_fp87_36m(unsigned char *mem_ref)
{
    illegal_op("036m");
}

void
hsw_fp87_36r(int reg_num)
{
/* FCOMI */
    not_implemented("036r");
}

void
hsw_fp87_37m(unsigned char *mem_ref)
{
/* FSTPm80r */
#ifndef __i386__
	int expdif;
	signed short i16;
#endif
	ST80R(mem_ref,&FPR_ST0);
	POPFSP;
}

void
hsw_fp87_37r(int reg_num)
{
    illegal_op("037r");
}

void
hsw_fp87_40m(unsigned char *mem_ref)
{
/* FADDm64r_tosti */
	double fptemp;
	FPR_ST0 += GET64REAL(fptemp,mem_ref);
}

void
hsw_fp87_40r(int reg_num)
{
/* FADDm64r_tosti */
	FPR_ST(reg_num) += FPR_ST0;
}

void
hsw_fp87_41m(unsigned char *mem_ref)
{
/* FMULm64r_tosti */
	double fptemp;
	FPR_ST0 *= GET64REAL(fptemp,mem_ref);
}

void
hsw_fp87_41r(int reg_num)
{
/* FMULm64r_tosti */
	FPR_ST(reg_num) *= FPR_ST0;
}

void
hsw_fp87_42m(unsigned char *mem_ref)
{
/* FCOMm64r */
	double fptemp;
	Ldouble fpsrcop;
    fpsrcop = GET64REAL(fptemp,mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_42r(int reg_num)
{
    illegal_op("042r");
}

void
hsw_fp87_43m(unsigned char *mem_ref)
{
/* FCOMPm64r */
	double fptemp;
	Ldouble fpsrcop;
    fpsrcop = GET64REAL(fptemp,mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    POPFSP;
}

void
hsw_fp87_43r(int reg_num)
{
    illegal_op("043r");
}

void
hsw_fp87_44m(unsigned char *mem_ref)
{
/* FSUBm64r_FSUBRfromsti */
	double fptemp;
	FPR_ST0 -= GET64REAL(fptemp,mem_ref);
}

void
hsw_fp87_44r(int reg_num)
{
/* FSUBRm64r_FSUBRfromsti */
	FPR_ST(reg_num) = FPR_ST0 - FPR_ST(reg_num);
}

void
hsw_fp87_45m(unsigned char *mem_ref)
{
/* FSUBRm64r_FSUBfromsti */
	double fptemp;
	FPR_ST0 = GET64REAL(fptemp,mem_ref) - FPR_ST0;
}

void
hsw_fp87_45r(int reg_num)
{
/* FSUBm64r_FSUBfromsti */
	FPR_ST(reg_num) -= FPR_ST0;
}

void
hsw_fp87_46r(int reg_num)
{
/* FDIVRm64r_FDIVRtosti */
	if (!L_ISZERO(&FPR_ST0)) { hsw_env87.fpus |= FP_ZEROFLG; }
	FPR_ST(reg_num) = FPR_ST0 / FPR_ST(reg_num);
}

void
hsw_fp87_46m(unsigned char *mem_ref)
{
/* FDIVm64r_FDIVtosti */
	double fptemp;
	FPR_ST0 /= GET64REAL(fptemp,mem_ref);
	if (!L_ISZERO(&fptemp)) { hsw_env87.fpus |= FP_ZEROFLG; }
}

void
hsw_fp87_47r(int reg_num)
{
/* FDIVm64r_FDIVtosti */
	if (!L_ISZERO(&FPR_ST(reg_num))) { hsw_env87.fpus |= FP_ZEROFLG; }
	FPR_ST(reg_num) /= FPR_ST0;
}

void
hsw_fp87_47m(unsigned char *mem_ref)
{
/* FDIVRm64r_FDIVRtosti */
	double fptemp;
	if (!L_ISZERO(&FPR_ST0)) { hsw_env87.fpus |= FP_ZEROFLG; }
	FPR_ST0 = GET64REAL(fptemp,mem_ref) / FPR_ST0;
}

void
hsw_fp87_50m(unsigned char *mem_ref)
{
/* FLDm64r */
	double fptemp;
	PUSHFSP;
	FPR_ST0 = GET64REAL(fptemp,mem_ref);
	VALID_ST0;
}

void
hsw_fp87_50r(int reg_num)
{
/* FFREE: only tag as empty */
/* do nothing */
}

void
hsw_fp87_51m(unsigned char *mem_ref)
{
    illegal_op("051m");
}

void
hsw_fp87_51r(int reg_num)
{
    illegal_op("051r");
}

void
hsw_fp87_52m(unsigned char *mem_ref)
{
/* FSTm64r_sti */
	double fptemp;
	fptemp = FPR_ST0;
	STORE64REAL(mem_ref,fptemp);
}

void
hsw_fp87_52r(int reg_num)
{
/* FSTm64r_sti */
	TDBLCPY(&FPR_ST(reg_num),&FPR_ST0);
}

void
hsw_fp87_53m(unsigned char *mem_ref)
{
/* FSTPm64r_sti */
	double fptemp;
	fptemp = FPR_ST0;
	STORE64REAL(mem_ref,fptemp);
	POPFSP;
}

void
hsw_fp87_53r(int reg_num)
{
/* FSTPm64r_sti */
	TDBLCPY(&FPR_ST(reg_num),&FPR_ST0);
	POPFSP;
}

void
hsw_fp87_54m(unsigned char *mem_ref)
{
    int i, st;
#ifndef __i386__
    int expdif;
#endif
/* FRSTOR */
    if (data32) {
	hsw_env87.fpuc = GET16INT(mem_ref);
	hsw_env87.fpus = GET16INT(mem_ref+4);
	hsw_env87.fptag = GET16INT(mem_ref+8);
	mem_ref+=28;	/* IP,OP,opcode: n.i. */
    }
    else {
	hsw_env87.fpuc = GET16INT(mem_ref);
	hsw_env87.fpus = GET16INT(mem_ref+2);
	hsw_env87.fptag = GET16INT(mem_ref+4);
	mem_ref+=14;	/* IP,OP,opcode: n.i. */
    }
    st = hsw_env87.fpstt = (hsw_env87.fpus>>11)&7;
    for (i = 0; i < 8; i++ ) {
	LD80R(&hsw_env87.fpregs[(st+i)&0x7],mem_ref);
	mem_ref += 10;
    }
}

void
hsw_fp87_54r(int reg_num)
{
/* FUCOMsti */
	Ldouble fpsrcop;
    TDBLCPY(&fpsrcop,&FPR_ST(reg_num));
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_55m(unsigned char *mem_ref)
{
    illegal_op("055m");
}

void
hsw_fp87_55r(int reg_num)
{
/* FUCOMPsti */
	Ldouble fpsrcop;
    TDBLCPY(&fpsrcop,&FPR_ST(reg_num));
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    POPFSP;
}

/*
 * FSAVE: 4 modes - followed by FINIT
 *
 * A) 16-bit real (94 bytes)
 *	(00-01)		Control Word
 *	(02-03)		Status Word
 *	(04-05)		Tag Word
 *	(06-07)		IP 15..00
 *	(08-09)		IP 19..16,0,Opc 10..00
 *	(0a-0b)		OP 15..00
 *	(0c-0d)		OP 19..16,0...
 *	(0e-5d)		FP registers
 *
 * B) 16-bit protected (94 bytes)
 *	(00-01)		Control Word
 *	(02-03)		Status Word
 *	(04-05)		Tag Word
 *	(06-07)		IP offset
 *	(08-09)		IP selector
 *	(0a-0b)		OP offset
 *	(0c-0d)		OP selector
 *	(0e-5d)		FP registers
 *
 * C) 32-bit real (108 bytes)
 *	(00-01)		Control Word		(02-03) reserved
 *	(04-05)		Status Word		(06-07) reserved
 *	(08-09)		Tag Word		(0a-0b) reserved
 *	(0c-0d)		IP 15..00		(0e-0f) reserved
 *	(10-13)		0,0,0,0,IP 31..16,0,Opc 10..00
 *	(14-15)		OP 15..00		(16-17) reserved
 *	(18-1b)		0,0,0,0,OP 31..16,0...
 *	(1c-6b)		FP registers
 *
 * D) 32-bit protected (108 bytes)
 *	(00-01)		Control Word		(02-03) reserved
 *	(04-05)		Status Word		(06-07) reserved
 *	(08-09)		Tag Word		(0a-0b) reserved
 *	(0c-0d)		IP offset		(0e-0f) reserved
 *	(10-13)		0,0,0,0,Opc 10..00,IP selector
 *	(14-15)		OP offset		(16-17) reserved
 *	(18-19)		OP selector		(1a-1b) reserved
 *	(1c-6b)		FP registers
 */
void
hsw_fp87_56m(unsigned char *mem_ref)
{
    int i, st, fptag, ntag;
#ifndef __i386__
    int expdif;
#endif
/* FSAVE */
    hsw_env87.fpus &= (~0x3800);	/* set TOP field to 0 */
    hsw_env87.fpus |= ((st=(hsw_env87.fpstt&0x7)))<<11;

    fptag = hsw_env87.fptag; ntag=0;
    for (i=7; i>=0; --i) {
	unsigned short *sf = (unsigned short *)&(hsw_env87.fpregs[i]);
	ntag <<= 2;
	if ((fptag & 0xc000) == 0xc000) ntag |= 3;
	else {
	    if (!L_ISZERO(sf)) {
	        ntag |= 1;
	    }
	    else {
	        unsigned short sf2 = sf[4]&0x7fff;
	        if ((sf2==0)||(sf2==0x7fff)||((sf[3]&0x8000)==0)) {
		  ntag |= 2;
	        }		/* else tag=00 valid */
	    }
	}
	fptag <<= 2;
    }
    hsw_env87.fptag = ntag;
    if (data32) {
	STORE32INT((mem_ref), hsw_env87.fpuc);
	STORE32INT((mem_ref+4), hsw_env87.fpus);
	STORE32INT((mem_ref+8), hsw_env87.fptag);
	*((long *)(mem_ref+12)) = 0;
	*((long *)(mem_ref+16)) = 0;
	*((long *)(mem_ref+20)) = 0;
	*((long *)(mem_ref+24)) = 0; /* IP,OP,opcode: n.i. */
	mem_ref += 28;
    }
    else {
	STORE16INT((mem_ref), hsw_env87.fpuc);
	STORE16INT((mem_ref+2), hsw_env87.fpus);
	STORE16INT((mem_ref+4), hsw_env87.fptag);
	*((long *)(mem_ref+6)) = 0;
	*((long *)(mem_ref+10)) = 0; /* IP,OP,opcode: n.i. */
	mem_ref += 14;
    }
    for (i = 0; i < 8; i++) {
        ST80R((mem_ref),&hsw_env87.fpregs[(st+i)&0x7]);
        mem_ref+=10;
    };
    hsw_env87.fpus = 0; hsw_env87.fpstt = 0;
    hsw_env87.fpuc = 0x37f; hsw_env87.fptag = 0xffff;
}

void
hsw_fp87_56r(int reg_num)
{
    illegal_op("056r");
}

void
hsw_fp87_57m(unsigned char *mem_ref)
{
/* FSTSWm16i */
	hsw_env87.fpus &= (~0x3800);
	hsw_env87.fpus |= (hsw_env87.fpstt&0x7)<<11;
	STORE16INT(mem_ref,hsw_env87.fpus);
}

void
hsw_fp87_57r(int reg_num)
{
    illegal_op("057r");
}

void
hsw_fp87_60m(unsigned char *mem_ref)
{
/* FADDm16i_tostipop */
	FPR_ST0 += GET16INT(mem_ref);
}

void
hsw_fp87_60r(int reg_num)
{
/* FADDm16i_tostipop */
	FPR_ST(reg_num) += FPR_ST0;
	POPFSP;
}

void
hsw_fp87_61m(unsigned char *mem_ref)
{
/* FMULm16i_tostipop */
	FPR_ST0 *= GET16INT(mem_ref);
}

void
hsw_fp87_61r(int reg_num)
{
/* FMULPm16i_tostipop */
	FPR_ST(reg_num) *= FPR_ST0;
	POPFSP;
}

void
hsw_fp87_62m(unsigned char *mem_ref)
{
/* FICOMm16i */
	Ldouble fpsrcop;
    fpsrcop = GET16INT(mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_62r(int reg_num)
{
    illegal_op("062r");
}

void
hsw_fp87_63m(unsigned char *mem_ref)
{
/* FICOMPm16i_FCOMPPst1 */
	Ldouble fpsrcop;
    fpsrcop = GET16INT(mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    POPFSP;
}

void
hsw_fp87_63r(int reg_num)
{
/* FICOMPm16i_FCOMPPst1 */
	Ldouble fpsrcop;
    if (reg_num!=1) {
	illegal_op("063r d!=1"); return;
    }
    TDBLCPY(&fpsrcop,&FPR_ST1);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( FPR_ST0 < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( FPR_ST0 == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( FPR_ST0 > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    POPFSP;
    POPFSP;
}

void
hsw_fp87_64m(unsigned char *mem_ref)
{
/* FISUBm16i_FSUBRPfromsti */
	FPR_ST0 -= GET16INT(mem_ref);
}

void
hsw_fp87_64r(int reg_num)
{
/* FISUBRm16i_FSUBRPfromsti */
	FPR_ST(reg_num) = FPR_ST0 - FPR_ST(reg_num);
	POPFSP;
}

void
hsw_fp87_65m(unsigned char *mem_ref)
{
/* FISUBRm16i_FSUBPfromsti */
	FPR_ST0 = GET16INT(mem_ref) - FPR_ST0;
}

void
hsw_fp87_65r(int reg_num)
{
/* FISUBm16i_FSUBPfromsti */
	FPR_ST(reg_num) -= FPR_ST0;
	POPFSP;
}

void
hsw_fp87_66m(unsigned char *mem_ref)
{
/* FIDIVm16i_FDIVPtosti */
	Ldouble fptemp;
	fptemp = GET16INT(mem_ref);
	FPR_ST0 /= fptemp;
	if (!L_ISZERO(&fptemp)) { hsw_env87.fpus |= FP_ZEROFLG; }
}

void
hsw_fp87_66r(int reg_num)
{
/* FIDIVRm16i_FDIVRPtosti */
/* @@checked */
	if (!L_ISZERO(&FPR_ST(reg_num))) { hsw_env87.fpus |= FP_ZEROFLG; }
	FPR_ST(reg_num) = FPR_ST0 / FPR_ST(reg_num);
	POPFSP;
}

void
hsw_fp87_67m(unsigned char *mem_ref)
{
/* FIDIVRm16i_FDIVRPtosti */
	if (!L_ISZERO(&FPR_ST0)) { hsw_env87.fpus |= FP_ZEROFLG; }
	FPR_ST0 = GET16INT(mem_ref) / FPR_ST0;
}

void
hsw_fp87_67r(int reg_num)
{
/* FIDIVm16i_FDIVPtosti */
	if (!L_ISZERO(&FPR_ST0)) { hsw_env87.fpus |= FP_ZEROFLG; }
	FPR_ST(reg_num) /= FPR_ST0;
	POPFSP;
}

void
hsw_fp87_70m(unsigned char *mem_ref)
{
/* FILDm16i */
	PUSHFSP;
	FPR_ST0 = GET16INT(mem_ref);
	VALID_ST0;
}

void
hsw_fp87_70r(int reg_num)
{
    illegal_op("070r");
}

void
hsw_fp87_71m(unsigned char *mem_ref)
{
    illegal_op("071m");
}

void
hsw_fp87_71r(int reg_num)
{
    illegal_op("071r");
}

void
hsw_fp87_72m(unsigned char *mem_ref)
{
/* FISTm16i */
	Ldouble fpsrcop;
	signed short i16;
    TDBLCPY(&fpsrcop,&FPR_ST0);
    if (FPUC_RC == RC_CHOP) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if (FPUC_RC == RC_NEAR) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
                      floor(fpsrcop): ceil(fpsrcop);
    else if (FPUC_RC == RC_DOWN) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    i16 = (short)fpsrcop;
    STORE16INT(mem_ref,i16);
}

void
hsw_fp87_72r(int reg_num)
{
    illegal_op("072r");
}

void
hsw_fp87_73m(unsigned char *mem_ref)
{
/* FISTPm16i */
    Ldouble fpsrcop;
    signed short i16;
    TDBLCPY(&fpsrcop,&FPR_ST0);
    if (FPUC_RC == RC_CHOP) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if (FPUC_RC == RC_NEAR) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
                      floor(fpsrcop): ceil(fpsrcop);
    else if (FPUC_RC == RC_DOWN) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    i16 = (short)fpsrcop;
#ifdef DEBUG
    e_printf("FP: storing %#x\n",i16);
#endif
    STORE16INT(mem_ref,i16);
    POPFSP;
}

void
hsw_fp87_73r(int reg_num)
{
    illegal_op("073r");
}

void
hsw_fp87_74m(unsigned char *mem_ref)
{
/* FBLDm80dec_FSTSWax */
	unsigned char * seg;
	Ldouble fpsrcop;
	int m32i;
    /* in this code, seg/m32i will be used as temporary ptr/int */
    seg = mem_ref + 8;
    if ( *seg-- != 0 || (*seg & 0xf0) != 0 ) /* d15..d17 non-zero*/
        e_printf0("This BCD number exceeds the range of DOUBLE REAL!\n");
    m32i = *seg--;  /* <-- d14 */
    m32i = MUL10(m32i) + (*seg >> 4);  /* <-- val * 10 + d13 */
    m32i = MUL10(m32i) + (*seg & 0xf); /* <-- val * 10 + d12 */
    seg--;
    m32i = MUL10(m32i) + (*seg >> 4);  /* <-- val * 10 + d11 */
    m32i = MUL10(m32i) + (*seg & 0xf); /* <-- val * 10 + d10 */
    seg--;
    m32i = MUL10(m32i) + (*seg >> 4);  /* <-- val * 10 + d9 */
    m32i = MUL10(m32i) + (*seg & 0xf); /* <-- val * 10 + d8 */
    seg--;
    fpsrcop = ((Ldouble)m32i) * 100000000.0;
    m32i = (*seg >> 4);  /* <-- d7 */
    m32i = MUL10(m32i) + (*seg & 0xf); /* <-- val * 10 + d6 */
    seg--;
    m32i = MUL10(m32i) + (*seg >> 4);  /* <-- val * 10 + d5 */
    m32i = MUL10(m32i) + (*seg & 0xf); /* <-- val * 10 + d4 */
    seg--;
    m32i = MUL10(m32i) + (*seg >> 4);  /* <-- val * 10 + d3 */
    m32i = MUL10(m32i) + (*seg & 0xf); /* <-- val * 10 + d2 */
    seg--;
    m32i = MUL10(m32i) + (*seg >> 4);  /* <-- val * 10 + d1 */
    m32i = MUL10(m32i) + (*seg & 0xf); /* <-- val * 10 + d0 */
    fpsrcop += ((Ldouble)m32i);
    if ( *(seg+9) & 0x80 )  fpsrcop = -fpsrcop;
    PUSHFSP;
    FPR_ST0 = fpsrcop;
    VALID_ST0;
}

void
hsw_fp87_74r(int reg_num)
{
/* FBLDm80dec_FSTSWax */
    if (reg_num>0) {
	illegal_op("074r r>0"); return;
    }
    hsw_env87.fpus &= (~0x3800);
    hsw_env87.fpus |= (hsw_env87.fpstt&0x7)<<11;
    envp_global->rax.x.x.x = hsw_env87.fpus;
}

void
hsw_fp87_75m(unsigned char *mem_ref)
{
/* FILDm64i */
    Ldouble fptemp;
    QLONG i64lh;

    i64lh = GET64INT((mem_ref));
    PUSHFSP;
#ifndef __i386__
    /* check if significant digits can be loaded into a 53-bit mantissa */
    { QLONG absi64l = (i64lh>0? i64lh:-i64lh);
    if ((absi64l & 0xffe0000000000000LL) && (absi64l & 0x7ff)) {
	e_printf("This 64-bit INT exceeds DOUBLE REAL mantissa!\n"); 
    } }
#endif
    fptemp = (Ldouble)i64lh;
    FPR_ST0 = fptemp;
    VALID_ST0;
}

void
hsw_fp87_75r(int reg_num)
{
/* FUCOMIP */
    not_implemented("075r");
}

void
hsw_fp87_76m(unsigned char *mem_ref)
{
/* FBSTPm80dec */
    Ldouble fptemp;
    Ldouble fpsrcop;
    signed short i16;
    TDBLCPY(&fpsrcop,&FPR_ST0);
    if (FPUC_RC == RC_CHOP) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if (FPUC_RC == RC_NEAR) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
                      floor(fpsrcop): ceil(fpsrcop);
    else if (FPUC_RC == RC_DOWN) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    if ( fpsrcop < 0.0 ) {
         *(mem_ref+9) = 0xff;
         fpsrcop = -fpsrcop;
    }
	else *(mem_ref+9) = 0x0;
    for ( mem_ref += 8, i16 = 0;  i16 < 9;  i16++ ) *mem_ref-- = 0;
    for ( mem_ref++;  fpsrcop != 0.0;  mem_ref++ ) {
        fptemp = floor(fpsrcop/10.0);
        i16 = ((int)(fpsrcop - fptemp*10.0));
        if  (!L_ISZERO(&fptemp))  { *mem_ref = (BYTE)i16; break; }
        fpsrcop = fptemp;
        fptemp = floor(fpsrcop/10.0);
        *mem_ref = i16 | (((int)(fpsrcop - fptemp*10.0)) << 4);
        fpsrcop = fptemp;
    }
    POPFSP;
}

void
hsw_fp87_76r(int reg_num)
{
/* FCOMIP */
    not_implemented("076r");
}

void
hsw_fp87_77m(unsigned char *mem_ref)
{
/* FISTPm64i */
    Ldouble fpsrcop;
    QLONG i64lh;

    TDBLCPY(&fpsrcop,&FPR_ST0);
    if (FPUC_RC == RC_CHOP) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if (FPUC_RC == RC_NEAR) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
                      floor(fpsrcop): ceil(fpsrcop);
    else if (FPUC_RC == RC_DOWN) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    i64lh = (QLONG)fpsrcop;
#ifdef DEBUG
    e_printf("FP: storing %#Lx\n",i64lh);
#endif
    STORE64INT((mem_ref),i64lh);
    POPFSP;
}

void
hsw_fp87_77r(int reg_num)
{
    illegal_op("077r");
}
