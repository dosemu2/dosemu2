/* 
 * Copyright (c) 1997 Steffen Winterfeldt (original code)
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Low-level assembler functions for remapping 2D images.
 *
 * Note that these functions mostly call other functions that
 * have been calculated in remap.c.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1997/08/07: Fixed some bugs when remapping very narrow images.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * DANG_END_CHANGELOG
 *
 */

#ifdef __i386__

#include "remap.h"

		.globl pent_8to16_test
		.globl pent_8to16_1
		.globl pent_8to16_all

		.globl pent_8to32_1
		.globl pent_8to32_all

		.globl pent_8to8_1
		.globl pent_8to8_all

		.globl pent_8to8p_all

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
		.equ LocalVars, 16
		.equ StackSize, 16+4*LocalVars	/* pushed 4 regs */

		.equ Loc0,  16+0
		.equ Loc1,  16+4
		.equ Loc2,  16+8
		.equ Loc3,  16+12
		.equ Loc4,  16+16
		.equ Loc5,  16+20
		.equ Loc6,  16+24
		.equ Loc7,  16+28
		.equ Loc8,  16+32
		.equ Loc9,  16+36
		.equ Loc10, 16+40
		.equ Loc11, 16+44
		.equ Loc12, 16+48
		.equ Loc13, 16+52
		.equ Loc14, 16+56
		.equ Loc15, 16+60

		.equ Arg0, StackSize+4

		.text

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define xmi		(%ebp)

#define p_xmi		Loc0(%esp)
#define src0		Loc1(%esp)
#define x_len		Loc2(%esp)
#define x_rest		Loc3(%esp)
#define x_wrap		Loc4(%esp)
#define y_cnt		Loc5(%esp)
#define bre_y		Loc6(%esp)
#define bre_x		Loc7(%esp)

		.align 2,0x90
pent_8to16_test:
		subl $4*LocalVars,%esp
		pushl %ebp
		pushl %edi
		pushl %esi
		pushl %ebx
		movl Arg0(%esp),%ebp

		movl %ebp,p_xmi
		movl ro_src_image xmi,%esi
		movl ro_dst_image xmi,%ebx
		movl ro_dst_offset xmi,%eax
		movl %esi,src0
		leal (%ebx,%eax,1),%edi

		movl ro_dst_width xmi,%ecx
		movl ro_dst_scan_len xmi,%eax
		movl %ecx,%edx
		leal (%ecx,%ecx,1),%ebx
		shrl $2,%ecx
		andl $3,%edx
		movl %ecx,x_len
		movl %edx,x_rest
		subl %ebx,%eax
		movl %eax,x_wrap

		movl ro_dst_y0 xmi,%edx
		movl ro_dst_y1 xmi,%eax
		subl %edx,%eax
		shll $2,%edx
		addl ro_bre_y xmi,%edx
		movl %eax,y_cnt
		movl %edx,bre_y
		movl ro_bre_x xmi,%eax
		movl %eax,bre_x

		movl ro_true_color_lut xmi,%ebp
		/* xmi no longer accessible !*/

		cmpl $0,y_cnt
		jle pent16_test_90

pent16_test_10:
		movl bre_y,%edx
		movl x_len,%ecx
		movl (%edx),%esi
		xorl %eax,%eax
		addl src0,%esi
		movl bre_x,%ebx

		orl %ecx,%ecx
		jz pent16_test_50

pent16_test_20:

		movb (%esi),%al
		movl 0x400(%ebp,%eax,4),%edx
		addl (%ebx),%esi

		movb (%esi),%al
		addl 0x800(%ebp,%eax,4),%edx
		addl 4(%ebx),%esi
		movl %edx,(%edi)

		movb (%esi),%al
		movl 0x400(%ebp,%eax,4),%edx
		addl 8(%ebx),%esi

		movb (%esi),%al
		addl 0x800(%ebp,%eax,4),%edx
		addl 12(%ebx),%esi
		movl %edx,4(%edi)

                addl $8,%edi
		addl $16,%ebx

		decl %ecx
		jnz pent16_test_20

pent16_test_50:

		movl x_rest,%ecx
		orl %ecx,%ecx
		jz pent16_test_70

