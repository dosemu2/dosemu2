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

changes for use with dosemu-0.67 1997/10/20 vignani@mbox.vol.it
 */

#include "hsw_interp.h"
#include "mod_rm.h"
#include <stdio.h>
#include <math.h>

#define DPLOG10_2 log10(2.0)
#define DPLOGE_2 0.69314718055994530942  /*  log(2.0)  */
#define DPPI 3.14159265358979323846      /* 3.141592654 */
#define DPLOG2_E 1.4426950408889634074  /* 1.0/log(2.0) */
#define DPLOG2_10 log(10.0)/log(2.0)
#define MAXTAN pow(2.0,63.0)
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
#define EXPONENT(addr) ( ( (((BYTE*)addr)[0] & 0x7f) << 4 ) |  \
						 ( (((BYTE*)addr)[1] & 0xf0) >> 4 )     )
#define BIASEXPONENT(addr) ( ((BYTE*)addr)[0] &= (~0x40),  \
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

#define MUL10(iv) ( iv + iv + (iv << 3) )

ENV87 hsw_env87;

extern Interp_ENV *envp_global;

#ifdef DOSEMU
#  define fprintf our_fprintf
#  define our_fprintf(stream, args...) error(##args)
#endif


void
hsw_fp87_00m(unsigned char *mem_ref)
{
/* FADDm32r_sti */
	float m32r;
	hsw_env87.fpregs[hsw_env87.fpstt] += GET32REAL(m32r, mem_ref);
}

void
hsw_fp87_00r(int reg_num)
{
/* FADDm32r_sti */
	hsw_env87.fpregs[hsw_env87.fpstt] += hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
}

void
hsw_fp87_01m(unsigned char *mem_ref)
{
/* FMULm32r_sti */
	float m32r;
	hsw_env87.fpregs[hsw_env87.fpstt] *= GET32REAL(m32r, mem_ref);
}

void
hsw_fp87_01r(int reg_num)
{
/* FMULm32r_sti */
	hsw_env87.fpregs[hsw_env87.fpstt] *= hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
}

