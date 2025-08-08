/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "codegen.h"
#include "codegen-sim.h"
#ifdef X86_JIT
#include "codegen-x86.h"
#else
#define NodeLinker(LG,G)
#define NodeUnlinker(nG)
static inline unsigned char *CodeGen(unsigned char *CodePtr, unsigned char *BaseGenBuf, const IGen *IG) { return NULL; }
#define Exec_x86_asm(m,f,e,s) (*m=0, 0)
#define Exec_x86_asm_fpu(m,f,e,s,sf) (*m=0, 0)
#endif
