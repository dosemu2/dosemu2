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


/**********************************************************************

    @(#)hsw_interp.h	1.23
  
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

The maintainer of the Willows TWIN Libraries may be reached (Email) 
at the address twin@willows.com	

changes for use with dosemu-0.67 1997/10/20 vignani@mbox.vol.it
changes for use with dosemu-0.99 1998/12/13 vignani@mbox.vol.it

*********************************************************************/

#ifndef hsw_interp__h
#define hsw_interp__h
/* "@(#)hsw_interp.h	1.23 :/users/sccs/src/win/intp32/s.hsw_interp.h 1/23/97 17:25:08"  */

#include "windows.h"
#include "kerndef.h"
#include "BinTypes.h"
#include "DPMI.h"

#ifndef DOSEMU_TYPESONLY

#define ADDbfrm		0x00
#define ADDwfrm		0x01
#define ADDbtrm		0x02
#define ADDwtrm		0x03
#define ADDbia		0x04
#define ADDwia		0x05
#define PUSHes		0x06
#define POPes		0x07
#define ORbfrm		0x08
#define ORwfrm		0x09
#define ORbtrm		0x0a
#define ORwtrm		0x0b
#define ORbi		0x0c
#define ORwi		0x0d
#define PUSHcs		0x0e
#define TwoByteESC	0x0f

#define ADCbfrm		0x10
#define ADCwfrm		0x11
#define ADCbtrm		0x12
#define ADCwtrm		0x13
#define ADCbi		0x14
#define ADCwi		0x15
#define PUSHss		0x16
#define POPss		0x17
#define SBBbfrm		0x18
#define SBBwfrm		0x19
#define SBBbtrm		0x1a
#define SBBwtrm		0x1b
#define SBBbi		0x1c
#define SBBwi		0x1d
#define PUSHds		0x1e
#define POPds		0x1f

#define ANDbfrm		0x20
#define ANDwfrm		0x21
#define ANDbtrm		0x22
#define ANDwtrm		0x23
#define ANDbi		0x24
#define ANDwi		0x25
#define SEGes		0x26
#define DAA		0x27
#define SUBbfrm		0x28
#define SUBwfrm		0x29
#define SUBbtrm		0x2a
#define SUBwtrm		0x2b
#define SUBbi		0x2c
#define SUBwi		0x2d
#define SEGcs		0x2e
#define DAS		0x2f

#define XORbfrm		0x30
#define XORwfrm		0x31
#define XORbtrm		0x32
#define XORwtrm		0x33
#define XORbi		0x34
#define XORwi		0x35
#define SEGss		0x36
#define AAA		0x37
#define CMPbfrm		0x38
#define CMPwfrm		0x39
#define CMPbtrm		0x3a
#define CMPwtrm		0x3b
#define CMPbi		0x3c
#define CMPwi		0x3d
#define SEGds		0x3e
#define AAS		0x3f

#define INCax		0x40
#define INCcx		0x41
#define INCdx		0x42
#define INCbx		0x43
#define INCsp		0x44
#define INCbp		0x45
#define INCsi		0x46
#define INCdi		0x47
#define DECax		0x48
#define DECcx		0x49
#define DECdx		0x4a
#define DECbx		0x4b
#define DECsp		0x4c
#define DECbp		0x4d
#define DECsi		0x4e
#define DECdi		0x4f

#define PUSHax		0x50
#define PUSHcx		0x51
#define PUSHdx		0x52
#define PUSHbx		0x53
#define PUSHsp		0x54
#define PUSHbp		0x55
#define PUSHsi		0x56
#define PUSHdi		0x57
#define POPax		0x58
#define POPcx		0x59
#define POPdx		0x5a
#define POPbx		0x5b
#define POPsp		0x5c
#define POPbp		0x5d
#define POPsi		0x5e
#define POPdi		0x5f

#define PUSHA		0x60
#define POPA		0x61
#define BOUND		0x62
#define ARPL		0x63
#define SEGfs		0x64
#define SEGgs		0x65
#define OPERoverride	0x66
#define ADDRoverride	0x67
#define PUSHwi		0x68
#define IMULwrm		0x69 
#define PUSHbi		0x6a
#define IMULbrm		0x6b
#define INSb		0x6c
#define INSw		0x6d
#define OUTSb		0x6e
#define OUTSw		0x6f

#define JO		0x70
#define JNO		0x71
#define JB_JNAE		0x72
#define JNB_JAE		0x73
#define JE_JZ		0x74
#define JNE_JNZ		0x75
#define JBE_JNA		0x76
#define JNBE_JA		0x77
#define JS		0x78
#define JNS		0x79
#define JP_JPE		0x7a
#define JNP_JPO		0x7b
#define JL_JNGE		0x7c
#define JNL_JGE		0x7d
#define JLE_JNG		0x7e
#define JNLE_JG		0x7f

#define IMMEDbrm	0x80
#define IMMEDwrm	0x81
#define IMMEDbrm2	0x82
#define IMMEDisrm	0x83
#define TESTbrm		0x84
#define TESTwrm		0x85
#define XCHGbrm		0x86
#define XCHGwrm		0x87
#define MOVbfrm		0x88
#define MOVwfrm		0x89
#define MOVbtrm		0x8a
#define MOVwtrm		0x8b
#define MOVsrtrm	0x8c
#define LEA		0x8d
#define MOVsrfrm	0x8e
#define POPrm		0x8f

#define NOP		0x90
#define XCHGcx		0x91
#define XCHGdx		0x92
#define XCHGbx		0x93
#define XCHGsp		0x94
#define XCHGbp		0x95
#define XCHGsi		0x96
#define XCHGdi		0x97
#define CBW		0x98
#define CWD		0x99
#define CALLl		0x9a
#define WAIT		0x9b
#define PUSHF		0x9c
#define POPF		0x9d
#define SAHF		0x9e
#define LAHF		0x9f

#define MOVmal		0xa0
#define MOVmax		0xa1
#define MOValm		0xa2
#define MOVaxm		0xa3
#define MOVSb		0xa4
#define MOVSw		0xa5
#define CMPSb		0xa6
#define CMPSw		0xa7
#define	TESTbi		0xa8
#define TESTwi		0xa9
#define STOSb		0xaa
#define STOSw		0xab
#define LODSb		0xac
#define LODSw		0xad
#define SCASb		0xae
#define SCASw		0xaf

#define MOVial		0xb0
#define MOVicl		0xb1
#define MOVidl		0xb2
#define MOVibl		0xb3
#define MOViah		0xb4
#define MOVich		0xb5
#define MOVidh		0xb6
#define MOVibh		0xb7
#define MOViax		0xb8
#define MOVicx		0xb9
#define MOVidx		0xba
#define MOVibx		0xbb
#define MOVisp		0xbc
#define MOVibp		0xbd
#define MOVisi		0xbe
#define MOVidi		0xbf

#define SHIFTbi		0xc0
#define SHIFTwi		0xc1 
#define RETisp		0xc2
#define RET		0xc3
#define LES		0xc4
#define LDS		0xc5
#define MOVbirm		0xc6
#define MOVwirm		0xc7
#define ENTER		0xc8
#define LEAVE		0xc9 
#define RETlisp		0xca
#define RETl		0xcb
#define INT3		0xcc
#define INT		0xcd
#define INTO		0xce
#define IRET		0xcf

#define SHIFTb		0xd0
#define SHIFTw		0xd1
#define SHIFTbv		0xd2
#define SHIFTwv		0xd3
#define AAM		0xd4
#define AAD		0xd5
#define RESERVED1	0xd6
#define XLAT		0xd7
#define ESC0		0xd8
#define ESC1		0xd9
#define ESC2		0xda
#define ESC3		0xdb
#define ESC4		0xdc
#define ESC5		0xdd
#define ESC6		0xde
#define ESC7		0xdf