void
hsw_fp87_02m(unsigned char *mem_ref)
{
/* FCOMm32r_sti */
	double fpsrcop;
	float m32r;

	fpsrcop = GET32REAL(m32r,mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000; /* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_02r(int reg_num)
{
/* FCOMm32r_sti */
	double fpsrcop;
	fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000; /* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_03m(unsigned char *mem_ref)
{
/* FCOMPm32r_sti */
	double fpsrcop;
	float m32r;

    fpsrcop = GET32REAL(m32r, mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_03r(int reg_num)
{
/* FCOMPm32r_sti */
	double fpsrcop;
	fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_04m(unsigned char *mem_ref)
{
/* FSUBm32r_sti */
	float m32r;

	hsw_env87.fpregs[hsw_env87.fpstt] -= GET32REAL(m32r, mem_ref);
}

void
hsw_fp87_04r(int reg_num)
{
/* FSUBm32r_sti */
	hsw_env87.fpregs[hsw_env87.fpstt] -= hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
}

void
hsw_fp87_05m(unsigned char *mem_ref)
{
/* FSUBRm32r_sti */
	double fpsrcop;
	float m32r;

    fpsrcop = GET32REAL(m32r, mem_ref);
	hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop - hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_05r(int reg_num)
{
/* FSUBRm32r_sti */
	double fpsrcop;
	fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
	hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop - hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_06m(unsigned char *mem_ref)
{
/* FDIVm32r_sti */
	double fpsrcop;
	float m32r;
    fpsrcop = GET32REAL(m32r, mem_ref);
	hsw_env87.fpregs[hsw_env87.fpstt] /= fpsrcop;
	if(fpsrcop==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
}

void
hsw_fp87_06r(int reg_num)
{
/* FDIVm32r_sti */
	double fpsrcop;
	fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
	hsw_env87.fpregs[hsw_env87.fpstt] /= fpsrcop;
	if(fpsrcop==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
}

void
hsw_fp87_07m(unsigned char *mem_ref)
{
/* FDIVRm32r_sti */
	double fpsrcop;
	float m32r;
    fpsrcop = GET32REAL(m32r, mem_ref);
	if(hsw_env87.fpregs[hsw_env87.fpstt]==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
	hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop / hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_07r(int reg_num)
{
/* FDIVRm32r_sti */
	double fpsrcop;
	fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
	if(hsw_env87.fpregs[hsw_env87.fpstt]==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
	hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop / hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_10m(unsigned char *mem_ref)
{
/* FLDm32r_sti */
	double fpsrcop;
	float m32r;

    fpsrcop = GET32REAL(m32r, mem_ref);
	hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
	hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop;
}

void
hsw_fp87_10r(int reg_num)
{
/* FLDm32r_sti */
	double fpsrcop;
	fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
	hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
	hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop;
}

void
hsw_fp87_11m(unsigned char *mem_ref)
{
    fprintf(stderr,"illegal FP opcode: 011m\n");
}

void
hsw_fp87_11r(int reg_num)
{
/* FXCH */
	double fptemp;
	fptemp = hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] = hsw_env87.fpregs[hsw_env87.fpstt];
	hsw_env87.fpregs[hsw_env87.fpstt] = fptemp;
}

void
hsw_fp87_12m(unsigned char *mem_ref)
{
/* FSTm32r_FNOP */
	float m32r;
	m32r = hsw_env87.fpregs[hsw_env87.fpstt];
	STORE32REAL(mem_ref,m32r);
}

void
hsw_fp87_12r(int reg_num)
{
/* FSTm32r_FNOP */
/* FNOP do nothing */
}

void
hsw_fp87_13m(unsigned char *mem_ref)
{
/* FSTPm32r */
	float m32r;
	m32r = hsw_env87.fpregs[hsw_env87.fpstt];
	STORE32REAL(mem_ref,m32r);
	hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_13r(int reg_num)
{
    fprintf(stderr,"illegal FP opcode: 013r\n");
}

void
hsw_fp87_14m(unsigned char *mem_ref)
{
/* FLDENV */
	hsw_env87.fpus = GET16INT(mem_ref + 2); /* 14-byte env-record */
	hsw_env87.fpstt = (hsw_env87.fpus>>11) & 0x7;
	hsw_env87.fpuc = GET16INT(mem_ref);
}

void
hsw_fp87_14r(int reg_num)
{
	unsigned int expdif;

	switch (reg_num) {
		case 0: /* FCHS */
			hsw_env87.fpregs[hsw_env87.fpstt] = -hsw_env87.fpregs[hsw_env87.fpstt];
			return;
		case 1: /* FABS */
			if ( hsw_env87.fpregs[hsw_env87.fpstt] < 0.0 )
				hsw_env87.fpregs[hsw_env87.fpstt] = -hsw_env87.fpregs[hsw_env87.fpstt];
			return;
		case 2:
			fprintf(stderr,"illegal FP opcode: 014r (reg=2)\n");
		case 3:
			fprintf(stderr,"illegal FP opcode: 014r (reg=3)\n");
			return;
		case 4: /* FTST */ {
			double fpsrcop = 0.0;
			hsw_env87.fpus &= (~0x4500);   /* (C3,C2,C0) <-- 000 */
			if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
				hsw_env87.fpus |= 0x100;   /* (C3,C2,C0) <-- 001 */
			else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
				hsw_env87.fpus |= 0x4000;  /* (C3,C2,C0) <-- 100 */
			/* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
			/* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
			} return;
		case 5: /* FXAM */ {
			unsigned int i64lo, i64hi;
            hsw_env87.fpus &= (~0x4700);  /* (C3,C2,C1,C0) <-- 0000 */
            if ( hsw_env87.fpregs[hsw_env87.fpstt] < 0.0 )
                 hsw_env87.fpus |= 0x200; /* C1 <-- 1 */
            expdif = EXPONENT(&hsw_env87.fpregs[hsw_env87.fpstt]);
            if ( expdif == 0x7ff ) {
                i64hi = *((int *)&hsw_env87.fpregs[hsw_env87.fpstt]) & 0xfffff;
                i64lo = *(((int *)&hsw_env87.fpregs[hsw_env87.fpstt]) + 1);
                hsw_env87.fpus |= (i64hi == 0 && i64lo == 0)?
                            0x500 /*Infinity*/: 0x100 /*NaN*/;
            }
			else if ( expdif == 0x0 ) {
                i64hi = *((int *)&hsw_env87.fpregs[hsw_env87.fpstt]) & 0xfffff;
                i64lo = *(((int *)&hsw_env87.fpregs[hsw_env87.fpstt]) + 1);
                hsw_env87.fpus |= (i64hi == 0 && i64lo == 0)?
                            0x4000 /*Zero*/: 0x4400 /*Denormal*/;
            }
			else hsw_env87.fpus |= 0x400;
			} return;
		case 6:
			fprintf(stderr,"illegal FP opcode: 014r (reg=6)\n");
			return;
		case 7:
			fprintf(stderr,"illegal FP opcode: 014r (reg=7)\n");
			return;
	}
}

void
hsw_fp87_15m(unsigned char *mem_ref)
{
/* FLDCONST */
/* FLDCW */
	hsw_env87.fpuc = GET16INT(mem_ref);
	/* NOT FULLY IMPLEMENTED YET!!! (requires a sys call) */
}

void
hsw_fp87_15r(int reg_num)
{
/* FLDCONST */
	hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
	switch (reg_num) {
		case 0: /* FLD1 */
			hsw_env87.fpregs[hsw_env87.fpstt] = 1.0;
			return;
		case 1: /* FLDL2T */
			hsw_env87.fpregs[hsw_env87.fpstt] = DPLOG2_10;
			return;
		case 2: /* FLDL2E */
			hsw_env87.fpregs[hsw_env87.fpstt] = DPLOG2_E;
			return;
		case 3: /* FLDPI */
			hsw_env87.fpregs[hsw_env87.fpstt] = DPPI;
			return;
		case 4: /* FLDLG2 */
			hsw_env87.fpregs[hsw_env87.fpstt] = DPLOG10_2;
			return;
		case 5: /* FLDLN2 */
			hsw_env87.fpregs[hsw_env87.fpstt] = DPLOGE_2;
			return;
		case 6: /* FLDZ */
			hsw_env87.fpregs[hsw_env87.fpstt] = 0.0;
			return;
		case 7:
			fprintf(stderr,"illegal FP opcode: 015r (reg=7)\n");
			return;
	}
}

void
hsw_fp87_16m(unsigned char *mem_ref)
{
/* FSTENV */
	hsw_env87.fpus &= (~0x3800);
	hsw_env87.fpus |= (hsw_env87.fpstt&0x7)<<11;
	STORE16INT( (mem_ref+2), hsw_env87.fpus);  /* 14-byte env-record */
	STORE16INT( (mem_ref), hsw_env87.fpuc);
}

void
hsw_fp87_16r(int reg_num)
{
	double fptemp;
	double fpsrcop;
	switch (reg_num) {
		case 0: /* F2XM1 */
			hsw_env87.fpregs[hsw_env87.fpstt] = pow(2.0,hsw_env87.fpregs[hsw_env87.fpstt]) - 1.0;
			return;
		case 1: /* FYL2X */
			fptemp = hsw_env87.fpregs[hsw_env87.fpstt];
			if(fptemp>0.0){
			fptemp = log(fptemp)/log(2.0);	 /* log2(ST) */
			hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7] *= fptemp;
			hsw_env87.fpstt++; hsw_env87.fpstt &= 0x7;}
			else { hsw_env87.fpus &= (~0x4700);
				hsw_env87.fpus |= 0x400;
			}
			return;
		case 2: /* FPTAN */
			fptemp = hsw_env87.fpregs[hsw_env87.fpstt];
			if((fptemp > MAXTAN)||(fptemp < -MAXTAN))  hsw_env87.fpus |= 0x400;
			else {
			hsw_env87.fpregs[hsw_env87.fpstt] = tan(fptemp);
			hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
			hsw_env87.fpregs[hsw_env87.fpstt] = 1.0;
			hsw_env87.fpus &= (~0x400);  /* C2 <-- 0 */
			/* the above code is for  |arg| < 2**52 only */
			}
			return;
		case 3: /* FPATAN */
			fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7];
			fptemp = hsw_env87.fpregs[hsw_env87.fpstt];
			hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7] = atan2(fpsrcop,fptemp);
			hsw_env87.fpstt++; hsw_env87.fpstt &= 0x7;
			return;
		case 4: /* FXTRACT */ {
			unsigned int expdif;
			fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
			expdif = EXPONENT(&hsw_env87.fpregs[hsw_env87.fpstt]) - 1023;
					/*DP exponent bias*/
			hsw_env87.fpregs[hsw_env87.fpstt] = expdif;
			hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
			BIASEXPONENT(&fpsrcop);
			hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop;
			}
			return;
		case 5: /* FPREM1 */ {
			double dblq;
			int expdif;
			int q;

			fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
			fptemp = hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7];
			expdif = EXPONENT(&fpsrcop) - EXPONENT(&fptemp);
			if ( expdif < 53 ) {
				dblq = fpsrcop / fptemp;
				dblq = (dblq < 0.0)? ceil(dblq): floor(dblq);
				hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop - fptemp*dblq;
				q = dblq; /* cutting off top bits is assumed here */
				hsw_env87.fpus &= (~0x4700); /* (C3,C2,C1,C0) <-- 0000 */
				/* (C0,C1,C3) <-- (q2,q1,q0) */
				hsw_env87.fpus |= (q&0x4) << 6; /* (C0) <-- q2 */
				hsw_env87.fpus |= (q&0x2) << 8; /* (C1) <-- q1 */
				hsw_env87.fpus |= (q&0x1) << 14; /* (C3) <-- q0 */
			}
			else {
				hsw_env87.fpus |= 0x400;  /* C2 <-- 1 */
				fptemp = pow(2.0, ((double)(expdif-50)) );
				fpsrcop = (hsw_env87.fpregs[hsw_env87.fpstt] /
						hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7]) / fptemp;
				/* fpsrcop = integer obtained by rounding to the nearest */
				fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
							floor(fpsrcop): ceil(fpsrcop);
				hsw_env87.fpregs[hsw_env87.fpstt] =
						hsw_env87.fpregs[hsw_env87.fpstt] -
						hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7] * fpsrcop * fptemp;
			}
			}
			return;
		case 6: /* FDECSTP */
			hsw_env87.fpstt--; hsw_env87.fpstt &= 0x7;
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
/* FD9SLASH7 */
/* FSTCW */
	STORE16INT(mem_ref,hsw_env87.fpuc);
}

void
hsw_fp87_17r(int reg_num)
{
/* FD9SLASH7 */
	double fptemp;
	double fpsrcop;
	switch (reg_num) {
		case 0: /* FPREM */ {
			double dblq;
			int expdif;
			int q;

			fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
			fptemp = hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7];
			expdif = EXPONENT(&fpsrcop) - EXPONENT(&fptemp);
			if ( expdif < 53 ) {
				dblq = fpsrcop / fptemp;
				dblq = (dblq < 0.0)? ceil(dblq): floor(dblq);
				hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop - fptemp*dblq;
				q = dblq; /* cutting off top bits is assumed here */
				hsw_env87.fpus &= (~0x4700); /* (C3,C2,C1,C0) <-- 0000 */
				/* (C0,C1,C3) <-- (q2,q1,q0) */
				hsw_env87.fpus |= (q&0x4) << 6; /* (C0) <-- q2 */
				hsw_env87.fpus |= (q&0x2) << 8; /* (C1) <-- q1 */
				hsw_env87.fpus |= (q&0x1) << 14; /* (C3) <-- q0 */
           	}
            else {
               	hsw_env87.fpus |= 0x400;  /* C2 <-- 1 */
   	           	fptemp = pow(2.0, ((double)(expdif-50)) );
   	           	fpsrcop = (hsw_env87.fpregs[hsw_env87.fpstt] /
						hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7]) / fptemp;
                /* fpsrcop = integer obtained by chopping */
   	            fpsrcop = (fpsrcop < 0.0)?
							-(floor(fabs(fpsrcop))): floor(fpsrcop);
   	            hsw_env87.fpregs[hsw_env87.fpstt] =
						hsw_env87.fpregs[hsw_env87.fpstt] - 
						hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7] * fpsrcop * fptemp;
	        }
            }
            return;
		case 1: /* FYL2XP1 */
			fptemp = hsw_env87.fpregs[hsw_env87.fpstt];
			if((fptemp+1.0)>0.0){
			fptemp = log(fptemp+1.0) /
						log(2.0); /* log2(ST+1.0) */
			hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7] *= fptemp;
			hsw_env87.fpstt++; hsw_env87.fpstt &= 0x7;}
			else { hsw_env87.fpus &= (~0x4700);
				hsw_env87.fpus |= 0x400;} 
			return;
		case 2: /* FSQRT */
			fptemp = hsw_env87.fpregs[hsw_env87.fpstt];
			if(fptemp<0.0){ hsw_env87.fpus &= (~0x4700);  /* (C3,C2,C1,C0) <-- 0000 */
			hsw_env87.fpus |= 0x400;}
			hsw_env87.fpregs[hsw_env87.fpstt] = sqrt(fptemp);
			return;
		case 3: /* FSINCOS */
			fptemp=hsw_env87.fpregs[hsw_env87.fpstt];
			if((fptemp > MAXTAN)||(fptemp < -MAXTAN))  hsw_env87.fpus |= 0x400;
			else {
			hsw_env87.fpregs[hsw_env87.fpstt] = sin(fptemp);
			hsw_env87.fpstt--; hsw_env87.fpstt &= 0x7;
			hsw_env87.fpregs[hsw_env87.fpstt] = cos(fptemp);
			hsw_env87.fpus &= (~0x400);  /* C2 <-- 0 */
			/* the above code is for  |arg| < 2**63 only */
			}
			return;
		case 4: /* FRNDINT */
            fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
            if ( (hsw_env87.fpuc&0xc00) == 0xc00 ) /*Chop towards zero*/ {
                if ( fpsrcop < 0.0 )
                     hsw_env87.fpregs[hsw_env87.fpstt] = -(floor(-fpsrcop));
                else hsw_env87.fpregs[hsw_env87.fpstt] = floor(fpsrcop);
            }
            else if ( (hsw_env87.fpuc&0xc00) == 0x0 ) /*Round to the nearest*/ {
                if ( (fpsrcop-floor(fpsrcop)) < (ceil(fpsrcop)-fpsrcop) )
                     hsw_env87.fpregs[hsw_env87.fpstt] = floor(fpsrcop);
                else hsw_env87.fpregs[hsw_env87.fpstt] = ceil(fpsrcop);
            }
            else if ( (hsw_env87.fpuc&0xc00) == 0x400 ) /*Chop towards -INFI*/
                hsw_env87.fpregs[hsw_env87.fpstt] = floor(fpsrcop);
            else  /*Round towards +INFI*/
                hsw_env87.fpregs[hsw_env87.fpstt] = ceil(fpsrcop);
			return;
		case 5: /* FSCALE */
			fpsrcop = 2.0;
			fptemp = pow(fpsrcop,hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7]);
			hsw_env87.fpregs[hsw_env87.fpstt] *= fptemp;
			return;
		case 6: /* FSIN */
			fptemp=hsw_env87.fpregs[hsw_env87.fpstt];
			if((fptemp > MAXTAN)||(fptemp < -MAXTAN))  hsw_env87.fpus |= 0x400;
			else {
			hsw_env87.fpregs[hsw_env87.fpstt] = sin(fptemp);
			hsw_env87.fpus &= (~0x400);  /* C2 <-- 0 */
			/* the above code is for  |arg| < 2**53 only */
			}
			return;
		case 7: /* FCOS */
			fptemp=hsw_env87.fpregs[hsw_env87.fpstt];
			if((fptemp > MAXTAN)||(fptemp < -MAXTAN))  hsw_env87.fpus |= 0x400;
			else {
			hsw_env87.fpregs[hsw_env87.fpstt] = cos(fptemp);
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
	hsw_env87.fpregs[hsw_env87.fpstt] += GET32INT(mem_ref);
}

void
hsw_fp87_20r(int reg_num)
{
/* FADDm32i */
    fprintf(stderr,"illegal FP opcode: 020r\n");
}

void
hsw_fp87_21m(unsigned char *mem_ref)
{
/* FMULm32i */
	hsw_env87.fpregs[hsw_env87.fpstt] *= GET32INT(mem_ref);
}

void
hsw_fp87_21r(int reg_num)
{
/* FMULm32i */
    fprintf(stderr,"illegal FP opcode: 021r\n");
}

void
hsw_fp87_22m(unsigned char *mem_ref)
{
/* FICOMm32i */
	double fpsrcop;
    fpsrcop = GET32INT(mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_22r(int reg_num)
{
/* FICOMm32i */
    fprintf(stderr,"illegal FP opcode: 022r\n");
}

void
hsw_fp87_23m(unsigned char *mem_ref)
{
/* FICOMPm32i */
	double fpsrcop;
    fpsrcop = GET32INT(mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_23r(int reg_num)
{
/* FICOMPm32i */
    fprintf(stderr,"illegal FP opcode: 023r\n");
}

void
hsw_fp87_24m(unsigned char *mem_ref)
{
/* FISUBm32i */
	double fpsrcop;
	fpsrcop = GET32INT(mem_ref);
	hsw_env87.fpregs[hsw_env87.fpstt] -= fpsrcop;
}

void
hsw_fp87_24r(int reg_num)
{
/* FISUBm32i */
    fprintf(stderr,"illegal FP opcode: 024r\n");
}

void
hsw_fp87_25m(unsigned char *mem_ref)
{
/* FISUBRm32i_FUCOMPPst1 */
	double fpsrcop;
	fpsrcop = GET32INT(mem_ref);
	hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop - hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_25r(int reg_num)
{
/* FISUBRm32i_FUCOMPPst1 */
	double fpsrcop;
    fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7];
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    hsw_env87.fpstt += 2;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_26m(unsigned char *mem_ref)
{
/* FIDIVm32i */
	double fpsrcop;
	fpsrcop = GET32INT(mem_ref);
	hsw_env87.fpregs[hsw_env87.fpstt] /= fpsrcop;
	if(fpsrcop==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
}

void
hsw_fp87_26r(int reg_num)
{
/* FIDIVm32i */
    fprintf(stderr,"illegal FP opcode: 026r\n");
}

void
hsw_fp87_27m(unsigned char *mem_ref)
{
/* FIDIVRm32i */
	double fpsrcop;
	fpsrcop = GET32INT(mem_ref);
	if(hsw_env87.fpregs[hsw_env87.fpstt]==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
	hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop / hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_27r(int reg_num)
{
/* FIDIVRm32i */
    fprintf(stderr,"illegal FP opcode: 027r\n");
}

void
hsw_fp87_30m(unsigned char *mem_ref)
{
/* FILDm32i */
	hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
	hsw_env87.fpregs[hsw_env87.fpstt] = GET32INT(mem_ref);
}

void
hsw_fp87_30r(int reg_num)
{
/* FILDm32i */
    fprintf(stderr,"illegal FP opcode: 030r\n");
}

void
hsw_fp87_31m(unsigned char *mem_ref)
{
    fprintf(stderr,"illegal FP opcode: 031m\n");
}

void
hsw_fp87_31r(int reg_num)
{
    fprintf(stderr,"illegal FP opcode: 031r\n");
}

void
hsw_fp87_32m(unsigned char *mem_ref)
{
/* FISTm32i */
	double fpsrcop;
	int m32i;
    fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
    if ( (hsw_env87.fpuc&0xc00) == 0xc00 ) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x0 ) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
              floor(fpsrcop): ceil(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x400 ) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    m32i = fpsrcop;
    STORE32INT(mem_ref,m32i);
}

void
hsw_fp87_32r(int reg_num)
{
/* FISTm32i */
    fprintf(stderr,"illegal FP opcode: 032r\n");
}

void
hsw_fp87_33m(unsigned char *mem_ref)
{
/* FISTPm32i */
	double fpsrcop;
	int m32i;
    fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
    if ( (hsw_env87.fpuc&0xc00) == 0xc00 ) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x0 ) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
              floor(fpsrcop): ceil(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x400 ) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    m32i = fpsrcop;
    STORE32INT(mem_ref,m32i);
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_33r(int reg_num)
{
/* FISTPm32i */
    fprintf(stderr,"illegal FP opcode: 033r\n");
}

void
hsw_fp87_34m(unsigned char *mem_ref)
{
/* FRSTORm94B_FINIT_FCLEX */
	signed short i16;
	int expdif;
	hsw_env87.fpuc = GET16INT(mem_ref);
	hsw_env87.fpus = GET16INT(mem_ref + 2); /* 14-byte env-record */
	hsw_env87.fpstt = (hsw_env87.fpus>>11) & 0x7;
	for ( i16 = 0;  i16 < 8;  i16++ ) {
		LD80R(&hsw_env87.fpregs[(hsw_env87.fpstt+i16)&0x7],(mem_ref+14+i16*10));
	}
}

void
hsw_fp87_34r(int reg_num)
{
/* FRSTORm94B_FINIT_FCLEX */
	if (reg_num == 3) /* FINIT */ {
		hsw_env87.fpus = 0;  hsw_env87.fpstt = 0; hsw_env87.fpuc = 0x37f;
		return;
	} else if (reg_num == 2) /* FCLEX */ {
		hsw_env87.fpus &= 0x7f00;
		return;
	}
}

void
hsw_fp87_35m(unsigned char *mem_ref)
{
/* FLDm80r */
	int expdif;
	signed short i16;
    hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
	LD80R(&hsw_env87.fpregs[hsw_env87.fpstt],(mem_ref));
}

void
hsw_fp87_35r(int reg_num)
{
/* FLDm80r */
    fprintf(stderr,"illegal FP opcode: 035r\n");
}

void
hsw_fp87_36m(unsigned char *mem_ref)
{
    fprintf(stderr,"illegal FP opcode: 036m\n");
}

void
hsw_fp87_36r(int reg_num)
{
    fprintf(stderr,"illegal FP opcode: 036r\n");
}

void
hsw_fp87_37m(unsigned char *mem_ref)
{
/* FSTPm80r */
	int expdif;
	signed short i16;
	ST80R((mem_ref),&hsw_env87.fpregs[hsw_env87.fpstt])
	hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_37r(int reg_num)
{
/* FSTPm80r */
    fprintf(stderr,"illegal FP opcode: 037r\n");
}

void
hsw_fp87_40m(unsigned char *mem_ref)
{
/* FADDm64r_tosti */
	double fptemp;
	hsw_env87.fpregs[hsw_env87.fpstt] += GET64REAL(fptemp,mem_ref);
}

void
hsw_fp87_40r(int reg_num)
{
/* FADDm64r_tosti */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] += hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_41m(unsigned char *mem_ref)
{
/* FMULm64r_tosti */
	double fptemp;
	hsw_env87.fpregs[hsw_env87.fpstt] *= GET64REAL(fptemp,mem_ref);
}

void
hsw_fp87_41r(int reg_num)
{
/* FMULm64r_tosti */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] *= hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_42m(unsigned char *mem_ref)
{
/* FCOMm64r */
	double fptemp;
	double fpsrcop;
    fpsrcop = GET64REAL(fptemp,mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_42r(int reg_num)
{
/* FCOMm64r */
    fprintf(stderr,"illegal FP opcode: 042r\n");
}

void
hsw_fp87_43m(unsigned char *mem_ref)
{
/* FCOMPm64r */
	double fptemp;
	double fpsrcop;
    fpsrcop = GET64REAL(fptemp,mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_43r(int reg_num)
{
/* FCOMPm64r */
    fprintf(stderr,"illegal FP opcode: 043r\n");
}

void
hsw_fp87_44m(unsigned char *mem_ref)
{
/* FSUBm64r_FSUBRfromsti */
	double fptemp;
	hsw_env87.fpregs[hsw_env87.fpstt] -= GET64REAL(fptemp,mem_ref);
}

void
hsw_fp87_44r(int reg_num)
{
/* FSUBm64r_FSUBRfromsti */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] -= hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_45m(unsigned char *mem_ref)
{
/* FSUBRm64r_FSUBfromsti */
	double fptemp;
	hsw_env87.fpregs[hsw_env87.fpstt] = GET64REAL(fptemp,mem_ref) - hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_45r(int reg_num)
{
/* FSUBRm64r_FSUBfromsti */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] = hsw_env87.fpregs[hsw_env87.fpstt] - hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
}

void
hsw_fp87_46m(unsigned char *mem_ref)
{
/* FDIVm64r_FDIVRtosti */
	double fptemp;
	hsw_env87.fpregs[hsw_env87.fpstt] /= GET64REAL(fptemp,mem_ref);
	if(fptemp==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
}

void
hsw_fp87_47r(int reg_num)
{
/* FDIVm64r_FDIVRtosti */
	if(hsw_env87.fpregs[hsw_env87.fpstt]==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] /= hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_47m(unsigned char *mem_ref)
{
/* FDIVRm64r_FDIVtosti */
	double fptemp;
	if(hsw_env87.fpregs[hsw_env87.fpstt]==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
	hsw_env87.fpregs[hsw_env87.fpstt] = GET64REAL(fptemp,mem_ref) / hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_46r(int reg_num)
{
/* FDIVRm64r_FDIVtosti */
	if(hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7]==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] = hsw_env87.fpregs[hsw_env87.fpstt] / hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
}

void
hsw_fp87_50m(unsigned char *mem_ref)
{
/* FLDm64r_FFREE */
	double fptemp;
	hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
	hsw_env87.fpregs[hsw_env87.fpstt] = GET64REAL(fptemp,mem_ref);
}

void
hsw_fp87_50r(int reg_num)
{
/* FLDm64r_FFREE */
/* else FFREE, do nothing */
}

void
hsw_fp87_51m(unsigned char *mem_ref)
{
    fprintf(stderr,"illegal FP opcode: 051m\n");
}

void
hsw_fp87_51r(int reg_num)
{
    fprintf(stderr,"illegal FP opcode: 051r\n");
}

void
hsw_fp87_52m(unsigned char *mem_ref)
{
/* FSTm64r_sti */
	double fptemp;
	fptemp = hsw_env87.fpregs[hsw_env87.fpstt];
	STORE64REAL(mem_ref,fptemp);
}

void
hsw_fp87_52r(int reg_num)
{
/* FSTm64r_sti */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] = hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_53m(unsigned char *mem_ref)
{
/* FSTPm64r_sti */
	double fptemp;
	fptemp = hsw_env87.fpregs[hsw_env87.fpstt];
	STORE64REAL(mem_ref,fptemp);
	hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_53r(int reg_num)
{
/* FSTPm64r_sti */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] = hsw_env87.fpregs[hsw_env87.fpstt];
	hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_54m(unsigned char *mem_ref)
{
/* FUCOMsti */
    fprintf(stderr,"illegal FP opcode: 054m\n");
}

void
hsw_fp87_54r(int reg_num)
{
/* FUCOMsti */
	double fpsrcop;
    fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_55m(unsigned char *mem_ref)
{
/* FUCOMPsti */
    fprintf(stderr,"illegal FP opcode: 055m\n");
}

void
hsw_fp87_55r(int reg_num)
{
/* FUCOMPsti */
	double fpsrcop;
    fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_56m(unsigned char *mem_ref)
{
/* FSAVEm94B */
	signed short i16;
	int expdif;
    STORE16INT( (mem_ref), hsw_env87.fpuc);
    hsw_env87.fpus &= (~0x3800);
	hsw_env87.fpus |= (hsw_env87.fpstt&0x7)<<11;
    STORE16INT( (mem_ref+2), hsw_env87.fpus); /* 14-byte env-record */
    for ( i16 = 0;  i16 < 8;  i16++ ) {
        ST80R((mem_ref+14+i16*10),&hsw_env87.fpregs[(hsw_env87.fpstt+i16)&0x7]);
    };
}

void
hsw_fp87_56r(int reg_num)
{
/* FSAVEm94B */
    fprintf(stderr,"illegal FP opcode: 056r\n");
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
/* FSTSWm16i */
    fprintf(stderr,"illegal FP opcode: 057r\n");
}

void
hsw_fp87_60m(unsigned char *mem_ref)
{
/* FADDm16i_tostipop */
	hsw_env87.fpregs[hsw_env87.fpstt] += GET16INT(mem_ref);
}

void
hsw_fp87_60r(int reg_num)
{
/* FADDm16i_tostipop */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] += hsw_env87.fpregs[hsw_env87.fpstt];
	hsw_env87.fpstt++; hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_61m(unsigned char *mem_ref)
{
/* FMULm16i_tostipop */
	hsw_env87.fpregs[hsw_env87.fpstt] *= GET16INT(mem_ref);
}

void
hsw_fp87_61r(int reg_num)
{
/* FMULm16i_tostipop */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] *= hsw_env87.fpregs[hsw_env87.fpstt];
	hsw_env87.fpstt++; hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_62m(unsigned char *mem_ref)
{
/* FICOMm16i */
	double fpsrcop;
    fpsrcop = GET16INT(mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
}

void
hsw_fp87_62r(int reg_num)
{
/* FICOMm16i */
    fprintf(stderr,"illegal FP opcode: 062r\n");
}

void
hsw_fp87_63m(unsigned char *mem_ref)
{
/* FICOMPm16i_FCOMPPst1 */
	double fpsrcop;
    fpsrcop = GET16INT(mem_ref);
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_63r(int reg_num)
{
/* FICOMPm16i_FCOMPPst1 */
	double fpsrcop;
    fpsrcop = hsw_env87.fpregs[(hsw_env87.fpstt+1)&0x7];
    hsw_env87.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
    if ( hsw_env87.fpregs[hsw_env87.fpstt] < fpsrcop )
        hsw_env87.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
    else if ( hsw_env87.fpregs[hsw_env87.fpstt] == fpsrcop )
        hsw_env87.fpus |= 0x4000;	/* (C3,C2,C0) <-- 100 */
    /* else if ( hsw_env87.fpregs[hsw_env87.fpstt] > fpsrcop )  do nothing */
    /* else ( not comparable ) hsw_env87.fpus |= 0x4500 */
    hsw_env87.fpstt += 2;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_64m(unsigned char *mem_ref)
{
/* FISUBm16i_FSUBRPfromsti */
	hsw_env87.fpregs[hsw_env87.fpstt] -= GET16INT(mem_ref);
}

void
hsw_fp87_65r(int reg_num)
{
/* FISUBm16i_FSUBRPfromsti */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] -= hsw_env87.fpregs[hsw_env87.fpstt];
	hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_65m(unsigned char *mem_ref)
{
/* FISUBRm16i_FSUBPfromsti */
	hsw_env87.fpregs[hsw_env87.fpstt] = GET16INT(mem_ref) - hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_64r(int reg_num)
{
/* FISUBRm16i_FSUBPfromsti */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] = hsw_env87.fpregs[hsw_env87.fpstt] - hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
	hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_66m(unsigned char *mem_ref)
{
/* FIDIVm16i_FDIVRPtosti */
	double fptemp;
	fptemp=GET16INT(mem_ref);
	hsw_env87.fpregs[hsw_env87.fpstt] /= fptemp;
	if(fptemp==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
}

void
hsw_fp87_67r(int reg_num)
{
/* FIDIVm16i_FDIVRPtosti */
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] /= hsw_env87.fpregs[hsw_env87.fpstt];
	if(hsw_env87.fpregs[hsw_env87.fpstt]==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
	hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_67m(unsigned char *mem_ref)
{
/* FIDIVRm16i_FDIVPtosti */
	if(hsw_env87.fpregs[hsw_env87.fpstt]==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
	hsw_env87.fpregs[hsw_env87.fpstt] = GET16INT(mem_ref) / hsw_env87.fpregs[hsw_env87.fpstt];
}

void
hsw_fp87_66r(int reg_num)
{
/* FIDIVRm16i_FDIVPtosti */
	if(hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7]==0.0)hsw_env87.fpus |= (0x4&(hsw_env87.fpuc&0x3f));
	hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7] = hsw_env87.fpregs[hsw_env87.fpstt] / hsw_env87.fpregs[(hsw_env87.fpstt+reg_num)&0x7];
	hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_70m(unsigned char *mem_ref)
{
/* FILDm16i */
	hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
	hsw_env87.fpregs[hsw_env87.fpstt] = GET16INT(mem_ref);
}

void
hsw_fp87_70r(int reg_num)
{
/* FILDm16i */
    fprintf(stderr,"illegal FP opcode: 070r\n");
}

void
hsw_fp87_71m(unsigned char *mem_ref)
{
    fprintf(stderr,"illegal FP opcode: 071m\n");
}

void
hsw_fp87_71r(int reg_num)
{
    fprintf(stderr,"illegal FP opcode: 071r\n");
}

void
hsw_fp87_72m(unsigned char *mem_ref)
{
/* FISTm16i */
	double fpsrcop;
	signed short i16;
    fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
    if ( (hsw_env87.fpuc&0xc00) == 0xc00 ) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x0 ) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
                      floor(fpsrcop): ceil(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x400 ) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    i16 = fpsrcop;
    STORE16INT(mem_ref,i16);
}

void
hsw_fp87_72r(int reg_num)
{
/* FISTm16i */
    fprintf(stderr,"illegal FP opcode: 072r\n");
}

void
hsw_fp87_73m(unsigned char *mem_ref)
{
/* FISTPm16i */
	double fpsrcop;
	signed short i16;
    fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
    if ( (hsw_env87.fpuc&0xc00) == 0xc00 ) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x0 ) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
                      floor(fpsrcop): ceil(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x400 ) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    i16 = fpsrcop;
    STORE16INT(mem_ref,i16);
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_73r(int reg_num)
{
/* FISTPm16i */
    fprintf(stderr,"illegal FP opcode: 073r\n");
}

void
hsw_fp87_74m(unsigned char *mem_ref)
{
/* FBLDm80dec_FSTSWax */
	unsigned char * seg;
	double fpsrcop;
	int m32i;
    /* in this code, seg/m32i will be used as temporary ptr/int */
    seg = mem_ref + 8;
    if ( *seg-- != 0 || (*seg & 0xf0) != 0 ) /* d15..d17 non-zero*/
        fprintf(stderr,"This BCD number exceeds the range of DOUBLE REAL!\n");
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
    fpsrcop = ((double)m32i) * 100000000.0;
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
    fpsrcop += ((double)m32i);
    if ( *(seg+9) & 0x80 )  fpsrcop = -fpsrcop;
    hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
    hsw_env87.fpregs[hsw_env87.fpstt] = fpsrcop;
}

void
hsw_fp87_74r(int reg_num)
{
/* FBLDm80dec_FSTSWax */
    hsw_env87.fpus &= (~0x3800);
	hsw_env87.fpus |= (hsw_env87.fpstt&0x7)<<11;
	envp_global->rax.x.x.x = hsw_env87.fpus;
}

void
hsw_fp87_75m(unsigned char *mem_ref)
{
/* FILDm64i */
	double fptemp;
	long signed_temp;
	unsigned int i64lo, i64hi;
	i64lo = GET32INT((mem_ref));
	i64hi = GET32INT((mem_ref+4));
    hsw_env87.fpstt--;  hsw_env87.fpstt &= 0x7;
	signed_temp = 0;
	if ( i64hi & 0x80000000 ) {
        signed_temp = -1;
        i64lo = ~i64lo;
        i64hi = ~i64hi;
        if ( i64lo == 0xffffffff )  i64hi += 1;
        i64lo += 1;
    }
	if ( i64hi & 0xffe00000 )
	    fprintf(stderr,"This 64-bit INT exceeds range of DOUBLE REAL!\n"); 
    fptemp = 4294967296.0 * ((double)i64hi) + ((double)i64lo);
    if ( signed_temp < 0 )  fptemp = -fptemp;
    hsw_env87.fpregs[hsw_env87.fpstt] = fptemp;
}

void
hsw_fp87_75r(int reg_num)
{
/* FILDm64i */
    fprintf(stderr,"illegal FP opcode: 075r\n");
}

void
hsw_fp87_76m(unsigned char *mem_ref)
{
/* FBSTPm80dec */
	double fptemp;
	double fpsrcop;
	signed short i16;
    fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
    if ( (hsw_env87.fpuc&0xc00) == 0xc00 ) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x0 ) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
                      floor(fpsrcop): ceil(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x400 ) /*Round towards -INFI*/
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
        if ( fptemp == 0.0 )  { *mem_ref = i16; break; }
        fpsrcop = fptemp;
        fptemp = floor(fpsrcop/10.0);
        *mem_ref = i16 | (((int)(fpsrcop - fptemp*10.0)) << 4);
        fpsrcop = fptemp;
    }
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_76r(int reg_num)
{
/* FBSTPm80dec */
    fprintf(stderr,"illegal FP opcode: 076r\n");
}

void
hsw_fp87_77m(unsigned char *mem_ref)
{
/* FISTPm64i */
	double fpsrcop;
	long signed_temp;
	unsigned int i64lo, i64hi;
    fpsrcop = hsw_env87.fpregs[hsw_env87.fpstt];
    if ( (hsw_env87.fpuc&0xc00) == 0xc00 ) /*Chop towards zero*/
        fpsrcop = (fpsrcop<0.0)? -(floor(-fpsrcop)): floor(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x0 ) /*Round to the nearest*/
        fpsrcop = (fpsrcop-floor(fpsrcop) < ceil(fpsrcop)-fpsrcop)?
                      floor(fpsrcop): ceil(fpsrcop);
    else if ( (hsw_env87.fpuc&0xc00) == 0x400 ) /*Round towards -INFI*/
        fpsrcop = floor(fpsrcop);
    else  /*Round towards +INFI*/
        fpsrcop = ceil(fpsrcop);
    signed_temp = 0;
    if ( fpsrcop < 0.0 ) {
        signed_temp = -1;
        fpsrcop = -fpsrcop;
    }
    i64lo = ((unsigned int)fmod(fpsrcop,4294967296.0));
    i64hi = ((unsigned int)fpsrcop/4294967296.0);
    if ( signed_temp < 0 ) {
        i64lo = ~i64lo;
        i64hi = ~i64hi;
        if ( i64lo == 0xffffffff )  i64hi += 1;
        i64lo += 1;
    }
    STORE32INT((mem_ref),i64lo);
    STORE32INT((mem_ref+4),i64hi);
    hsw_env87.fpstt++;  hsw_env87.fpstt &= 0x7;
}

void
hsw_fp87_77r(int reg_num)
{
/* FISTPm64i */
    fprintf(stderr,"illegal FP opcode: 077r\n");
}