pent16_test_60:

		movb (%esi),%al
		movl 0x400(%ebp,%eax,4),%edx
		addl (%ebx),%esi
		movw %dx,(%edi)

		addl $4,%ebx
		addl $2,%edi

		decl %ecx
		jnz pent16_test_60

pent16_test_70:
		addl $4,bre_y
		addl x_wrap,%edi

		decl y_cnt
		jnz pent16_test_10

pent16_test_90:

		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		addl $4*LocalVars,%esp
		ret

#undef xmi
#undef p_xmi
#undef src0
#undef x_len
#undef x_rest
#undef x_wrap
#undef y_cnt
#undef bre_y
#undef bre_x


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define xmi		(%ebp)

#define y_cnt		Loc0(%esp)
#define x_rest_len	Loc1(%esp)
#define x_cnt_len	Loc2(%esp)
#define dst_skip	Loc3(%esp)
#define x_cnt		Loc4(%esp)
#define x_rest		Loc5(%esp)
#define src_skip	Loc6(%esp)
#define p_xmi		Loc7(%esp)

		.align 2, 0x90
pent_8to16_1:
		subl $4*LocalVars,%esp
		pushl %ebp
		pushl %edi
		pushl %esi
		pushl %ebx
		movl Arg0(%esp),%ebp
		movl %ebp,p_xmi

		movl ro_src_image xmi,%esi
		movl ro_dst_image xmi,%edi
		addl ro_src_offset xmi,%esi
		addl ro_dst_offset xmi,%edi

		movl ro_src_y1 xmi,%eax
		subl ro_src_y0 xmi,%eax
		movl %eax,y_cnt
		
		movl ro_src_x1 xmi,%eax
		subl ro_src_x0 xmi,%eax
		movl %eax,x_rest_len
		movl %eax,%edx
		andl $0xf,x_rest_len
		shrl $4,%eax
		movl %eax,x_cnt_len

		movl ro_src_scan_len xmi, %eax
		subl %edx,%eax
		movl %eax,src_skip
		addl %edx,%edx		/* 2 bytes/pixel */
		movl ro_dst_scan_len xmi, %eax
		subl %edx,%eax
		movl %eax,dst_skip
		movl ro_true_color_lut xmi,%ebp
		/* xmi no longer accessible !*/

		cmpl $0,y_cnt
		jle pent16_1_90

pent16_1_10:
		movl x_cnt_len,%eax
		movl x_rest_len,%edx
		movl %eax,x_cnt
		movl %edx,x_rest
		xorl %eax,%eax
		xorl %edx,%edx
		cmpl %eax,x_cnt_len
		jz pent16_1_30

pent16_1_20:
		movb (%esi),%al
		movb 4(%esi),%dl
		movl 0x400(%ebp,%eax,4),%ebx
		movl 0x400(%ebp,%edx,4),%ecx
		movb 1(%esi),%al
		movb 5(%esi),%dl
		addl 0x800(%ebp,%eax,4),%ebx
		addl 0x800(%ebp,%edx,4),%ecx
		movl %ebx,(%edi)
		movl %ecx,8(%edi)
		movb 2(%esi),%al
		movb 6(%esi),%dl
		movl 0x400(%ebp,%eax,4),%ebx
		movl 0x400(%ebp,%edx,4),%ecx
		movb 3(%esi),%al
		movb 7(%esi),%dl
		addl 0x800(%ebp,%eax,4),%ebx
		addl 0x800(%ebp,%edx,4),%ecx
		movl %ebx,4(%edi)
		movl %ecx,12(%edi)
		movb 8(%esi),%al
		movb 12(%esi),%dl
		movl 0x400(%ebp,%eax,4),%ebx
		movl 0x400(%ebp,%edx,4),%ecx
		movb 9(%esi),%al
		movb 13(%esi),%dl
		addl 0x800(%ebp,%eax,4),%ebx
		addl 0x800(%ebp,%edx,4),%ecx
		movl %ebx,16(%edi)
		movl %ecx,24(%edi)
		movb 10(%esi),%al
		movb 14(%esi),%dl
		movl 0x400(%ebp,%eax,4),%ebx
		movl 0x400(%ebp,%edx,4),%ecx
		movb 11(%esi),%al
		movb 15(%esi),%dl
		addl 0x800(%ebp,%eax,4),%ebx
		addl 0x800(%ebp,%edx,4),%ecx
		movl %ebx,20(%edi)
		movl %ecx,28(%edi)

		addl $16,%esi
		addl $32,%edi

		decl x_cnt
		jnz pent16_1_20