#define LOOPNZ_LOOPNE	0xe0
#define LOOPZ_LOOPE	0xe1
#define LOOP		0xe2
#define JCXZ		0xe3
#define INb		0xe4
#define INw		0xe5
#define OUTb		0xe6
#define OUTw		0xe7
#define CALLd		0xe8
#define JMPd		0xe9
#define JMPld		0xea
#define JMPsid		0xeb
#define INvb		0xec
#define INvw		0xed
#define OUTvb		0xee
#define OUTvw		0xef

#define LOCK		0xf0
#define REPNE		0xf2
#define REP		0xf3
#define HLT		0xf4
#define CMC		0xf5
#define GRP1brm 	0xf6
#define GRP1wrm 	0xf7
#define CLC		0xf8
#define STC		0xf9
#define CLI		0xfa
#define STI		0xfb
#define CLD		0xfc
#define STD		0xfd
#define GRP2brm		0xfe
#define GRP2wrm		0xff

#ifdef FP87 /* [ */
#define FADDm32r_sti	((ESC0<<3) | (0x0 & 0x38) >> 3)
#define FMULm32r_sti	((ESC0<<3) | (0x8 & 0x38) >> 3)
#define FCOMm32r_sti	((ESC0<<3) | (0x10 /*or 0xd0*/ & 0x38) >> 3)
#define FCOMPm32r_sti	((ESC0<<3) | (0x18 /*or 0xd8*/ & 0x38) >> 3)
#define FSUBm32r_sti	((ESC0<<3) | (0x20 /*or 0xe0*/ & 0x38) >> 3)
#define FSUBRm32r_sti	((ESC0<<3) | (0x28 /*or 0xe8*/ & 0x38) >> 3)
#define FDIVm32r_sti	((ESC0<<3) | (0x30 /*or 0xf0*/ & 0x38) >> 3)
#define FDIVRm32r_sti	((ESC0<<3) | (0x38 /*or 0xf8*/ & 0x38) >> 3)

#define FLDm32r_sti  	((ESC1<<3) | (0x0 & 0x38) >> 3)
#define FXCH        	((ESC1<<3) | (0xc8 & 0x38) >> 3)
#define FSTm32r_FNOP	((ESC1<<3) | (0x10 /*or 0xd0*/ & 0x38) >> 3)
#define FSTPm32r    	((ESC1<<3) | (0x18 & 0x38) >> 3)
#define FD9SLASH4 		((ESC1<<3) | (0x20 /*or 0xe0-0xe5*/ & 0x38) >> 3)
#define    FLDENV 		0x20
#define    FCHS 		0xe0
#define    FABS 		0xe1
#define    FTST 		0xe4
#define    FXAM 		0xe5
#define FLDCONST		((ESC1<<3) | (0xe8 /*or 0xe9-0xee*/ & 0x38) >> 3)
#define    FLDCW		0x28
#define    FLD1 		0xe8
#define    FLDL2T		0xe9
#define    FLDL2E		0xea
#define    FLDPI		0xeb
#define    FLDLG2		0xec
#define    FLDLN2		0xed
#define    FLDZ 		0xee
#define FD9SLASH6		((ESC1<<3) | (0x30 /*or 0xf0-0xf7*/ & 0x38) >> 3)
#define    FSTENV 		0x30
#define    F2XM1 		0xf0
#define    FYL2X 		0xf1
#define    FPTAN 		0xf2
#define    FPATAN 		0xf3
#define    FXTRACT 		0xf4
#define    FPREM1 		0xf5
#define    FDECSTP		0xf6
#define    FINCSTP		0xf7
#define FD9SLASH7		((ESC1<<3) | (0x38 /*or 0xf8-0xff*/ & 0x38) >> 3)
#define    FSTCW 		0x38
#define    FPREM 		0xf8
#define    FYL2XP1 		0xf9
#define    FSQRT 		0xfa
#define    FSINCOS 		0xfb
#define    FRNDINT 		0xfc
#define    FSCALE 		0xfd
#define    FSIN			0xfe
#define    FCOS			0xff

#define FADDm32i		((ESC2<<3) | (0x0 & 0x38) >> 3)
#define FMULm32i		((ESC2<<3) | (0x8 & 0x38) >> 3)
#define FICOMm32i		((ESC2<<3) | (0x10 & 0x38) >> 3)
#define FICOMPm32i		((ESC2<<3) | (0x18 & 0x38) >> 3)
#define FISUBm32i		((ESC2<<3) | (0x20 & 0x38) >> 3)
#define FISUBRm32i_FUCOMPPst1	((ESC2<<3) | (0x28 /*or 0xe9*/ & 0x38) >> 3)
#define FIDIVm32i		((ESC2<<3) | (0x30 & 0x38) >> 3)
#define FIDIVRm32i		((ESC2<<3) | (0x38 & 0x38) >> 3)

#define FILDm32i		((ESC3<<3) | (0x0 & 0x38) >> 3)
#define FISTm32i		((ESC3<<3) | (0x10 & 0x38) >> 3)
#define FISTPm32i		((ESC3<<3) | (0x18 & 0x38) >> 3)
#define FRSTORm94B_FINIT_FCLEX	((ESC3<<3) | (0x20 /*or 0xe3-2*/ & 0x38) >> 3)
#define FLDm80r  		((ESC3<<3) | (0x28 & 0x38) >> 3)
#define FSTPm80r  		((ESC3<<3) | (0x38 & 0x38) >> 3)

#define FADDm64r_tosti	((ESC4<<3) | (0x0 & 0x38) >> 3)
#define FMULm64r_tosti	((ESC4<<3) | (0x8 & 0x38) >> 3)
#define FCOMm64r     	((ESC4<<3) | (0x10 & 0x38) >> 3)
#define FCOMPm64r    	((ESC4<<3) | (0x18 & 0x38) >> 3)
#define FSUBm64r_FSUBRfromsti ((ESC4<<3) | (0x20 /*or 0xe0*/ & 0x38) >> 3)
#define FSUBRm64r_FSUBfromsti ((ESC4<<3) | (0x28 /*or 0xe8*/ & 0x38) >> 3)
#define FDIVm64r_FDIVRtosti   ((ESC4<<3) | (0x30 /*or 0xf0*/ & 0x38) >> 3)
#define FDIVRm64r_FDIVtosti   ((ESC4<<3) | (0x38 /*or 0xf8*/ & 0x38) >> 3)

#define FLDm64r_FFREE         ((ESC5<<3) | (0x0 /*or 0xc0*/ & 0x38) >> 3)
#define FSTm64r_sti           ((ESC5<<3) | (0x10 /*or 0xd0+i*/ & 0x38) >> 3)
#define FSTPm64r_sti          ((ESC5<<3) | (0x18 /*or 0xd8+i*/ & 0x38) >> 3)
#define FUCOMsti              ((ESC5<<3) | (0x20 /*or 0xe0+i*/ & 0x38) >> 3)
#define FUCOMPsti             ((ESC5<<3) | (0x28 /*or 0xe8+i*/ & 0x38) >> 3)
#define FSAVEm94B             ((ESC5<<3) | (0x30 & 0x38) >> 3)
#define FSTSWm16i             ((ESC5<<3) | (0x38 & 0x38) >> 3)

