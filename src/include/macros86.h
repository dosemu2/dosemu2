/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef __MACROS86_H_
#define __MACROS86_H_

#define SIM_INT(n,rtnlabel) \
	push ds !!!\
	push #0 !!!\
	pop ds !!!\
	pushf !!!\
	push cs !!!\
	push #rtnlabel !!!\
	push word ptr [(4*n)+2] !!!\
	push word ptr [(4*n)] !!!\
	retf !!!\
	rtnlabel: pop ds

#define FILL_BYTE(x,v) .REPT x db v!!!.ENDR
#define FILL_DWORD(x,v) .REPT x dd v!!!.ENDR
#define FILL_WORD(x,v) .REPT x dw v!!!.ENDR
#define FILL_SHORT(x,v) .REPT x .short v!!!.ENDR
#define FILL_LONG(x,v) .REPT x .long v!!!.ENDR
#define FILL_OPCODE(x,v) .REPT x v!!!.ENDR

#if 0
#define JMPL(label) db 0xe9!!!dw (label-(*+2)) ; jmp near label
#else
#define JMPL(label) br label ; 'branch' on as86 does a 'jmp near label'
#endif

#define JAL(label) dw 0x870f, (label-(*+4)) ; ja near label
#define JAEL(label) dw 0x830f, (label-(*+4)) ; jae near label
#define JBL(label) dw 0x820f, (label-(*+4)) ; jb  near label
#define JCL(label) dw 0x820f, (label-(*+4)) ; jc  near label
#define JBEL(label) dw 0x860f, (label-(*+4)) ; jbe  near label
#define JEL(label) dw 0x840f, (label-(*+4)) ; je  near label
#define JZL(label) dw 0x840f, (label-(*+4)) ; jz  near label
#define JGL(label) dw 0x8F0f, (label-(*+4)) ; jg  near label
#define JGEL(label) dw 0x8d0f, (label-(*+4)) ; jge  near label
#define JLL(label) dw 0x8C0f, (label-(*+4))  ; jl  near label
#define JLEL(label) dw 0x8E0f, (label-(*+4)) ; jle  near label
#define JNAL(label) dw 0x860f, (label-(*+4)) ; jna  near label
#define JNAEL(label) dw 0x820f, (label-(*+4)) ; jnae  near label
#define JNBL(label) dw 0x830f, (label-(*+4)) ; jnb  near label
#define JNCL(label) dw 0x830f, (label-(*+4)) ; jnc  near label
#define JNBEL(label) dw 0x870f, (label-(*+4)) ; jnbe  near label
#define JNEL(label) dw 0x850f, (label-(*+4)) ; jne  near label
#define JNGL(label) dw 0x8E0f, (label-(*+4)) ; jng  near label
#define JNGEL(label) dw 0x8c0f, (label-(*+4)) ; jnge  near label
#define JNLL(label) dw 0x8d0f, (label-(*+4)) ; jnl  near label
#define JNLEL(label) dw 0x8f0f, (label-(*+4)) ; jnle  near label
#define JNOL(label) dw 0x810f, (label-(*+4)) ; jno  near label
#define JNPL(label) dw 0x8B0f, (label-(*+4)) ; jnp  near label
#define JNSL(label) dw 0x890f, (label-(*+4)) ; jns  near label
#define JNZL(label) dw 0x850f, (label-(*+4)) ; jnz  near label
#define JOL(label) dw 0x800f, (label-(*+4)) ; jo  near label
#define JPL(label) dw 0x8A0f, (label-(*+4)) ; jp near label
#define JPEL(label) dw 0x8a0f, (label-(*+4)) ; jpe  near label
#define JPOL(label) dw 0x8b0f, (label-(*+4)) ; jpo  near label
#define JSL(label) dw 0x880f, (label-(*+4)) ; js near label


#endif
