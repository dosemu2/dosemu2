/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _NETBSD_VM86_H_
#define  _NETBSD_VM86_H_

#include <machine/sysarch.h>
#include <machine/vm86.h>
#define CPU_086 VCPU_086
#define CPU_186 VCPU_186
#define CPU_286 VCPU_286
#define CPU_386 VCPU_386
#define CPU_486 VCPU_486
#define CPU_586 VCPU_586


/* compat with linux defines: */
#include <machine/psl.h>
#define VIP_MASK	PSL_VIP
#define	VIF_MASK	PSL_VIF
#define	TF_MASK		PSL_T
#define	IF_MASK		PSL_I
#define	NT_MASK		PSL_NT
#define	VM_MASK		PSL_VM
#define	AC_MASK		PSL_AC
#define	ID_MASK		PSL_ID

#endif