#define FADDm16i_tostipop     ((ESC6<<3) | (0x0 & 0x38) >> 3)
#define FMULm16i_tostipop     ((ESC6<<3) | (0x8 & 0x38) >> 3)
#define FICOMm16i             ((ESC6<<3) | (0x10 & 0x38) >> 3)
#define FICOMPm16i_FCOMPPst1  ((ESC6<<3) | (0x18 /*or 0xd9*/ & 0x38) >> 3)
#define FISUBm16i_FSUBRPfromsti ((ESC6<<3) | (0x20 /*or 0xe0*/ & 0x38) >> 3)
#define FISUBRm16i_FSUBPfromsti ((ESC6<<3) | (0x28 /*or 0xe8*/ & 0x38) >> 3)
#define FIDIVm16i_FDIVRPtosti ((ESC6<<3) | (0x30 /*or 0xf0*/ & 0x38) >> 3)
#define FIDIVRm16i_FDIVPtosti ((ESC6<<3) | (0x38 /*or 0xf8*/ & 0x38) >> 3)

#define FILDm16i		((ESC7<<3) | (0x0 & 0x38) >> 3)
#define FISTm16i		((ESC7<<3) | (0x10 & 0x38) >> 3)
#define FISTPm16i		((ESC7<<3) | (0x18 & 0x38) >> 3)
#define FBLDm80dec_FSTSWax		((ESC7<<3) | (0x20 /*or 0xe0*/ & 0x38) >> 3)
#define FILDm64i		((ESC7<<3) | (0x28 & 0x38) >> 3)
#define FBSTPm80dec		((ESC7<<3) | (0x30 & 0x38) >> 3)
#define FISTPm64i		((ESC7<<3) | (0x38 & 0x38) >> 3)

#endif	/* ] */

#define CARRY_FLAG		0x0001
/* reserved			0x0002 */
#define PARITY_FLAG		0x0004
/* reserved			0x0008 */
#define AUX_CARRY_FLAG		0x0010
/* reserved			0x0020 */
#define ZERO_FLAG		0x0040
#define SIGN_FLAG		0x0080
#define TRAP_FLAG		0x0100
#define INTERRUPT_FLAG		0x0200
#define DIRECTION_FLAG		0x0400
#define OVERFLOW_FLAG		0x0800
#define IOPL_FLAG_MASK		0x3000
#define NESTED_FLAG		0x4000
#define BYTE_FL			0x8000	/* Intel reserved! */
/* RF				0x10000 */
/* VM				0x20000 */
/* AC				0x40000 */

#define CPL	0x3000
#define IOPL	(env->flags&IOPL_FLAG_MASK)

#define BYTE_OP 0x45

/*
 * Test condition uses:	RES	SRC1,SRC2
 *	Z,NZ		 Y	    N
 *	S,NS		 Y	    N
 *	CY,NC		 Y	    N
 *	P,NP		 Y	    N
 *	OV,NO		 Y	    Y
 *	AF		 N	    Y
 *	CX,LOOP		 N	    N
 *
 */

/* quite tricky.... works because AUX_FLAG is 0x10 */
/*
 *		[                  ][------word--------] SRCn_16
 *		[        ][        ][--byte--][--aux---]
 *				      SRCn_8
 */
#define IS_AF_SET (BOOL)((int)((BYTE_FLAG==BYTE_OP)?\
	(((SRC1_8 & 0xf) + (SRC2_8 & 0xf)) & AUX_CARRY_FLAG):\
	(((SRC1_16 & 0xf) + (SRC2_16 & 0xf)) & AUX_CARRY_FLAG)) >> 4)

#define IS_CF_SET (BOOL)(CARRY & CARRY_FLAG)

/* overflow rule using bit15:
 *	src1 src2  res	b15
 *	  0    0    0    0
 *	  0    0    1    1
 *	  0    1    0    0
 *	  0    1    1    0
 *	  1    0    0    0
 *	  1    0    1    0
 *	  1    1    0    1
 *	  1    1    1    0
 */ 
#define IS_OF_SET ((WORD)((~(SRC1_16 ^ SRC2_16))&(SRC1_16 ^ RES_16))>>15)

#define IS_ZF_SET (BOOL)(RES_16 == 0)
#define IS_SF_SET (BOOL)((RES_8 >> 7) & 1)
#define IS_PF_SET (parity[((BYTE_FLAG==BYTE_OP)?RES_8:PAR_16)]==PARITY_FLAG)

#define CLEAR_CF CARRYB=0
#define CLEAR_AF { SRC1_16 &= ~0x808; SRC2_16 &= ~0x808; }
#define CLEAR_DF env->flags &= (~ DIRECTION_FLAG)

#define SET_CF CARRYB=1
#define SET_AF { SRC1_16 |= 0x808; SRC2_16 |= 0x808; }
#define SET_DF env->flags |= DIRECTION_FLAG

#define ROL	0
#define ROR	1
#define RCL	2
#define RCR	3
#define SHL	4
#define SHR	5
#define SAR	7

#define ADD	0
#define OR	1
#define ADC	2
#define SBB	3
#define AND	4
#define SUB	5
#define XOR	6
#define CMP	7

#define INC	0
#define DEC	1

#define TEST	0
#define NOT	2
#define NEG	3
#define MUL	4
#define IMUL	5
#define DIV	6
#define IDIV	7

#define AL env->rax.x.x.h.l
#define AH env->rax.x.x.h.h
#define AX env->rax.x.x.x
#define EAX env->rax.e
#define BL env->rbx.x.x.h.l
#define BH env->rbx.x.x.h.h
#define BX env->rbx.x.x.x
#define EBX env->rbx.e
#define CL env->rcx.x.x.h.l
#define CH env->rcx.x.x.h.h
#define CX env->rcx.x.x.x
#define ECX env->rcx.e
#define DL env->rdx.x.x.h.l
#define DH env->rdx.x.x.h.h
#define DX env->rdx.x.x.x
#define EDX env->rdx.e
#define SI env->rsi.si.si
#define ESI env->rsi.esi
#define DI env->rdi.di.di
#define EDI env->rdi.edi
#ifdef	BP
#undef	BP
#endif
#define BP env->rbp.bp.bp
#define EBP env->rbp.ebp
#ifdef	SP
#undef	SP
#endif
#define SP env->rsp.sp.sp
#define ESP env->rsp.esp
#define SHORT_CS_16 env->cs.word16.cs
#define SHORT_CS_32 env->cs.cs
#define SHORT_DS_16 env->ds.word16.ds
#define SHORT_DS_32 env->ds.ds
#define SHORT_ES_16 env->es.word16.es
#define SHORT_ES_32 env->es.es
#define SHORT_SS_16 env->ss.word16.ss
#define SHORT_SS_32 env->ss.ss
#define SHORT_FS_16 env->fs.word16.fs
#define SHORT_FS_32 env->fs.fs
#define SHORT_GS_16 env->gs.word16.gs
#define SHORT_GS_32 env->gs.gs
#define LONG_CS env->seg_regs[0]
#define LONG_DS env->seg_regs[1]
#define LONG_ES env->seg_regs[2]
#define LONG_SS env->seg_regs[3]
#define LONG_FS env->seg_regs[4]
#define LONG_GS env->seg_regs[5]
#define BIG_CS env->bigseg[0]
#define BIG_DS env->bigseg[1]
#define BIG_ES env->bigseg[2]
#define BIG_SS env->bigseg[3]
#define BIG_FS env->bigseg[4]
#define BIG_GS env->bigseg[5]

#define OVERRIDE env->seg_regs[6]
#define INVALID_OVR	((unsigned char *)(-1l))