pent16_1_30:
		testl $8,x_rest
		jz pent16_1_40
		movb (%esi),%al
		movb 4(%esi),%dl
		movl 0x400(%ebp,%eax,4),%ebx
		movl 0x400(%ebp,%edx,4),%ecx
		movb 1(%esi),%al
		movb 5(%esi),%dl
		addl 0x800(%ebp,%eax,4),%ebx
		addl 0x800(%ebp,%edx,4),%ecx
		movl %ebx,(%edi)
		movl %ecx,8(%edi)
		movb 2(%esi),%al
		movb 6(%esi),%dl
		movl 0x400(%ebp,%eax,4),%ebx
		movl 0x400(%ebp,%edx,4),%ecx
		movb 3(%esi),%al
		movb 7(%esi),%dl
		addl 0x800(%ebp,%eax,4),%ebx
		addl 0x800(%ebp,%edx,4),%ecx
		movl %ebx,4(%edi)
		movl %ecx,12(%edi)
		addl $8,%esi
		addl $16,%edi
pent16_1_40:
		testl $4,x_rest
		jz pent16_1_50
		movb (%esi),%al
		movb 2(%esi),%dl
		movl 0x400(%ebp,%eax,4),%ebx
		movl 0x400(%ebp,%edx,4),%ecx
		movb 1(%esi),%al
		movb 3(%esi),%dl
		addl 0x800(%ebp,%eax,4),%ebx
		addl 0x800(%ebp,%edx,4),%ecx
		movl %ebx,(%edi)
		movl %ecx,4(%edi)
		addl $4,%esi
		addl $8,%edi
pent16_1_50:
		testl $2,x_rest
		jz pent16_1_60
		movb (%esi),%al
		movl 0x400(%ebp,%eax,4),%ebx
		movb 1(%esi),%al
		addl 0x800(%ebp,%eax,4),%ebx
		movl %ebx,(%edi)
		addl $2,%esi
		addl $4,%edi
pent16_1_60:
		testl $1,x_rest
		jz pent16_1_70
		movb (%esi),%al
		movl 0x400(%ebp,%eax,4),%ebx
		movw %bx,(%edi)
		incl %esi
		addl $2,%edi
pent16_1_70:

		addl src_skip,%esi
		addl dst_skip,%edi

		decl y_cnt
		jnz pent16_1_10

pent16_1_90:

		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		addl $4*LocalVars,%esp
		ret

#undef xmi
#undef y_cnt
#undef x_rest_len
#undef x_cnt_len
#undef dst_skip
#undef x_cnt
#undef x_rest
#undef src_skip
#undef p_xmi


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define xmi		(%ebp)

#define p_xmi		Loc0(%esp)
#define src0		Loc1(%esp)
#define y_cnt		Loc2(%esp)
#define bre_y		Loc3(%esp)
#define r_func		Loc4(%esp)

		.align 2,0x90
pent_8to16_all:
		subl $4*LocalVars,%esp
		pushl %ebp
		pushl %edi
		pushl %esi
		pushl %ebx
		movl Arg0(%esp),%ebp

		movl %ebp,p_xmi
		movl ro_src_image xmi,%esi
		movl ro_dst_image xmi,%ebx
		movl ro_dst_offset xmi,%eax
		movl %esi,src0
		leal (%ebx,%eax,1),%edi

		movl ro_dst_y0 xmi,%edx
		movl ro_dst_y1 xmi,%eax
		subl %edx,%eax
		movl ro_remap_line xmi,%ecx
		shll $2,%edx
		movl %ecx,r_func
		addl ro_bre_y xmi,%edx
		movl %eax,y_cnt
		movl %edx,bre_y

		movl ro_true_color_lut xmi,%ebx
		leal 0x400(%ebx),%ecx
		leal 0x800(%ebx),%edx

		cmpl $0,y_cnt
		jle pent16_all_90

		xorl %eax,%eax
		/* xmi no longer accessible !*/

