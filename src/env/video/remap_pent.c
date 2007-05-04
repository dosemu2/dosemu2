/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* Pentium optimized remapping functions */

#ifdef __i386__

#include "config.h"
#include <stdio.h>

#include "remap.h"

void _a_ret(CodeObj *);
void _a_movb_dl_dh(CodeObj *);
void _a_movb_dh_dl(CodeObj *);
void _a_movb_n_ecx_4eax_dl(CodeObj *, int);
void _a_movb_n_ecx_4eax_dh(CodeObj *, int);
void _a_movb_n_ecx_4ebx_dl(CodeObj *, int);
void _a_movb_n_ecx_4ebx_dh(CodeObj *, int);
void _a_movl_ebx_4eax_ebp(CodeObj *);
void _a_movl_ebx_4ecx_edx(CodeObj *);
void _a_movl_ecx_4eax_ebp(CodeObj *);
void _a_movl_edx_4eax_ebp(CodeObj *);
void _a_addl_edx_4eax_ebp(CodeObj *);
void _a_movb_esi_al(CodeObj *, int);
void _a_movb_esi_bl(CodeObj *, int);
void _a_movb_esi_cl(CodeObj *, int);
void _a_movb_esi_dl(CodeObj *, int);
void _a_movb_esi_dh(CodeObj *, int);
void _a_movl_ebp_edi(CodeObj *, int);
void _a_movw_bp_edi(CodeObj *, int);
void _a_movl_edx_edi(CodeObj *, int);
void _a_movw_dx_edi(CodeObj *, int);
void _a_movb_dl_edi(CodeObj *, int);
void _a_addl_n_edi(CodeObj *, int);
void _a_addl_n_esi(CodeObj *, int);
void _a_shrl_n_ebp(CodeObj *, int);
void _a_shll_n_edx(CodeObj *, int);

/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * some functions that generate specific instructions
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if GCC_VERSION_CODE >= 2095
  /*
   * NOTE: 
   *	gcc-2.95 will optimize away all code behind the 'return'
   *	and the function end (closing curly bracket),
   *	hence it will remove our code generator templates.
   *	To fool gcc, we define 'return' to be conditional on a variable
   *	_we_ know will ever be true.
   *	For this we define START_TEMPLATE, which contains this
   *	conditional return statement:
   */
  int trick_out_gcc295_for_remap = 1;
  #define START_TEMPLATE if (trick_out_gcc295_for_remap) return;
#else
  /*
   * For all gcc below gcc-2.95 we use a normal return statement:
   */
  #define START_TEMPLATE return;
#endif

void _a_ret(CodeObj *co)
{
  __label__ _cf_001;
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tret\n", co->pc);
#endif
  code_append_ins(co, 1, &&_cf_001);
  START_TEMPLATE
  _cf_001: asm("ret\n");
}

void _a_movb_dl_dh(CodeObj *co)
{
  __label__ _cf_001;
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb %%dl,%%dh\n", co->pc);
#endif
  code_append_ins(co, 2, &&_cf_001);
  START_TEMPLATE
  _cf_001: asm("movb %dl,%dh\n");
}

void _a_movb_dh_dl(CodeObj *co)
{
  __label__ _cf_001;
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb %%dh,%%dl\n", co->pc);
#endif
  code_append_ins(co, 2, &&_cf_001);
  START_TEMPLATE
  _cf_001: asm("movb %dh,%dl\n");
}