#define SIGNb (res & 0x80)
#define SIGNw ((res & 0x8000) >> 8)
#define CARRYb ((res >> 8) & 0x1)
#define CARRYw ((res >> 16) & 0x1)
#define ZEROb (((unsigned char)(res) == 0) ? 0x40 : 0)
#define ZEROw (((unsigned short)(res) == 0) ? 0x40 : 0)
#define AUXCARRY 0
#define OVFLOWb (((!(res ^ src1)) & (!(src2 ^ src1)) << 4) & 0x800)
#define OVFLOWw (((!(res ^ src1)) & (!(src2 ^ src1)) >> 4) & 0x800)
#define PARITY (parity [(unsigned char)(res)])
#define ADDbFLAGS SIGNb | CARRYb | ZEROb | OVFLOWb | AUXCARRY | PARITY | flags & (~(DIRECTION_FLAG | INTERRUPT_FLAG))
#define ADDwFLAGS SIGNw | CARRYw | ZEROw | OVFLOWw | AUXCARRY | PARITY | flags & (~(DIRECTION_FLAG | INTERRUPT_FLAG))
#define LOGICbFLAGS SIGNb | ZEROb | PARITY | flags & (~(DIRECTION_FLAG | INTERRUPT_FLAG))
#define LOGICwFLAGS SIGNw | ZEROw | PARITY | flags & (~(DIRECTION_FLAG | INTERRUPT_FLAG))

#define AT_EXECUTE	1
#define AT_READ		2
#define AT_WRITE	3

#ifndef DOSEMU
#define ERRORLOG(s,o,m,r) fprintf(stderr,"Error accessing selector %x, offset %x, mode %x, errorcode %x\n",s,o,m,r)
#endif

#define UINT_86		2
#define INT_86		2
#define LP_86		4

#define JUMP(lp)	PC = PC + 2 +(signed char)(*(lp)); 

#if defined(DOSEMU) && defined(__i386__)
#define TOS_WORD	(BIG_SS? *((unsigned short *)(LONG_SS+ESP)):\
			*((unsigned short *)(LONG_SS+SP)))

#define PUSHWORD(w) if (BIG_SS) {ESP-=2; \
		*((unsigned short *)(LONG_SS+ESP))=(w);} else \
		{SP-=2; *((unsigned short *)(LONG_SS+SP))=(w);}
#define PUSHQUAD(dw) if (BIG_SS) {ESP-=4; \
		*((unsigned long *)(LONG_SS+ESP))=(dw);} else \
		{SP-=4; *((unsigned long *)(LONG_SS+SP))=(dw);}

#define POPWORD(w)  if (BIG_SS) {w=*((unsigned short *)(LONG_SS+ESP));\
		ESP+=2;} else {w=*((unsigned short *)(LONG_SS+SP)); SP+=2;}
#define POPQUAD(dw)  if (BIG_SS) {dw=*((unsigned long *)(LONG_SS+ESP));\
		ESP+=4;} else {dw=*((unsigned long *)(LONG_SS+SP)); SP+=4;}
#else
#define PUSHWORD(w) {unsigned char *sp=env->seg_regs[3]+env->rsp.sp.sp; \
		*(sp-2)=w; \
		*(sp-1)=w>>8; \
		env->rsp.sp.sp-=2;}
#define PUSHQUAD(w) {unsigned char *sp=env->seg_regs[3]+env->rsp.sp.sp; \
		*(sp-4)=w; \
		*(sp-3)=w>>8; \
		*(sp-2)=w>>16; \
		*(sp-1)=w>>24; \
		env->rsp.sp.sp-=4;}

#define POPWORD(w) {unsigned char *sp=env->seg_regs[3]+env->rsp.sp.sp; \
		w = *(sp) | (((unsigned short) (*(sp+1)))<<8); \
		env->rsp.sp.sp+=2;}
#define POPQUAD(w) {unsigned char *sp=env->seg_regs[3]+env->rsp.sp.sp; \
		w = *(sp) | (((unsigned short) *(sp+1))<<8) | \
			(((unsigned long) *(sp+2))<<16) | \
			(((unsigned long) *(sp+3))<<24); \
		env->rsp.sp.sp+=4;}
#endif	/* DOSEMU */

#define SELECTOR_PADDRESS(sel) GetPhysicalAddress(sel)

#ifdef DOSEMU
extern int SetSegreg(unsigned char **lp, unsigned char *big,
	unsigned long csel);
#define SET_SEGREG(lp,big,mk,sel)	SetSegreg(&(lp),&(big),(mk|sel))

#else
#define SET_SEGREG(lp,big,mk,sel) ({ \
	WORD wFlags; \
	char prnt_buf[40]; \
	if ((sel >> 3) == 0) \
	    lp = 0; \
	else { \
	    wFlags = GetSelectorFlags(sel); \
	    if (!(wFlags & DF_PRESENT)) { \
	        if (!LoadSegment(sel)) { \
		    sprintf(prnt_buf, \
		       "INTP32: failed to load selector %x\n",(unsigned int)sel); \
		    printf("%s\n",prnt_buf); \
		    FatalAppExit(0,prnt_buf); \
	        } \
	    } \
	    lp = GetPhysicalAddress(sel), 0 \
	} })

#endif	/* DOSEMU */

#endif	/* DOSEMU_TYPESONLY */

/* ======================= target-specific stuff ====================== */

#if defined(X386) 
/* use true 80-bit registers on i386 */
typedef	long double	Ldouble;
#else
/* use 64-bit doubles on other targets */
typedef	double	Ldouble;
#endif

typedef struct tagENV87
{   	Ldouble         fpregs[8];  /* floating point register stack */
	int             fpstt;      /* top of register stack */
	unsigned short  fpus, fpuc, fptag;
} ENV87;


/* X386 layout:
 *		[-----------------res------------------] RES_32
 *	CARRY	[------carry-------][------res16-------] RES_16
 *		[byte_op-][-carryb-][--res8--][parity16]
 *		BYTE_FLAG	      RES_8
 */
#if defined(sparc) || defined(mips) || defined(ppc) || defined(hppa) || defined(arm)
typedef	union {
		unsigned long res;
		struct {
			unsigned short carry;
			unsigned short res16;
		} word16;
		struct {
			unsigned char byte_op;
			unsigned char carryb;
			unsigned char res8;
			unsigned char parity16;
		} byte;
	} Interp_VAR_flags_czsp;
#elif defined(alpha)
typedef	union {
		unsigned long res;
		struct {
			unsigned short res16;
			unsigned short carry;
			unsigned int dummy1;
		} word16;
		struct {
			unsigned char parity16;
			unsigned char res8;
			unsigned char carryb;
			unsigned char byte_op;
			unsigned int dummy1;
		} byte;
	} Interp_VAR_flags_czsp;
#elif defined(X386) 
typedef	union {
		unsigned long res;
		struct {
			unsigned short res16;
			unsigned short carry;
		} word16;
		struct {
			unsigned char parity16;
			unsigned char res8;
			unsigned char carryb;
			unsigned char byte_op;
		} byte;
	} Interp_VAR_flags_czsp;
#endif