pent16_all_10:
		movl src0,%esi
		movl bre_y,%ebp
		addl (%ebp),%esi

		call *r_func

		addl $4,bre_y
		decl y_cnt

		jnz pent16_all_10

pent16_all_90:

		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		addl $4*LocalVars,%esp
		ret

#undef xmi
#undef p_xmi
#undef src0
#undef y_cnt
#undef bre_y
#undef r_func


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define xmi		(%ebp)

#define y_cnt		Loc0(%esp)
#define x_rest_len	Loc1(%esp)
#define x_cnt_len	Loc2(%esp)
#define dst_skip	Loc3(%esp)
#define x_cnt		Loc4(%esp)
#define x_rest		Loc5(%esp)
#define src_skip	Loc6(%esp)
#define p_xmi		Loc7(%esp)

		.align 2, 0x90
pent_8to32_1:
		subl $4*LocalVars,%esp
		pushl %ebp
		pushl %edi
		pushl %esi
		pushl %ebx
		movl Arg0(%esp),%ebp
		movl %ebp,p_xmi

		movl ro_src_image xmi,%esi
		movl ro_dst_image xmi,%edi
		addl ro_src_offset xmi,%esi
		addl ro_dst_offset xmi,%edi

		movl ro_src_y1 xmi,%eax
		subl ro_src_y0 xmi,%eax
		movl %eax,y_cnt
		
		movl ro_src_x1 xmi,%eax
		subl ro_src_x0 xmi,%eax
		movl %eax,x_rest_len

		movl %eax,%edx
		andl $0xf,x_rest_len
		shrl $4,%eax
		movl %eax,x_cnt_len

		movl ro_src_scan_len xmi, %eax
		subl %edx,%eax
		movl %eax,src_skip
		shll $2,%edx		/* 4 bytes/pixel */
		movl ro_dst_scan_len xmi, %eax
		subl %edx,%eax
		movl %eax,dst_skip
		movl ro_true_color_lut xmi,%ebp
		/* xmi no longer accessible !*/

		cmpl $0,y_cnt
		jle pent32_1_90

pent32_1_10:
		movl x_cnt_len,%eax
		movl x_rest_len,%edx
		movl %eax,x_cnt
		movl %edx,x_rest
		xorl %eax,%eax
		xorl %edx,%edx
		cmpl %eax,x_cnt_len
		jz pent32_1_30