void _a_movb_n_ecx_4eax_dl(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb (%%ecx,%%eax,4),%%dl\n", co->pc);
#endif
    code_append_ins(co, 3, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb (%ecx,%eax,4),%dl\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb $%d(%%ecx,%%eax,4),%%dl\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 3, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb 0x1(%ecx,%eax,4),%dl\n");
    return;
  }
  code_append_ins(co, 3, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb 0x100(%ecx,%eax,4),%dl\n");
  return; /* gcc >= 2.96 wants a statement after label */
}

void _a_movb_n_ecx_4eax_dh(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb (%%ecx,%%eax,4),%%dh\n", co->pc);
#endif
    code_append_ins(co, 3, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb (%ecx,%eax,4),%dh\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb $%d(%%ecx,%%eax,4),%%dh\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 3, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb 0x1(%ecx,%eax,4),%dh\n");
    return;
  }
  code_append_ins(co, 3, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb 0x100(%ecx,%eax,4),%dh\n");
  return; /* gcc >= 2.96 wants a statement after label */
}

void _a_movb_n_ecx_4ebx_dl(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb (%%ecx,%%ebx,4),%%dl\n", co->pc);
#endif
    code_append_ins(co, 3, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb (%ecx,%ebx,4),%dl\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb $%d(%%ecx,%%ebx,4),%%dl\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 3, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb 0x1(%ecx,%ebx,4),%dl\n");
    return;
  }
  code_append_ins(co, 3, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb 0x100(%ecx,%ebx,4),%dl\n");
  return; /* gcc >= 2.96 wants a statement after label */
}

void _a_movb_n_ecx_4ebx_dh(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb (%%ecx,%%ebx,4),%%dh\n", co->pc);
#endif
    code_append_ins(co, 3, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb (%ecx,%ebx,4),%dh\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb $%d(%%ecx,%%ebx,4),%%dh\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 3, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb 0x1(%ecx,%ebx,4),%dh\n");
    return;
  }
  code_append_ins(co, 3, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb 0x100(%ecx,%ebx,4),%dh\n");
  return; /* gcc >= 2.96 wants a statement after label */
}

void _a_movl_ebx_4eax_ebp(CodeObj *co)
{
  __label__ _cf_001;
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovl (%%ebx,%%eax,4),%%ebp\n", co->pc);
#endif
  code_append_ins(co, 3, &&_cf_001);
  START_TEMPLATE
  _cf_001: asm("movl (%ebx,%eax,4),%ebp\n");
}

void _a_movl_ebx_4ecx_edx(CodeObj *co)
{
  __label__ _cf_001;
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovl (%%ebx,%%ecx,4),%%edx\n", co->pc);
#endif
  code_append_ins(co, 3, &&_cf_001);
  START_TEMPLATE
  _cf_001: asm("movl (%ebx,%ecx,4),%edx\n");
}

void _a_movl_ecx_4eax_ebp(CodeObj *co)
{
  __label__ _cf_001;
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovl (%%ecx,%%eax,4),%%ebp\n", co->pc);
#endif
  code_append_ins(co, 3, &&_cf_001);
  START_TEMPLATE
  _cf_001: asm("movl (%ecx,%eax,4),%ebp\n");
}

void _a_movl_edx_4eax_ebp(CodeObj *co)
{
  __label__ _cf_001;
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovl (%%edx,%%eax,4),%%ebp\n", co->pc);
#endif
  code_append_ins(co, 3, &&_cf_001);
  START_TEMPLATE
  _cf_001: asm("movl (%edx,%eax,4),%ebp\n");
}

void _a_addl_edx_4eax_ebp(CodeObj *co)
{
  __label__ _cf_001;
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\taddl (%%edx,%%eax,4),%%ebp\n", co->pc);
#endif
  code_append_ins(co, 3, &&_cf_001);
  START_TEMPLATE
  _cf_001: asm("addl (%edx,%eax,4),%ebp\n");
}

void _a_movb_esi_al(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb (%%esi),%%al\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb (%esi),%al\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb $%d(%%esi),%%al\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb 0x1(%esi),%al\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb 0x100(%esi),%al\n");
}

void _a_movb_esi_bl(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb (%%esi),%%bl\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb (%esi),%bl\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb $%d(%%esi),%%bl\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb 0x1(%esi),%bl\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb 0x100(%esi),%bl\n");
}

void _a_movb_esi_cl(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb (%%esi),%%cl\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb (%esi),%cl\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb $%d(%%esi),%%cl\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb 0x1(%esi),%cl\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb 0x100(%esi),%cl\n");
}

void _a_movb_esi_dl(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb (%%esi),%%dl\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb (%esi),%dl\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb $%d(%%esi),%%dl\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb 0x1(%esi),%dl\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb 0x100(%esi),%dl\n");
}

void _a_movb_esi_dh(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb (%%esi),%%dh\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb (%esi),%dh\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb $%d(%%esi),%%dh\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb 0x1(%esi),%dh\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb 0x100(%esi),%dh\n");
}

void _a_movl_ebp_edi(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovl %%ebp,(%%edi)\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movl %ebp,(%edi)\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovl %%ebp,$%d(%%edi)\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movl %ebp,0x1(%edi)\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movl %ebp,0x100(%edi)\n");
}

void _a_movw_bp_edi(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovw %%bp,(%%edi)\n", co->pc);
#endif
    code_append_ins(co, 3, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movw %bp,(%edi)\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovw %%bp,$%d(%%edi)\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 3, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movw %bp,0x1(%edi)\n");
    return;
  }
  code_append_ins(co, 3, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movw %bp,0x100(%edi)\n");
}

void _a_movl_edx_edi(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovl %%edx,(%%edi)\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movl %edx,(%edi)\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovl %%edx,$%d(%%edi)\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movl %edx,0x1(%edi)\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movl %edx,0x100(%edi)\n");
}

void _a_movw_dx_edi(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovw %%dx,(%%edi)\n", co->pc);
#endif
    code_append_ins(co, 3, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movw %dx,(%edi)\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovw %%dx,$%d(%%edi)\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 3, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movw %dx,0x1(%edi)\n");
    return;
  }
  code_append_ins(co, 3, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movw %dx,0x100(%edi)\n");
}

void _a_movb_dl_edi(CodeObj *co, int ofs)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!ofs) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tmovb %%dl,(%%edi)\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("movb %dl,(%edi)\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tmovb %%dl,$%d(%%edi)\n", co->pc, ofs);
#endif
  if(ofs >=-128 && ofs <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &ofs);
    START_TEMPLATE
    _cf_002: asm("movb %dl,0x1(%edi)\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &ofs);
  START_TEMPLATE
  _cf_003: asm("movb %dl,0x100(%edi)\n");
}

void _a_addl_n_edi(CodeObj *co, int n)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!n) return;

  if(n == 1) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tincl %%edi\n", co->pc);
#endif
    code_append_ins(co, 1, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("incl %edi\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\taddl $%d,%%edi\n", co->pc, n);
#endif
  if(n >=-128 && n <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &n);
    START_TEMPLATE
    _cf_002: asm("addl $1,%edi\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &n);
  START_TEMPLATE
  _cf_003: asm("addl $0x100,%edi\n");
}

void _a_addl_n_esi(CodeObj *co, int n)
{
  __label__ _cf_001, _cf_002, _cf_003;

  if(!n) return;

  if(n == 1) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tincl %%esi\n", co->pc);
#endif
    code_append_ins(co, 1, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("incl %esi\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\taddl $%d,%%esi\n", co->pc, n);
#endif
  if(n >=-128 && n <=127) {
    code_append_ins(co, 2, &&_cf_002);
    code_append_ins(co, 1, &n);
    START_TEMPLATE
    _cf_002: asm("addl $1,%esi\n");
    return;
  }
  code_append_ins(co, 2, &&_cf_003);
  code_append_ins(co, 4, &n);
  START_TEMPLATE
  _cf_003: asm("addl $0x100,%esi\n");
}

void _a_shrl_n_ebp(CodeObj *co, int n)
{
  __label__ _cf_001, _cf_002;

  n &= 0x1f;

  if(!n) return;

  if(n == 1) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tshrl %%ebp\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("shrl %ebp\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tshrl $%d,%%ebp\n", co->pc, n);
#endif
  code_append_ins(co, 2, &&_cf_002);
  code_append_ins(co, 1, &n);
  START_TEMPLATE
  _cf_002: asm("shrl $2,%ebp\n");
}

void _a_shll_n_edx(CodeObj *co, int n)
{
  __label__ _cf_001, _cf_002;

  n &= 0x1f;

  if(!n) return;

  if(n == 1) {
#ifdef REMAP_CODE_DEBUG
    fprintf(rdm, "0x%04x:\tshll %%edx\n", co->pc);
#endif
    code_append_ins(co, 2, &&_cf_001);
    START_TEMPLATE
    _cf_001: asm("shll %edx\n");
    return;
  }
#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "0x%04x:\tshll $%d,%%edx\n", co->pc, n);
#endif
  code_append_ins(co, 2, &&_cf_002);
  code_append_ins(co, 1, &n);
  START_TEMPLATE
  _cf_002: asm("shll $2,%edx\n");
}


/*
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *                     Pentium optimized functions
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

void pent_8to8_1(RemapObject *);
void pent_8to8_all(RemapObject *);
static void pent_8to8_all_init(RemapObject *);

void pent_8to8p_all(RemapObject *);
static void pent_8to8p_all_init(RemapObject *);

void pent_8to16_test(RemapObject *);
void pent_8to16_1(RemapObject *);
void pent_8to16_all(RemapObject *);
static void pent_8to16_all_init(RemapObject *);

void pent_8to32_1(RemapObject *);
void pent_8to32_all(RemapObject *);
static void pent_8to32_all_init(RemapObject *);

static RemapFuncDesc remap_opt_list[] = {

  REMAP_DESC(
    RFF_SCALE_1   | RFF_OPT_PENTIUM | RFF_REMAP_RECT,
    MODE_PSEUDO_8,
    MODE_TRUE_8,
    pent_8to8_1,
    NULL
  ),

  REMAP_DESC(
    RFF_SCALE_ALL   | RFF_OPT_PENTIUM | RFF_REMAP_LINES,
    MODE_VGA_X | MODE_PSEUDO_8,
    MODE_TRUE_8,
    pent_8to8_all,
    pent_8to8_all_init
  ),

  REMAP_DESC(
    RFF_SCALE_ALL   | RFF_OPT_PENTIUM | RFF_REMAP_LINES,
    MODE_VGA_X | MODE_PSEUDO_8,
    MODE_PSEUDO_8,
    pent_8to8p_all,
    pent_8to8p_all_init
  ),

#if 0
  REMAP_DESC(RFF_SCALE_ALL | RFF_OPT_PENTIUM | RFF_REMAP_LINES,
    MODE_PSEUDO_8,
    MODE_TRUE_15 | MODE_TRUE_16,
    pent_8to16_test,
    NULL
  ),
#endif

  REMAP_DESC(
    RFF_SCALE_1   | RFF_OPT_PENTIUM | RFF_REMAP_RECT,
    MODE_PSEUDO_8,
    MODE_TRUE_15 | MODE_TRUE_16,
    pent_8to16_1,
    NULL
  ),

  REMAP_DESC(
    RFF_SCALE_ALL | RFF_OPT_PENTIUM | RFF_REMAP_LINES,
    MODE_VGA_X | MODE_PSEUDO_8,
    MODE_TRUE_15 | MODE_TRUE_16,
    pent_8to16_all,
    pent_8to16_all_init
  ),

  REMAP_DESC(
    RFF_SCALE_1   | RFF_OPT_PENTIUM | RFF_REMAP_RECT,
    MODE_PSEUDO_8,
    MODE_TRUE_32,
    pent_8to32_1,
    NULL
  ),

  REMAP_DESC(
    RFF_SCALE_ALL   | RFF_OPT_PENTIUM | RFF_REMAP_LINES,
    MODE_VGA_X | MODE_PSEUDO_8,
    MODE_TRUE_32,
    pent_8to32_all,
    pent_8to32_all_init
  )

};

/*
 * returns chained list of modes
 */
RemapFuncDesc *remap_opt(void)
{
  int i;

  for(i = 0; i < sizeof(remap_opt_list) / sizeof(*remap_opt_list) - 1; i++) {
    remap_opt_list[i].next = remap_opt_list + i + 1;
  }

  return remap_opt_list;
}


/*
 * create code that remaps a complete line;
 * needed for pent_8to8_all
 */
void pent_8to8_all_init(RemapObject *ro)
{
  int i, j, d_o_r, s_o_r, d_o, s_o;
  int d_len = ro->dst_width & ~3;
  int *bre_x = ro->bre_x;
  int b0, b1, b2, b3;
  CodeObj *co = ro->co;

  if(co != NULL) { code_done(co); }
  *co = code_init();

#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "- - - - - - - - \n");
  fprintf(rdm, "pent_8to8_all_line_remap:\n");
#endif

/*
 *      esi, edi: src, dst
 *      eax, ebx: 0
 *           ecx: true_color_lut
 *           edx: [4*eax], [4*ebx]
 */

  for(i = s_o_r = d_o = s_o = d_o_r = 0; i < d_len; ) {
    if(d_o > 127) {
      j = d_o + 128;
      if(j > ro->dst_scan_len - d_o_r) j = ro->dst_scan_len - d_o_r;
      _a_addl_n_edi(co, j);
      d_o_r += j;
      d_o -= j;
    }
    if(s_o > 127) {
      _a_addl_n_esi(co, j = s_o + 128);
      s_o_r += j;
      s_o -= j;
    }

    b0 = 0;
    b1 = bre_x[i++];
    b2 = b1 + bre_x[i++];
    b3 = b2 + bre_x[i++];

    _a_movb_esi_al(co, s_o + b3);
    if(b3 != b2) {
      _a_movb_esi_bl(co, s_o + b2);
    }
    _a_movb_n_ecx_4eax_dh(co, 1);
    if(b3 == b2) {
      _a_movb_n_ecx_4eax_dl(co, 0);
    }
    else {
      _a_movb_n_ecx_4ebx_dl(co, 0);
    }

    _a_shll_n_edx(co, 16);

    if(b1 == b2 && b2 != b3) {
      if(b0 != b1) {
        _a_movb_esi_al(co, s_o + b0);
      }
      _a_movb_n_ecx_4ebx_dh(co, 1);
      if(b0 == b1) {
        _a_movb_n_ecx_4ebx_dl(co, 0);
      }
      else {
        _a_movb_n_ecx_4eax_dl(co, 0);
      }
    }
    else {
      if(b1 != b3) {
        _a_movb_esi_al(co, s_o + b1);
      }
      if(b0 != b1) {
        _a_movb_esi_bl(co, s_o + b0);
      }
      _a_movb_n_ecx_4eax_dh(co, 1);
      if(b0 == b1) {
        _a_movb_n_ecx_4eax_dl(co, 0);
      }
      else {
        _a_movb_n_ecx_4ebx_dl(co, 0);
      }
    }

    _a_movl_edx_edi(co, d_o);

    d_o += 4;
    s_o += b3 + bre_x[i++];
  }

  if(ro->dst_width & 2) {
    _a_movb_esi_al(co, s_o);
    s_o += bre_x[i++];
    _a_movb_esi_bl(co, s_o);
    s_o += bre_x[i++];
    _a_movb_n_ecx_4eax_dl(co, 0);
    _a_movb_n_ecx_4ebx_dh(co, 1);
    _a_movw_dx_edi(co, d_o);
    d_o += 2;
  }

  if(ro->dst_width & 1) {
    _a_movb_esi_al(co, s_o);
    _a_movb_n_ecx_4eax_dl(co, 0);
    _a_movb_dl_edi(co, d_o);
  }

  _a_addl_n_edi(co, ro->dst_scan_len - d_o_r);

  _a_ret(co);

#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "- - - - - - - - \n");
#endif

  ro->remap_line = co->exec;
}


/*
 * create code that remaps a complete line;
 * needed for pent_8to8p_all
 */
void pent_8to8p_all_init(RemapObject *ro)
{
  int i, j, d_o_r, s_o_r, d_o, s_o;
  int d_len = ro->dst_width & ~3;
  int *bre_x = ro->bre_x;
  int b0, b1, b2, b3;
  CodeObj *co = ro->co;

  if(co != NULL) { code_done(co); }
  *co = code_init();

#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "- - - - - - - - \n");
  fprintf(rdm, "pent_8to8p_all_line_remap:\n");
#endif

/*
 *           esi, edi: src, dst
 * eax, ebx, ecx, edx: 0
 */

  for(i = s_o_r = d_o = s_o = d_o_r = 0; i < d_len; ) {
    if(d_o > 127) {
      j = d_o + 128;
      if(j > ro->dst_scan_len - d_o_r) j = ro->dst_scan_len - d_o_r;
      _a_addl_n_edi(co, j);
      d_o_r += j;
      d_o -= j;
    }
    if(s_o > 127) {
      _a_addl_n_esi(co, j = s_o + 128);
      s_o_r += j;
      s_o -= j;
    }

    b0 = 0;
    b1 = bre_x[i++];
    b2 = b1 + bre_x[i++];
    b3 = b2 + bre_x[i++];

    _a_movb_esi_dh(co, s_o + b3);
    if(b3 == b2) {
      _a_movb_dh_dl(co);
    }
    else {
      _a_movb_esi_dl(co, s_o + b2);
    }

    _a_shll_n_edx(co, 16);

    _a_movb_esi_dh(co, s_o + b1);
    if(b3 == b2) {
      _a_movb_dh_dl(co);
    }
    else {
      _a_movb_esi_dl(co, s_o + b0);
    }

    _a_movl_edx_edi(co, d_o);

    d_o += 4;
    s_o += b3 + bre_x[i++];
  }

  if(ro->dst_width & 2) {
    _a_movb_esi_dl(co, s_o);
    s_o += bre_x[i++];
    _a_movb_esi_dh(co, s_o);
    s_o += bre_x[i++];
    _a_movw_dx_edi(co, d_o);
    d_o += 2;
  }

  if(ro->dst_width & 1) {
    _a_movb_esi_dl(co, s_o);
    _a_movb_dl_edi(co, d_o);
  }

  _a_addl_n_edi(co, ro->dst_scan_len - d_o_r);

  _a_ret(co);

#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "- - - - - - - - \n");
#endif

  ro->remap_line = co->exec;
}


/*
 * create code that remaps a complete line;
 * needed for pent_8to16_all
 */
void pent_8to16_all_init(RemapObject *ro)
{
  int i, j, d_o_r, s_o_r, d_o, s_o;
  int d_len = ro->dst_width & ~1;
  int *bre_x = ro->bre_x;
  int last_dx0 = -1, last_dx = -1;
  CodeObj *co = ro->co;

  if(co != NULL) { code_done(co); }
  *co = code_init();

#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "- - - - - - - - \n");
  fprintf(rdm, "pent_8to16_all_line_remap:\n");
#endif

/*
 *      esi, edi: src, dst
 *           eax: 0
 * ebx, ecx, edx: &true_color_lut[0, 100, 200]
 *           ebp: [4*eax]
 */

  for(i = s_o_r = d_o = s_o = d_o_r = 0; i < d_len; i += 2) {
    if(d_o > 127) {
      j = d_o + 128;
      if(j > ro->dst_scan_len - d_o_r) j = ro->dst_scan_len - d_o_r;
      _a_addl_n_edi(co, j);
      d_o_r += j;
      d_o -= j;
    }

    if(ro->src_mode != MODE_VGA_X) {
      if(s_o > 127) {
        _a_addl_n_esi(co, j = s_o + 128);
        s_o_r += j;
        s_o -= j;
      }
    }

    if(bre_x[i]) {
      if(last_dx) {
        _a_movb_esi_al(co, s_o);
        _a_movl_ecx_4eax_ebp(co);
      }
      else {
        _a_shrl_n_ebp(co, 16);
      }
      s_o += bre_x[i];
      _a_movb_esi_al(co, s_o);
      _a_addl_edx_4eax_ebp(co);
      _a_movl_ebp_edi(co, d_o);
    }
    else {
      if(last_dx) {
        _a_movb_esi_al(co, s_o);
        _a_movl_ebx_4eax_ebp(co);
      }
      else {
        if(last_dx0) _a_movl_ebx_4eax_ebp(co);
      }
      _a_movl_ebp_edi(co, d_o);
    }

    last_dx0 = bre_x[i];
    d_o += 4;
    s_o += (last_dx = bre_x[i + 1]);
  }

  if(ro->dst_width & 1) {
    if(last_dx) {
      _a_movb_esi_al(co, s_o);
      _a_movl_ecx_4eax_ebp(co);
    }
    else {
      _a_shrl_n_ebp(co, 16);
    }
    _a_movw_bp_edi(co, d_o);
  }

  _a_addl_n_edi(co, ro->dst_scan_len - d_o_r);

  _a_ret(co);

#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "- - - - - - - - \n");
#endif

  ro->remap_line = co->exec;
}


/*
 * create code that remaps a complete line;
 * needed for pent_8to32_all
 */
void pent_8to32_all_init(RemapObject *ro)
{
  int i, j, k, d_o_r, s_o_r, d_o, s_o;
  int d_len = ro->dst_width;
  int *bre_x = ro->bre_x;
  int l0, l1;
  int b0, b1;
  CodeObj *co = ro->co;

  if(co != NULL) { code_done(co); }
  *co = code_init();

#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "- - - - - - - - \n");
  fprintf(rdm, "pent_8to32_all_line_remap:\n");
#endif

/*
 *      esi, edi: src, dst
 *      eax, ecx: 0
 *           ebx: true_color_lut
 *      ebp, edx: [4*eax], [4*ecx]
 */

  l0 = l1 = 1;
  b0 = b1 = j = 0;

  for(i = s_o_r = d_o = s_o = d_o_r = 0; i < d_len; i++) {
    if(bre_x[i] == 0) {
      if(j == 0)
        l0++;
      else
        l1++;
    }
    else {
      if(j == 0)
        b0 = bre_x[i];
      else
        b1 = bre_x[i];
      j++;
    }

    if(j == 2 || i == d_len - 1) {

      if(d_o > 127) {
        k = d_o + 128;
        if(k > ro->dst_scan_len - d_o_r) k = ro->dst_scan_len - d_o_r;
        _a_addl_n_edi(co, k);
        d_o_r += k;
        d_o -= k;
      }

      if(ro->src_mode != MODE_VGA_X) {
        if(s_o > 127) {
          _a_addl_n_esi(co, k = s_o + 128);
          s_o_r += k;
          s_o -= k;
        }
      }

      if(j == 1) {
        _a_movb_esi_al(co, s_o);
        _a_movl_ebx_4eax_ebp(co);
        _a_movl_ebp_edi(co, d_o);
      }

      if(j == 2) {
        _a_movb_esi_al(co, s_o);
        _a_movb_esi_cl(co, s_o + b0);
        _a_movl_ebx_4eax_ebp(co);
        _a_movl_ebx_4ecx_edx(co);
        for(k = 0; k < l0; k++) _a_movl_ebp_edi(co, d_o + (k << 2));
        for(k = 0; k < l1; k++) _a_movl_edx_edi(co, d_o + ((k + l0) << 2));

        d_o += (l0 + l1) << 2;
        s_o += b0 + b1;
      }

      l0 = l1 = 1;
      j = 0;
    }
  
  }

  _a_addl_n_edi(co, ro->dst_scan_len - d_o_r);

  _a_ret(co);

#ifdef REMAP_CODE_DEBUG
  fprintf(rdm, "- - - - - - - - \n");
#endif

  ro->remap_line = co->exec;
}

#endif