#if defined(sparc) || defined(mips) || defined(ppc) || defined(hppa) || defined(arm)
typedef struct keyENV{
	union {
		unsigned int ds;
		struct {
			unsigned short ds_top;
			unsigned short ds;
		} word16;
	} ds;
	union {
		unsigned int es;
		struct {
			unsigned short es_top;
			unsigned short es;
		} word16;
	} es;
	union {
		unsigned int ss;
		struct {
			unsigned short ss_top;
			unsigned short ss;
		} word16;
	} ss;
	unsigned long flags;
	union {
		unsigned long e;
		struct {
			unsigned short dummy;
			union {
				unsigned short x;
				struct {
					unsigned char h;
					unsigned char l;
				} h;
			} x;
		} x;
	} rax;
	union {
		unsigned long e;
		struct {
			unsigned short dummy;
			union {
				unsigned short x;
				struct {
					unsigned char h;
					unsigned char l;
				} h;
			} x;
		} x;
	} rbx;
	union {
		unsigned long e;
		struct {
			unsigned short dummy;
			union {
				unsigned short x;
				struct {
					unsigned char h;
					unsigned char l;
				} h;
			} x;
		} x;
	} rcx;
	union {
		unsigned long e;
		struct {
			unsigned short dummy;
			union {
				unsigned short x;
				struct {
					unsigned char h;
					unsigned char l;
				} h;
			} x;
		} x;
	} rdx;
	union {
		unsigned long esi;
		struct {
			unsigned short si_top;
			unsigned short si;
		} si;
	} rsi;
	union {
		unsigned long edi;
		struct {
			unsigned short di_top;
			unsigned short di;
		} di;
	} rdi;
	union {
		unsigned long ebp;
		struct {
			unsigned short bp_top;
			unsigned short bp;
		} bp;
	} rbp;
	union {
		unsigned long esp;
		struct {
			unsigned short sp_top;
			unsigned short sp;
		} sp;
	} rsp;
	union {
		unsigned int cs;
		struct {
			unsigned short cs_top;
			unsigned short cs;
		} word16;
	} cs;
	union {
		unsigned int fs;
		struct {
			unsigned short fs_top;
			unsigned short fs;
		} word16;
	} fs;
	union {
		unsigned int gs;
		struct {
			unsigned short gs_top;
			unsigned short gs;
		} word16;
	} gs;
	BINADDR		trans_addr;
	BINADDR		return_addr;
	LPVOID		machine_stack;
	struct ENV	*prev_env;
	unsigned long   active;
	unsigned long   is_catch;
	unsigned long   level;
	struct HSW_86_CATCHBUF *buf;
	jmp_buf		jump_buffer;
	unsigned char *seg_regs[7];
	Interp_VAR_flags_czsp flags_czsp;
	union {
		unsigned long longword;
		struct {
			unsigned short dummy;
			unsigned short word;
		} word16;
		struct {
			unsigned char dummy1;
			unsigned char dummy2;
			unsigned char byte;
 			unsigned char aux;
		} byte;
	} src1;
	union {
		unsigned long longword;
		struct {
			unsigned short dummy;
			unsigned short word;
		} word16;
		struct {
			unsigned char dummy1;
			unsigned char dummy2;
			unsigned char byte;
 			unsigned char aux;
		} byte;
	} src2;
	unsigned char *reg1;	/* from REG field of mod_rm */
	unsigned char *mem_ref;	/* from RM field of mod_rm */
	int mode_reg;
} Interp_ENV;
#endif

#if defined(X386) 
typedef struct keyENV{
	union {
		unsigned int ds;
		struct {
			unsigned short ds;
			unsigned short ds_top;
		} word16;
	} ds;
	union {
		unsigned int es;
		struct {
			unsigned short es;
			unsigned short es_top;
		} word16;
	} es;
	union {
		unsigned int ss;
		struct {
			unsigned short ss;
			unsigned short ss_top;
		} word16;
	} ss;
	unsigned long flags;
	union {
		unsigned long e;
		struct {
			union {
				unsigned short x;
				struct {
					unsigned char l;
					unsigned char h;
				} h;
			} x;
			unsigned short dummy;
		} x;
	} rax;
	union {
		unsigned long e;
		struct {
			union {
				unsigned short x;
				struct {
					unsigned char l;
					unsigned char h;
				} h;
			} x;
			unsigned short dummy;
		} x;
	} rbx;
	union {
		unsigned long e;
		struct {
			union {
				unsigned short x;
				struct {
					unsigned char l;
					unsigned char h;
				} h;
			} x;
			unsigned short dummy;
		} x;
	} rcx;
	union {
		unsigned long e;
		struct {
			union {
				unsigned short x;
				struct {
					unsigned char l;
					unsigned char h;
				} h;
			} x;
			unsigned short dummy;
		} x;
	} rdx;
	union {
		unsigned long esi;
		struct {
			unsigned short si;
			unsigned short si_top;
		} si;
	} rsi;
	union {
		unsigned long edi;
		struct {
			unsigned short di;
			unsigned short di_top;
		} di;
	} rdi;
	union {
		unsigned long ebp;
		struct {
			unsigned short bp;
			unsigned short bp_top;
		} bp;
	} rbp;
	union {
		unsigned long esp;
		struct {
			unsigned short sp;
			unsigned short sp_top;
		} sp;
	} rsp;
	union {
		unsigned int cs;
		struct {
			unsigned short cs;
			unsigned short cs_top;
		} word16;
	} cs;
	union {
		unsigned int fs;
		struct {
			unsigned short fs;
			unsigned short fs_top;
		} word16;
	} fs;
	union {
		unsigned int gs;
		struct {
			unsigned short gs;
			unsigned short gs_top;
		} word16;
	} gs;
	BINADDR		trans_addr;
	BINADDR		return_addr;
#ifdef DOSEMU
	unsigned long	error_addr;
#else
	LPVOID		machine_stack;
	struct ENV	*prev_env;
	unsigned long   active;
	unsigned long   is_catch;
	unsigned long   level;
	struct HSW_86_CATCHBUF *buf;
	jmp_buf		jump_buffer;
#endif
	unsigned char *seg_regs[7];
	Interp_VAR_flags_czsp flags_czsp;
/*
 *		[--------------longword----------------] SRC1_32
 *		[                  ][------word--------] SRC1_16
 *		[        ][        ][--byte--][--aux---]
 *				      SRC1_8    AUX1_8
 */
	union {
		unsigned long longword;
		struct {
			unsigned short word;
			unsigned short dummy;
		} word16;
		struct {
 			unsigned char aux;
			unsigned char byte;
			unsigned char dummy2;
			unsigned char dummy1;
		} byte;
	} src1;
/*
 *		[--------------longword----------------] SRC2_32
 *		[                  ][------word--------] SRC2_16
 *		[        ][        ][--byte--][--aux---]
 *				      SRC2_8    AUX2_8
 */
	union {
		unsigned long longword;
		struct {
			unsigned short word;
			unsigned short dummy;
		} word16;
		struct {
 			unsigned char aux;
			unsigned char byte;
			unsigned char dummy2;
			unsigned char dummy1;
		} byte;
	} src2;
	unsigned char *reg1;	/* from REG field of mod_rm */
	unsigned char *mem_ref;	/* from RM field of mod_rm */
	int mode_reg;
	unsigned char bigseg[6];
} Interp_ENV;
#endif