pent32_1_20:
		movb (%esi),%al
		movb 4(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,(%edi)
		movl %ecx,16(%edi)
		movb 1(%esi),%al
		movb 5(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,4(%edi)
		movl %ecx,20(%edi)
		movb 2(%esi),%al
		movb 6(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,8(%edi)
		movl %ecx,24(%edi)
		movb 3(%esi),%al
		movb 7(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,12(%edi)
		movl %ecx,28(%edi)
		movb 8(%esi),%al
		movb 12(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,32(%edi)
		movl %ecx,48(%edi)
		movb 9(%esi),%al
		movb 13(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,36(%edi)
		movl %ecx,52(%edi)
		movb 10(%esi),%al
		movb 14(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,40(%edi)
		movl %ecx,56(%edi)
		movb 11(%esi),%al
		movb 15(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,44(%edi)
		movl %ecx,60(%edi)

                addl $16,%esi
                addl $64,%edi

		decl x_cnt
		jnz pent32_1_20

pent32_1_30:
		testl $8,x_rest
		jz pent32_1_40
		movb (%esi),%al
		movb 4(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,(%edi)
		movl %ecx,16(%edi)
		movb 1(%esi),%al
		movb 5(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,4(%edi)
		movl %ecx,20(%edi)
		movb 2(%esi),%al
		movb 6(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,8(%edi)
		movl %ecx,24(%edi)
		movb 3(%esi),%al
		movb 7(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,12(%edi)
		movl %ecx,28(%edi)
		addl $8,%esi
		addl $32,%edi
pent32_1_40:
		testl $4,x_rest
		jz pent32_1_50
		movb (%esi),%al
		movb 1(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,(%edi)
		movl %ecx,4(%edi)
		movb 2(%esi),%al
		movb 3(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,8(%edi)
		movl %ecx,12(%edi)
		addl $4,%esi
		addl $16,%edi
pent32_1_50:
		testl $2,x_rest
		jz pent32_1_60
		movb (%esi),%al
		movb 1(%esi),%dl
		movl (%ebp,%eax,4),%ebx
		movl (%ebp,%edx,4),%ecx
		movl %ebx,(%edi)
		movl %ecx,4(%edi)
		addl $2,%esi
		addl $8,%edi
pent32_1_60:
		testl $1,x_rest
		jz pent32_1_70
		movb (%esi),%al
		movl (%ebp,%eax,4),%ebx
		movl %ebx,(%edi)
		incl %esi
		addl $4,%edi
pent32_1_70:

		addl src_skip,%esi
		addl dst_skip,%edi

		decl y_cnt
		jnz pent32_1_10

pent32_1_90:

		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		addl $4*LocalVars,%esp
		ret

#undef xmi
#undef y_cnt
#undef x_rest_len
#undef x_cnt_len
#undef dst_skip
#undef x_cnt
#undef x_rest
#undef src_skip
#undef p_xmi


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define xmi		(%ebp)

#define p_xmi		Loc0(%esp)
#define src0		Loc1(%esp)
#define y_cnt		Loc2(%esp)
#define bre_y		Loc3(%esp)
#define r_func		Loc4(%esp)

		.align 2,0x90
pent_8to32_all:
		subl $4*LocalVars,%esp
		pushl %ebp
		pushl %edi
		pushl %esi
		pushl %ebx
		movl Arg0(%esp),%ebp

		movl %ebp,p_xmi
		movl ro_src_image xmi,%esi
		movl ro_dst_image xmi,%ebx
		movl ro_dst_offset xmi,%eax
		movl %esi,src0
		leal (%ebx,%eax,1),%edi

		movl ro_dst_y0 xmi,%edx
		movl ro_dst_y1 xmi,%eax
		subl %edx,%eax
		movl ro_remap_line xmi,%ecx
		shll $2,%edx
		movl %ecx,r_func
		addl ro_bre_y xmi,%edx
		movl %eax,y_cnt
		movl %edx,bre_y

		movl ro_true_color_lut xmi,%ebx

		cmpl $0,y_cnt
		jle pent32_all_90

		xorl %eax,%eax
		xorl %ecx,%ecx
		/* xmi no longer accessible !*/

pent32_all_10:
		movl src0,%esi
		movl bre_y,%ebp
		addl (%ebp),%esi

		call *r_func

		addl $4,bre_y
		decl y_cnt

		jnz pent32_all_10

pent32_all_90:

		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		addl $4*LocalVars,%esp
		ret

#undef xmi
#undef p_xmi
#undef src0
#undef y_cnt
#undef bre_y
#undef r_func


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define xmi		(%ebp)

#define y_cnt		Loc0(%esp)
#define x_rest_len	Loc1(%esp)
#define x_cnt_len	Loc2(%esp)
#define dst_skip	Loc3(%esp)
#define x_cnt		Loc4(%esp)
#define x_rest		Loc5(%esp)
#define src_skip	Loc6(%esp)
#define p_xmi		Loc7(%esp)

		.align 2, 0x90
pent_8to8_1:
		subl $4*LocalVars,%esp
		pushl %ebp
		pushl %edi
		pushl %esi
		pushl %ebx
		movl Arg0(%esp),%ebp
		movl %ebp,p_xmi

		movl ro_src_image xmi,%esi
		movl ro_dst_image xmi,%edi
		addl ro_src_offset xmi,%esi
		addl ro_dst_offset xmi,%edi

		movl ro_src_y1 xmi,%eax
		subl ro_src_y0 xmi,%eax
		movl %eax,y_cnt
		
		movl ro_src_x1 xmi,%eax
		subl ro_src_x0 xmi,%eax
		movl %eax,x_rest_len
		movl %eax,%edx
		andl $0xf,x_rest_len
		shrl $4,%eax
		movl %eax,x_cnt_len

		movl ro_src_scan_len xmi, %eax
		subl %edx,%eax
		movl %eax,src_skip
		movl ro_dst_scan_len xmi, %eax
		subl %edx,%eax
		movl %eax,dst_skip
		movl ro_dst_y0 xmi,%eax
		movl ro_true_color_lut xmi,%ebp
		/* xmi no longer accessible !*/

		cmpl $0,y_cnt
		jle pent8_1_90

		andl $1,%eax
		addl %eax,%eax
		addl %eax,%ebp

pent8_1_10:
		movl x_cnt_len,%eax
		movl x_rest_len,%edx
		movl %eax,x_cnt
		movl %edx,x_rest
		xorl %eax,%eax
		xorl %edx,%edx
		cmpl %eax,x_cnt_len
		jz pent8_1_30

pent8_1_20:
		movb 2(%esi),%al
		movb 6(%esi),%dl
		movb (%ebp,%eax,4),%bl
		movb (%ebp,%edx,4),%cl
		movb 3(%esi),%al
		movb 7(%esi),%dl
		movb 1(%ebp,%eax,4),%bh
		movb 1(%ebp,%edx,4),%ch
		shll $16,%ebx
		movb (%esi),%al
		shll $16,%ecx
		movb 4(%esi),%dl
		movb (%ebp,%eax,4),%bl
		movb (%ebp,%edx,4),%cl
		movb 1(%esi),%al
		movb 5(%esi),%dl
		movb 1(%ebp,%eax,4),%bh
		movb 1(%ebp,%edx,4),%ch
		movl %ebx,(%edi)
		movl %ecx,4(%edi)
		movb 10(%esi),%al
		movb 14(%esi),%dl
		movb (%ebp,%eax,4),%bl
		movb (%ebp,%edx,4),%cl
		movb 11(%esi),%al
		movb 15(%esi),%dl
		movb 1(%ebp,%eax,4),%bh
		movb 1(%ebp,%edx,4),%ch
		shll $16,%ebx
		movb 8(%esi),%al
		shll $16,%ecx
		movb 12(%esi),%dl
		movb (%ebp,%eax,4),%bl
		movb (%ebp,%edx,4),%cl
		movb 9(%esi),%al
		movb 13(%esi),%dl
		movb 1(%ebp,%eax,4),%bh
		movb 1(%ebp,%edx,4),%ch
		movl %ebx,8(%edi)
		movl %ecx,12(%edi)

		addl $16,%esi
		addl $16,%edi

		decl x_cnt
		jnz pent8_1_20

pent8_1_30:
		testl $8,x_rest
		jz pent8_1_40
		movb 2(%esi),%al
		movb 6(%esi),%dl
		movb (%ebp,%eax,4),%bl
		movb (%ebp,%edx,4),%cl
		movb 3(%esi),%al
		movb 7(%esi),%dl
		movb 1(%ebp,%eax,4),%bh
		movb 1(%ebp,%edx,4),%ch
		shll $16,%ebx
		movb (%esi),%al
		shll $16,%ecx
		movb 4(%esi),%dl
		movb (%ebp,%eax,4),%bl
		movb (%ebp,%edx,4),%cl
		movb 1(%esi),%al
		movb 5(%esi),%dl
		movb 1(%ebp,%eax,4),%bh
		movb 1(%ebp,%edx,4),%ch
		movl %ebx,(%edi)
		movl %ecx,4(%edi)
		addl $8,%esi
		addl $8,%edi
pent8_1_40:
		testl $4,x_rest
		jz pent8_1_50
		movb 2(%esi),%al
		movb 3(%esi),%dl
		movb (%ebp,%eax,4),%bl
		movb 1(%ebp,%edx,4),%bh
		movb (%esi),%al
		shll $16,%ebx
		movb 1(%esi),%dl
		movb (%ebp,%eax,4),%bl
		movb 1(%ebp,%edx,4),%bh
		movl %ebx,(%edi)
		addl $4,%esi
		addl $4,%edi
pent8_1_50:
		testl $2,x_rest
		jz pent8_1_60
		movb (%esi),%al
		movb 1(%esi),%dl
		movb (%ebp,%eax,4),%bl
		movb 1(%ebp,%edx,4),%bh
		movw %bx,(%edi)
		addl $2,%esi
		addl $2,%edi
pent8_1_60:
		testl $1,x_rest
		jz pent8_1_70
		movb (%esi),%al
		movb (%ebp,%eax,4),%bl
		movb %bl,(%edi)
		incl %esi
		incl %edi
pent8_1_70:

		addl src_skip,%esi
		addl dst_skip,%edi

		xorl $2,%ebp

		decl y_cnt
		jnz pent8_1_10

pent8_1_90:

		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		addl $4*LocalVars,%esp
		ret

#undef xmi
#undef y_cnt
#undef x_rest_len
#undef x_cnt_len
#undef dst_skip
#undef x_cnt
#undef x_rest
#undef src_skip
#undef p_xmi


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define xmi		(%ebp)

#define p_xmi		Loc0(%esp)
#define src0		Loc1(%esp)
#define y_cnt		Loc2(%esp)
#define bre_y		Loc3(%esp)
#define r_func		Loc4(%esp)

		.align 2,0x90
pent_8to8_all:
		subl $4*LocalVars,%esp
		pushl %ebp
		pushl %edi
		pushl %esi
		pushl %ebx
		movl Arg0(%esp),%ebp

		movl %ebp,p_xmi
		movl ro_src_image xmi,%esi
		movl ro_dst_image xmi,%ebx
		movl ro_dst_offset xmi,%eax
		movl %esi,src0
		leal (%ebx,%eax,1),%edi

		movl ro_dst_y0 xmi,%edx
		movl ro_dst_y1 xmi,%eax
		subl %edx,%eax
		movl ro_remap_line xmi,%ecx
		shll $2,%edx
		movl %ecx,r_func
		addl ro_bre_y xmi,%edx
		movl %eax,y_cnt
		movl %edx,bre_y

		movl ro_true_color_lut xmi,%ecx
		movl ro_dst_y0 xmi,%eax
		andl $1,%eax
		addl %eax,%eax
		addl %eax,%ecx

		cmpl $0,y_cnt
		jle pent8_all_90

		xorl %eax,%eax
		xorl %ebx,%ebx

pent8_all_10:
		movl src0,%esi
		movl bre_y,%ebp
		addl (%ebp),%esi

		call *r_func

		addl $4,bre_y
		xorl $2,%ecx
		decl y_cnt

		jnz pent8_all_10

pent8_all_90:

		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		addl $4*LocalVars,%esp
		ret

#undef xmi
#undef p_xmi
#undef src0
#undef y_cnt
#undef bre_y
#undef r_func


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
#define xmi		(%ebp)

#define p_xmi		Loc0(%esp)
#define src0		Loc1(%esp)
#define y_cnt		Loc2(%esp)
#define bre_y		Loc3(%esp)
#define r_func		Loc4(%esp)

		.align 2,0x90
pent_8to8p_all:
		subl $4*LocalVars,%esp
		pushl %ebp
		pushl %edi
		pushl %esi
		pushl %ebx
		movl Arg0(%esp),%ebp

		movl %ebp,p_xmi
		movl ro_src_image xmi,%esi
		movl ro_dst_image xmi,%ebx
		movl ro_dst_offset xmi,%eax
		movl %esi,src0
		leal (%ebx,%eax,1),%edi

		movl ro_dst_y0 xmi,%edx
		movl ro_dst_y1 xmi,%eax
		subl %edx,%eax
		movl ro_remap_line xmi,%ecx
		shll $2,%edx
		movl %ecx,r_func
		addl ro_bre_y xmi,%edx
		movl %eax,y_cnt
		movl %edx,bre_y

		cmpl $0,y_cnt
		jle pent8p_all_90

pent8p_all_10:
		movl src0,%esi
		movl bre_y,%ebp
		addl (%ebp),%esi

		xorl %eax,%eax
		xorl %ebx,%ebx
		xorl %ecx,%ecx
		xorl %edx,%edx

		call *r_func

		addl $4,bre_y
		decl y_cnt

		jnz pent8p_all_10

pent8p_all_90:

		popl %ebx
		popl %esi
		popl %edi
		popl %ebp
		addl $4*LocalVars,%esp
		ret

#undef xmi
#undef p_xmi
#undef src0
#undef y_cnt
#undef bre_y
#undef r_func


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

		.align 2,0x90

#endif

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