#if defined(alpha)
typedef struct keyENV{
	union {
	    unsigned long ds;
		struct {
			unsigned short ds;
			unsigned short ds_top;
			unsigned int dummy1;
		} word16;
	} ds;
	union {
		unsigned long es;
		struct {
			unsigned short es;
			unsigned short es_top;
			unsigned int dummy1;
		} word16;
	} es;
	union {
		unsigned long ss;
		struct {
			unsigned short ss;
			unsigned short ss_top;
			unsigned int dummy1;
		} word16;
	} ss;
	unsigned long flags;
	union {
		unsigned long e;
		struct {
			union {
				unsigned short x;
				struct {
					unsigned char l;
					unsigned char h;
				} h;
			} x;
			unsigned short dummy;
			unsigned int dummy1;
		} x;
	} rax;
	union {
		unsigned long e;
		struct {
			union {
				unsigned short x;
				struct {
					unsigned char l;
					unsigned char h;
				} h;
			} x;
			unsigned short dummy;
			unsigned int dummy1;
		} x;
	} rbx;
	union {
		unsigned long e;
		struct {
			union {
				unsigned short x;
				struct {
					unsigned char l;
					unsigned char h;
				} h;
			} x;
			unsigned short dummy;
			unsigned int dummy1;
		} x;
	} rcx;
	union {
		unsigned long e;
		struct {
			union {
				unsigned short x;
				struct {
					unsigned char l;
					unsigned char h;
				} h;
			} x;
			unsigned short dummy;
			unsigned int dummy1;
		} x;
	} rdx;
	union {
		unsigned long esi;
		struct {
			unsigned short si;
			unsigned short si_top;
			unsigned int dummy1;
		} si;
	} rsi;
	union {
		unsigned long edi;
		struct {
			unsigned short di;
			unsigned short di_top;
			unsigned int dummy1;
		} di;
	} rdi;
	union {
		unsigned long ebp;
		struct {
			unsigned short bp;
			unsigned short bp_top;
			unsigned int dummy1;
		} bp;
	} rbp;
	union {
		unsigned long esp;
		struct {
			unsigned short sp;
			unsigned short sp_top;
			unsigned int dummy1;
		} sp;
	} rsp;
	union {
		unsigned long cs;
		struct {
			unsigned short cs;
			unsigned short cs_top;
			unsigned int dummy1;
		} word16;
	} cs;
	union {
		unsigned long fs;
		struct {
			unsigned short fs;
			unsigned short fs_top;
			unsigned int dummy1;
		} word16;
	} fs;
	union {
		unsigned long gs;
		struct {
			unsigned short gs;
			unsigned short gs_top;
			unsigned int dummy1;
		} word16;
	} gs;
	BINADDR		trans_addr;
	BINADDR		return_addr;
	LPVOID		machine_stack;
	struct ENV	*prev_env;
	unsigned long   active;
	unsigned long   is_catch;
	unsigned long   level;
	struct HSW_86_CATCHBUF *buf;
	jmp_buf		jump_buffer;
	unsigned char *seg_regs[7];
	Interp_VAR_flags_czsp flags_czsp;
	union {
		unsigned long longword;
		struct {
			unsigned short word;
			unsigned short dummy;
			unsigned int dummy1;
		} word16;
		struct {
 			unsigned char aux;
			unsigned char byte;
			unsigned char dummy2;
			unsigned char dummy1;
			unsigned int dummy3;
		} byte;
	} src1;
	union {
		unsigned long longword;
		struct {
			unsigned short word;
			unsigned short dummy;
			unsigned int dummy1;
		} word16;
		struct {
 			unsigned char aux;
			unsigned char byte;
			unsigned char dummy2;
			unsigned char dummy1;
			unsigned int dummy3;
		} byte;
	} src2;
	unsigned char *reg1;	/* from REG field of mod_rm */
	unsigned char *mem_ref;	/* from RM field of mod_rm */
	int mode_reg;
} Interp_ENV;
#endif


#ifndef DOSEMU_TYPESONLY

/* CARRY is tricky, as it accesses also byte 3 (BYTE_OP); use with care
 * when writing into it, if in doubt use CARRYB -- AV */
#define CARRY env->flags_czsp.word16.carry
#define CARRYB env->flags_czsp.byte.carryb
#define RES_32 env->flags_czsp.res
#define RES_16 env->flags_czsp.word16.res16
#define RES_8 env->flags_czsp.byte.res8
#define PAR_16 env->flags_czsp.byte.parity16
#define AUX1_8 env->src1.byte.aux
#define AUX2_8 env->src2.byte.aux
#define BYTE_FLAG env->flags_czsp.byte.byte_op
#define SRC1_32 env->src1.longword
#define SRC2_32 env->src2.longword
#define SRC1_16 env->src1.word16.word
#define SRC2_16 env->src2.word16.word
#define SRC1_8 env->src1.byte.byte
#define SRC2_8 env->src2.byte.byte
#define HREG1 (unsigned char *)(env->reg1)
#define XREG1 (unsigned short *)(env->reg1)
#define EREG1 (DWORD *)(env->reg1)
#define MEM_REF env->mem_ref

#define IS_MODE_REG env->mode_reg

/* try to gain some speed/space/regs on a heavily-used macro... */
#if (GCC_VERSION_CODE>=2091) && defined(DOSEMU) && defined(__i686__)
/* a bonus for Intel chips only ... never mind */
#define ALLOW_OVERRIDE(default)	({ unsigned char *_v; \
	__asm__ __volatile__ ("movl	%1,%%eax\n \
	cmpl	$-1,%%eax\n \
	cmove	%2,%0\n \
	cmovne	%%eax,%0" \
	: "=r"(_v) \
	: "g"(OVERRIDE), "g"(default) \
	: "%eax", "memory"); \
	_v; })
#elif (GCC_VERSION_CODE>=2091) && defined(DOSEMU) && defined(__i586__)
#define ALLOW_OVERRIDE(default)	({ unsigned char *_v; \
	__asm__ __volatile__ ("movl	$-1,%%eax\n \
	movl	%1,%0\n \
	movl	%2,%%ecx\n \
	cmpxchgl	%%ecx,%0" \
	: "=&r"(_v) \
	: "g"(OVERRIDE), "g"(default) \
	: "%eax", "%ecx", "memory"); \
	_v; })
#else
/* plain version for old gcc */
#define ALLOW_OVERRIDE(default) ((OVERRIDE!=INVALID_OVR)?OVERRIDE:default)
#endif

#define FETCH_XREG(addr)	*(WORD *)addr
#define FETCH_EREG(addr)	*(DWORD *)addr
#define PUT_XREG(addr,data)	*(WORD *)addr=data
#define PUT_EREG(addr,data)	*(DWORD *)addr=data

/* ========================== FLAG setting ============================ */
/* Use similar macros to save the op state for later flag evaluation -
 * this greatly clarifies the source and avoids mistakes -- AV */
/*
 *		[-----------------res------------------]
 *	     	[----------0000000c][--res(0)-----00---]
 *		[01000101][        ][        ][        ]
 *
 * The parameter s is for the SUB flags setting (1) vs the ADD case (0).
 * s should always be a compile-time constant to make gcc skip the code
 * we don't need.
 *
 * SETBFLAGS requires definition of variables: src1,src2,res
 */
#define SETBFLAGS(s) { RES_32=((res&0x1ff)|(BYTE_OP<<16))<<8;\
	SRC1_8=src1;\
	if (s) SRC2_8=((src2&0xff)==0x80? 0:-src2); else SRC2_8=src2;}

/*
 *		[-----------------res------------------]
 *	     	[----------0000000c][--res(1)---res(0)-]
 *		[00000000][        ][        ][        ]
 *
 * The parameter s is for the SUB flags setting (1) vs the ADD case (0).
 * s should always be a compile-time constant to make gcc skip the code
 * we don't need.
 *
 * SETWFLAGS requires definition of variables: src1,src2,res
 */
#define SETWFLAGS(s) { RES_32=res&0x1ffff;\
	SRC1_16=src1;\
	if (s) SRC2_16=((src2&0xffff)==0x8000? 0:-src2); else SRC2_16=src2;}

/* add/sub rule for carry using bit31:
 *	src1 src2  res	b31(a) b31(s)
 *	  0    0    0    0	0
 *	  0    0    1    0	1
 *	  0    1    0    1	1
 *	  0    1    1    0	1
 *	  1    0    0    1	0
 *	  1    0    1    0	0
 *	  1    1    0    1	0
 *	  1    1    1    1	1
 *
 * This add/sub flag evaluation is tricky too. In 16-bit mode, the carry
 * flag is always at its correct position (bit 16), while in 32-bit mode
 * it has to be calculated from src1,src2,res. The original Willows code
 * had the correct definition for the ADD case, but totally failed in the
 * SUB/CMP case. It turned out that there isn't any simple expression
 * covering both cases, BUT that the sub carry is the inverse of what we
 * get for ADD if src2 is inverted (NOT negated!); see for yourself.
 */ 
#define IS_CF_SETD(s1,s2,r) ((unsigned)((((s1)^(s2)) & ~(r)) | ((s1)&(s2))) >> 31)

/*
 * for 32-bit ops we pack hi and lo bytes into a 16-bit value; this way
 * we can use the same tests for sign (bit 15) and parity (lo byte). It
 * is only necessary to add a case for the 32-bit zero condition; this is
 * done by setting bit 8 if the original value was not zero, so that 16-bit
 * zero test will give the correct answer -- AV
 */
#if (GCC_VERSION_CODE < 2092) && defined(DOSEMU) && defined(__i386__)
#define PACK32_16(sr,de)	__asm__ __volatile__ ("\
	bswap	%0\n\
	roll	$8,%0\n\
	testl	%0,%0\n\
	je	1f\n\
	orb	$1,%h0\n\
1:	movw	%w0,%w1"\
	: "=q"(sr),"=g"(*((short *)&de))\
	: "0"(sr)\
	: "memory" )
#elif (GCC_VERSION_CODE>=2092) && defined(DOSEMU) && defined(__i386__)
/* gcc 2.7.2.3 and egcs-1.1 versions do not like this one,
 * while development egcs versions accept it */
#define PACK32_16(sr,de)	__asm__ __volatile__ ("\
	bswap	%0\n\
	roll	$8,%0\n\
	testl	%0,%0\n\
	je	1f\n\
	orb	$1,%h0\n\
1:	movw	%w0,%w1"\
	: "=r"(sr),"=g"(*((short *)&de))\
	: "0"(sr)\
	: "memory" )
#else
#define PACK32_16(sr,de)	de=((((sr)>>16)&0xff00) |\
				((sr)&0xff) | ((sr)!=0? 0x100:0))
#endif

#if defined(DOSEMU) && defined(__i386__)
/*
 *		[-----------------res------------------]
 *	     	[----------sssssssc][--res(3)---res(0)-]
 *		[00000000][        ][-------1][        ]
 *
 * There are two parameters for the flag setting macros; c controls if
 * carry-setting flag has to be compiled in, s is for the SUB flags
 * setting (1) vs the ADD case (0). c ad s should always be compile-time
 * constants to make gcc skip the code we don't need.
 *
 * SETDFLAGS requires definition of variables: src1,src2,res
 */
#define SETDFLAGS(c,s) { register DWORD _res1=res;\
	PACK32_16(_res1,RES_16);\
	_res1=src1; PACK32_16(_res1,SRC1_16);\
	if (s) {_res1=(src2==0x80000000? 0:-src2); src2=~src2;}\
	else _res1=src2; PACK32_16(_res1,SRC2_16);\
	if (c) CARRY = IS_CF_SETD(src1,src2,res) ^ (s); else BYTE_FLAG=0;}

#else	/* original */

#define SETDFLAGS(c,s) { register DWORD res1=res;\
	if (s) src2 = ((src2==0x80000000)? 0:-src2);\
	if (d.emu>4) e_printf("S1=%#lx S2=%#lx R=%#lx\n",src1,src2,res);\
	PACK32_16(res,RES_16);\
	CARRY = IS_CF_SETD;\
	BYTE_FLAG = 0;\
	if (res1) RES_8 |= 0x1;\
	PACK32_16(src1,SRC1_16);\
	PACK32_16(src2,SRC2_16);}
#endif

#if defined(DOSEMU) && defined(__i386__)
#define	GETWORD(p)	(unsigned short)*((unsigned short *)(p))
#define PUTWORD(p,w)	{*((unsigned short *)(p))=(unsigned short)(w);}
#define PUTDWORD(p,dw)	{*((unsigned long *)(p))=(unsigned long)(dw);}
#define GETDWORD(p)	(unsigned long)*((unsigned long *)(p))
#else
#include "Endian.h"
#endif

#define	FETCH_WORD	GETWORD
#define	FETCH_QUAD	GETDWORD
#define	PUT_WORD	PUTWORD
#define	PUT_QUAD	PUTDWORD

/* These are for multiply/divide. I ASSUME that every version of gcc on
 * every 32-bit target has a 'long long' 64-bit type */
typedef union {
  long long sd;
  struct { long sl,sh; } s;
} s_i64_u;

typedef union {
  unsigned long long ud;
  struct { long ul,uh; } u;
} u_i64_u;

#ifdef DOSEMU
#include "cpu-emu.h"
#include "dosemu_debug.h"

#else
#define error(s...)	fprintf(stderr,##s)
extern void INT_handler(int, ENV *);
#endif

typedef void (*FUNCT_PTR)();

#ifdef DEBUG
extern void e_debug (Interp_ENV *, unsigned char *, unsigned char *,
  	int);
extern void e_debug_fp (ENV87 *);
#endif
/* debug registers (DRs) emulation */
extern int e_debug_check(unsigned char *);

extern unsigned char *
  hsw_interp_16_16 (Interp_ENV *, unsigned char *,
	unsigned char *, int *, int);
extern unsigned char *
  hsw_interp_16_32 (Interp_ENV *, unsigned char *,
	unsigned char *, int *);
extern unsigned char *
  hsw_interp_32_16 (Interp_ENV *, unsigned char *,
	unsigned char *, int *);
extern unsigned char *
  hsw_interp_32_32 (Interp_ENV *, unsigned char *,
	unsigned char *, int *, int);

extern int
  hsw_modrm_16_byte (Interp_ENV *, unsigned char *);
extern int
  hsw_modrm_16_word (Interp_ENV *, unsigned char *);
extern int
  hsw_modrm_16_quad (Interp_ENV *, unsigned char *);
extern int
  hsw_modrm_32_byte (Interp_ENV *, unsigned char *);
extern int
  hsw_modrm_32_word (Interp_ENV *, unsigned char *);
extern int
  hsw_modrm_32_quad (Interp_ENV *, unsigned char *);

extern void invoke_data (Interp_ENV *);

extern void
trans_flags_to_interp(Interp_ENV *, DWORD);
extern DWORD
trans_interp_flags(Interp_ENV *);

#ifdef DOSEMU
#define e_AL env->rax.x.x.h.l
#define e_AH env->rax.x.x.h.h
#define e_AX env->rax.x.x.x
#define e_EAX env->rax.e
#define e_BL env->rbx.x.x.h.l
#define e_BH env->rbx.x.x.h.h
#define e_BX env->rbx.x.x.x
#define e_EBX env->rbx.e
#define e_CL env->rcx.x.x.h.l
#define e_CH env->rcx.x.x.h.h
#define e_CX env->rcx.x.x.x
#define e_ECX env->rcx.e
#define e_DL env->rdx.x.x.h.l
#define e_DH env->rdx.x.x.h.h
#define e_DX env->rdx.x.x.x
#define e_EDX env->rdx.e
#define e_SI env->rsi.si.si
#define e_ESI env->rsi.esi
#define e_DI env->rdi.di.di
#define e_EDI env->rdi.edi
#define e_BP env->rbp.bp.bp
#define e_EBP env->rbp.ebp
#define e_SP env->rsp.sp.sp
#define e_ESP env->rsp.esp
#define e_CS env->cs.cs
#define e_DS env->ds.ds
#define e_ES env->es.es
#define e_SS env->ss.ss
#define e_FS env->fs.fs
#define e_GS env->gs.gs
#define e_EIP env->trans_addr
#define e_EFLAGS env->flags

#endif	/* DOSEMU */
#endif	/* DOSEMU_TYPESONLY */

extern BOOL code32, data32;
extern void FatalAppExit(UINT, LPCSTR);
extern unsigned long long EMUtime;

#define MK_CS	0x00000
#define MK_DS	0x10000
#define MK_ES	0x20000
#define MK_SS	0x30000
#define MK_FS	0x40000
#define MK_GS	0x50000

/*
 * INT 00 C  - CPU-generated - DIVIDE ERROR
 * INT 01 C  - CPU-generated - SINGLE STEP
 * INT 02 C  - external hardware - NON-MASKABLE INTERRUPT
 * INT 03 C  - CPU-generated - BREAKPOINT
 * INT 04 C  - CPU-generated - INTO DETECTED OVERFLOW
 * INT 05 C  - CPU-generated (80186+) - BOUND RANGE EXCEEDED
 * INT 06 C  - CPU-generated (80286+) - INVALID OPCODE
 * INT 07 C  - CPU-generated (80286+) - PROCESSOR EXTENSION NOT AVAILABLE
 * INT 08 C  - CPU-generated (80286+) - DOUBLE EXCEPTION DETECTED
 * INT 09 C  - CPU-generated (80286,80386) - PROCESSOR EXTENSION PROTECTION ERROR
 * INT 0A CP - CPU-generated (80286+) - INVALID TASK STATE SEGMENT
 * INT 0B CP - CPU-generated (80286+) - SEGMENT NOT PRESENT
 * INT 0C C  - CPU-generated (80286+) - STACK FAULT
 * INT 0D C  - CPU-generated (80286+) - GENERAL PROTECTION VIOLATION
 * INT 0E C  - CPU-generated (80386+ native mode) - PAGE FAULT
 * INT 10 C  - CPU-generated (80286+) - COPROCESSOR ERROR
 * INT 11    - CPU-generated (80486+) - ALIGNMENT CHECK
 * INT 12    - CPU-generated (Pentium) - MACHINE CHECK EXCEPTION
 */
#define EXCP00_DIVZ	1
#define EXCP01_SSTP	2
#define EXCP02_NMI	3
#define EXCP03_INT3	4
#define EXCP04_INTO	5
#define EXCP05_BOUND	6
#define EXCP06_ILLOP	7
#define EXCP07_PREX	8
#define EXCP08_DBLE	9
#define EXCP09_XERR	10
#define EXCP0A_TSS	11
#define EXCP0B_NOSEG	12
#define EXCP0C_STACK	13
#define EXCP0D_GPF	14
#define EXCP0E_PAGE	15
#define EXCP10_COPR	17
#define EXCP11_ALGN	18
#define EXCP12_MCHK	19

#define EXCP_GOBACK	64
#define EXCP_SIGNAL	65

#ifdef TWIN

extern void hsw_fp87_00m(), hsw_fp87_01m(), hsw_fp87_02m(), hsw_fp87_03m();
extern void hsw_fp87_04m(), hsw_fp87_05m(), hsw_fp87_06m(), hsw_fp87_07m();
extern void hsw_fp87_10m(), hsw_fp87_11m(), hsw_fp87_12m(), hsw_fp87_13m();
extern void hsw_fp87_14m(), hsw_fp87_15m(), hsw_fp87_16m(), hsw_fp87_17m();
extern void hsw_fp87_20m(), hsw_fp87_21m(), hsw_fp87_22m(), hsw_fp87_23m();
extern void hsw_fp87_24m(), hsw_fp87_25m(), hsw_fp87_26m(), hsw_fp87_27m();
extern void hsw_fp87_30m(), hsw_fp87_31m(), hsw_fp87_32m(), hsw_fp87_33m();
extern void hsw_fp87_34m(), hsw_fp87_35m(), hsw_fp87_36m(), hsw_fp87_37m();
extern void hsw_fp87_40m(), hsw_fp87_41m(), hsw_fp87_42m(), hsw_fp87_43m();
extern void hsw_fp87_44m(), hsw_fp87_45m(), hsw_fp87_46m(), hsw_fp87_47m();
extern void hsw_fp87_50m(), hsw_fp87_51m(), hsw_fp87_52m(), hsw_fp87_53m();
extern void hsw_fp87_54m(), hsw_fp87_55m(), hsw_fp87_56m(), hsw_fp87_57m();
extern void hsw_fp87_60m(), hsw_fp87_61m(), hsw_fp87_62m(), hsw_fp87_63m();
extern void hsw_fp87_64m(), hsw_fp87_65m(), hsw_fp87_66m(), hsw_fp87_67m();
extern void hsw_fp87_70m(), hsw_fp87_71m(), hsw_fp87_72m(), hsw_fp87_73m();
extern void hsw_fp87_74m(), hsw_fp87_75m(), hsw_fp87_76m(), hsw_fp87_77m();
extern void hsw_fp87_00r(), hsw_fp87_01r(), hsw_fp87_02r(), hsw_fp87_03r();
extern void hsw_fp87_04r(), hsw_fp87_05r(), hsw_fp87_06r(), hsw_fp87_07r();
extern void hsw_fp87_10r(), hsw_fp87_11r(), hsw_fp87_12r(), hsw_fp87_13r();
extern void hsw_fp87_14r(), hsw_fp87_15r(), hsw_fp87_16r(), hsw_fp87_17r();
extern void hsw_fp87_20r(), hsw_fp87_21r(), hsw_fp87_22r(), hsw_fp87_23r();
extern void hsw_fp87_24r(), hsw_fp87_25r(), hsw_fp87_26r(), hsw_fp87_27r();
extern void hsw_fp87_30r(), hsw_fp87_31r(), hsw_fp87_32r(), hsw_fp87_33r();
extern void hsw_fp87_34r(), hsw_fp87_35r(), hsw_fp87_36r(), hsw_fp87_37r();
extern void hsw_fp87_40r(), hsw_fp87_41r(), hsw_fp87_42r(), hsw_fp87_43r();
extern void hsw_fp87_44r(), hsw_fp87_45r(), hsw_fp87_46r(), hsw_fp87_47r();
extern void hsw_fp87_50r(), hsw_fp87_51r(), hsw_fp87_52r(), hsw_fp87_53r();
extern void hsw_fp87_54r(), hsw_fp87_55r(), hsw_fp87_56r(), hsw_fp87_57r();
extern void hsw_fp87_60r(), hsw_fp87_61r(), hsw_fp87_62r(), hsw_fp87_63r();
extern void hsw_fp87_64r(), hsw_fp87_65r(), hsw_fp87_66r(), hsw_fp87_67r();
extern void hsw_fp87_70r(), hsw_fp87_71r(), hsw_fp87_72r(), hsw_fp87_73r();
extern void hsw_fp87_74r(), hsw_fp87_75r(), hsw_fp87_76r(), hsw_fp87_77r();

extern FUNCT_PTR hsw_fp87_mem0[8];
extern FUNCT_PTR hsw_fp87_mem1[8];
extern FUNCT_PTR hsw_fp87_mem2[8];
extern FUNCT_PTR hsw_fp87_mem3[8];
extern FUNCT_PTR hsw_fp87_mem4[8];
extern FUNCT_PTR hsw_fp87_mem5[8];
extern FUNCT_PTR hsw_fp87_mem6[8];
extern FUNCT_PTR hsw_fp87_mem7[8];
extern FUNCT_PTR hsw_fp87_reg0[8];
extern FUNCT_PTR hsw_fp87_reg1[8];
extern FUNCT_PTR hsw_fp87_reg2[8];
extern FUNCT_PTR hsw_fp87_reg3[8];
extern FUNCT_PTR hsw_fp87_reg4[8];
extern FUNCT_PTR hsw_fp87_reg5[8];
extern FUNCT_PTR hsw_fp87_reg6[8];
extern FUNCT_PTR hsw_fp87_reg7[8];

extern BYTE parity[];
extern char unknown_msg[], illegal_msg[], unsupp_msg[];
extern ENV87 hsw_env87;

#endif	/* TWIN */

#endif /* hsw_interp__h */
