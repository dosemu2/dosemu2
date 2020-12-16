/* 			DPMI support for DOSEMU
 *
 * DANG_BEGIN_MODULE dpmi.c
 *
 * REMARK
 * DOS Protected Mode Interface allows DOS programs to run in the
 * protected mode of [2345..]86 processors
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * First Attempted by James B. MacLean macleajb@ednet.ns.ca
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#ifdef __linux__
#include <linux/version.h>
#endif
extern long int __sysconf (int); /* for Debian eglibc 2.13-3 */
#include <sys/user.h>
#include <sys/syscall.h>
#include <Asm/ldt.h>

#include "version.h"
#include "emu.h"
#include "dosemu_config.h"
#include "memory.h"
#include "dos2linux.h"
#include "timers.h"
#include "mhpdbg.h"
#include "hlt.h"
#include "libpcl/pcl.h"
#include "sig.h"
#include "dpmi.h"
#include "dpmisel.h"
#include "msdos.h"
#include "msdos_ex.h"
#include "msdoshlp.h"
#include "msdos_ldt.h"
#include "vxd.h"
#include "bios.h"
#include "bitops.h"
#include "pic.h"
#include "int.h"
#include "port.h"
#include "utilities.h"
#include "mapping.h"
#include "vgaemu.h"
#include "cpu-emu.h"
#include "emu-ldt.h"
#include "kvm.h"

#define D_16_32(reg)		(DPMI_CLIENT.is_32 ? reg : reg & 0xffff)
#define ADD_16_32(acc, val)	{ if (DPMI_CLIENT.is_32) acc+=val; else LO_WORD(acc)+=val; }
static int current_client;
#define DPMI_CLIENT (DPMIclient[current_client])
#define PREV_DPMI_CLIENT (DPMIclient[current_client-1])

/* in prot mode the IF field of the flags image is ignored by popf or iret,
 * and on push - it is always forced to 1 */
#define get_vFLAGS(flags) ({ \
  int __flgs = (flags); \
  ((isset_IF() ? __flgs | IF : __flgs & ~IF) | IOPL_MASK); \
})
#define eflags_VIF(flags) (((flags) & ~VIF) | (isset_IF() ? VIF : 0) | IF | IOPL_MASK)
#define sanitize_flags(flags) do { \
  (flags) |= 2 | IF | IOPL_MASK; \
  (flags) &= ~(AC | NT | VM); \
} while (0)

static SEGDESC Segments[MAX_SELECTORS];
static int in_dpmi;/* Set to 1 when running under DPMI */
static int dpmi_pm;
static int in_dpmi_irq;
unsigned char dpmi_mhp_intxxtab[256];
static int dpmi_is_cli;
static coroutine_t dpmi_tid;
static cohandle_t co_handle;
static sigcontext_t emu_stack_frame;
static struct sigaction emu_tmp_act;
#define DPMI_TMP_SIG SIGUSR1
static int in_dpmi_thr;
static int dpmi_thr_running;

#define CLI_BLACKLIST_LEN 128
static unsigned char * cli_blacklist[CLI_BLACKLIST_LEN];
static unsigned char * current_cli;
static int cli_blacklisted = 0;
static int return_requested = 0;
#ifdef __x86_64__
static unsigned int *iret_frame;
#endif
static int dpmi_ret_val;
static int find_cli_in_blacklist(unsigned char *);
#ifdef USE_MHPDBG
static int dpmi_mhp_intxx_check(sigcontext_t *scp, int intno);
#endif
static int dpmi_fault1(sigcontext_t *scp);
static far_t s_i1c, s_i23, s_i24;

static struct RealModeCallStructure DPMI_rm_stack[DPMI_max_rec_rm_func];
static int DPMI_rm_procedure_running = 0;

#define DPMI_max_rec_pm_func 16
static sigcontext_t DPMI_pm_stack[DPMI_max_rec_pm_func];
static int DPMI_pm_procedure_running = 0;

static struct DPMIclient_struct DPMIclient[DPMI_MAX_CLIENTS];

static dpmi_pm_block_root host_pm_block_root;

static uint8_t _ldt_buffer[LDT_ENTRIES * LDT_ENTRY_SIZE];
uint8_t *ldt_buffer = _ldt_buffer;
static unsigned short dpmi_sel16, dpmi_sel32;
unsigned short dpmi_sel()
{
  return DPMI_CLIENT.is_32 ? dpmi_sel32 : dpmi_sel16;
}

static int RSP_num = 0;
static struct RSP_s RSP_callbacks[DPMI_MAX_CLIENTS];

static int dpmi_not_supported;

#define CHECK_SELECTOR(x) \
{ if (!ValidAndUsedSelector(x) || SystemSelector(x)) { \
      _LWORD(eax) = 0x8022; \
      _eflags |= CF; \
      break; \
    } \
}

#define CHECK_SELECTOR_ALLOC(x) \
{ if (SystemSelector(x)) { \
      _LWORD(eax) = 0x8022; \
      _eflags |= CF; \
      break; \
    } \
}

static void quit_dpmi(sigcontext_t *scp, unsigned short errcode,
    int tsr, unsigned short tsr_para, int dos_exit);

#ifdef __linux__
#define modify_ldt dosemu_modify_ldt
static inline int modify_ldt(int func, void *ptr, unsigned long bytecount)
{
  return syscall(SYS_modify_ldt, func, ptr, bytecount);
}
#endif

static void *SEL_ADR_LDT(unsigned short sel, unsigned int reg, int is_32)
{
  unsigned long p;
  if (is_32)
    p = GetSegmentBase(sel) + reg;
  else
    p = GetSegmentBase(sel) + LO_WORD(reg);
  /* The address needs to wrap, also in 64-bit! */
  return LINEAR2UNIX(p);
}

void *SEL_ADR(unsigned short sel, unsigned int reg)
{
  if (!(sel & 0x0004)) {
    /* GDT */
    return (void *)(uintptr_t)reg;
  }
  /* LDT */
  return SEL_ADR_LDT(sel, reg, Segments[sel>>3].is_32);
}

void *SEL_ADR_CLNT(unsigned short sel, unsigned int reg, int is_32)
{
  if (!(sel & 0x0004)) {
    /* GDT */
    dosemu_error("GDT not allowed\n");
    return (void *)(uintptr_t)reg;
  }
  return SEL_ADR_LDT(sel, reg, is_32);
}

int get_ldt(void *buffer)
{
#ifdef __linux__
  int i, ret;
  struct ldt_descriptor *dp;
  if (config.cpu_vm_dpmi != CPUVM_NATIVE)
	return emu_modify_ldt(0, buffer, LDT_ENTRIES * LDT_ENTRY_SIZE);
  ret = modify_ldt(0, buffer, LDT_ENTRIES * LDT_ENTRY_SIZE);
  /* do emu_modify_ldt even if modify_ldt fails, so cpu_vm_dpmi fallbacks can
     still work */
  if (ret != LDT_ENTRIES * LDT_ENTRY_SIZE)
    return emu_modify_ldt(0, buffer, LDT_ENTRIES * LDT_ENTRY_SIZE);
  for (i = 0, dp = buffer; i < LDT_ENTRIES; i++, dp++) {
    unsigned int base_addr = DT_BASE(dp);
    if (base_addr || DT_LIMIT(dp)) {
      base_addr -= (uintptr_t)mem_base;
      MKBASE(dp, base_addr);
    }
  }
  return ret;
#else
  return emu_modify_ldt(0, buffer, LDT_ENTRIES * LDT_ENTRY_SIZE);
#endif
}

static int set_ldt_entry(int entry, unsigned long base, unsigned int limit,
	      int seg_32bit_flag, int contents, int read_only_flag,
	      int limit_in_pages_flag, int seg_not_present, int useable)
{
  /*
   * We initialize this explicitly in order to pacify valgrind.
   * Otherwise, there would be some uninitialized padding bits at the end.
   */
  struct user_desc ldt_info = {};

  int __retval;
  ldt_info.entry_number = entry;
  ldt_info.base_addr = base;
  ldt_info.limit = limit;
  ldt_info.seg_32bit = seg_32bit_flag;
  ldt_info.contents = contents;
  ldt_info.read_exec_only = read_only_flag;
  ldt_info.limit_in_pages = limit_in_pages_flag;
  ldt_info.seg_not_present = seg_not_present;
  ldt_info.useable = useable;
#ifdef __linux__
  if (config.cpu_vm_dpmi == CPUVM_NATIVE)
  {
    /* NOTE: the real LDT in kernel space uses the real addresses, but
       the LDT we emulate, and DOS applications work with,
       has all base addresses with respect to mem_base */
    if (base || limit) {
      ldt_info.base_addr += (uintptr_t)mem_base;
      __retval = modify_ldt(LDT_WRITE, &ldt_info, sizeof(ldt_info));
      ldt_info.base_addr -= (uintptr_t)mem_base;
    } else {
      __retval = modify_ldt(LDT_WRITE, &ldt_info, sizeof(ldt_info));
    }
    if (__retval)
      return __retval;
  }
#endif

  __retval = emu_modify_ldt(LDT_WRITE, &ldt_info, sizeof(ldt_info));
  return __retval;
}

static void _print_dt(char *buffer, int nsel, int isldt) /* stolen from WINE */
{
  static const char *cdsdescs[] = {
	"RO data", "RW data/upstack", "RO data", "RW data/dnstack",
	"FO nonconf code", "FR nonconf code", "FO conf code", "FR conf code"
  };
  static const char *sysdescs[] = {
	"reserved", "avail 16bit TSS", "LDT" ,"busy 16bit TSS",
	"16bit call gate", "task gate", "16bit int gate", "16bit trap gate",
	"reserved", "avail 32bit TSS", "reserved" ,"busy 32bit TSS",
	"32bit call gate", "reserved", "32bit int gate", "32bit trap gate"
  };
  unsigned int *lp;
  unsigned int base_addr, limit;
  int type, i;

  lp = (unsigned int *) buffer;

  for (i = 0; i < nsel; i++, lp++) {
    /* First 32 bits of descriptor */
    base_addr = (*lp >> 16) & 0x0000FFFF;
    limit = *lp & 0x0000FFFF;
    lp++;

    /* First 32 bits of descriptor */
    base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
    limit |= (*lp & 0x000F0000);
    type = (*lp >> 8) & 15;
    if ((base_addr > 0) || (limit > 0 ) || Segments[i].used) {
      if (*lp & 0x1000)  {
	D_printf("Entry 0x%04x: Base %08x, Limit %05x, Type %d (desc %#x)\n",
	       i, base_addr, limit, (type&7), (i<<3)|(isldt?7:0));
	D_printf("              ");
	if (*lp & 0x100)
	  D_printf("Accessed, ");
	if (!(*lp & 0x200))
	  D_printf("Read/Execute only, ");
	if (*lp & 0x8000)
	  D_printf("Present, ");
	if (*lp & 0x100000)
	  D_printf("User, ");
	if (*lp & 0x200000)
	  D_printf("X, ");
	if (*lp & 0x400000)
	  D_printf("32, ");
	else
	  D_printf("16, ");
	if (*lp & 0x800000)
	  D_printf("page limit, ");
	else
	  D_printf("byte limit, ");
	D_printf("\n");
	D_printf("              %08x %08x [%s]\n", *(lp), *(lp-1),
		cdsdescs[type>>1]);
      }
      else {
	D_printf("Entry 0x%04x: Base %08x, Limit %05x, Type %d (desc %#x)\n",
	       i, base_addr, limit, type, (i<<3)|(isldt?7:0));
	D_printf("              SYSTEM: %08x %08x [%s]\n", *lp, *(lp-1),
		sysdescs[type]);
      }
      if (isldt) {
	unsigned int *lp2;
	lp2 = (unsigned int *) &ldt_buffer[i*LDT_ENTRY_SIZE];
	D_printf("       cache: %08x %08x\n", *(lp2+1), *lp2);
	D_printf("         seg: Base %08x, Limit %05x, Type %d, Big %d\n",
	     Segments[i].base_addr, Segments[i].limit, Segments[i].type, Segments[i].is_big);
      }
    }
  }
}

static void print_ldt(void)
{
  static char buffer[0x10000];

  if (get_ldt(buffer) < 0)
    leavedos(0x544c);

  _print_dt(buffer, MAX_SELECTORS, 1);
}

static void dpmi_set_pm(int pm)
{
  assert(pm <= 1);
  if (pm == dpmi_pm) {
    if (!pm)
      dosemu_error("DPMI: switch from real to real mode?\n");
    return;
  }
  dpmi_pm = pm;
}

static dpmi_pm_block *lookup_pm_blocks_by_addr(dosaddr_t addr)
{
  int i;
  dpmi_pm_block *blk = lookup_pm_block_by_addr(&host_pm_block_root, addr);
  if (blk)
    return blk;
  for (i = 0; i < in_dpmi; i++) {
    blk = lookup_pm_block_by_addr(&DPMIclient[i].pm_block_root, addr);
    if (blk)
      return blk;
  }
  return NULL;
}

int dpmi_is_valid_range(dosaddr_t addr, int len)
{
  int i;
  dpmi_pm_block *blk;

  if (addr + len <= LOWMEM_SIZE + HMASIZE)
    return 1;
  if (!in_dpmi)
    return 0;
  blk = lookup_pm_blocks_by_addr(addr);
  if (!blk)
    return 0;
  if (blk->base + blk->size < addr + len)
    return 0;
  for (i = (addr - blk->base) >> PAGE_SHIFT;
       i < (PAGE_ALIGN(addr + len - 1) - blk->base) >> PAGE_SHIFT; i++)
    if ((blk->attrs[i] & 7) != 1)
      return 0;
  return 1;
}

int dpmi_read_access(dosaddr_t addr)
{
  dpmi_pm_block *blk = lookup_pm_blocks_by_addr(addr);
  return blk && (blk->attrs[(addr - blk->base) >> PAGE_SHIFT] & 1);
}

int dpmi_write_access(dosaddr_t addr)
{
  dpmi_pm_block *blk = lookup_pm_blocks_by_addr(addr);
  return blk && (blk->attrs[(addr - blk->base) >> PAGE_SHIFT] & 9) == 9;
}

/* client_esp return the proper value of client\'s esp, if scp != 0, */
/* get esp from scp, otherwise get esp from dpmi_stack_frame         */
static inline unsigned long client_esp(sigcontext_t *scp)
{
    if (scp == NULL)
	scp = &DPMI_CLIENT.stack_frame;
    if( Segments[_ss >> 3].is_32)
	return _esp;
    else
	return (_esp)&0xffff;
}

#ifdef __x86_64__
static void iret_frame_setup(sigcontext_t *scp)
{
  /* set up a frame to get back to DPMI via iret. The kernel does not save
     %ss, and the SYSCALL instruction in sigreturn() destroys it.

     IRET pops off everything in 64-bit mode even if the privilege
     does not change which is nice, but clobbers the high 48 bits
     of rsp if the DPMI client uses a 16-bit stack which is not so
     nice (see EMUfailure.txt). Setting %rsp to 0x100000000 so that
     bits 16-31 are zero works around this problem, as DPMI code
     can't see bits 32-63 anyway.
 */

  iret_frame[0] = _eip;
  iret_frame[1] = _cs;
  iret_frame[2] = _eflags;
  iret_frame[3] = _esp;
  iret_frame[4] = _ss;
  _rsp = (unsigned long)iret_frame;
}

void dpmi_iret_setup(sigcontext_t *scp)
{
  iret_frame_setup(scp);
  _eflags &= ~TF;
  _rip = (unsigned long)DPMI_iret;
  _cs = getsegment(cs);
}

void dpmi_iret_unwind(sigcontext_t *scp)
{
  if (_rip != (unsigned long)DPMI_iret)
    return;
  _eip = iret_frame[0];
  _cs = iret_frame[1];
  _eflags = iret_frame[2];
  _esp = iret_frame[3];
  _ss = iret_frame[4];
}
#endif

/* ======================================================================== */
/*
 * DANG_BEGIN_FUNCTION dpmi_control
 *
 * This function is similar to the vm86() syscall in the kernel and
 * switches to dpmi code.
 *
 * DANG_END_FUNCTION
 */

static int do_dpmi_control(sigcontext_t *scp)
{
    if (in_dpmi_thr)
      signal_switch_to_dpmi();
    dpmi_thr_running++;
    co_call(dpmi_tid);
    dpmi_thr_running--;
    if (in_dpmi_thr)
      signal_switch_to_dosemu();
    /* we may return here with sighandler's signal mask.
     * This is done for speed-up. dpmi_control() restores the mask. */
    return dpmi_ret_val;
}

static int do_dpmi_exit(sigcontext_t *scp)
{
    int ret;
    D_printf("DPMI: leaving\n");
    dpmi_ret_val = DPMI_RET_EXIT;
    ret = do_dpmi_control(scp);
    if (in_dpmi_thr)
        error("DPMI thread have not terminated properly\n");
    return ret;
}

static int _dpmi_control(void)
{
    int ret;
    sigcontext_t *scp = &DPMI_CLIENT.stack_frame;

    do {
      if (CheckSelectors(scp, 1) == 0)
        leavedos(36);
      sanitize_flags(_eflags);
      if (debug_level('M') > 5) {
        D_printf("DPMI: switch to dpmi\n");
        if (debug_level('M') >= 8)
          D_printf("DPMI: Return to client at %04x:%08x, Stack 0x%x:0x%08x, flags=%#x\n",
                   _cs, _eip, _ss, _esp, eflags_VIF(_eflags));
      }
      if (config.cpu_vm_dpmi == CPUVM_KVM)
        ret = kvm_dpmi(scp);
#ifdef X86_EMULATOR
      else if (config.cpu_vm_dpmi == CPUVM_EMU)
        ret = e_dpmi(scp);
#endif
      else
        ret = do_dpmi_control(scp);
      if (debug_level('M') > 5)
        D_printf("DPMI: switch to dosemu\n");
      if (ret == DPMI_RET_FAULT)
        ret = dpmi_fault1(scp);
      if (!in_dpmi && in_dpmi_thr) {
        ret = do_dpmi_exit(scp);
        break;
      }
      if (ret == DPMI_RET_CLIENT) {
        uncache_time();
        hardware_run();
      }

      if (!in_dpmi_pm() || (ret == DPMI_RET_CLIENT &&
          ((isset_IF() && pic_pending()) || return_requested))) {
        return_requested = 0;
        ret = DPMI_RET_DOSEMU;
        break;
      }
    } while (ret == DPMI_RET_CLIENT);
    if (debug_level('M') >= 8)
      D_printf("DPMI: Return to dosemu at %04x:%08x, Stack 0x%x:0x%08x, flags=%#x\n",
            _cs, _eip, _ss, _esp, eflags_VIF(_eflags));

    return ret;
}

static int dpmi_control(void)
{
    int ret;

    /* if we are going directly to a sighandler, mask async signals. */
    if (in_dpmi_thr)
	signal_restore_async_sigs();
    ret = _dpmi_control();
    /* for speed-up, DPMI switching corrupts signal mask. Fix it here. */
    signal_unblock_async_sigs();
    return ret;
}

void dpmi_get_entry_point(void)
{
    D_printf("Request for DPMI entry\n");

    if (dpmi_not_supported) {
      p_dos_str("DPMI is not supported on your linux kernel!\n");
      CARRY;
      return;
    }
    if (!config.dpmi) {
      p_dos_str("DPMI disabled, please check the dosemu config and log\n");
      CARRY;
      return;
    }

    REG(eax) = 0; /* no error */

    /* 32 bit programs are O.K. */
    LWORD(ebx) = 1;
    LO(cx) = vm86s.cpu_type;

    /* Version 0.9 */
    HI(dx) = DPMI_VERSION;
    LO(dx) = DPMI_DRIVER_VERSION;

    /* Entry Address for DPMI */
    SREG(es) = DPMI_SEG;
    REG(edi) = DPMI_OFF;

    /* private data */
    LWORD(esi) = DPMI_private_paragraphs + msdos_get_lowmem_size();

    D_printf("DPMI entry returned, needs %#x lowmem paragraphs (%i)\n",
	    LWORD(esi), LWORD(esi) << 4);
}

int SetSelector(unsigned short selector, dosaddr_t base_addr, unsigned int limit,
                       unsigned char is_32, unsigned char type, unsigned char readonly,
                       unsigned char is_big, unsigned char seg_not_present, unsigned char useable)
{
  int ldt_entry = selector >> 3;
  if (!DPMIValidSelector(selector)) {
    D_printf("ERROR: Invalid selector passed to SetSelector(): %#x\n", selector);
    return -1;
  }
  if (Segments[ldt_entry].used) {
    if (set_ldt_entry(ldt_entry, base_addr, limit, is_32, type, readonly, is_big,
        seg_not_present, useable)) {
      D_printf("DPMI: set_ldt_entry() failed\n");
      return -1;
    }
    D_printf("DPMI: SetSelector: 0x%04x base=0x%x "
      "limit=0x%x big=%hhi type=%hhi np=%hhi\n",
      selector, base_addr, limit, is_big, type, seg_not_present);
  }
  Segments[ldt_entry].base_addr = base_addr;
  Segments[ldt_entry].limit = limit;
  Segments[ldt_entry].type = type;
  Segments[ldt_entry].is_32 = is_32;
  Segments[ldt_entry].readonly = readonly;
  Segments[ldt_entry].is_big = is_big;
  Segments[ldt_entry].not_present = seg_not_present;
  Segments[ldt_entry].useable = useable;
  return 0;
}

static int SystemSelector(unsigned int selector)
{
  if ((selector >> 3) >= MAX_SELECTORS)
    return 0;
  /* 0xff refers to dpmi_sel16 & dpmi_sel32 */
  return !DPMIValidSelector(selector) || Segments[selector >> 3].used == 0xff;
}

static unsigned short allocate_descriptors_at(unsigned short selector,
    int number_of_descriptors)
{
  int ldt_entry = selector >> 3, i;

  if (ldt_entry > MAX_SELECTORS-number_of_descriptors) {
    D_printf("DPMI: Insufficient descriptors, requested %i\n",
      number_of_descriptors);
    return 0;
  }
  for (i=0;i<number_of_descriptors;i++) {
    if (Segments[ldt_entry+i].used || SystemSelector(((ldt_entry+i)<<3)|7))
      return 0;
  }

  for (i=0;i<number_of_descriptors;i++) {
    if (in_dpmi)
      Segments[ldt_entry+i].used = in_dpmi;
    else {
      Segments[ldt_entry+i].used = 0xff;  /* mark as unavailable for API */
    }
    Segments[ldt_entry+i].cstd = 0;
  }
  D_printf("DPMI: Allocate %d descriptors started at 0x%04x\n",
	number_of_descriptors, selector);
  return number_of_descriptors;
}

static unsigned short AllocateDescriptorsAt(unsigned short selector,
    int number_of_descriptors)
{
  int i, ldt_entry;
  if (!in_dpmi) {
    dosemu_error("AllocDescriptors error\n");
    return 0;
  }
  if (!allocate_descriptors_at(selector, number_of_descriptors))
    return 0;
  /* dpmi spec says, the descriptor allocated should be "data" with */
  /* base and limit set to 0 */
  ldt_entry = selector >> 3;
  for (i = 0; i < number_of_descriptors; i++) {
      if (SetSelector(((ldt_entry+i)<<3) | 0x0007, 0, 0, DPMI_CLIENT.is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return 0;
  }
  msdos_ldt_update(selector, number_of_descriptors);
  return selector;
}

static unsigned short allocate_descriptors_from(int first_ldt, int number_of_descriptors)
{
  int next_ldt=first_ldt, i;
  unsigned short selector;
  unsigned char used=1;
#if 0
  if (number_of_descriptors > MAX_SELECTORS - 0x100)
    number_of_descriptors = MAX_SELECTORS - 0x100;
#endif
  while (used) {
    next_ldt++;
    if (next_ldt > MAX_SELECTORS-number_of_descriptors) {
      D_printf("DPMI: Insufficient descriptors, requested %i\n",
        number_of_descriptors);
      return 0;
    }
    used=0;
    for (i=0;i<number_of_descriptors;i++)
      used |= Segments[next_ldt+i].used || SystemSelector(((next_ldt+i)<<3)|7);
  }
  selector = (next_ldt<<3) | 0x0007;
  if (allocate_descriptors_at(selector, number_of_descriptors) !=
	number_of_descriptors)
    return 0;
  return selector;
}

static unsigned short allocate_descriptors(int number_of_descriptors)
{
  /* first 0x10 descriptors are reserved */
  return allocate_descriptors_from(0x10, number_of_descriptors);
}

unsigned short AllocateDescriptors(int number_of_descriptors)
{
  int selector, i, ldt_entry;
  if (!in_dpmi) {
    dosemu_error("AllocDescriptors error\n");
    return 0;
  }
  selector = allocate_descriptors(number_of_descriptors);
  if (!selector)
    return 0;
  /* dpmi spec says, the descriptor allocated should be "data" with */
  /* base and limit set to 0 */
  ldt_entry = selector >> 3;
  for (i = 0; i < number_of_descriptors; i++) {
      if (SetSelector(((ldt_entry+i)<<3) | 0x0007, 0, 0, DPMI_CLIENT.is_32,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return 0;
  }
  msdos_ldt_update(selector, number_of_descriptors);
  return selector;
}

void FreeSegRegs(sigcontext_t *scp, unsigned short selector)
{
    if ((_ds | 7) == (selector | 7)) _ds = 0;
    if ((_es | 7) == (selector | 7)) _es = 0;
    if ((_fs | 7) == (selector | 7)) _fs = 0;
    if ((_gs | 7) == (selector | 7)) _gs = 0;
}

int FreeDescriptor(unsigned short selector)
{
  int ret, ldt_entry = selector >> 3;
  D_printf("DPMI: Free descriptor, selector=0x%x\n", selector);
  if (SystemSelector(selector)) {
    D_printf("DPMI: Cannot free system descriptor.\n");
    return -1;
  }
  ret = SetSelector(selector, 0, 0, 0, MODIFY_LDT_CONTENTS_DATA, 1, 0, 1, 0);
  Segments[ldt_entry].used = 0;
  msdos_ldt_update(selector, 1);
  return ret;
}

static void FreeAllDescriptors(void)
{
    int i;
    for (i=0;i<MAX_SELECTORS;i++) {
      if (Segments[i].used==in_dpmi)
        FreeDescriptor((i << 3) | 7);
    }
}

int ConvertSegmentToDescriptor(unsigned short segment)
{
  unsigned long baseaddr = segment << 4;
  unsigned short selector;
  int i, ldt_entry;
  D_printf("DPMI: convert seg %#x to desc\n", segment);
  for (i=1;i<MAX_SELECTORS;i++)
    if ((Segments[i].base_addr == baseaddr) && Segments[i].cstd &&
	(Segments[i].used == in_dpmi)) {
      D_printf("DPMI: found descriptor at %#x\n", (i<<3) | 0x0007);
      return (i<<3) | 0x0007;
    }
  D_printf("DPMI: SEG at base=%#lx not found, allocate a new one\n", baseaddr);
  if (!(selector = AllocateDescriptors(1))) return 0;
  if (SetSelector(selector, baseaddr, 0xffff, 0,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return 0;
  ldt_entry = selector >> 3;
  Segments[ldt_entry].cstd = 1;
  msdos_ldt_update(selector, 1);
  return selector;
}

static inline unsigned short GetNextSelectorIncrementValue(void)
{
  return 8;
}

/* define selector as int only to catch out-of-bound errors */
int ValidAndUsedSelector(unsigned int selector)
{
  if ((selector >> 3) >= MAX_SELECTORS)
    return 0;
  return DPMIValidSelector(selector) && Segments[selector >> 3].used;
}

static inline int check_verr(unsigned short selector)
{
  int ret;
  asm volatile(
    "verrw %%ax\n"
    "jz 1f\n"
    "xorl %%eax, %%eax\n"
    "1:\n"
    : "=a"(ret)
    : "a"(selector));
  return ret;
}

/*
IF DS, ES, FS or GS is loaded with non-null selector;
THEN
   Selector index must be within its descriptor table limits
      else #GP(selector);
   AR byte must indicate data or readable code segment else
      #GP(selector);
   IF data or nonconforming code segment
   THEN both the RPL and the CPL must be less than or equal to DPL in
      AR byte;
   ELSE #GP(selector);
   FI;
   Segment must be marked present else #NP(selector);
   Load segment register with selector;
   Load segment register with descriptor;
FI;
IF DS, ES, FS or GS is loaded with a null selector;
THEN
   Load segment register with selector;
   Clear descriptor valid bit;
FI;
*/
static inline int CheckDataSelector(sigcontext_t *scp,
				    unsigned short selector,
				    char letter, int in_dosemu)
{
  /* a null selector can be either 0,1,2 or 3 (RPL field ignored);
     apparently fs=3 and gs=3 show up sometimes when running in
     x86-64 */
  if (selector > 3 && (!ValidAndUsedSelector(selector)
		       || Segments[selector >> 3].not_present)) {
    if (in_dosemu) {
      error("%cS selector invalid: 0x%04X, type=%x np=%i\n",
        letter, selector, Segments[selector >> 3].type,
	    Segments[selector >> 3].not_present);
      D_printf("%s", DPMI_show_state(scp));
#if 1
      /* Some buggy programs load the arbitrary LDT or even GDT
       * selectors after doing "verr" on them. We have to do the same. :( */
      if (check_verr(selector)) {
        error("... although verr succeeded, trying to continue\n");
        return 1;
      }
#endif
    }
    return 0;
  }
  return 1;
}

int CheckSelectors(sigcontext_t *scp, int in_dosemu)
{
#ifdef TRACE_DPMI
  if (debug_level('t') == 0 || _trapno!=1)
#endif
  D_printf("DPMI: cs=%#x, ss=%#x, ds=%#x, es=%#x, fs=%#x, gs=%#x ip=%#x\n",
    _cs,_ss,_ds,_es,_fs,_gs,_eip);
/* NONCONFORMING-CODE-SEGMENT:
   RPL of destination selector must be <= CPL ELSE #GP(selector);
   Descriptor DPL must be = CPL ELSE #GP(selector);
   Segment must be present ELSE # NP(selector);
   Instruction pointer must be within code-segment limit ELSE #GP(0);
*/
  if (!ValidAndUsedSelector(_cs) || Segments[_cs >> 3].not_present ||
    Segments[_cs >> 3].type != MODIFY_LDT_CONTENTS_CODE) {
    if (in_dosemu) {
      error("CS selector invalid: 0x%04X, type=%x np=%i\n",
        _cs, Segments[_cs >> 3].type, Segments[_cs >> 3].not_present);
      D_printf("%s", DPMI_show_state(scp));
    }
    return 0;
  }
  if (_eip > GetSegmentLimit(_cs)) {
    if (in_dosemu) {
      error("IP outside CS limit: ip=%#x, cs=%#x, lim=%#x, 32=%i\n",
          _eip, _cs, GetSegmentLimit(_cs), Segments[_cs >> 3].is_32);
      D_printf("%s", DPMI_show_state(scp));
    }
    return 0;
  }

/*
IF SS is loaded;
THEN
   IF selector is null THEN #GP(0);
FI;
   Selector index must be within its descriptor table limits else
      #GP(selector);
   Selector's RPL must equal CPL else #GP(selector);
AR byte must indicate a writable data segment else #GP(selector);
   DPL in the AR byte must equal CPL else #GP(selector);
   Segment must be marked present else #SS(selector);
   Load SS with selector;
   Load SS with descriptor.
FI;
*/
  if (!ValidAndUsedSelector(_ss) || ((_ss & 3) != 3) ||
     Segments[_ss >> 3].readonly || Segments[_ss >> 3].not_present ||
     (Segments[_ss >> 3].type != MODIFY_LDT_CONTENTS_STACK &&
      Segments[_ss >> 3].type != MODIFY_LDT_CONTENTS_DATA)) {
    if (in_dosemu) {
      error("SS selector invalid: 0x%04X, type=%x np=%i\n",
        _ss, Segments[_ss >> 3].type, Segments[_ss >> 3].not_present);
      D_printf("%s", DPMI_show_state(scp));
    }
    return 0;
  }

  if (!CheckDataSelector(scp, _ds, 'D', in_dosemu)) return 0;
  if (!CheckDataSelector(scp, _es, 'E', in_dosemu)) return 0;
  if (!CheckDataSelector(scp, _fs, 'F', in_dosemu)) return 0;
  if (!CheckDataSelector(scp, _gs, 'G', in_dosemu)) return 0;
  return 1;
}

unsigned int GetSegmentBase(unsigned short selector)
{
  if (!ValidAndUsedSelector(selector))
    return 0;
  return Segments[selector >> 3].base_addr;
}

unsigned int GetSegmentLimit(unsigned short selector)
{
  unsigned int limit;
  if (!ValidAndUsedSelector(selector))
    return 0;
  limit = Segments[selector >> 3].limit;
  if (Segments[selector >> 3].is_big)
    limit = (limit << 12) | 0xfff;
  return limit;
}

int SetSegmentBaseAddress(unsigned short selector, unsigned long baseaddr)
{
  unsigned short ldt_entry = selector >> 3;
  int ret;
  D_printf("DPMI: SetSegmentBaseAddress[0x%04x;0x%04x] 0x%08lx\n", ldt_entry, selector, baseaddr);
  if (!ValidAndUsedSelector((ldt_entry << 3) | 7))
    return -1;
  Segments[ldt_entry].base_addr = baseaddr;
  ret = set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, Segments[ldt_entry].readonly,
	Segments[ldt_entry].is_big,
	Segments[ldt_entry].not_present, Segments[ldt_entry].useable);
  msdos_ldt_update(selector, 1);
  return ret;
}

int SetSegmentLimit(unsigned short selector, unsigned int limit)
{
  unsigned short ldt_entry = selector >> 3;
  int ret;
  D_printf("DPMI: SetSegmentLimit[0x%04x;0x%04x] 0x%08x\n", ldt_entry, selector, limit);
  if (!ValidAndUsedSelector((ldt_entry << 3) | 7))
    return -1;
  if (limit > 0x0fffff) {
    /* "Segment limits greater than 1M must have the low 12 bits set" */
    if (~limit & 0xfff) return -1;
    Segments[ldt_entry].limit = (limit>>12) & 0xfffff;
    Segments[ldt_entry].is_big = 1;
  } else {
    Segments[ldt_entry].limit = limit;
    Segments[ldt_entry].is_big = 0;
  }
  ret = set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr,
	Segments[ldt_entry].limit, Segments[ldt_entry].is_32,
	Segments[ldt_entry].type, Segments[ldt_entry].readonly,
	Segments[ldt_entry].is_big,
	Segments[ldt_entry].not_present, Segments[ldt_entry].useable);
  msdos_ldt_update(selector, 1);
  return ret;
}

int SetDescriptorAccessRights(unsigned short selector, unsigned short acc_rights)
{
  unsigned short ldt_entry = selector >> 3;
  int ret;
  D_printf("DPMI: SetDescriptorAccessRights[0x%04x;0x%04x] 0x%04x\n", ldt_entry, selector, acc_rights);
  if (!ValidAndUsedSelector((ldt_entry << 3) | 7))
    return -1; /* invalid selector 8022 */
  /* Check DPL and "must be 1" fields, as suggested by specs */
  if ((acc_rights & 0x70) != 0x70)
    return -2; /* invalid value 8021 */

  Segments[ldt_entry].type = (acc_rights >> 2) & 3;
  Segments[ldt_entry].is_32 = (acc_rights >> 14) & 1;
  Segments[ldt_entry].is_big = (acc_rights >> 15) & 1;
  Segments[ldt_entry].readonly = ((acc_rights >> 1) & 1) ? 0 : 1;
  Segments[ldt_entry].not_present = ((acc_rights >> 7) & 1) ? 0 : 1;
  Segments[ldt_entry].useable = (acc_rights >> 12) & 1;
  ret = set_ldt_entry(ldt_entry , Segments[ldt_entry].base_addr, Segments[ldt_entry].limit,
	Segments[ldt_entry].is_32, Segments[ldt_entry].type,
	Segments[ldt_entry].readonly, Segments[ldt_entry].is_big,
	Segments[ldt_entry].not_present, Segments[ldt_entry].useable);
  msdos_ldt_update(selector, 1);
  return ret;
}

unsigned short CreateAliasDescriptor(unsigned short selector)
{
  us ds_selector;
  us cs_ldt= selector >> 3;

  ds_selector = AllocateDescriptors(1);
  if (SetSelector(ds_selector, Segments[cs_ldt].base_addr, Segments[cs_ldt].limit,
			Segments[cs_ldt].is_32, MODIFY_LDT_CONTENTS_DATA,
			Segments[cs_ldt].readonly, Segments[cs_ldt].is_big,
			Segments[cs_ldt].not_present, Segments[cs_ldt].useable))
    return 0;
  msdos_ldt_update(selector, 1);
  return ds_selector;
}

static inline int do_LAR(us selector)
{
  int ret;
#ifdef X86_EMULATOR
  if (config.cpu_vm_dpmi != CPUVM_NATIVE)
    return emu_do_LAR(selector);
  else
#endif
    asm volatile(
      "larw %%ax,%%ax\n"
      "jz 1f\n"
      "xorl %%eax,%%eax\n"
      "1: shrl $8,%%eax\n"
      : "=a"(ret)
      : "a"(selector)
    );
  return ret;
}

int GetDescriptor(us selector, unsigned int *lp)
{
  int typebyte;
  unsigned char *type_ptr;
  if (!ValidAndUsedSelector(selector))
    return -1; /* invalid value 8021 */
  memcpy((unsigned char *)lp, &ldt_buffer[selector & 0xfff8], LDT_ENTRY_SIZE);
  /* DANG_BEGIN_REMARK
   * Hopefully the below LAR can serve as a replacement for the KERNEL_LDT,
   * which we are abandoning now. Especially the 'accessed-bit' will get
   * updated in the ldt-cache with the code below.
   * Most DPMI-clients fortunately _are_ using LAR also to get this info,
   * however, some do not. Some of those which do _not_, at least use the
   * DPMI-GetDescriptor function, so this may solve the problem.
   *                      (Hans Lermen, July 1996)
   * DANG_END_REMARK
   */
  if (!Segments[selector >> 3].not_present) {
    typebyte = do_LAR(selector);
    type_ptr = ((unsigned char *)lp) + 5;
    if (typebyte != *type_ptr) {
	D_printf("DPMI: change type only in local selector\n");
	*type_ptr=typebyte;
    }
  }
  D_printf("DPMI: GetDescriptor[0x%04x;0x%04x]: 0x%08x%08x\n",
    selector >> 3, selector, lp[1], lp[0]);
  return 0;
}

int SetDescriptor(unsigned short selector, unsigned int *lp)
{
  unsigned int base_addr, limit;
  int np, ro, type, ld, ret;
  D_printf("DPMI: SetDescriptor[0x%04x;0x%04x] 0x%08x%08x\n", selector>>3, selector, *(lp+1), *lp);
  if (!ValidAndUsedSelector(selector) || SystemSelector(selector))
    return -1; /* invalid value 8022 */
  base_addr = (lp[1] & 0xFF000000) | ((lp[1] << 16) & 0x00FF0000) |
	((lp[0] >> 16) & 0x0000FFFF);
  limit = (lp[1] & 0x000F0000) | (lp[0] & 0x0000FFFF);
  type = (lp[1] >> 10) & 3;
  ro = ((lp[1] >> 9) & 1) ^ 1;
  np = ((lp[1] >> 15) & 1) ^ 1;
  ld = (lp[1] >> 12) & 1;
  if (!ld && !np) {
    D_printf("DPMI: invalid access type %x\n", lp[1] >> 8);
    return -1; /* invalid value 8021 */
  }

  ret = SetSelector(selector, base_addr, limit, (lp[1] >> 22) & 1, type, ro,
			(lp[1] >> 23) & 1, np, (lp[1] >> 20) & 1);
  msdos_ldt_update(selector, 1);
  return ret;
}

void GetFreeMemoryInformation(unsigned int *lp)
{
  /*00h*/	lp[0] = dpmi_free_memory();
  /*04h*/	lp[1] = dpmi_free_memory()/DPMI_page_size;
  /*08h*/	lp[2] = dpmi_free_memory()/DPMI_page_size;
  /*0ch*/	lp[3] = dpmi_lin_mem_rsv()/DPMI_page_size;  // not accurate
  /*10h*/	lp[4] = dpmi_total_memory/DPMI_page_size;
  /*14h*/	lp[5] = dpmi_free_memory()/DPMI_page_size;
  /*18h*/	lp[6] = dpmi_total_memory/DPMI_page_size;
  /*1ch*/	lp[7] = dpmi_lin_mem_free()/DPMI_page_size;
#if 0
  /*20h*/	lp[8] = dpmi_total_memory/DPMI_page_size;
#else
		/* report no swap, or the locked/unlocked
		 * amounts will have to be adjusted */
  /*20h*/	lp[8] = 0;
#endif
  /*24h*/	lp[9] = 0xffffffff;
  /*28h*/	lp[0xa] = 0xffffffff;
  /*2ch*/	lp[0xb] = 0xffffffff;
}

void copy_context(sigcontext_t *d, sigcontext_t *s,
    int copy_fpu)
{
#ifdef __linux__
  fpregset_t fptr = d->fpregs;
  *d = *s;
  switch (copy_fpu) {
    case 1:   // copy FPU context
      if (fptr == s->fpregs)
        dosemu_error("Copy FPU context between the same locations?\n");
      *fptr = *s->fpregs;
      /* fallthrough */
    case -1:  // don't copy
      d->fpregs = fptr;
      break;
  }
#endif
  sigcontext_t *scp = d;
  sanitize_flags(_eflags);
}

void dpmi_return(sigcontext_t *scp, int retcode)
{
  /* only used for CPUVM_NATIVE (from sigsegv.c: dosemu_fault1()) */
  if (!DPMIValidSelector(_cs)) {
    dosemu_error("Return to dosemu requested within dosemu context\n");
    return;
  }
  copy_context(&DPMI_CLIENT.stack_frame, scp, 1);
  dpmi_ret_val = retcode;
  signal_return_to_dosemu();
  co_resume(co_handle);
  signal_return_to_dpmi();
  if (dpmi_ret_val == DPMI_RET_EXIT)
    copy_context(scp, &emu_stack_frame, 1);
  else
    copy_context(scp, &DPMI_CLIENT.stack_frame, 1);
}

static void dpmi_switch_sa(int sig, siginfo_t *inf, void *uc)
{
  ucontext_t *uct = uc;
  sigcontext_t *scp = &uct->uc_mcontext;
#ifdef __linux__
  emu_stack_frame.fpregs = aligned_alloc(16, sizeof(*__fpstate));
#endif
  copy_context(&emu_stack_frame, scp, 1);
  copy_context(scp, &DPMI_CLIENT.stack_frame, 1);
  sigaction(DPMI_TMP_SIG, &emu_tmp_act, NULL);
  deinit_handler(scp, &uct->uc_flags);
}

static void indirect_dpmi_transfer(void)
{
  struct sigaction act;

  act.sa_flags = SA_SIGINFO;
  sigfillset(&act.sa_mask);
  sigdelset(&act.sa_mask, SIGSEGV);
  act.sa_sigaction = dpmi_switch_sa;
  sigaction(DPMI_TMP_SIG, &act, &emu_tmp_act);
  signal_set_altstack(1);
  /* for some absolutely unclear reason neither pthread_self() nor
   * pthread_kill() are the memory barriers. */
  asm volatile("" ::: "memory");
  pthread_kill(pthread_self(), DPMI_TMP_SIG);
  /* and we are back */
  signal_set_altstack(0);
#ifdef __linux__
  free(emu_stack_frame.fpregs);
#endif
}

static void *enter_lpms(sigcontext_t *scp)
{
  unsigned short pmstack_sel;
  unsigned long pmstack_esp;
  if (!DPMI_CLIENT.in_dpmi_pm_stack) {
    D_printf("DPMI: Switching to locked stack\n");
    pmstack_sel = DPMI_CLIENT.PMSTACK_SEL;
    if (_ss == DPMI_CLIENT.PMSTACK_SEL)
      error("DPMI: App is working on host\'s PM locked stack, expect troubles!\n");
  }
  else {
    D_printf("DPMI: Not switching to locked stack, in_dpmi_pm_stack=%d\n",
      DPMI_CLIENT.in_dpmi_pm_stack);
    pmstack_sel = _ss;
  }

  if (_ss == DPMI_CLIENT.PMSTACK_SEL || DPMI_CLIENT.in_dpmi_pm_stack)
    pmstack_esp = client_esp(scp);
  else
    pmstack_esp = D_16_32(DPMI_pm_stack_size);

  if (pmstack_esp < 100) {
      error("PM stack overflowed: in_dpmi_pm_stack=%i\n", DPMI_CLIENT.in_dpmi_pm_stack);
      leavedos(25);
  }

  _ss = pmstack_sel;
  _esp = D_16_32(pmstack_esp);
  DPMI_CLIENT.in_dpmi_pm_stack++;

  return SEL_ADR_CLNT(pmstack_sel, pmstack_esp, DPMI_CLIENT.is_32);
}

static void leave_lpms(sigcontext_t *scp)
{
  if (DPMI_CLIENT.in_dpmi_pm_stack) {
    DPMI_CLIENT.in_dpmi_pm_stack--;
    if (!DPMI_CLIENT.in_dpmi_pm_stack && _ss != DPMI_CLIENT.PMSTACK_SEL) {
      error("DPMI: Client's PM Stack corrupted during PM callback!\n");
//    leavedos(91);
    }
  }
}

/* note: the below register translations are used only when
 * calling some (non-msdos.c) real-mode interrupts reflected
 * from protected mode int instruction. They are not used by
 * any DPMI API services that call to real-mode, like 0x301, 0x302.
 * So we use the conservative approach of preserving the high
 * register words in protected mode, and zero out high register
 * words in real mode. The preserving may be needed because some
 * realmode int handlers may trash the high register words. */
static void pm_to_rm_regs(sigcontext_t *scp, unsigned int mask)
{
  if (mask & (1 << eflags_INDEX))
    REG(eflags) = eflags_VIF(_eflags);
  if (mask & (1 << eax_INDEX))
    REG(eax) = D_16_32(_eax);
  if (mask & (1 << ebx_INDEX))
    REG(ebx) = D_16_32(_ebx);
  if (mask & (1 << ecx_INDEX))
    REG(ecx) = D_16_32(_ecx);
  if (mask & (1 << edx_INDEX))
    REG(edx) = D_16_32(_edx);
  if (mask & (1 << esi_INDEX))
    REG(esi) = D_16_32(_esi);
  if (mask & (1 << edi_INDEX))
    REG(edi) = D_16_32(_edi);
  if (mask & (1 << ebp_INDEX))
    REG(ebp) = D_16_32(_ebp);
}

static void rm_to_pm_regs(sigcontext_t *scp, unsigned int mask)
{
  /* NDN insists on full 32bit copy. It sets up its own
   * int handlers in real mode, using those not covered
   * by msdos.c. */
#define CP_16_32(reg) do { \
  if (DPMI_CLIENT.is_32) \
    _##reg = REG(reg); \
  else \
    _LWORD(reg) = LWORD(reg); \
} while(0)
  if (mask & (1 << eflags_INDEX))
    _eflags = 0x0202 | (0x0dd5 & REG(eflags));
  if (mask & (1 << eax_INDEX))
    CP_16_32(eax);
  if (mask & (1 << ebx_INDEX))
    CP_16_32(ebx);
  if (mask & (1 << ecx_INDEX))
    CP_16_32(ecx);
  if (mask & (1 << edx_INDEX))
    CP_16_32(edx);
  if (mask & (1 << esi_INDEX))
    CP_16_32(esi);
  if (mask & (1 << edi_INDEX))
    CP_16_32(edi);
  if (mask & (1 << ebp_INDEX))
    CP_16_32(ebp);
}

/* the below are used by DPMI API realmode services, and should
 * be able to pass full 32bit register values. */
static void DPMI_save_rm_regs(struct RealModeCallStructure *rmreg)
{
    rmreg->edi = REG(edi);
    rmreg->esi = REG(esi);
    rmreg->ebp = REG(ebp);
    rmreg->ebx = REG(ebx);
    rmreg->edx = REG(edx);
    rmreg->ecx = REG(ecx);
    rmreg->eax = REG(eax);
    rmreg->flags = get_FLAGS(REG(eflags));
    rmreg->es = SREG(es);
    rmreg->ds = SREG(ds);
    rmreg->fs = SREG(fs);
    rmreg->gs = SREG(gs);
    rmreg->cs = SREG(cs);
    rmreg->ip = LWORD(eip);
    rmreg->ss = SREG(ss);
    rmreg->sp = LWORD(esp);
}

static void DPMI_restore_rm_regs(struct RealModeCallStructure *rmreg, int mask)
{
#define RMR(x) \
    if (mask & (1 << x##_INDEX)) \
	REG(x) = rmreg->x
#define SRMR(x) \
    if (mask & (1 << x##_INDEX)) \
	SREG(x) = rmreg->x
    RMR(edi);
    RMR(esi);
    RMR(ebp);
    RMR(ebx);
    RMR(edx);
    RMR(ecx);
    RMR(eax);
    if (mask & (1 << eflags_INDEX))
	REG(eflags) = get_EFLAGS(rmreg->flags);
    SRMR(es);
    SRMR(ds);
    SRMR(fs);
    SRMR(gs);
    if (mask & (1 << eip_INDEX))
	LWORD(eip) = rmreg->ip;
    SRMR(cs);
    if (mask & (1 << esp_INDEX))
	LWORD(esp) = rmreg->sp;
    SRMR(ss);
}

static void save_rm_regs(void)
{
  int in_dpmi_rm_stack;
  int clnt_idx;

  if (DPMI_rm_procedure_running >= DPMI_max_rec_rm_func) {
    error("DPMI: DPMI_rm_procedure_running = 0x%x\n",DPMI_rm_procedure_running);
    leavedos(25);
  }
  in_dpmi_rm_stack = DPMI_rm_procedure_running % DPMI_rm_stacks;
  clnt_idx = DPMI_rm_procedure_running / DPMI_rm_stacks;
  DPMI_save_rm_regs(&DPMI_rm_stack[DPMI_rm_procedure_running]);
  DPMI_rm_procedure_running++;
  in_dpmi_rm_stack++;
  if (clnt_idx < in_dpmi) {
    D_printf("DPMI: switching to realmode stack, in_dpmi_rm_stack=%i\n",
      in_dpmi_rm_stack);
    SREG(ss) = DPMIclient[clnt_idx].private_data_segment;
    REG(esp) = DPMI_rm_stack_size * in_dpmi_rm_stack;
  } else {
    error("DPMI: too many nested realmode invocations, in_dpmi_rm_stack=%i\n",
      DPMI_rm_procedure_running);
  }
}

static void restore_rm_regs(void)
{
  if (DPMI_rm_procedure_running > DPMI_max_rec_rm_func ||
    DPMI_rm_procedure_running < 1) {
    error("DPMI: DPMI_rm_procedure_running = 0x%x\n",DPMI_rm_procedure_running);
    leavedos(25);
  }
  DPMI_rm_procedure_running--;
  DPMI_restore_rm_regs(&DPMI_rm_stack[DPMI_rm_procedure_running], ~0);
  D_printf("DPMI: return from realmode procedure, in_dpmi_rm_stack=%i\n",
      DPMI_rm_procedure_running);
}

static void post_rm_call(int old_client)
{
  assert(current_client == in_dpmi - 1);
  assert(old_client <= in_dpmi - 1);
  assert(DPMI_rm_procedure_running);
  if (old_client != current_client) {
    /* 32rtm returned w/o terminating (stayed resident).
     * We switch to the prev client here. */
    D_printf("DPMI: client switch %i --> %i\n", current_client, old_client);
    current_client = old_client;
  }
}

static void save_pm_regs(sigcontext_t *scp)
{
  if (DPMI_pm_procedure_running >= DPMI_max_rec_pm_func) {
    error("DPMI: DPMI_pm_procedure_running = 0x%x\n",DPMI_pm_procedure_running);
    leavedos(25);
  }
  _eflags = eflags_VIF(_eflags);
  copy_context(&DPMI_pm_stack[DPMI_pm_procedure_running++], scp, 0);
}

static void restore_pm_regs(sigcontext_t *scp)
{
  if (DPMI_pm_procedure_running > DPMI_max_rec_pm_func ||
    DPMI_pm_procedure_running < 1) {
    error("DPMI: DPMI_pm_procedure_running = 0x%x\n",DPMI_pm_procedure_running);
    leavedos(25);
  }
  copy_context(scp, &DPMI_pm_stack[--DPMI_pm_procedure_running], -1);
  if (_eflags & VIF) {
    if (!isset_IF())
      D_printf("DPMI: set IF on restore_pm_regs\n");
    set_IF();
  } else {
    if (isset_IF())
      D_printf("DPMI: clear IF on restore_pm_regs\n");
    clear_IF();
  }
}

static void __fake_pm_int(sigcontext_t *scp)
{
  D_printf("DPMI: fake_pm_int() called, dpmi_pm=0x%02x\n", in_dpmi_pm());
  save_rm_regs();
  pm_to_rm_regs(scp, ~0);
  SREG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
  dpmi_set_pm(0);
  clear_TF();
  clear_NT();
  if (IS_CR0_AM_SET())
      clear_AC();
  clear_IF();
}

void fake_pm_int(void)
{
  if (fault_cnt)
    dosemu_error("fake_pm_int called from within the signal context\n");
  __fake_pm_int(&DPMI_CLIENT.stack_frame);
}

static void get_ext_API(sigcontext_t *scp)
{
      char *ptr = SEL_ADR_CLNT(_ds, _esi, DPMI_CLIENT.is_32);
      D_printf("DPMI: GetVendorAPIEntryPoint: %s\n", ptr);
      if (!strcmp("VIRTUAL SUPPORT", ptr)) {
	_LO(ax) = 0;
      } else
      if (!strcmp("PHARLAP.HWINT_SUPPORT", ptr) || !strcmp("PHARLAP.CE_SUPPORT", ptr) ||
	    !strcmp("PHARLAP.16", ptr) || !strncmp("PHARLAP.", ptr, 8)) {
//	p_dos_str("DPMI: pharlap extender unsupported (%s)\n", ptr);
	D_printf("DPMI: pharlap extender unsupported (%s)\n", ptr);
	DPMI_CLIENT.feature_flags |= DF_PHARLAP;
	_LO(ax) = 0;
      } else if (!strcmp("THUNK_16_32", ptr)) {
	_LO(ax) = 0;
	_es = dpmi_sel();
	_edi = DPMI_SEL_OFF(DPMI_API_extension);
      } else {
	if (!(_LWORD(eax)==0x168a))
	  _eax = 0x8001;
	_eflags |= CF;
      }
}

static int ResizeDescriptorBlock(sigcontext_t *scp,
 unsigned short begin_selector, unsigned long length)
{
    unsigned short num_descs, old_num_descs;
    unsigned int old_length, base;
    int i;

    if (!ValidAndUsedSelector(begin_selector)) return 0;
    base = GetSegmentBase(begin_selector);
    old_length = GetSegmentLimit(begin_selector) + 1;
    old_num_descs = (old_length ? (DPMI_CLIENT.is_32 ? 1 : (old_length/0x10000 +
				((old_length%0x10000) ? 1 : 0))) : 0);
    num_descs = (length ? (DPMI_CLIENT.is_32 ? 1 : (length/0x10000 +
				((length%0x10000) ? 1 : 0))) : 0);

    if (num_descs > old_num_descs) {
	/* grow block. This can fail so do this before anything else. */
	if (! AllocateDescriptorsAt(begin_selector + (old_num_descs<<3),
	    num_descs - old_num_descs)) {
	    _LWORD(eax) = 0x8011;
	    _LWORD(ebx) = old_length >> 4;
	    D_printf("DPMI: Unable to allocate %i descriptors starting at 0x%x\n",
		num_descs - old_num_descs, begin_selector + (old_num_descs<<3));
	    return 0;
	}
	/* last descriptor has a valid base, lets adjust the limit */
	if (SetSegmentLimit(begin_selector + ((old_num_descs-1)<<3), 0xffff))
	    return 0;
        /* init all the newly allocated descs, including the last one */
	for (i = old_num_descs; i < num_descs; i++) {
	    if (SetSelector(begin_selector + (i<<3), base+i*0x10000,
		    0xffff, DPMI_CLIENT.is_32,
		    MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) return 0;
	}
    }
    if (old_num_descs > num_descs) {
	/* shrink block */
	for (i = num_descs; i < old_num_descs; i++) {
	    FreeDescriptor(begin_selector + (i<<3));
	    FreeSegRegs(scp, begin_selector + (i<<3));
	}
    }

    if (num_descs > 0) {
	/* adjust the limit of the leading descriptor */
	if (SetSegmentLimit(begin_selector, length-1)) return 0;
    }
    if (num_descs > 1) {
	/* now adjust the last descriptor. Base is already set. */
	if (SetSegmentLimit(begin_selector + ((num_descs-1)<<3),
	    (length%0x10000 ? (length%0x10000)-1 : 0xffff))) return 0;
    }

    return 1;
}

DPMI_INTDESC dpmi_get_interrupt_vector(unsigned char num)
{
    DPMI_INTDESC desc;
    desc.selector = DPMI_CLIENT.Interrupt_Table[num].selector;
    desc.offset32 = DPMI_CLIENT.Interrupt_Table[num].offset;
    return desc;
}

void dpmi_set_interrupt_vector(unsigned char num, DPMI_INTDESC desc)
{
    DPMI_CLIENT.Interrupt_Table[num].selector = desc.selector;
    DPMI_CLIENT.Interrupt_Table[num].offset = desc.offset32;
    if (config.cpu_vm_dpmi == CPUVM_KVM) {
        /* KVM: we can directly use the IDT but don't do it when debugging */
#ifdef USE_MHPDBG
        if (mhpdbg.active && dpmi_mhp_intxxtab[num])
            kvm_set_idt_default(num);
        else
#endif
        if (desc.selector == dpmi_sel())
            kvm_set_idt_default(num);
        else
            kvm_set_idt(num, desc.selector, desc.offset32, DPMI_CLIENT.is_32);
    }
}

dpmi_pm_block DPMImalloc(unsigned long size)
{
    dpmi_pm_block dummy, *ptr;
    memset(&dummy, 0, sizeof(dummy));
    ptr = DPMI_malloc(&DPMI_CLIENT.pm_block_root, size);
    if (ptr)
	return *ptr;
    return dummy;
}
dpmi_pm_block DPMImallocLinear(unsigned long base, unsigned long size, int committed)
{
    dpmi_pm_block dummy, *ptr;
    memset(&dummy, 0, sizeof(dummy));
    ptr = DPMI_mallocLinear(&DPMI_CLIENT.pm_block_root, base, size, committed);
    if (ptr)
	return *ptr;
    return dummy;
}
int DPMIfree(unsigned long handle)
{
    return DPMI_free(&DPMI_CLIENT.pm_block_root, handle);
}
dpmi_pm_block DPMIrealloc(unsigned long handle, unsigned long size)
{
    dpmi_pm_block dummy, *ptr;
    memset(&dummy, 0, sizeof(dummy));
    ptr = DPMI_realloc(&DPMI_CLIENT.pm_block_root, handle, size);
    if (ptr)
	return *ptr;
    return dummy;
}
dpmi_pm_block DPMIreallocLinear(unsigned long handle, unsigned long size,
  int committed)
{
    dpmi_pm_block dummy, *ptr;
    memset(&dummy, 0, sizeof(dummy));
    ptr = DPMI_reallocLinear(&DPMI_CLIENT.pm_block_root, handle, size,
	    committed);
    if (ptr)
	return *ptr;
    return dummy;
}
void DPMIfreeAll(void)
{
    return DPMI_freeAll(&DPMI_CLIENT.pm_block_root);
}

int DPMIMapConventionalMemory(unsigned long handle, unsigned long offset,
			  unsigned long low_addr, unsigned long cnt)
{
    return DPMI_MapConventionalMemory(&DPMI_CLIENT.pm_block_root,
	handle, offset, low_addr, cnt);
}
int DPMISetPageAttributes(unsigned long handle, int offs, us attrs[], int count)
{
    return DPMI_SetPageAttributes(&DPMI_CLIENT.pm_block_root,
	handle, offs, attrs, count);
}
int DPMIGetPageAttributes(unsigned long handle, int offs, us attrs[], int count)
{
    return DPMI_GetPageAttributes(&DPMI_CLIENT.pm_block_root,
	handle, offs, attrs, count);
}

#ifdef __linux__
static int get_dr(pid_t pid, int i, unsigned int *dri)
{
  *dri = ptrace(PTRACE_PEEKUSER, pid,
		(void *)offsetof(struct user, u_debugreg[i]), 0);
  D_printf("DPMI: ptrace peek user dr%d=%x\n", i, *dri);
  return *dri != -1 || errno == 0;
}

static int set_dr(pid_t pid, int i, unsigned long dri)
{
  int r = ptrace(PTRACE_POKEUSER, pid,
		 (void *)offsetof(struct user, u_debugreg[i]), (void *)dri);
  D_printf("DPMI: ptrace poke user r=%d dr%d=%lx\n", r, i, dri);
  return r == 0;
}
#endif

static int dpmi_debug_breakpoint(int op, sigcontext_t *scp)
{
#ifdef __linux__
  pid_t pid, vpid;
  int err, r, status;

  if (debug_level('M'))
  {
    switch(op) {
    case 0:
      D_printf("DPMI: Set breakpoint type %x size %x at %04x%04x\n",
	_HI(dx),_LO(dx),_LWORD(ebx),_LWORD(ecx));
      break;
    case 1:
      D_printf("DPMI: Clear breakpoint %x\n",_LWORD(ebx));
      break;
    case 2:
      D_printf("DPMI: Breakpoint %x state\n",_LWORD(ebx));
      break;
    case 3:
      D_printf("DPMI: Reset breakpoint %x\n",_LWORD(ebx));
      break;
    }
  }

  if (op == 0) {
    err = 0x21; /* invalid value (in DL or DH) */
    if (_LO(dx) == 0 || _LO(dx) == 3 || _LO(dx) > 4 || _HI(dx) > 2)
      return err;
    err = 0x16; /* too many breakpoints */
  } else {
    err = 0x23; /* invalid handle */
    if (_LWORD(ebx) > 3)
      return err;
  }

#ifdef X86_EMULATOR
  if (config.cpuemu>3) {
    e_dpmi_b0x(op,scp);
    return 0;
  }
#endif

  pid = getpid();
  vpid = fork();
  if (vpid == (pid_t)-1)
    return err;
  if (vpid == 0) {
    unsigned int dr6, dr7;
    /* child ptraces parent */
    r = ptrace(PTRACE_ATTACH, pid, 0, 0);
    D_printf("DPMI: ptrace attach %d op=%d\n", r, op);
    if (r == -1)
      _exit(err);
    do {
      r = waitpid(pid, &status, 0);
    } while (r == pid && !WIFSTOPPED(status));
    if (r == pid) switch (op) {
      case 0: {   /* set */
	int i;
	if(get_dr(pid, 7, &dr7)) for (i=0; i<4; i++) {
	  if ((~dr7 >> (i*2)) & 3) {
	    unsigned mask;
	    if (!set_dr(pid, i, (_LWORD(ebx) << 16) | _LWORD(ecx))) {
	      err = 0x25;
	      break;
	    }
	    dr7 |= (3 << (i*2));
	    mask = _HI(dx) & 3; if (mask==2) mask++;
	    mask |= ((_LO(dx)-1) << 2) & 0x0c;
	    dr7 |= mask << (i*4 + 16);
	    if (set_dr(pid, 7, dr7))
	      err = i;
	    break;
	  }
	}
	break;
      }
      case 1:   /* clear */
	if(get_dr(pid, 6, &dr6) && get_dr(pid, 7, &dr7)) {
	  int i = _LWORD(ebx);
	  dr6 &= ~(1 << i);
	  dr7 &= ~(3 << (i*2));
	  dr7 &= ~(15 << (i*4+16));
	  if (set_dr(pid, 6, dr6) && set_dr(pid, 7, dr7))
	    err = 0;
	  break;
	}
      case 2:   /* get */
	if(get_dr(pid, 6, &dr6))
	  err = (dr6 >> _LWORD(ebx)) & 1;
        break;
      case 3:   /* reset */
	if(get_dr(pid, 6, &dr6)) {
          dr6 &= ~(1 << _LWORD(ebx));
          if (set_dr(pid, 6, dr6))
	    err = 0;
	}
        break;
    }
    ptrace(PTRACE_DETACH, pid, 0, 0);
    D_printf("DPMI: ptrace detach\n");
    _exit(err);
  }
  D_printf("DPMI: waitpid start\n");
  r = waitpid(vpid, &status, 0);
  if (r != vpid || !WIFEXITED(status))
    return err;
  err = WEXITSTATUS(status);
  if (err >= 0 && err < 4) {
    if (op == 0)
      _LWORD(ebx) = err;
    else if (op == 2)
      _LWORD(eax) = err;
    err = 0;
  }
  D_printf("DPMI: waitpid end, err=%#x, op=%d\n", err, op);
  return err;
#else
  return -1;
#endif
}

far_t DPMI_allocate_realmode_callback(u_short sel, int offs, u_short rm_sel,
	int rm_offs)
{
  int i;
  far_t ret = {};
  u_short rm_seg, rm_off;
  for (i = 0; i < DPMI_MAX_RMCBS; i++)
    if ((DPMI_CLIENT.realModeCallBack[i].selector == 0) &&
        (DPMI_CLIENT.realModeCallBack[i].offset == 0))
      break;
  if (i >= DPMI_MAX_RMCBS) {
    D_printf("DPMI: Allocate real mode call back address failed.\n");
    return ret;
  }
  if (!(DPMI_CLIENT.realModeCallBack[i].rm_ss_selector
	     = AllocateDescriptors(1))) {
    D_printf("DPMI: Allocate real mode call back address failed.\n");
    return ret;
  }

  DPMI_CLIENT.realModeCallBack[i].selector = sel;
  DPMI_CLIENT.realModeCallBack[i].offset = offs;
  DPMI_CLIENT.realModeCallBack[i].rmreg_selector = rm_sel;
  DPMI_CLIENT.realModeCallBack[i].rmreg_offset = rm_offs;
  DPMI_CLIENT.realModeCallBack[i].rmreg = SEL_ADR(rm_sel, rm_offs);
  rm_seg = DPMI_CLIENT.rmcb_seg;
  rm_off = DPMI_CLIENT.rmcb_off + i;
  D_printf("DPMI: Allocate realmode callback for %#04x:%#08x use #%i callback address, %#4x:%#4x\n",
		DPMI_CLIENT.realModeCallBack[i].selector,
		DPMI_CLIENT.realModeCallBack[i].offset,i,
		rm_seg, rm_off);
  ret.segment = rm_seg;
  ret.offset = rm_off;
  return ret;
}

int DPMI_free_realmode_callback(u_short seg, u_short off)
{
  if ((seg == DPMI_CLIENT.rmcb_seg)
        && (off >= DPMI_CLIENT.rmcb_off &&
	    off < DPMI_CLIENT.rmcb_off + DPMI_MAX_RMCBS)) {
    int i = off - DPMI_CLIENT.rmcb_off;
    D_printf("DPMI: Free realmode callback #%i\n", i);
    DPMI_CLIENT.realModeCallBack[i].selector = 0;
    DPMI_CLIENT.realModeCallBack[i].offset = 0;
    FreeDescriptor(DPMI_CLIENT.realModeCallBack[i].rm_ss_selector);
    return 0;
  }
  return -1;
}

int DPMI_get_save_restore_address(far_t *raddr, struct pmaddr_s *paddr)
{
      raddr->segment = DPMI_SEG;
      raddr->offset = DPMI_OFF + HLT_OFF(DPMI_save_restore_rm);
      paddr->selector = dpmi_sel();
      paddr->offset = DPMI_SEL_OFF(DPMI_save_restore_pm);
      return max(sizeof(struct RealModeCallStructure), 52); /* size to hold all registers */
}

int DPMI_allocate_specific_ldt_descriptor(unsigned short selector)
{
    if (!AllocateDescriptorsAt(selector, 1))
	return -1;
    return 0;
}

far_t DPMI_get_real_mode_interrupt_vector(int vec)
{
    return get_int_vector(vec);
}

static int count_shms(const char *name)
{
    int i;
    int cnt = 0;
    for (i = 0; i < in_dpmi; i++)
	cnt += count_shm_blocks(&DPMIclient[i].pm_block_root, name);
    return cnt;
}

static unsigned int get_shm_size(const char *name)
{
    int i;
    dpmi_pm_block *ptr = NULL;

    for (i = 0; i < in_dpmi; i++) {
	if ((ptr = lookup_pm_block_by_shmname(&DPMIclient[i].pm_block_root,
		name)))
	    break;
    }
    if (!ptr)
	return 0;
    return ptr->shmsize;
}

int DPMIAllocateShared(struct SHM_desc *shm)
{
    char *name = SEL_ADR_CLNT(shm->name_selector, shm->name_offset32,
	    DPMI_CLIENT.is_32);
    dpmi_pm_block *ptr = DPMI_mallocShared(&DPMI_CLIENT.pm_block_root, name,
	    shm->req_len, get_shm_size(name), shm->flags);
    if (!ptr)
	return -1;
    shm->ret_len = ptr->size;
    shm->handle = ptr->handle;
    shm->addr = ptr->base;
    return 0;
}

int DPMIFreeShared(uint32_t handle)
{
    int cnt;
    dpmi_pm_block *ptr = lookup_pm_block(&DPMI_CLIENT.pm_block_root, handle);

    cnt = count_shms(ptr->shmname);
    return DPMI_freeShared(&DPMI_CLIENT.pm_block_root, handle, cnt == 1);
}

DPMI_INTDESC dpmi_get_pm_exc_addr(int num)
{
    DPMI_INTDESC ret;

    ret.selector = DPMI_CLIENT.Exception_Table_PM[num].selector;
    ret.offset32 = DPMI_CLIENT.Exception_Table_PM[num].offset;
    return ret;
}

void dpmi_set_pm_exc_addr(int num, DPMI_INTDESC addr)
{
    DPMI_CLIENT.Exception_Table_PM[num].selector = addr.selector;
    DPMI_CLIENT.Exception_Table_PM[num].offset = addr.offset32;
}

static void do_int31(sigcontext_t *scp)
{
#if 0
/* old way to use DPMI API, as per specs */
#define API_32(s) DPMI_CLIENT.is_32
#else
/* allow 16bit clients to access the 32bit API. dosemu's DPMI extension. */
#define API_32(scp) (DPMI_CLIENT.is_32 || (Segments[_cs >> 3].is_32 && \
    DPMI_CLIENT.ext__thunk_16_32))
#endif
#define API_16_32(x) (API_32(scp) ? (x) : (x) & 0xffff)
#define SEL_ADR_X(s, a) SEL_ADR_LDT(s, a, API_32(scp))

  if (debug_level('M')) {
    D_printf("DPMI: int31, ax=%04x, ebx=%08x, ecx=%08x, edx=%08x\n",
	_LWORD(eax),_ebx,_ecx,_edx);
    D_printf("        edi=%08x, esi=%08x, ebp=%08x, esp=%08x\n"
	     "        eip=%08x, eflags=%08x\n",
	_edi,_esi,_ebp,_esp,_eip,eflags_VIF(_eflags));
    D_printf("        cs=%04x, ds=%04x, ss=%04x, es=%04x, fs=%04x, gs=%04x\n",
	_cs,_ds,_ss,_es,_fs,_gs);
  }

  _eflags &= ~CF;
  switch (_LWORD(eax)) {
  case 0x0000:
    if (!(_LWORD(eax) = AllocateDescriptors(_LWORD(ecx)))) {
      _LWORD(eax) = 0x8011;
      _eflags |= CF;
    }
    break;
  case 0x0001:
    CHECK_SELECTOR(_LWORD(ebx));
    if (FreeDescriptor(_LWORD(ebx))){
      _LWORD(eax) = 0x8022;
      _eflags |= CF;
      break;
    }
    /* do it dpmi 1.00 host\'s way */
    FreeSegRegs(scp, _LWORD(ebx));
    break;
  case 0x0002:
    if (!(_LWORD(eax)=ConvertSegmentToDescriptor(_LWORD(ebx)))) {
      _LWORD(eax) = 0x8011;
      _eflags |= CF;
    }
    break;
  case 0x0003:
    _LWORD(eax) = GetNextSelectorIncrementValue();
    break;
  case 0x0004:	/* Reserved lock selector, see interrupt list */
  case 0x0005:	/* Reserved unlock selector, see interrupt list */
    break;
  case 0x0006:
    CHECK_SELECTOR(_LWORD(ebx));
    {
      unsigned int baddress = GetSegmentBase(_LWORD(ebx));
      _LWORD(edx) = (us)(baddress & 0xffff);
      _LWORD(ecx) = (us)(baddress >> 16);
    }
    break;
  case 0x0007:
    CHECK_SELECTOR(_LWORD(ebx));
    if (SetSegmentBaseAddress(_LWORD(ebx), ((uint32_t)_LWORD(ecx))<<16 | (_LWORD(edx)))) {
      _eflags |= CF;
      _LWORD(eax) = 0x8025;
    }
    break;
  case 0x0008:
    CHECK_SELECTOR(_LWORD(ebx));
    if (SetSegmentLimit(_LWORD(ebx), ((uint32_t)(_LWORD(ecx))<<16) | (_LWORD(edx)))) {
      _eflags |= CF;
      _LWORD(eax) = 0x8025;
    }
    break;
  case 0x0009:
    CHECK_SELECTOR(_LWORD(ebx));
    { int x;
      if ((x=SetDescriptorAccessRights(_LWORD(ebx), _LWORD(ecx))) !=0) {
        if (x == -1) _LWORD(eax) = 0x8021;
        else if (x == -2) _LWORD(eax) = 0x8022;
        else _LWORD(eax) = 0x8025;
        _eflags |= CF;
      }
    }
    break;
  case 0x000a:
    CHECK_SELECTOR(_LWORD(ebx));
    if (!(_LWORD(eax) = CreateAliasDescriptor(_LWORD(ebx)))) {
       _LWORD(eax) = 0x8011;
      _eflags |= CF;
    }
    break;
  case 0x000b:
    {
      unsigned int lp[2];
      GetDescriptor(_LWORD(ebx), lp);
      memcpy_2dos(GetSegmentBase(_es) + API_16_32(_edi), lp, sizeof(lp));
    }
    break;
  case 0x000c:
    if (SetDescriptor(_LWORD(ebx), SEL_ADR_X(_es, _edi))) {
      _LWORD(eax) = 0x8022;
      _eflags |= CF;
      break;
    }
    /* DPMI-1.0 requires us to reload segregs */
    if (Segments[_LWORD(ebx) >> 3].not_present)
      FreeSegRegs(scp, _LWORD(ebx));
    break;
  case 0x000d:
    if (DPMI_allocate_specific_ldt_descriptor(_LWORD(ebx))) {
      _LWORD(eax) = 0x8022;
      _eflags |= CF;
    }
    break;
  case 0x000e: {
      int i;
      struct sel_desc_s *array = SEL_ADR_X(_es, _edi);
      for (i = 0; i < _LWORD(ecx); i++) {
        if (GetDescriptor(array[i].selector, array[i].descriptor) == -1)
	  break;
      }
      if (i < _LWORD(ecx)) {
        _LWORD(eax) = 0x8022;
        _eflags |= CF;
      }
      _LWORD(ecx) = i;
    }
    break;
  case 0x000f: {
      int i;
      struct sel_desc_s *array = SEL_ADR_X(_es, _edi);
      for (i = 0; i < _LWORD(ecx); i++) {
        if (SetDescriptor(array[i].selector, array[i].descriptor) == -1)
	  break;
      }
      if (i < _LWORD(ecx)) {
        _LWORD(eax) = 0x8022;
        _eflags |= CF;
      }
      _LWORD(ecx) = i;
    }
    break;

  case 0x0100:			/* Dos allocate memory */
  case 0x0101:			/* Dos free memory */
  case 0x0102:			/* Dos resize memory */
    {
        D_printf("DPMI: call DOS memory service AX=0x%04X, BX=0x%04x, DX=0x%04x\n",
                  _LWORD(eax), _LWORD(ebx), _LWORD(edx));

	save_rm_regs();
	REG(eflags) = eflags_VIF(_eflags);
	REG(ebx) = _LWORD(ebx);
	SREG(es) = GetSegmentBase(_LWORD(edx)) >> 4;

	switch(_LWORD(eax)) {
	  case 0x0100: {
	    unsigned short begin_selector, num_descs;
	    unsigned long length;
	    int i;

	    length = _LWORD(ebx) << 4;
	    num_descs = (length ? (DPMI_CLIENT.is_32 ? 1 : (length/0x10000 +
					((length%0x10000) ? 1 : 0))) : 0);
	    if (!num_descs) goto err;

	    if (!(begin_selector = AllocateDescriptors(num_descs))) goto err;
	    _LWORD(edx) = begin_selector;

	    if (SetSelector(begin_selector, 0, length-1, DPMI_CLIENT.is_32,
			    MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) goto err;
	    for(i = 1; i < num_descs - 1; i++) {
		if (SetSelector(begin_selector + (i<<3), 0, 0xffff,
		    DPMI_CLIENT.is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0))
			goto err;
	    }
	    if (num_descs > 1) {
		if (SetSelector(begin_selector + ((num_descs-1)<<3), 0,
			    (length%0x10000 ? (length%0x10000)-1 : 0xffff),
			    DPMI_CLIENT.is_32, MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0))
		    goto err;
	    }

	    LWORD(eax) = 0x4800;
	  }
	  break;

	  case 0x0101:
	    if (!ResizeDescriptorBlock(scp, _LWORD(edx), 0))
		goto err;

	    LWORD(eax) = 0x4900;
	    break;

	  case 0x0102: {
	    unsigned long old_length;

	    if (!ValidAndUsedSelector(_LWORD(edx))) goto err;
	    old_length = GetSegmentLimit(_LWORD(edx)) + 1;
	    if (!ResizeDescriptorBlock(scp, _LWORD(edx), _LWORD(ebx) << 4))
		goto err;
	    _LWORD(ebx) = old_length >> 4; /* in case of a failure */

	    LWORD(eax) = 0x4a00;
	  }
	  break;
	}

	SREG(cs) = DPMI_SEG;
	REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos_memory);
	dpmi_set_pm(0);
	return do_int(0x21); /* call dos for memory service */

err:
        D_printf("DPMI: DOS memory service failed\n");
	_eflags |= CF;
	restore_rm_regs();
	dpmi_set_pm(1);
    }
    break;
  case 0x0200: {	/* Get Real Mode Interrupt Vector */
      far_t addr = DPMI_get_real_mode_interrupt_vector(_LO(bx));
      _LWORD(ecx) = addr.segment;
      _LWORD(edx) = addr.offset;
      D_printf("DPMI: Getting RM vec %#x = %#x:%#x\n", _LO(bx),_LWORD(ecx),_LWORD(edx));
    }
    break;
  case 0x0201:	/* Set Real Mode Interrupt Vector */
    SETIVEC(_LO(bx), _LWORD(ecx), _LWORD(edx));
    D_printf("DPMI: Setting RM vec %#x = %#x:%#x\n", _LO(bx),_LWORD(ecx),_LWORD(edx));
    break;
  case 0x0202:	/* Get Processor Exception Handler Vector */
    if (_LO(bx) >= 0x20) {
      _eflags |= CF;
      _eax = 0x8021;
      break;
    }
    _LWORD(ecx) = DPMI_CLIENT.Exception_Table[_LO(bx)].selector;
    _edx = DPMI_CLIENT.Exception_Table[_LO(bx)].offset;
    D_printf("DPMI: Getting Excp %#x = %#x:%#x\n", _LO(bx),_LWORD(ecx),_edx);
    break;
  case 0x0203:	/* Set Processor Exception Handler Vector */
    if (_LO(bx) >= 0x20) {
      _eflags |= CF;
      _eax = 0x8021;
      break;
    }
    D_printf("DPMI: Setting Excp %#x = %#x:%#x\n", _LO(bx),_LWORD(ecx),_edx);
    DPMI_CLIENT.Exception_Table[_LO(bx)].selector = _LWORD(ecx);
    DPMI_CLIENT.Exception_Table[_LO(bx)].offset = API_16_32(_edx);
    break;
  case 0x0204: {	/* Get Protected Mode Interrupt vector */
      DPMI_INTDESC desc = dpmi_get_interrupt_vector(_LO(bx));
      _LWORD(ecx) = desc.selector;
      _edx = desc.offset32;
      D_printf("DPMI: Get Prot. vec. bx=%x sel=%x, off=%x\n", _LO(bx), _LWORD(ecx), _edx);
    }
    break;
  case 0x0205: {	/* Set Protected Mode Interrupt vector */
    DPMI_INTDESC desc;
    desc.selector = _LWORD(ecx);
    desc.offset32 = API_16_32(_edx);
    dpmi_set_interrupt_vector(_LO(bx), desc);
    D_printf("DPMI: Put Prot. vec. bx=%x sel=%x, off=%x\n", _LO(bx),
      _LWORD(ecx), DPMI_CLIENT.Interrupt_Table[_LO(bx)].offset);
    break;
  }
  case 0x0210: {	/* Get Ext Processor Exception Handler Vector - PM */
    DPMI_INTDESC desc;
    if (_LO(bx) >= 0x20) {
      _eflags |= CF;
      _eax = 0x8021;
      break;
    }
    desc = dpmi_get_pm_exc_addr(_LO(bx));
    _LWORD(ecx) = desc.selector;
    _edx = desc.offset32;
    D_printf("DPMI: Getting Ext Excp %#x = %#x:%#x\n", _LO(bx),_LWORD(ecx),_edx);
    break;
  }
  case 0x0211:	/* Get Ext Processor Exception Handler Vector - RM */
    if (_LO(bx) >= 0x20) {
      _eflags |= CF;
      _eax = 0x8021;
      break;
    }
    _LWORD(ecx) = DPMI_CLIENT.Exception_Table_RM[_LO(bx)].selector;
    _edx = DPMI_CLIENT.Exception_Table_RM[_LO(bx)].offset;
    D_printf("DPMI: Getting RM Excp %#x = %#x:%#x\n", _LO(bx),_LWORD(ecx),_edx);
    break;
  case 0x0212: {	/* Set Ext Processor Exception Handler Vector - PM */
    DPMI_INTDESC desc;
    if (_LO(bx) >= 0x20) {
      _eflags |= CF;
      _eax = 0x8021;
      break;
    }
    D_printf("DPMI: Setting Ext Excp %#x = %#x:%#x\n", _LO(bx),_LWORD(ecx),_edx);
    desc.selector = _LWORD(ecx);
    desc.offset32 = API_16_32(_edx);
    dpmi_set_pm_exc_addr(_LO(bx), desc);
    break;
  }
  case 0x0213:	/* Set Ext Processor Exception Handler Vector - RM */
    if (_LO(bx) >= 0x20) {
      _eflags |= CF;
      _eax = 0x8021;
      break;
    }
    D_printf("DPMI: Setting Ext Excp %#x = %#x:%#x\n", _LO(bx),_LWORD(ecx),_edx);
    DPMI_CLIENT.Exception_Table_RM[_LO(bx)].selector = _LWORD(ecx);
    DPMI_CLIENT.Exception_Table_RM[_LO(bx)].offset = API_16_32(_edx);
    break;

  case 0x0300:	/* Simulate Real Mode Interrupt */
  case 0x0301:	/* Call Real Mode Procedure With Far Return Frame */
  case 0x0302:	/* Call Real Mode Procedure With Iret Frame */
    save_rm_regs();
    {
      struct RealModeCallStructure *rmreg = SEL_ADR_X(_es, _edi);
      us *ssp;
      unsigned int rm_ssp, rm_sp;
      int rmask = ~((1 << cs_INDEX) | (1 << eip_INDEX));
      int i;

      D_printf("DPMI: RealModeCallStructure at %p\n", rmreg);
      if (rmreg->ss == 0 && rmreg->sp == 0)
        rmask &= ~((1 << esp_INDEX) | (1 << ss_INDEX));
      DPMI_restore_rm_regs(rmreg, rmask);

      ssp = (us *) SEL_ADR(_ss, _esp);
      rm_ssp = SEGOFF2LINEAR(SREG(ss), 0);
      rm_sp = LWORD(esp);
      for (i = 0; i < _LWORD(ecx); i++)
	pushw(rm_ssp, rm_sp, *(ssp + _LWORD(ecx) - 1 - i));
      LWORD(esp) -= 2 * _LWORD(ecx);
      dpmi_set_pm(0);
      SREG(cs) = DPMI_SEG;
      LWORD(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_realmode) +
          current_client;
      switch (_LWORD(eax)) {
        case 0x0300:
          if (_LO(bx)==0x21)
            D_printf("DPMI: int 0x21 fn %04x\n",LWORD(eax));
#ifdef USE_MHPDBG
	  mhp_debug(DBG_INTx + (_LO(bx) << 8), 0, 0);
#endif
	  do_int(_LO(bx));
	  break;
        case 0x0301:
	  fake_call_to(rmreg->cs, rmreg->ip);
	  break;
        case 0x0302:
	  fake_int_to(rmreg->cs, rmreg->ip);
	  break;
      }
      /* 32rtm work-around */
      current_client = in_dpmi - 1;

/* --------------------------------------------------- 0x300:
     RM |  FC90C   |
	| dpmi_seg |
	|  flags   |
	| cx words |
   --------------------------------------------------- */
    }
#ifdef SHOWREGS
    if (debug_level('e')==0) {
      show_regs();
    }
#endif
    break;
  case 0x0303:	/* Allocate realmode call back address */
    {
      far_t rmc = DPMI_allocate_realmode_callback(_ds, API_16_32(_esi),
        _es, API_16_32(_edi));
      if (rmc.segment == 0 && rmc.offset == 0) {
        _eflags |= CF;
      } else {
        _LWORD(ecx) = rmc.segment;
        _LWORD(edx) = rmc.offset;
      }
    }
    break;
  case 0x0304: /* free realmode call back address */
    {
      int err = DPMI_free_realmode_callback(_LWORD(ecx), _LWORD(edx));
      if (err)
        _eflags |= CF;
    }
    break;
  case 0x0305: {	/* Get State Save/Restore Adresses */
      far_t raddr;
      struct pmaddr_s paddr;
      _LWORD(eax) = DPMI_get_save_restore_address(&raddr, &paddr);
      _LWORD(ebx) = raddr.segment;
      _LWORD(ecx) = raddr.offset;
      _LWORD(esi) = paddr.selector;
      _edi = paddr.offset;
    break;
  }
  case 0x0306:	/* Get Raw Mode Switch Adresses */
      _LWORD(ebx) = DPMI_SEG;
      _LWORD(ecx) = DPMI_OFF + HLT_OFF(DPMI_raw_mode_switch_rm);
      _LWORD(esi) = dpmi_sel();
      _edi = DPMI_SEL_OFF(DPMI_raw_mode_switch_pm);
    break;
  case 0x0400:	/* Get Version */
    _LWORD(eax) = DPMI_VERSION << 8 | DPMI_DRIVER_VERSION;
    _LO(bx) = 5;
    _LO(cx) = vm86s.cpu_type;
    _LWORD(edx) = 0x0870; /* PIC base imaster/slave interrupt */
    break;
  case 0x0401:			/* Get DPMI Capabilities 1.0 */
      {
	  char *buf = (char *)SEL_ADR(_es, _edi);
	  /*
	   * Our capabilities include:
	   * conventional memory mapping,
	   * demand zero fill,
	   * write-protect client,
	   * write-protect host.
	   */
	  _LWORD(eax) = 0x78;
	  _LWORD(ecx) = 0;
	  _LWORD(edx) = 0;
	  *buf = DPMI_VERSION;
	  *(buf+1) = DPMI_DRIVER_VERSION;
	  snprintf(buf+2, 126, "DOSEMU Version %d.%d", VERSION_NUM, SUBLEVEL);
      }
    break;

  case 0x0500:
    {
      unsigned int lp[0xc];
      GetFreeMemoryInformation(lp);
      memcpy_2dos(GetSegmentBase(_es) + API_16_32(_edi), lp, sizeof(lp));
    }
    break;
  case 0x0501:	/* Allocate Memory Block */
    {
      dpmi_pm_block block;
      unsigned int mem_required = (_LWORD(ebx))<<16 | (_LWORD(ecx));

      block = DPMImalloc(mem_required);
      if (!block.size) {
	D_printf("DPMI: client allocate memory failed.\n");
	_eflags |= CF;
	break;
      }
      D_printf("DPMI: malloc attempt for siz 0x%08x\n", (_LWORD(ebx))<<16 | (_LWORD(ecx)));
      D_printf("      malloc returns address %#x\n", block.base);
      D_printf("                using handle 0x%08x\n",block.handle);
      _LWORD(edi) = (block.handle)&0xffff;
      _LWORD(esi) = ((block.handle) >> 16) & 0xffff;
      _LWORD(ecx) = block.base & 0xffff;
      _LWORD(ebx) = (block.base >> 16) & 0xffff;
    }
    break;
  case 0x0502:	/* Free Memory Block */
    {
	D_printf(" DPMI: Free Mem Blk. for handle %08x\n",(_LWORD(esi))<<16 | (_LWORD(edi)));

        if(DPMIfree((_LWORD(esi))<<16 | (_LWORD(edi)))) {
	    D_printf("DPMI: client attempt to free a non exist handle\n");
	    _eflags |= CF;
	    break;
	}
    }
    break;
  case 0x0503:	/* Resize Memory Block */
    {
	unsigned newsize, handle;
	dpmi_pm_block block;
	handle = (_LWORD(esi))<<16 | (_LWORD(edi));
	newsize = (_LWORD(ebx)<<16 | _LWORD(ecx));
	D_printf("DPMI: Realloc to size %x\n", (_LWORD(ebx)<<16 | _LWORD(ecx)));
	D_printf("DPMI: For Mem Blk. for handle   0x%08x\n", handle);

	block = DPMIrealloc(handle, newsize);
	if (!block.size) {
	    D_printf("DPMI: client resize memory failed\n");
	    _eflags |= CF;
	    break;
	}
	D_printf("DPMI: realloc attempt for siz 0x%08x\n", newsize);
	D_printf("      realloc returns address %#x\n", block.base);
	_LWORD(ecx) = (unsigned long)(block.base) & 0xffff;
	_LWORD(ebx) = ((unsigned long)(block.base) >> 16) & 0xffff;
    }
    break;
  case 0x0504:			/* Allocate linear mem, 1.0 */
      {
	  unsigned int base_address = _ebx;
	  dpmi_pm_block block;
	  unsigned long length = _ecx;
	  D_printf("DPMI: allocate linear mem attempt for siz 0x%08x at 0x%08x (%s)\n",
		   _ecx, base_address, _edx ? "committed" : "uncommitted");
	  if (!length) {
	      _eflags |= CF;
	      _LWORD(eax) = 0x8021;
	      break;
	  }
	  if (base_address & 0xfff) {
	      _eflags |= CF;
	      _LWORD(eax) = 0x8025; /* not page aligned */
	      break;
	  }
	  block = DPMImallocLinear(base_address, length, _edx & 1);
	  if (!block.size) {
	      if (!base_address)
		  _LWORD(eax) = 0x8013;	/* mem not available */
	      else
		  _LWORD(eax) = 0x8012;	/* linear mem not avail. */
	      _eflags |= CF;
	      break;
	  }
	  _ebx = block.base;
	  _esi = block.handle;
	  D_printf("      malloc returns address %#x\n", block.base);
	  D_printf("                using handle 0x%08x\n",block.handle);
	  break;
      }
  case 0x0505:			/* Resize memory block, 1.0 */
    {
	unsigned long newsize, handle;
	dpmi_pm_block *old_block, block;
	unsigned short *sel_array;
	unsigned long old_base, old_len;

/* JES Unsure of ASSUMING sel_array OK to be nit to NULL */
	sel_array = NULL;
	handle = _esi;
	newsize = _ecx;

	if (!newsize) {
	    _eflags |= CF;
	    _LWORD(eax) = 0x8021; /* invalid value */
	    break;
	}

	D_printf("DPMI: Resize linear mem to size %lx, flags %x\n", newsize, _edx);
	D_printf("DPMI: For Mem Blk. for handle   0x%08lx\n", handle);
	old_block = lookup_pm_block(&DPMI_CLIENT.pm_block_root, handle);
	if(!old_block) {
	    _eflags |= CF;
	    _LWORD(eax) = 0x8023; /* invalid handle */
	    break;
	}
	old_base = (unsigned long)old_block->base;
	old_len = old_block->size;

	if(_edx & 0x2) {		/* update descriptor required */
	    sel_array = SEL_ADR_X(_es, _ebx);
	    D_printf("DPMI: update descriptor required\n");
	}
	block = DPMIreallocLinear(handle, newsize, _edx & 1);
	if (!block.size) {
	    _LWORD(eax) = 0x8012;
	    _eflags |= CF;
	    break;
	}
	_ebx = (unsigned long)block.base;
	_esi = block.handle;
	if(_edx & 0x2)	{	/* update descriptor required */
	    int i;
	    unsigned short sel;
	    unsigned long sel_base;
	    for(i=0; i< _edi; i++) {
		sel = sel_array[i];
		if (Segments[sel >> 3].used == 0)
		    continue;
		if (Segments[sel >> 3].type & MODIFY_LDT_CONTENTS_STACK) {
		    if(Segments[sel >> 3].is_big)
			sel_base = Segments[sel>>3].base_addr +
			    Segments[sel>>3].limit*DPMI_page_size-1;
		    else
			sel_base = Segments[sel>>3].base_addr + Segments[sel>>3].limit - 1;
		}else
		    sel_base = Segments[sel>>3].base_addr;
		if ((old_base <= sel_base) &&
		    ( sel_base < (old_base+old_len)))
		    SetSegmentBaseAddress(sel,
					  Segments[sel>>3].base_addr +
					  (unsigned long)block.base -
					  old_base);
	    }
	}
    }
    break;
  case 0x0509:			/* Map conventional memory,1.0 */
    {
	unsigned long low_addr, handle, offset;

	handle = _esi;
	low_addr = _edx;
	offset = _ebx;

	if (low_addr > 0xf0000) {
	    _eflags |= CF;
	    _LWORD(eax) = 0x8003; /* system integrity */
	    break;
	}

	if ((low_addr & 0xfff) || (offset & 0xfff)) {
	    _eflags |= CF;
	    _LWORD(eax) = 0x8025; /* invalid linear address */
	    break;
	}

	D_printf("DPMI: Map conventional mem for handle %ld, offset %lx at low address %lx\n", handle, offset, low_addr);
	switch (DPMIMapConventionalMemory(handle, offset, low_addr, _ecx)) {
	  case -2:
	    _eflags |= CF;
	    _LWORD(eax) = 0x8023; /* invalid handle */
	    break;
	  case -1:
	    _LWORD(eax) = 0x8001;
	    _eflags |= CF;
	    break;
	}
    }
    break;
  case 0x050a:	/* Get mem block and base. 10 */
    {
	unsigned long handle;
	dpmi_pm_block *block;
	handle = (_LWORD(esi))<<16 | (_LWORD(edi));

	if((block = lookup_pm_block(&DPMI_CLIENT.pm_block_root, handle)) == NULL) {
	    _LWORD(eax) = 0x8023;
	    _eflags |= CF;
	    break;
	}

	_LWORD(edi) = LO_WORD(block->size);
	_LWORD(esi) = HI_WORD(block->size);
	_LWORD(ecx) = LO_WORD(block->base);
	_LWORD(ebx) = HI_WORD(block->base);
    }
    break;
  case 0x0600:	/* Lock Linear Region */
  case 0x0601:	/* Unlock Linear Region */
  case 0x0602:	/* Mark Real Mode Region as Pageable */
  case 0x0603:	/* Relock Real Mode Region */
    break;
  case 0x0604:	/* Get Page Size */
    _LWORD(ebx) = 0;
    _LWORD(ecx) = DPMI_page_size;
    break;
  case 0x0701:	/* Reserved, DISCARD PAGES, see interrupt lst */
    D_printf("DPMI: undoc. func. 0x0701 called\n");
    D_printf("      BX=0x%04x, CX=0x%04x, SI=0x%04x, DI=0x%04x\n",
			_LWORD(ebx), _LWORD(ecx), _LWORD(esi), _LWORD(edi));
    break;
  case 0x0900:	/* Get and Disable Virtual Interrupt State */
    _LO(ax) = isset_IF() ? 1 : 0;
    D_printf("DPMI: Get-and-clear VIF, %i\n", _LO(ax));
    clear_IF();
    break;
  case 0x0901:	/* Get and Enable Virtual Interrupt State */
    _LO(ax) = isset_IF() ? 1 : 0;
    D_printf("DPMI: Get-and-set VIF, %i\n", _LO(ax));
    set_IF();
    break;
  case 0x0902:	/* Get Virtual Interrupt State */
    _LO(ax) = isset_IF() ? 1 : 0;
    D_printf("DPMI: Get VIF, %i\n", _LO(ax));
    break;
  case 0x0a00:	/* Get Vendor Specific API Entry Point */
    get_ext_API(scp);
    break;
  case 0x0b00:	/* Set Debug Breakpoint ->bx=handle(!CF) */
  case 0x0b01:	/* Clear Debug Breakpoint, bx=handle */
  case 0x0b02:	/* Get Debug Breakpoint State, bx=handle->ax=state(!CF) */
  case 0x0b03:	/* Reset Debug Breakpoint, bx=handle */
    {
      int err = dpmi_debug_breakpoint(_LWORD(eax)-0x0b00, scp);
      if (err) {
	_LWORD(eax) = 0x8000 | err;
	_eflags |= CF;
      }
    }
    break;

  case 0x0c00:	/* Install Resident Service Provider Callback */
    {
      struct RSPcall_s *callback = SEL_ADR_X(_es, _edi);
      if (RSP_num >= DPMI_MAX_CLIENTS) {
        _eflags |= CF;
	_LWORD(eax) = 0x8015;
      } else {
        RSP_callbacks[RSP_num].call = *callback;
        DPMI_CLIENT.RSP_installed = 1;
        D_printf("installed Resident Service Provider Callback %i\n", RSP_num);
      }
    }
    break;
  case 0x0c01:	/* Terminate and Stay Resident */
    quit_dpmi(scp, _LO(bx), 1, _LWORD(edx), 1);
    break;

  case 0x0d00: {	/* Allocate Shared Memory */
    int err = DPMIAllocateShared(SEL_ADR_X(_es, _edi));
    if (err) {
      _eflags |= CF;
      _LWORD(eax) = 0x8014;
    }
    break;
  }

  case 0x0d01: {	/* Free Shared Memory */
    int err = DPMIFreeShared((_LWORD(esi) << 16) | _LWORD(edi));
    if (err) {
      _eflags |= CF;
      _LWORD(eax) = 0x8023;
    }
    break;
  }

  case 0x0e00:	/* Get Coprocessor Status */
    _LWORD(eax) = 0x4d;
    break;
  case 0x0e01:	/* Set Coprocessor Emulation */
    break;

  case 0x0506:			/* Get Page Attributes */
    D_printf("DPMI: Get Page Attributes for %i pages\n", _ecx);
    if (!DPMIGetPageAttributes(_esi, _ebx, (us *)SEL_ADR(_es, _edx), _ecx))
      _eflags |= CF;
    break;

  case 0x0507:			/* Set Page Attributes */
    D_printf("DPMI: Set Page Attributes for %i pages\n", _ecx);
    if (!DPMISetPageAttributes(_esi, _ebx, (us *)SEL_ADR(_es, _edx), _ecx))
      _eflags |= CF;
    break;

  case 0x0508:	/* Map Device */
    D_printf("DPMI: ERROR: device mapping not supported\n");
    _LWORD(eax) = 0x8001;
    _eflags |= CF;
    break;

  case 0x0700:	/* Reserved,MARK PAGES AS PAGING CANDIDATES, see intr. lst */
  case 0x0702:	/* Mark Page as Demand Paging Candidate */
  case 0x0703:	/* Discard Page Contents */
    D_printf("DPMI: unimplemented int31 func %#x\n",_LWORD(eax));
    break;

  case 0x0800: {	/* create Physical Address Mapping */
      unsigned addr, size;
      dpmi_pm_block *blk;

      addr = ((uint32_t)_LWORD(ebx)) << 16 | (_LWORD(ecx));
      size = ((uint32_t)_LWORD(esi)) << 16 | (_LWORD(edi));

      D_printf("DPMI: Map Physical Memory, addr=%#08x size=%#x\n", addr, size);

      blk = DPMI_mapHWRam(&DPMI_CLIENT.pm_block_root, addr, size);
      if (!blk) {
	_eflags |= CF;
	break;
      }
      _LWORD(ebx) = blk->base >> 16;
      _LWORD(ecx) = blk->base;
      D_printf("DPMI: getting physical memory area at 0x%x, size 0x%x, "
		     "ret=%#x:%#x\n",
	       addr, size, _LWORD(ebx), _LWORD(ecx));
    }
    break;
  case 0x0801: {	/* free Physical Address Mapping */
      size_t vbase;
      int rc;
      vbase = (_LWORD(ebx)) << 16 | (_LWORD(ecx));
      D_printf("DPMI: Unmap Physical Memory, vbase=%#08zx\n", vbase);
      rc = DPMI_unmapHWRam(&DPMI_CLIENT.pm_block_root, vbase);
      if (rc == -1) {
	_eflags |= CF;
	break;
      }
    }
    break;

  default:
    D_printf("DPMI: unimplemented int31 func %#x\n",_LWORD(eax));
    _eflags |= CF;
  } /* switch */
  if (_eflags & CF)
    D_printf("DPMI: dpmi function failed, CF=1\n");
}

static void make_iret_frame(sigcontext_t *scp, void *sp,
	uint32_t cs, uint32_t eip)
{
  if (DPMI_CLIENT.is_32) {
    unsigned int *ssp = sp;
    *--ssp = get_vFLAGS(_eflags);
    *--ssp = cs;
    *--ssp = eip;
    _esp -= 12;
  } else {
    unsigned short *ssp = sp;
    *--ssp = get_vFLAGS(_eflags);
    *--ssp = cs;
    *--ssp = eip;
    _LWORD(esp) -= 6;
  }
}

static void make_retf_frame(sigcontext_t *scp, void *sp,
	uint32_t cs, uint32_t eip)
{
  if (DPMI_CLIENT.is_32) {
    unsigned int *ssp = sp;
    *--ssp = cs;
    *--ssp = eip;
    _esp -= 8;
  } else {
    unsigned short *ssp = sp;
    *--ssp = cs;
    *--ssp = eip;
    _LWORD(esp) -= 4;
  }
}

static void dpmi_realmode_callback(int rmcb_client, int num)
{
    void *sp;
    sigcontext_t *scp = &DPMI_CLIENT.stack_frame;

    if (rmcb_client > current_client || num >= DPMI_MAX_RMCBS)
      return;

    D_printf("DPMI: Real Mode Callback for #%i address of client %i (from %i)\n",
      num, rmcb_client, current_client);
    DPMI_save_rm_regs(DPMIclient[rmcb_client].realModeCallBack[num].rmreg);
    save_pm_regs(&DPMI_CLIENT.stack_frame);
    sp = enter_lpms(&DPMI_CLIENT.stack_frame);

    /* the realmode callback procedure will return by an iret */
    /* WARNING - realmode flags can contain the dreadful NT flag which
     * will produce an exception 10 as soon as we return from the
     * callback! */
    _eflags =  REG(eflags)&(~(AC|VM|TF|NT));
    make_iret_frame(scp, sp, dpmi_sel(),
	    DPMI_SEL_OFF(DPMI_return_from_rm_callback));
    _cs = DPMIclient[rmcb_client].realModeCallBack[num].selector;
    _eip = DPMIclient[rmcb_client].realModeCallBack[num].offset;
    SetSelector(DPMIclient[rmcb_client].realModeCallBack[num].rm_ss_selector,
		(SREG(ss)<<4), 0xffff, DPMI_CLIENT.is_32,
		MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0);
    _ds = DPMIclient[rmcb_client].realModeCallBack[num].rm_ss_selector;
    _esi = REG(esp);
    _es = DPMIclient[rmcb_client].realModeCallBack[num].rmreg_selector;
    _edi = DPMIclient[rmcb_client].realModeCallBack[num].rmreg_offset;

    clear_IF();
    dpmi_set_pm(1);
}

static void rmcb_hlt(Bit16u off, void *arg)
{
    dpmi_realmode_callback((long)arg, off);
}

static void dpmi_RSP_call(sigcontext_t *scp, int num, int terminating)
{
  unsigned char *code, *data;
  void *sp;
  unsigned long eip;
  if (DPMI_CLIENT.is_32) {
    if ((RSP_callbacks[num].call.code32[5] & 0x88) != 0x88)
      return;
    code = RSP_callbacks[num].call.code32;
    data = RSP_callbacks[num].call.data32;
    eip = RSP_callbacks[num].call.eip;
  } else {
    if ((RSP_callbacks[num].call.code16[5] & 0x88) != 0x88)
      return;
    code = RSP_callbacks[num].call.code16;
    data = RSP_callbacks[num].call.data16;
    eip = RSP_callbacks[num].call.ip;
  }

  if (!terminating) {
    DPMI_CLIENT.RSP_cs[num] = AllocateDescriptors(1);
    SetDescriptor(DPMI_CLIENT.RSP_cs[num], (unsigned int *)code);
    if ((data[5] & 0x88) == 0x80) {
      DPMI_CLIENT.RSP_ds[num] = AllocateDescriptors(1);
      SetDescriptor(DPMI_CLIENT.RSP_ds[num], (unsigned int *)data);
    } else {
      DPMI_CLIENT.RSP_ds[num] = 0;
    }
  } else if (!DPMI_CLIENT.RSP_cs[num])
    return;

  save_pm_regs(scp);
  sp = enter_lpms(scp);
  if (DPMI_CLIENT.is_32) {
    unsigned int *ssp = sp;
    *--ssp = in_dpmi_pm();
    *--ssp = dpmi_sel();
    *--ssp = DPMI_SEL_OFF(DPMI_return_from_RSPcall);
    _esp -= 12;
  } else {
    unsigned short *ssp = sp;
    *--ssp = in_dpmi_pm();
    *--ssp = dpmi_sel();
    *--ssp = DPMI_SEL_OFF(DPMI_return_from_RSPcall);
    _LWORD(esp) -= 6;
  }

  _es = _fs = _gs = 0;
  _ds = DPMI_CLIENT.RSP_ds[num];
  _cs = DPMI_CLIENT.RSP_cs[num];
  _eip = eip;
  _eax = terminating;
  _ebx = in_dpmi;

  dpmi_set_pm(1);
}

static void dpmi_cleanup(void)
{
  int i;

  D_printf("DPMI: cleanup\n");
  if (in_dpmi_pm())
    dosemu_error("Quitting DPMI while in_dpmi_pm\n");
  if (current_client != in_dpmi - 1) {
    error("DPMI: termination of non-last client\n");
    /* leave the leak.
     * TODO: fixing that leak is a lot of work, in particular
     * msdos_done() would be out of sync and FreeAllDescriptors()
     * will need to use "current_client" instead of "in_dpmi"
     * to wipe only the needed descs.
     * That happens only if the one started 32rtm by some tool like
     * DosNavigator, that would invoke an additional DPMI shell (comcom32).
     * Using bc.bat doesn't go here because 32rtm is unloaded after
     * bc.exe exit, so the clients terminate in the right order.
     * Starting 32rtm by hands may go here if you terminate the
     * parent DPMI shell (comcom32), but nobody does that.
     * Lets say this is a very pathological case for a big code surgery. */
    current_client = in_dpmi - 1;
    return;
  }
  msdos_done();
  FreeAllDescriptors();
  DPMI_free(&host_pm_block_root, DPMI_CLIENT.pm_stack->handle);
  hlt_unregister_handler(DPMI_CLIENT.rmcb_off);
  if (!DPMI_CLIENT.RSP_installed) {
    sigcontext_t *scp = &DPMI_CLIENT.stack_frame;
    DPMIfreeAll();
    free(__fpstate);
  }

  if (win3x_mode != INACTIVE) {
    win3x_mode = (in_dpmi == 1 ? INACTIVE : PREV_DPMI_CLIENT.win3x_mode);
    if (win3x_mode == INACTIVE)
      SETIVEC(0x66, 0, 0);	// winos2
  }
  cli_blacklisted = 0;
  dpmi_is_cli = 0;
  in_dpmi--;
  current_client = in_dpmi - 1;

  if (in_dpmi && config.cpu_vm_dpmi == CPUVM_KVM) {
    /* need to update guest IDT */
    for (i=0;i<0x100;i++)
      kvm_set_idt(i, DPMI_CLIENT.Interrupt_Table[i].selector,
          DPMI_CLIENT.Interrupt_Table[i].offset, DPMI_CLIENT.is_32);
  }
}

static void dpmi_soft_cleanup(void)
{
  dpmi_cleanup();
  if (in_dpmi == 0) {
    SETIVEC(0x1c, s_i1c.segment, s_i1c.offset);
    SETIVEC(0x23, s_i23.segment, s_i23.offset);
    SETIVEC(0x24, s_i24.segment, s_i24.offset);
  }
}

static void quit_dpmi(sigcontext_t *scp, unsigned short errcode,
    int tsr, unsigned short tsr_para, int dos_exit)
{
  int i;
  int have_tsr = tsr && DPMI_CLIENT.RSP_installed;

  /* this is checked in dpmi_cleanup */
  DPMI_CLIENT.RSP_installed = have_tsr;

  /* do this all before doing RSP call */
  dpmi_set_pm(0);
  if (DPMI_CLIENT.in_dpmi_pm_stack) {
    error("DPMI: Warning: trying to leave DPMI when in_dpmi_pm_stack=%i\n",
        DPMI_CLIENT.in_dpmi_pm_stack);
    port_outb(0x21, DPMI_CLIENT.imr);
    DPMI_CLIENT.in_dpmi_pm_stack = 0;
  }

  if (DPMI_CLIENT.RSP_state == 0) {
    DPMI_CLIENT.RSP_state = 1;
    for (i = 0;i < RSP_num; i++) {
      D_printf("DPMI: Calling RSP %i for termination\n", i);
      dpmi_RSP_call(scp, i, 1);
    }
  }

  if (have_tsr) {
    RSP_callbacks[RSP_num].pm_block_root = DPMI_CLIENT.pm_block_root;
    RSP_num++;
  }
  if (!in_dpmi_pm())
    dpmi_soft_cleanup();

  if (dos_exit) {
    if (!have_tsr || !tsr_para) {
      HI(ax) = 0x4c;
      LO(ax) = errcode;
      do_int(0x21);
    } else {
      HI(ax) = 0x31;
      LO(ax) = errcode;
      LWORD(edx) = tsr_para;
      do_int(0x21);
    }
  }
}

static void chain_rm_int(sigcontext_t *scp, int i)
{
  D_printf("DPMI: Calling real mode handler for int 0x%02x\n", i);
  save_rm_regs();
  pm_to_rm_regs(scp, ~0);
  SREG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_rmint);
  do_int(i);
}

static void chain_hooked_int(sigcontext_t *scp, int i)
{
  far_t iaddr;
  D_printf("DPMI: Calling real mode handler for int 0x%02x\n", i);
  save_rm_regs();
  pm_to_rm_regs(scp, ~0);
  SREG(cs) = DPMI_SEG;
  REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos);
  switch (i) {
  case 0x1c:
    iaddr = s_i1c;
    break;
  case 0x23:
    iaddr = s_i23;
    break;
  case 0x24:
    iaddr = s_i24;
    break;
  }

  D_printf("DPMI: Calling real mode handler for int 0x%02x, %04x:%04x\n",
	i, iaddr.segment, iaddr.offset);
  fake_int_to(iaddr.segment, iaddr.offset);
}

static void do_dpmi_int(sigcontext_t *scp, int i)
{
  switch (i) {
    case 0x2f:
      switch (_LWORD(eax)) {
#if 0
	/* this is disabled. coopth currently can't handle delays
	 * in protected mode... :( */
	case 0x1680:	/* give up time slice */
	  idle(0, 100, 0, "int2f_idle_dpmi");
	  if (config.hogthreshold)
	    _LWORD(eax) = 0;
	  return;
#endif
	case 0x1684:
	  D_printf("DPMI: Get VxD entry point, BX = 0x%04x\n", _LWORD(ebx));
	  get_VXD_entry(scp);
	  return;
	case 0x1686:
          D_printf("DPMI: CPU mode check in protected mode.\n");
          _eax = 0;
          return;
        case 0x168a:
          get_ext_API(scp);
          if (!(_eflags & CF))
            return;
          break;
      }
      break;
    case 0x31:
      switch (_LWORD(eax)) {
        case 0x0700:
        case 0x0701: {
          static int once=1;
          if (once) once=0;
          else {
            _eflags &= ~CF;
            return;
          }
        }
      }
      return do_int31(scp);
    case 0x21:
      switch (_HI(ax)) {
        case 0x4c:
          D_printf("DPMI: leaving DPMI with error code 0x%02x, in_dpmi=%i\n",
            _LO(ax), in_dpmi);
	  quit_dpmi(scp, _LO(ax), 0, 0, 1);
	  return;
      }
      break;
  }

  if (i == 0x1c || i == 0x23 || i == 0x24) {
    dpmi_set_pm(0);
    chain_hooked_int(scp, i);
#ifdef USE_MHPDBG
    mhp_debug(DBG_INTx + (i << 8), 0, 1);
#endif
  } else if (config.pm_dos_api) {
    int msdos_ret;
    struct RealModeCallStructure rmreg;
    int rm_mask = (1 << cs_INDEX) | (1 << eip_INDEX);
    u_char stk[256];
    int stk_used;

    rmreg.cs = DPMI_SEG;
    rmreg.ip = DPMI_OFF + HLT_OFF(DPMI_return_from_dosext);
    msdos_ret = msdos_pre_extender(scp, i, &rmreg, &rm_mask, stk, sizeof(stk),
	    &stk_used);
    switch (msdos_ret) {
    case MSDOS_NONE:
      dpmi_set_pm(0);
      chain_rm_int(scp, i);
#ifdef USE_MHPDBG
      mhp_debug(DBG_INTx + (i << 8), 0, 1);
#endif
      break;
    case MSDOS_RM: {
      uint32_t *ssp;

      make_retf_frame(scp, SEL_ADR(_ss, _esp), _cs, _eip);
      ssp = SEL_ADR(_ss, _esp);
      *--ssp = i;
      _esp -= 4;
      _cs = dpmi_sel();
      _eip = DPMI_SEL_OFF(DPMI_return_from_dosint);
      dpmi_set_pm(0);
      save_rm_regs();
      pm_to_rm_regs(scp, ~rm_mask);
      DPMI_restore_rm_regs(&rmreg, rm_mask);
      LWORD(esp) -= stk_used;
      MEMCPY_2DOS(SEGOFF2LINEAR(SREG(ss), LWORD(esp)),
	    stk + sizeof(stk) - stk_used, stk_used);
#ifdef USE_MHPDBG
      mhp_debug(DBG_INTx + (i << 8), 0, 1);
#endif
      break;
    }
    case MSDOS_DONE:
      return;
    case MSDOS_ERROR:
      D_printf("MSDOS error, leaving DPMI\n");
      quit_dpmi(scp, 0xff, 0, 0, 1);
      return;
    }
  } else {
    dpmi_set_pm(0);
    chain_rm_int(scp, i);
#ifdef USE_MHPDBG
    mhp_debug(DBG_INTx + (i << 8), 0, 1);
#endif
  }

  D_printf("DPMI: calling real mode interrupt 0x%02x, ax=0x%04x\n",i,LWORD(eax));
}

/* DANG_BEGIN_FUNCTION run_pm_int
 *
 * This routine is used for running protected mode hardware
 * interrupts.
 * run_pm_int() switches to the locked protected mode stack
 * and calls the handler. If no handler is installed the
 * real mode interrupt routine is called.
 *
 * DANG_END_FUNCTION
 */

void run_pm_int(int i)
{
  void *sp;
  unsigned short old_ss;
  unsigned int old_esp;
  unsigned char imr;
  sigcontext_t *scp = &DPMI_CLIENT.stack_frame;

  D_printf("DPMI: run_pm_int(0x%02x) called, in_dpmi_pm=0x%02x\n",i,in_dpmi_pm());

  if (DPMI_CLIENT.Interrupt_Table[i].selector == dpmi_sel()) {

    D_printf("DPMI: Calling real mode handler for int 0x%02x\n", i);

    if (in_dpmi_pm())
      fake_pm_int();
    real_run_int(i);
    return;
  }

  old_ss = _ss;
  old_esp = _esp;
  sp = enter_lpms(&DPMI_CLIENT.stack_frame);
  imr = port_inb(0x21);

  D_printf("DPMI: Calling protected mode handler for int 0x%02x\n", i);
  if (DPMI_CLIENT.is_32) {
    unsigned int *ssp = sp;
    *--ssp = imr;
    *--ssp = 0;	/* reserved */
    *--ssp = in_dpmi_pm();
    *--ssp = old_ss;
    *--ssp = old_esp;
    *--ssp = _cs;
    *--ssp = _eip;
    *--ssp = get_vFLAGS(_eflags);
    *--ssp = dpmi_sel();
    *--ssp = DPMI_SEL_OFF(DPMI_return_from_pm);
    _esp -= 40;
  } else {
    unsigned short *ssp = sp;
    *--ssp = imr;
    /* store the high word of ESP, because CPU corrupts it */
    *--ssp = HI_WORD(old_esp);
    *--ssp = in_dpmi_pm();
    *--ssp = old_ss;
    *--ssp = LO_WORD(old_esp);
    *--ssp = _cs;
    *--ssp = (unsigned short) _eip;
    *--ssp = (unsigned short) get_vFLAGS(_eflags);
    *--ssp = dpmi_sel();
    *--ssp = DPMI_SEL_OFF(DPMI_return_from_pm);
    LO_WORD(_esp) -= 20;
  }
  _cs = DPMI_CLIENT.Interrupt_Table[i].selector;
  _eip = DPMI_CLIENT.Interrupt_Table[i].offset;
  _eflags &= ~(TF | NT | AC);
  dpmi_set_pm(1);
  clear_IF();
  in_dpmi_irq++;

  /* this is a protection for careless clients that do sti
   * in the inthandlers. There are plenty of those, unfortunately:
   * try playing Transport Tycoon without this hack.
   * The previous work-around was in a great bunch of hacks in PIC,
   * requiring dpmi to call pic_iret_dpmi() for re-enabling interrupts.
   * The alternative is to ignore STI while in a sighandler. There
   * are two problems with that:
   * - STI can be done also by the chained real-mode handler
   * - We need to allow processing the different IRQ levels for performance
   * So simply mask the currently processing IRQ on PIC. */
  if (i == 8 || i == 0x70) {
    /* PIT or RTC interrupt */
    unsigned char isr;
    port_outb(0x20, 0xb);
    isr = port_inb(0x20);
    port_outb(0x21, imr | isr);
  }
#ifdef USE_MHPDBG
  mhp_debug(DBG_INTx + (i << 8), 0, 0);
#endif
}

/* DANG_BEGIN_FUNCTION run_pm_dos_int
 *
 * This routine is used for reflecting the software
 * interrupts 0x1c, 0x23 and 0x24 to protected mode.
 *
 * DANG_END_FUNCTION
 */
static void run_pm_dos_int(int i)
{
  void  *sp;
  unsigned long ret_eip;
  sigcontext_t *scp = &DPMI_CLIENT.stack_frame;

  D_printf("DPMI: run_pm_dos_int(0x%02x) called\n",i);

  if (DPMI_CLIENT.Interrupt_Table[i].selector == dpmi_sel()) {
    far_t iaddr;
    D_printf("DPMI: Calling real mode handler for int 0x%02x\n", i);
    switch (i) {
    case 0x1c:
	iaddr = s_i1c;
	break;
    case 0x23:
	iaddr = s_i23;
	break;
    case 0x24:
	iaddr = s_i24;
	break;
    default:
	error("run_pm_dos_int with int=%x\n", i);
	return;
    }
    if (iaddr.segment == DPMI_SEG)
	return;
    jmp_to(iaddr.segment, iaddr.offset);
    return;
  }

  save_pm_regs(&DPMI_CLIENT.stack_frame);
  rm_to_pm_regs(&DPMI_CLIENT.stack_frame, ~0);
  sp = enter_lpms(&DPMI_CLIENT.stack_frame);

  switch(i) {
    case 0x1c:
      ret_eip = DPMI_SEL_OFF(DPMI_return_from_int_1c);
      break;
    case 0x23:
      ret_eip = DPMI_SEL_OFF(DPMI_return_from_int_23);
      break;
    case 0x24:
      ret_eip = DPMI_SEL_OFF(DPMI_return_from_int_24);
      _ebp = ConvertSegmentToDescriptor(LWORD(ebp));
      /* maybe copy the RM stack? */
      break;
    default:
      error("run_pm_dos_int called with arg 0x%x\n", i);
      leavedos(87);
      return;
  }

  D_printf("DPMI: Calling protected mode handler for DOS int 0x%02x\n", i);
  make_iret_frame(scp, sp, dpmi_sel(), ret_eip);
  _cs = DPMI_CLIENT.Interrupt_Table[i].selector;
  _eip = DPMI_CLIENT.Interrupt_Table[i].offset;
  _eflags &= ~(TF | NT | AC);
  dpmi_set_pm(1);
  clear_IF();
#ifdef USE_MHPDBG
  mhp_debug(DBG_INTx + (i << 8), 0, 0);
#endif
}

void run_dpmi(void)
{
#ifdef USE_MHPDBG
    int retcode;
#endif
    if (return_requested) {
      return_requested = 0;
      return;
    }
#ifdef USE_MHPDBG
    if (mhpdbg_is_stopped())
      return;
    retcode =
#endif
    dpmi_control();
#ifdef USE_MHPDBG
    if (retcode >= DPMI_RET_INT && mhpdbg.active)
      mhp_debug(DBG_INTxDPMI + (retcode << 8), 0, 0);
#endif
}

static void dpmi_thr(void *arg)
{
    in_dpmi_thr++;
    indirect_dpmi_transfer();
    in_dpmi_thr--;
}

void dpmi_setup(void)
{
    int i, type, err;
    unsigned int base_addr, limit, *lp;
    dpmi_pm_block *block;

    if (!config.dpmi) return;

#ifdef __x86_64__
    {
      unsigned int i, j;
      void *addr;
      /* search for page with bits 16-31 clear within first 47 bits
	 of address space */
      for (i = 1; i < 0x8000; i++) {
	for (j = 0; j < 0x10000; j += PAGE_SIZE) {
	  addr = (void *)(i*0x100000000UL + j);
	  iret_frame = mmap_mapping_ux(MAPPING_SCRATCH | MAPPING_NOOVERLAP,
				    addr, PAGE_SIZE,
				    PROT_READ | PROT_WRITE);
	  if (iret_frame != MAP_FAILED)
	    goto out;
	}
      }
    out:
      if (iret_frame != addr) {
	error("Can't find DPMI iret page, leaving\n");
	leavedos(0x24);
      }
    }

    /* reserve last page to prevent DPMI from allocating it.
     * Our code is full of potential 32bit integer overflows. */
    mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH | MAPPING_NOOVERLAP,
	    (uint32_t)PAGE_MASK, PAGE_SIZE, PROT_NONE);
#endif

    get_ldt(ldt_buffer);
    memset(Segments, 0, sizeof(Segments));
    for (i = 0; i < MAX_SELECTORS; i++) {
      lp = (unsigned int *)&ldt_buffer[i * LDT_ENTRY_SIZE];
      base_addr = (*lp >> 16) & 0x0000FFFF;
      limit = *lp & 0x0000FFFF;
      lp++;
      base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
      limit |= (*lp & 0x000F0000);
      type = (*lp >> 10) & 3;
      if (base_addr || limit || type) {
        D_printf("LDT entry 0x%x used: b=0x%x l=0x%x t=%i\n",i,base_addr,limit,type);
        Segments[i].used = 0xfe;
      }
    }

    if (dpmi_alloc_pool()) {
	leavedos(2);
	return;
    }
    if (!(dpmi_sel16 = allocate_descriptors(1))) goto err;
    if (!(dpmi_sel32 = allocate_descriptors(1))) goto err;

    block = DPMI_malloc(&host_pm_block_root,
			PAGE_ALIGN(DPMI_sel_code_end-DPMI_sel_code_start));
    if (block == NULL) {
      error("DPMI: can't allocate memory for DPMI host helper code\n");
      goto err2;
    }
    MEMCPY_2DOS(block->base, DPMI_sel_code_start,
		DPMI_sel_code_end-DPMI_sel_code_start);
    err = SetSelector(dpmi_sel16, block->base,
		    DPMI_SEL_OFF(DPMI_sel_code_end)-1, 0,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0, 0, 0);
    if (err) {
      dpmi_not_supported = 1;
      switch (config.cpu_vm_dpmi) {
      case CPUVM_KVM:
        error("DPMI: KVM unavailable\n");
        break;
      case CPUVM_NATIVE:
#ifdef __linux__
        if ((kernel_version_code & 0xffff00) >= KERNEL_VERSION(3, 14, 0)) {
          if ((kernel_version_code & 0xffff00) < KERNEL_VERSION(3, 16, 0)) {
            error("DPMI is not supported on your kernel (3.14, 3.15)\n");
            error("@Try \'$_cpu_vm_dpmi = \"kvm\"\'\n");
          } else {
            error("DPMI support is not enabled on your kernel.\n"
              "Make sure the following kernel options are set:\n"
              "\tCONFIG_MODIFY_LDT_SYSCALL=y\n"
              "\tCONFIG_X86_16BIT=y\n"
              "\tCONFIG_X86_ESPFIX64=y\n");
          }
        } else {
          error("DPMI is not supported on that kernel\n");
          error("@Try enabling CPU emulator with $_cpu_emu=\"full\" in dosemu.conf\n");
        }
#else
        error("DPMI is not supported on that kernel\n");
#endif
        break;
      case CPUVM_EMU:
        error("DPMI: cpu-emu error\n");
        break;
      }
      goto err2;
    }
    if (config.cpu_vm_dpmi == CPUVM_KVM)
      warn("Using DPMI inside KVM\n");
    if (SetSelector(dpmi_sel32, block->base,
		    DPMI_SEL_OFF(DPMI_sel_code_end)-1, 1,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0, 0, 0)) goto err;

    if (config.pm_dos_api)
      msdos_setup(EMM_SEGMENT);

    co_handle = co_thread_init(PCL_C_MC);
    return;

err:
    error("DPMI initialization failed, disabling\n");
err2:
    config.dpmi = 0;
}

void dpmi_reset(void)
{
    current_client = in_dpmi - 1;
    while (in_dpmi) {
	if (in_dpmi_pm())
	    dpmi_set_pm(0);
	if (in_dpmi_thr)
	    do_dpmi_exit(&DPMI_CLIENT.stack_frame);
	dpmi_cleanup();
    }
    if (config.pm_dos_api)
	msdos_reset();
}

void dpmi_init(void)
{
  /* Holding spots for REGS and Return Code */
  unsigned short CS, DS, ES, SS, psp, my_cs;
  unsigned int ssp, sp;
  unsigned int my_ip, i;
  unsigned char *cp;
  int inherit_idt;
  sigcontext_t *scp;
  emu_hlt_t hlt_hdlr = HLT_INITIALIZER;

  CARRY;

  if (!config.dpmi)
    return;

  if (in_dpmi>=DPMI_MAX_CLIENTS) {
    p_dos_str("Sorry, only %d DPMI clients supported under DOSEMU :-(\n", DPMI_MAX_CLIENTS);
    return;
  }

  current_client = in_dpmi++;
  D_printf("DPMI: initializing %i\n", in_dpmi);
  memset(&DPMI_CLIENT, 0, sizeof(DPMI_CLIENT));
  dpmi_is_cli = 0;

  DPMI_CLIENT.is_32 = LWORD(eax) ? 1 : 0;

  if (in_dpmi == 1 && !RSP_num) {
    DPMI_rm_procedure_running = 0;
    pm_block_handle_used = 1;
  }

  DPMI_CLIENT.private_data_segment = SREG(es);

  DPMI_CLIENT.pm_stack = DPMI_malloc(&host_pm_block_root,
				     PAGE_ALIGN(DPMI_pm_stack_size));
  if (DPMI_CLIENT.pm_stack == NULL) {
    error("DPMI: can't allocate memory for locked protected mode stack\n");
    goto err;
  }

  if (!(DPMI_CLIENT.PMSTACK_SEL = AllocateDescriptors(1))) goto err;
  if (SetSelector(DPMI_CLIENT.PMSTACK_SEL, DPMI_CLIENT.pm_stack->base,
        DPMI_pm_stack_size-1, DPMI_CLIENT.is_32,
        MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0)) goto err;

  if (in_dpmi > 1)
    inherit_idt = DPMI_CLIENT.is_32 == PREV_DPMI_CLIENT.is_32
	/* inheriting from PharLap causes 0x4c to be passed to DOS directly! */
	&& !(PREV_DPMI_CLIENT.feature_flags & DF_PHARLAP)
#if WINDOWS_HACKS
/* work around the disability of win31 in Standard mode to run the DPMI apps */
	&& (win3x_mode != STANDARD)
#endif
    ;
  else
    inherit_idt = 0;

  for (i=0;i<0x100;i++) {
    DPMI_INTDESC desc;
    if (inherit_idt) {
      desc.offset32 = PREV_DPMI_CLIENT.Interrupt_Table[i].offset;
      desc.selector = PREV_DPMI_CLIENT.Interrupt_Table[i].selector;
    } else {
      desc.offset32 = DPMI_SEL_OFF(DPMI_interrupt) + i;
      desc.selector = dpmi_sel();
    }
    dpmi_set_interrupt_vector(i, desc);
  }
  DPMI_CLIENT.imr = port_inb(0x21);

  for (i=0;i<0x20;i++) {
    if (inherit_idt) {
      DPMI_CLIENT.Exception_Table[i].offset = PREV_DPMI_CLIENT.Exception_Table[i].offset;
      DPMI_CLIENT.Exception_Table[i].selector = PREV_DPMI_CLIENT.Exception_Table[i].selector;
    } else {
      DPMI_CLIENT.Exception_Table[i].offset = DPMI_SEL_OFF(DPMI_exception) + i;
      DPMI_CLIENT.Exception_Table[i].selector = dpmi_sel();
    }
    DPMI_CLIENT.Exception_Table_PM[i].offset = DPMI_SEL_OFF(DPMI_ext_exception) + i;
    DPMI_CLIENT.Exception_Table_PM[i].selector = dpmi_sel();
    DPMI_CLIENT.Exception_Table_RM[i].offset = DPMI_SEL_OFF(DPMI_rm_exception) + i;
    DPMI_CLIENT.Exception_Table_RM[i].selector = dpmi_sel();
  }

  hlt_hdlr.name = "DPMI rm cb";
  hlt_hdlr.len = DPMI_MAX_RMCBS;
  hlt_hdlr.func = rmcb_hlt;
  hlt_hdlr.arg = (void *)(long)current_client;
  DPMI_CLIENT.rmcb_seg = BIOS_HLT_BLK_SEG;
  DPMI_CLIENT.rmcb_off = hlt_register_handler(hlt_hdlr);
  if (DPMI_CLIENT.rmcb_off == (Bit16u)-1)
    goto err;

  ssp = SEGOFF2LINEAR(SREG(ss), 0);
  sp = LWORD(esp);

  psp = popw(ssp, sp);
  LWORD(ebx) = popw(ssp, sp);
  REG(esp) += 4;
  fake_retf(0);
  sp += 4;
  my_ip = _IP;
  my_cs = _CS;

  if (debug_level('M')) {
    unsigned sp2;
    cp = MK_FP32(my_cs, my_ip);

    D_printf("Going protected with fingers crossed\n"
		"32bit=%d, CS=%04x SS=%04x DS=%04x PSP=%04x ip=%04x sp=%04x\n",
		LO(ax), my_cs, SREG(ss), SREG(ds), psp, my_ip, REG(esp));
  /* display the 10 bytes before and after CS:EIP.  the -> points
   * to the byte at address CS:EIP
   */
    D_printf("OPS  : ");
    cp -= 10;
    for (i = 0; i < 10; i++)
      D_printf("%02x ", *cp++);
    D_printf("-> ");
    for (i = 0; i < 10; i++)
      D_printf("%02x ", *cp++);
    D_printf("\n");

    D_printf("STACK: ");
    sp2 = (sp & 0xffff0000) | (((sp & 0xffff) - 10 ) & 0xffff);
    for (i = 0; i < 10; i++)
      D_printf("%02x ", popb(ssp, sp2));
    D_printf("-> ");
    for (i = 0; i < 10; i++)
      D_printf("%02x ", popb(ssp, sp2));
    D_printf("\n");
    flush_log();
  }

  if (!(CS = AllocateDescriptors(1))) goto err;
  if (SetSelector(CS, (unsigned long) (my_cs << 4), 0xffff, 0,
                  MODIFY_LDT_CONTENTS_CODE, 0, 0, 0, 0)) goto err;

  if (!(SS = ConvertSegmentToDescriptor(SREG(ss)))) goto err;
  /* if ds==ss, the selectors will be equal too */
  if (!(DS = ConvertSegmentToDescriptor(SREG(ds)))) goto err;
  if (!(ES = AllocateDescriptors(1))) goto err;
  SetSegmentBaseAddress(ES, psp << 4);
  SetSegmentLimit(ES, 0xff);

  if (debug_level('M')) {
    print_ldt();
    D_printf("dpmi_sel()=%x CS=%x DS=%x SS=%x ES=%x\n",
	    dpmi_sel(), CS, DS, SS, ES);
  }

  scp   = &DPMI_CLIENT.stack_frame;
  _eip	= my_ip;
  _cs	= CS;
  _esp	= sp;
  _ss	= SS;
  _ds	= DS;
  _es	= ES;
  _fs	= 0;
  _gs	= 0;
#ifdef __linux__
  /* fpu_state needs to be paragraph aligned for fxrstor/fxsave */
  __fpstate = aligned_alloc(16, sizeof(*__fpstate));
  *__fpstate = *vm86_fpu_state;
#endif
  NOCARRY;
  rm_to_pm_regs(&DPMI_CLIENT.stack_frame, ~0);

  DPMI_CLIENT.win3x_mode = win3x_mode;
  msdos_init(DPMI_CLIENT.is_32,
    DPMI_CLIENT.private_data_segment + DPMI_private_paragraphs, psp);
  if (in_dpmi == 1) {
    s_i1c.segment = ISEG(0x1c);
    s_i1c.offset  = IOFF(0x1c);
    s_i23.segment = ISEG(0x23);
    s_i23.offset  = IOFF(0x23);
    s_i24.segment = ISEG(0x24);
    s_i24.offset  = IOFF(0x24);
    SETIVEC(0x1c, DPMI_SEG, DPMI_OFF + HLT_OFF(DPMI_int1c));
    SETIVEC(0x23, DPMI_SEG, DPMI_OFF + HLT_OFF(DPMI_int23));
    SETIVEC(0x24, DPMI_SEG, DPMI_OFF + HLT_OFF(DPMI_int24));

    in_dpmi_irq = 0;

    if (config.cpu_vm_dpmi == CPUVM_NATIVE)
      dpmi_tid = co_create(co_handle, dpmi_thr, NULL, NULL, SIGSTACK_SIZE);
  }

  dpmi_set_pm(1);

  for (i = 0; i < RSP_num; i++) {
    D_printf("DPMI: Calling RSP %i\n", i);
    dpmi_RSP_call(&DPMI_CLIENT.stack_frame, i, 0);
  }

  return; /* return immediately to the main loop */

err:
  error("DPMI initialization failed!\n");
  if (in_dpmi_pm())
    dpmi_set_pm(0);
  CARRY;
  FreeAllDescriptors();
  DPMI_free(&host_pm_block_root, DPMI_CLIENT.pm_stack->handle);
  DPMIfreeAll();
  in_dpmi--;
  current_client = in_dpmi - 1;
}

void dpmi_sigio(sigcontext_t *scp)
{
  if (DPMIValidSelector(_cs)) {
/* DANG_FIXTHIS We shouldn't return to dosemu code if IF=0, but it helps - WHY? */
/*
   Because IF is not set by popf and because dosemu have to do some background
   job (like DMA transfer) regardless whether IF is set or not.
*/
    dpmi_return(scp, DPMI_RET_DOSEMU);
  }
}

static void return_from_exception(sigcontext_t *scp)
{
  void *sp;
  leave_lpms(scp);
  D_printf("DPMI: Return from client exception handler, "
    "in_dpmi_pm_stack=%i\n", DPMI_CLIENT.in_dpmi_pm_stack);

  sp = SEL_ADR(_ss,_esp);

  if (DPMI_CLIENT.is_32) {
    unsigned int *ssp = sp;
    /* poping error code */
    ssp++;
    _eip = *ssp++;
    _cs = *ssp++;
    set_EFLAGS(_eflags, *ssp++);
    _esp = *ssp++;
    _ss = *ssp++;
  } else {
    unsigned short *ssp = sp;
    /* poping error code */
    ssp++;
    _LWORD(eip) = *ssp++;
    _cs = *ssp++;
    set_EFLAGS(_eflags, *ssp++);
    _LWORD(esp) = *ssp++;
    _ss = *ssp++;
    _HWORD(esp) = *ssp++;  /* Get the high word of ESP. */
  }
}

static void cpu_exception_rm(sigcontext_t *scp, int trapno)
{
  switch (trapno) {
    case 0x01: /* debug */
    case 0x03: /* int3 */
    case 0x04: /* overflow */
	__fake_pm_int(scp);
        do_int(trapno);
	break;
    default:
	p_dos_str("DPMI: Unhandled Exception %02x - Terminating Client\n"
	  "It is likely that dosemu is unstable now and should be rebooted\n",
	  trapno);
	quit_dpmi(scp, 0xff, 0, 0, 1);
	break;
  }
}

/*
 * DANG_BEGIN_FUNCTION do_default_cpu_exception
 *
 * This is the default CPU exception handler.
 * Exceptions 0, 1, 2, 3, 4, 5 and 7 are reflected
 * to real mode. All other exceptions are terminating the client
 * (and may be dosemu too :-)).
 *
 * DANG_END_FUNCTION
 */

static void do_default_cpu_exception(sigcontext_t *scp, int trapno)
{
    void * sp;
    sp = (us *)SEL_ADR(_ss,_esp);

#ifdef TRACE_DPMI
    if (debug_level('t') && (trapno==1)) {
      if (debug_level('t')>1)
	dbug_printf("%s",e_scp_disasm(scp,1));
      return;
    }
#endif

#ifdef USE_MHPDBG
    mhp_intercept("\nCPU Exception occured, invoking dosdebug\n\n", "+9M");
#endif
    if ((_trapno != 0x3 && _trapno != 0x1)
#ifdef X86_EMULATOR
      || debug_level('e')
#endif
     )
    { D_printf("%s", DPMI_show_state(scp)); }
#ifdef SHOWREGS
    print_ldt();
#endif

    D_printf("DPMI: do_default_cpu_exception 0x%02x at %#x:%#x ss:sp=%x:%x\n",
      trapno, (int)_cs, (int)_eip, (int)_ss, (int)_esp);

    /* Route the 0.9 exception to protected-mode interrupt handler or
     * terminate the client if the one is not installed. */
    if (trapno == 6 || trapno >= 8 ||
        DPMI_CLIENT.Interrupt_Table[trapno].selector == dpmi_sel()) {
      cpu_exception_rm(scp, trapno);
      return;
    }
    make_iret_frame(scp, sp, _cs, _eip);
    clear_IF();
    _eflags &= ~(TF | NT | AC);
    _cs = DPMI_CLIENT.Interrupt_Table[trapno].selector;
    _eip = DPMI_CLIENT.Interrupt_Table[trapno].offset;
}

/*
 * DANG_BEGIN_FUNCTION do_cpu_exception
 *
 * This routine switches to the locked protected mode stack,
 * disables interrupts and calls the DPMI client exception handler.
 * If no handler is installed the default handler is called.
 *
 * DANG_END_FUNCTION
 */
static void do_pm_cpu_exception(sigcontext_t *scp, INTDESC entry)
{
  unsigned int *ssp;
  unsigned short old_ss;
  unsigned int old_esp;

  old_ss = _ss;
  old_esp = _esp;
  ssp = enter_lpms(scp);

  /* Extended exception stack frame - DPMI 1.0 */
  *--ssp = 0;	/* PTE */
  *--ssp = DOSADDR_REL(LINP(_cr2));
  *--ssp = _gs;
  *--ssp = _fs;
  *--ssp = _ds;
  *--ssp = _es;
  *--ssp = old_ss;
  *--ssp = old_esp;
  *--ssp = get_vFLAGS(_eflags);
  *--ssp = _cs;  // xflags<<16 are always 0
  *--ssp = _eip;
  *--ssp = _err;
  if (DPMI_CLIENT.is_32) {
    *--ssp = dpmi_sel();
    *--ssp = DPMI_SEL_OFF(DPMI_return_from_ext_exception);
  } else {
    *--ssp = 0;
    *--ssp = (dpmi_sel() << 16) | DPMI_SEL_OFF(DPMI_return_from_ext_exception);
  }
  /* Standard exception stack frame - DPMI 0.9 */
  if (DPMI_CLIENT.is_32) {
    *--ssp = old_ss;
    *--ssp = old_esp;
    *--ssp = get_vFLAGS(_eflags);
    *--ssp = _cs;
    *--ssp = _eip;
    *--ssp = _err;
    *--ssp = dpmi_sel();
    *--ssp = DPMI_SEL_OFF(DPMI_return_from_exception);
  } else {
    *--ssp = 0;
    *--ssp = 0;
    *--ssp = 0;
    *--ssp = old_esp >> 16;  // save high esp word or it can be corrupted

    *--ssp = (old_ss << 16) | (unsigned short) old_esp;
    *--ssp = ((unsigned short) get_vFLAGS(_eflags) << 16) | _cs;
    *--ssp = ((unsigned)_LWORD(eip) << 16) | _err;
    *--ssp = (dpmi_sel() << 16) | DPMI_SEL_OFF(DPMI_return_from_exception);
  }
  ADD_16_32(_esp, -0x58);

  _cs = entry.selector;
  _eip = entry.offset;
  D_printf("DPMI: Ext Exception Table jump to %04x:%08x\n",_cs,_eip);
  clear_IF();
  _eflags &= ~(TF | NT | AC);
}

int dpmi_realmode_exception(unsigned trapno, unsigned err, dosaddr_t cr2)
{
  sigcontext_t *scp = &DPMI_CLIENT.stack_frame;
  unsigned int *ssp;

  if (DPMI_CLIENT.Exception_Table_RM[trapno].selector == dpmi_sel())
    return 0;

  save_pm_regs(&DPMI_CLIENT.stack_frame);
  rm_to_pm_regs(&DPMI_CLIENT.stack_frame, ~0);
  ssp = enter_lpms(&DPMI_CLIENT.stack_frame);

  /* Extended exception stack frame - DPMI 1.0 */
  *--ssp = 0;	/* PTE */
  *--ssp = cr2;
  *--ssp = _GS;
  *--ssp = _FS;
  *--ssp = _DS;
  *--ssp = _ES;
  *--ssp = _SS;
  *--ssp = _SP;
  *--ssp = get_FLAGS(REG(eflags)) | VM_MASK;
  *--ssp = _CS;
  *--ssp = _IP;
  *--ssp = err;
  if (DPMI_CLIENT.is_32) {
    *--ssp = dpmi_sel();
    *--ssp = DPMI_SEL_OFF(DPMI_return_from_rm_ext_exception);
  } else {
    *--ssp = 0;
    *--ssp = (dpmi_sel() << 16) | DPMI_SEL_OFF(DPMI_return_from_rm_ext_exception);
  }
  /* Standard exception stack frame - DPMI 0.9 */
  if (DPMI_CLIENT.is_32) {
    *--ssp = _SS;
    *--ssp = _SP;
    *--ssp = get_FLAGS(REG(eflags)) | VM_MASK;
    *--ssp = _CS;
    *--ssp = _IP;
    *--ssp = err;
    *--ssp = dpmi_sel();
    *--ssp = DPMI_SEL_OFF(DPMI_return_from_rm_exception);
  } else {
    *--ssp = 0;
    *--ssp = 0;
    *--ssp = 0;
    *--ssp = 0;

    *--ssp = (_SS << 16) | (unsigned short) _SP;
    *--ssp = ((unsigned short) (get_FLAGS(REG(eflags)) | VM_MASK) << 16) | _CS;
    *--ssp = ((unsigned)_IP << 16) | err;
    *--ssp = (dpmi_sel() << 16) | DPMI_SEL_OFF(DPMI_return_from_rm_exception);
  }
  ADD_16_32(_esp, -0x58);

  _cs = DPMI_CLIENT.Exception_Table_RM[trapno].selector;
  _eip = DPMI_CLIENT.Exception_Table_RM[trapno].offset;

  dpmi_set_pm(1);
  D_printf("DPMI: RM Exception Table jump to %04x:%08x\n",_cs,_eip);
  clear_IF();
  _eflags &= ~(TF | NT | AC);

  return 1;
}

static void do_legacy_cpu_exception(sigcontext_t *scp, INTDESC entry)
{
  unsigned int *ssp;
  unsigned short old_ss;
  unsigned int old_esp;

  old_ss = _ss;
  old_esp = _esp;
  ssp = enter_lpms(scp);

  /* Standard exception stack frame - DPMI 0.9 */
  if (DPMI_CLIENT.is_32) {
    *--ssp = old_ss;
    *--ssp = old_esp;
    *--ssp = get_vFLAGS(_eflags);
    *--ssp = _cs;
    *--ssp = _eip;
    *--ssp = _err;
    *--ssp = dpmi_sel();
    *--ssp = DPMI_SEL_OFF(DPMI_return_from_exception);
    ADD_16_32(_esp, -0x20);
  } else {
    *--ssp = old_esp >> 16;  // save high esp word or it can be corrupted
    *--ssp = (old_ss << 16) | (unsigned short) old_esp;
    *--ssp = ((unsigned short) get_vFLAGS(_eflags) << 16) | _cs;
    *--ssp = ((unsigned)_LWORD(eip) << 16) | _err;
    *--ssp = (dpmi_sel() << 16) | DPMI_SEL_OFF(DPMI_return_from_exception);
    ADD_16_32(_esp, -0x14);
  }

  _cs = entry.selector;
  _eip = entry.offset;
  D_printf("DPMI: Legacy Exception Table jump to %04x:%08x\n",_cs,_eip);
  clear_IF();
  _eflags &= ~(TF | NT | AC);
}

static void do_cpu_exception(sigcontext_t *scp)
{
#ifdef USE_MHPDBG
  if (_trapno == 0xd)
    mhp_intercept("\nCPU Exception occured, invoking dosdebug\n\n", "+9M");
#endif
  D_printf("DPMI: do_cpu_exception(0x%02x) at %#x:%#x, ss:esp=%x:%x, cr2=%#"PRI_RG", err=%#x\n",
	_trapno, _cs, _eip, _ss, _esp, _cr2, _err);
  if (debug_level('M') > 5)
    D_printf("DPMI: %s\n", DPMI_show_state(scp));

  if (DPMI_CLIENT.Exception_Table_PM[_trapno].selector != dpmi_sel() ||
      DPMI_CLIENT.Exception_Table_PM[_trapno].offset >=
      DPMI_SEL_OFF(DPMI_sel_end)) {
    do_pm_cpu_exception(scp, DPMI_CLIENT.Exception_Table_PM[_trapno]);
    return;
  }
  if (DPMI_CLIENT.Exception_Table[_trapno].selector != dpmi_sel()) {
    do_legacy_cpu_exception(scp, DPMI_CLIENT.Exception_Table[_trapno]);
    return;
  }

  do_default_cpu_exception(scp, _trapno);
}

static void do_dpmi_retf(sigcontext_t *scp, void * const sp)
{
  if (DPMI_CLIENT.is_32) {
    unsigned int *ssp = sp;
    _eip = *ssp++;
    _cs = *ssp++;
    _esp += 8;
  } else {
    unsigned short *ssp = sp;
    _LWORD(eip) = *ssp++;
    _cs = *ssp++;
    _LWORD(esp) += 4;
  }
}

static void do_dpmi_iret(sigcontext_t *scp, void * const sp)
{
  if (DPMI_CLIENT.is_32) {
    unsigned int *ssp = sp;
    _eip = *ssp++;
    _cs = *ssp++;
    _eflags = eflags_VIF(*ssp++);
    _esp += 12;
  } else {
    unsigned short *ssp = sp;
    _LWORD(eip) = *ssp++;
    _cs = *ssp++;
    _eflags = eflags_VIF(*ssp++);
    _LWORD(esp) += 6;
  }
}

static int dpmi_gpf_simple(sigcontext_t *scp, uint8_t *lina, void *sp, int *rv)
{
    *rv = DPMI_RET_CLIENT;
    if ((_err & 7) == 2) {			/* int xx */
      int inum = _err >> 3;
      if (inum != lina[1]) {
        error("DPMI: internal error, %x %x\n", inum, lina[1]);
        p_dos_str("DPMI: internal error, %x %x\n", inum, lina[1]);
        quit_dpmi(scp, 0xff, 0, 0, 1);
        return 1;
      }
      D_printf("DPMI: int 0x%04x, AX=0x%04x\n", inum, _LWORD(eax));
#ifdef USE_MHPDBG
      if (mhpdbg.active) {
        if (dpmi_mhp_intxxtab[inum]) {
          int ret = dpmi_mhp_intxx_check(scp, inum);
          if (ret != DPMI_RET_CLIENT) {
            *rv = ret;
            return 1;
          }
        }
      }
#endif
      /* Bypass the int instruction */
      _eip += 2;
      if (DPMI_CLIENT.Interrupt_Table[inum].selector == dpmi_sel()) {
	do_dpmi_int(scp, inum);
      } else {
        us cs2 = _cs;
        unsigned long eip2 = _eip;
	if (debug_level('M')>=9)
          D_printf("DPMI: int 0x%x\n", lina[1]);
	make_iret_frame(scp, sp, _cs, _eip);
	if (inum<=7) {
	  clear_IF();
	}
	_eflags &= ~(TF | NT | AC);
	_cs = DPMI_CLIENT.Interrupt_Table[inum].selector;
	_eip = DPMI_CLIENT.Interrupt_Table[inum].offset;
	D_printf("DPMI: call inthandler %#02x(%#04x) at %#04x:%#08x\n\t\tret=%#04x:%#08lx\n",
		inum, _LWORD(eax), _cs, _eip, cs2, eip2);
	if ((inum == 0x2f)&&((_LWORD(eax)==
			      0xfb42)||(_LWORD(eax)==0xfb43)))
	    D_printf("DPMI: dpmiload function called, ax=0x%04x,bx=0x%04x\n"
		     ,_LWORD(eax), _LWORD(ebx));
	if ((inum == 0x21) && (_HI(ax) == 0x4c))
	    D_printf("DPMI: DOS exit called\n");
      }
      return 1;
    }

    switch (*lina) {
    case 0xf4:			/* hlt */
      _eip += 1;
      if (_cs == dpmi_sel()) {
	if (_eip==1+DPMI_SEL_OFF(DPMI_raw_mode_switch_pm)) {
	  D_printf("DPMI: switching from protected to real mode\n");
	  SREG(ds) = _LWORD(eax);
	  SREG(es) = _LWORD(ecx);
	  SREG(ss) = _LWORD(edx);
	  REG(esp) = _LWORD(ebx);
	  SREG(cs) = _LWORD(esi);
	  REG(eip) = _LWORD(edi);
	  REG(ebp) = _ebp;
	  REG(eflags) = 0x0202 | (0x0dd5 & _eflags);
	  SREG(fs) = SREG(gs) = 0;
	  /* zero out also the "undefined" registers? */
	  REG(eax) = REG(ebx) = REG(ecx) = REG(edx) = REG(esi) = REG(edi) = 0;
	  dpmi_set_pm(0);

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_save_restore_pm)) {
	  if (_LO(ax)==0) {
            D_printf("DPMI: save real mode registers\n");
            DPMI_save_rm_regs(SEL_ADR_X(_es, _edi));
	  } else {
            D_printf("DPMI: restore real mode registers\n");
            DPMI_restore_rm_regs(SEL_ADR_X(_es, _edi), ~0);
          }/* _eip point to FAR RET */

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_API_extension)) {
          D_printf("DPMI: extension API call: 0x%04x\n", _LWORD(eax));
          DPMI_CLIENT.ext__thunk_16_32 = _LO(ax);

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_pm)) {
	  unsigned char imr;
	  leave_lpms(scp);
          D_printf("DPMI: Return from hardware interrupt handler, "
	    "in_dpmi_pm_stack=%i\n", DPMI_CLIENT.in_dpmi_pm_stack);
	  if (DPMI_CLIENT.is_32) {
	    unsigned int *ssp = sp;
	    int pm;
	    _eip = *ssp++;
	    _cs = *ssp++;
	    _esp = *ssp++;
	    _ss = *ssp++;
	    pm = *ssp++;
	    if (pm > 1) {
	      error("DPMI: HW interrupt stack corrupted\n");
	      leavedos(38);
	    }
	    dpmi_set_pm(pm);
	    ssp++;
	    imr = *ssp++;
	  } else {
	    unsigned short *ssp = sp;
	    int pm;
	    _LWORD(eip) = *ssp++;
	    _cs = *ssp++;
	    _LWORD(esp) = *ssp++;
	    _ss = *ssp++;
	    pm = *ssp++;
	    if (pm > 1) {
	      error("DPMI: HW interrupt stack corrupted\n");
	      leavedos(38);
	    }
	    dpmi_set_pm(pm);
	    _HWORD(esp) = *ssp++;
	    imr = *ssp++;
	  }
	  in_dpmi_irq--;
	  port_outb(0x21, imr);
	  set_IF();

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_exception)) {
	  return_from_exception(scp);

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_ext_exception)) {
	  unsigned int *ssp = sp;
	  D_printf("DPMI: Return from client extended exception handler, "
	    "in_dpmi_pm_stack=%i\n", DPMI_CLIENT.in_dpmi_pm_stack);
	  leave_lpms(scp);
	  if (!DPMI_CLIENT.is_32)
	    ssp++;
	  ssp++;  /* popping error code */
	  _eip = *ssp++;
	  _cs = *ssp++;
	  set_EFLAGS(_eflags, *ssp++);
	  _esp = *ssp++;
	  _ss = *ssp++;
	  _es = *ssp++;
	  _ds = *ssp++;
	  _fs = *ssp++;
	  _gs = *ssp++;

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_rm_exception)) {
	  D_printf("DPMI: Return from RM client exception handler, "
	    "in_dpmi_pm_stack=%i\n", DPMI_CLIENT.in_dpmi_pm_stack);
	  leave_lpms(scp);
	  pm_to_rm_regs(scp, ~0);
	  restore_pm_regs(scp);
	  dpmi_set_pm(0);
	  if (DPMI_CLIENT.is_32) {
	    unsigned int *ssp = sp;
	    /* poping error code */
	    ssp++;
	    _IP = *ssp++;
	    _CS = *ssp++;
	    set_EFLAGS(REG(eflags), *ssp++);
	    _SP = *ssp++;
	    _SS = *ssp++;
	  } else {
	    unsigned short *ssp = sp;
	    /* poping error code */
	    ssp++;
	    _IP = *ssp++;
	    _CS = *ssp++;
	    set_EFLAGS(REG(eflags), *ssp++);
	    _SP = *ssp++;
	    _SS = *ssp++;
	  }

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_rm_ext_exception)) {
	  unsigned int *ssp = sp;
	  D_printf("DPMI: Return from client RM extended exception handler, "
	    "in_dpmi_pm_stack=%i\n", DPMI_CLIENT.in_dpmi_pm_stack);
	  leave_lpms(scp);
	  pm_to_rm_regs(scp, ~0);
	  restore_pm_regs(scp);
	  dpmi_set_pm(0);
	  if (!DPMI_CLIENT.is_32)
	    ssp++;
	  ssp++;  /* popping error code */
	  _IP = *ssp++;
	  _CS = *ssp++;
	  set_EFLAGS(REG(eflags), *ssp++);
	  _SP = *ssp++;
	  _SS = *ssp++;
	  _ES = *ssp++;
	  _DS = *ssp++;
	  _FS = *ssp++;
	  _GS = *ssp++;

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_rm_callback)) {

	  leave_lpms(scp);
	  D_printf("DPMI: Return from client realmode callback procedure, "
	    "in_dpmi_pm_stack=%i\n", DPMI_CLIENT.in_dpmi_pm_stack);

	  DPMI_restore_rm_regs(SEL_ADR_X(_es, _edi), ~0);
	  restore_pm_regs(scp);
	  dpmi_set_pm(0);

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_int_1c)) {
	  leave_lpms(scp);
	  D_printf("DPMI: Return from int1c, in_dpmi_pm_stack=%i\n",
	    DPMI_CLIENT.in_dpmi_pm_stack);

	  pm_to_rm_regs(scp, ~0);
	  restore_pm_regs(scp);
	  dpmi_set_pm(0);

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_int_23)) {
	  sigcontext_t old_ctx, *curscp;
	  unsigned int old_esp;
	  unsigned short *ssp;
	  int esp_delta;
	  leave_lpms(scp);
	  D_printf("DPMI: Return from int23 callback, in_dpmi_pm_stack=%i\n",
	    DPMI_CLIENT.in_dpmi_pm_stack);

	  pm_to_rm_regs(scp, ~0);
	  restore_pm_regs(&old_ctx);
	  dpmi_set_pm(0);
	  curscp = scp;
	  scp = &old_ctx;
	  old_esp = DPMI_CLIENT.in_dpmi_pm_stack ? D_16_32(_esp) : D_16_32(DPMI_pm_stack_size);
	  scp = curscp;
	  esp_delta = old_esp - D_16_32(_esp);
	  ssp = (us *) SEL_ADR(_ss, _esp);
	  copy_context(scp, &old_ctx, -1);
	  if (esp_delta) {
	    unsigned int rm_ssp, sp;
	    D_printf("DPMI: ret from int23 with esp_delta=%i\n", esp_delta);
	    rm_ssp = SEGOFF2LINEAR(SREG(ss), 0);
	    sp = LWORD(esp);
	    esp_delta >>= DPMI_CLIENT.is_32;
	    if (esp_delta == 2) {
	      pushw(rm_ssp, sp, *ssp);
	    } else {
	      error("DPMI: ret from int23 with esp_delta=%i\n", esp_delta);
	    }
	    LWORD(esp) -= esp_delta;
	    if (DPMI_CLIENT.in_dpmi_pm_stack) {
	      D_printf("DPMI: int23 invoked while on PM stack!\n");
	      REG(eflags) &= ~CF;
	    }
	    if (REG(eflags) & CF) {
	      D_printf("DPMI: int23 termination request\n");
	      quit_dpmi(scp, 0, 0, 0, 0);
	    }
	  }

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_int_24)) {
	  leave_lpms(scp);
	  D_printf("DPMI: Return from int24 callback, in_dpmi_pm_stack=%i\n",
	    DPMI_CLIENT.in_dpmi_pm_stack);

	  pm_to_rm_regs(scp, ~(1 << ebp_INDEX));
	  restore_pm_regs(scp);
	  dpmi_set_pm(0);

        } else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_RSPcall)) {
	  leave_lpms(scp);
	  if (DPMI_CLIENT.is_32) {
	    unsigned int *ssp = sp;
	    int pm = *ssp++;
	    if (pm > 1) {
	      error("DPMI: RSPcall stack corrupted\n");
	      leavedos(38);
	    }
	    dpmi_set_pm(pm);
	  } else {
	    unsigned short *ssp = sp;
	    int pm = *ssp++;
	    if (pm > 1) {
	      error("DPMI: RSPcall stack corrupted\n");
	      leavedos(38);
	    }
	    dpmi_set_pm(pm);
	  }
	  D_printf("DPMI: Return from RSPcall, in_dpmi_pm_stack=%i, dpmi_pm=%i\n",
	    DPMI_CLIENT.in_dpmi_pm_stack, in_dpmi_pm());
	  restore_pm_regs(scp);
	  if (!in_dpmi_pm()) {
	    /* app terminated */
	    dpmi_soft_cleanup();
	  }

	} else if ((_eip>=1+DPMI_SEL_OFF(DPMI_exception)) && (_eip<=32+DPMI_SEL_OFF(DPMI_exception))) {
	  int excp = _eip-1-DPMI_SEL_OFF(DPMI_exception);
	  D_printf("DPMI: default exception handler 0x%02x called\n",excp);
	  do_dpmi_retf(scp, sp);
	  /* legacy (0.9) exceptions are routed to PM int handlers */
	  return_from_exception(scp);
	  do_default_cpu_exception(scp, excp);

	} else if ((_eip>=1+DPMI_SEL_OFF(DPMI_ext_exception)) && (_eip<=32+DPMI_SEL_OFF(DPMI_ext_exception))) {
	  int excp = _eip-1-DPMI_SEL_OFF(DPMI_ext_exception);
	  D_printf("DPMI: default ext exception handler 0x%02x called\n",excp);
	  /* first check for legacy handler */
	  if (DPMI_CLIENT.Exception_Table[excp].selector != dpmi_sel()) {
	    D_printf("DPMI: chaining to old exception handler\n");
	    _cs = DPMI_CLIENT.Exception_Table[excp].selector;
	    _eip = DPMI_CLIENT.Exception_Table[excp].offset;
	  } else {
	    /* 1.0 DPMI spec says this should go straight to RM */
	    cpu_exception_rm(scp, excp);
	  }

	} else if ((_eip>=1+DPMI_SEL_OFF(DPMI_rm_exception)) && (_eip<=32+DPMI_SEL_OFF(DPMI_rm_exception))) {
	  int excp = _eip-1-DPMI_SEL_OFF(DPMI_rm_exception);
	  D_printf("DPMI: default rm exception handler 0x%02x called\n",excp);
	  cpu_exception_rm(scp, excp);

	} else if ((_eip>=1+DPMI_SEL_OFF(DPMI_interrupt)) &&
		   (_eip<1+256+DPMI_SEL_OFF(DPMI_interrupt))) {
	  int intr = _eip-1-DPMI_SEL_OFF(DPMI_interrupt);
	  D_printf("DPMI: default protected mode interrupthandler 0x%02x called\n",intr);
	  do_dpmi_iret(scp, sp);
	  do_dpmi_int(scp, intr);

	} else if (_eip==1+DPMI_SEL_OFF(DPMI_return_from_dosint)) {
	  int intr;
	  struct RealModeCallStructure rmreg;
	  uint32_t *ssp;

	  rmreg = *(struct RealModeCallStructure *)SEL_ADR(_ss, _esp);
	  _esp += sizeof(struct RealModeCallStructure);
	  ssp = SEL_ADR(_ss, _esp);
	  intr = *(ssp++);
	  _esp += 4;
	  do_dpmi_retf(scp, SEL_ADR(_ss, _esp));
	  D_printf("DPMI: Return from DOS Interrupt 0x%02x\n", intr);
	  msdos_post_extender(scp, intr, &rmreg);

	} else if ((_eip>=1+DPMI_SEL_OFF(DPMI_VXD_start)) &&
		(_eip<1+DPMI_SEL_OFF(DPMI_VXD_end))) {
	  D_printf("DPMI: VxD call, ax=%#x\n", _LWORD(eax));
	  vxd_call(scp);

	} else if ((_eip>=1+DPMI_SEL_OFF(MSDOS_spm_start)) &&
		(_eip<1+DPMI_SEL_OFF(MSDOS_spm_end))) {
	  int offs = _eip - (1+DPMI_SEL_OFF(MSDOS_spm_start));
	  struct RealModeCallStructure rmreg;
	  int ret;

	  D_printf("DPMI: Starting MSDOS pm callback\n");
	  save_rm_regs();
	  DPMI_save_rm_regs(&rmreg);
	  rmreg.cs = DPMI_SEG;
	  rmreg.ip = DPMI_OFF + HLT_OFF(DPMI_return_from_dosext);
	  ret = msdos_pre_pm(offs, scp, &rmreg);
	  if (!ret) {
	    restore_rm_regs();
	    break;
	  }
	  _eip = DPMI_SEL_OFF(MSDOS_epm_start) + offs;
	  DPMI_restore_rm_regs(&rmreg, ~0);
	  dpmi_set_pm(0);

	} else if ((_eip>=1+DPMI_SEL_OFF(MSDOS_epm_start)) &&
		(_eip<1+DPMI_SEL_OFF(MSDOS_epm_end))) {
	  int offs = _eip - (1+DPMI_SEL_OFF(MSDOS_epm_start));
	  struct RealModeCallStructure rmreg;

	  memcpy(&rmreg, SEL_ADR(_ss, _esp), sizeof(rmreg));
	  _esp += sizeof(struct RealModeCallStructure);
	  do_dpmi_retf(scp, SEL_ADR(_ss, _esp));
	  D_printf("DPMI: Ending MSDOS pm callback\n");
	  msdos_post_pm(offs, scp, &rmreg);

	} else if ((_eip>=1+DPMI_SEL_OFF(MSDOS_pmc_start)) &&
		(_eip<1+DPMI_SEL_OFF(MSDOS_pmc_end))) {
	  D_printf("DPMI: Starting MSDOS pm call\n");
	  msdos_pm_call(scp, DPMI_CLIENT.is_32);

	} else {
	  D_printf("DPMI: unhandled hlt\n");
	  return 1;
	}
      } else { 			/* in client\'s code, set back eip */
	_eip -= 1;
	do_cpu_exception(scp);
      }
      break;
    case 0xfa:			/* cli */
      if (debug_level('M')>=9)
        D_printf("DPMI: cli\n");
      _eip += 1;
      /*
       * are we trapped in a deadly loop?
       */
      if ((lina[1] == 0xeb) && (lina[2] == 0xfe)) {
	dbug_printf("OUCH! deadly loop, cannot continue");
	leavedos(97);
      }
      if (find_cli_in_blacklist(lina)) {
        D_printf("DPMI: Ignoring blacklisted cli\n");
	break;
      }
      current_cli = lina;
      /* look for "pushfd; pop eax; cli" (DOOM) and
       * "pushfd; cli" (NFS-SE) patterns */
      if (_eip >= 2 && ((lina[-2] == 0x9c && lina[-1] == 0x58) ||
          (lina[-2] == 0xc3 && lina[-1] == 0x9c))) {
        D_printf("DOOM cli work-around\n");
        clear_IF_timed();
      } else {
        clear_IF();
      }
      dpmi_is_cli = 1;
      break;
    case 0xfb:			/* sti */
      if (debug_level('M')>=9)
        D_printf("DPMI: sti\n");
      _eip += 1;
      set_IF();
      break;

    default:
      return 0;
    }
    return 1;
}

/*
 * DANG_BEGIN_FUNCTION dpmi_fault
 *
 * This is the brain of DPMI. All CPU exceptions are first
 * reflected (from the signal handlers) to this code.
 *
 * Exception from nonprivileged instructions INT XX, STI, CLI, HLT
 * and from WINDOWS 3.1 are handled here.
 *
 * All here unhandled exceptions are reflected to do_cpu_exception()
 *
 * Note for cpu-emu: exceptions generated from the emulator are handled
 *  here. 'Real' system exceptions (e.g. from an emulator fault) are
 *  redirected to emu_dpmi_fault() in fullemu mode
 *
 * DANG_END_FUNCTION
 */
static int dpmi_fault1(sigcontext_t *scp)
{
#define LWORD32(x,y) {if (Segments[_cs >> 3].is_32) _##x y; else _LWORD(x) y;}
#define ASIZE_IS_32 (Segments[_cs >> 3].is_32 ^ prefix67)
#define OSIZE_IS_32 (Segments[_cs >> 3].is_32 ^ prefix66)
	/* both clear: non-prefixed in a 16-bit CS = 16-bit
	 * one set: non-prefixed in 32-bit CS or prefixed in 16-bit CS = 32-bit
	 * both set: prefixed in a 32-bit CS = 16-bit
	 */
#define _LWECX	   (ASIZE_IS_32 ? _ecx : _LWORD(ecx))
#define set_LWECX(x) {if (ASIZE_IS_32) _ecx=(x); else _LWORD(ecx) = (x);}

  void *sp;
  unsigned char *csp, *lina;
  int ret = DPMI_RET_CLIENT;

  sanitize_flags(_eflags);

  /* 32-bit ESP in 16-bit code on a 32-bit expand-up stack outside the limit...
     this is so wrong that it can only happen inherited through a CPU bug
     (see EMUFailures.txt:1.6.2) or if someone did it on purpose.
     Happens with an ancient MS linker. Maybe fault again; ESP won't be
     corrupted anymore after an IRET because the stack is 32-bits now.
     Note: we used to check for kernel space bits in the high part of ESP
     but that method is unreliable for 32-bit DOSEMU on x86-64 kernels.
  */
  if (_esp > 0xffff && !Segments[_cs >> 3].is_32 && Segments[_ss >> 3].is_32 &&
      Segments[_ss >> 3].type != MODIFY_LDT_CONTENTS_STACK &&
      _esp > GetSegmentLimit(_ss)) {
    D_printf("DPMI: ESP bug, esp=%#x, ebp=%#x, limit=%#x\n",
	     _esp, _ebp, GetSegmentLimit(_ss));
    _esp &= 0xffff;
    return ret;
  }

  csp = lina = (unsigned char *) SEL_ADR(_cs, _eip);
  sp = SEL_ADR(_ss, _esp);

#ifdef USE_MHPDBG
  if (mhpdbg.active) {
    if (_trapno == 3 && mhp_debug(DBG_TRAP + (_trapno << 8), 0, 0))
       return DPMI_RET_TRAP_BP;
    if (_trapno == 1 && mhp_debug(DBG_TRAP + (_trapno << 8), 0, 0)) {
#if 0
      _eflags &= ~TF;
      /* the below crashes after long jump because csp[-1] may not be valid.
       * debugger should emulate the instructions, not here. */
      switch (csp[-1]) {
        case 0x9c:	/* pushf */
	{
	  unsigned short *ssp = sp;
	  ssp[0] &= ~TF;
	  break;
	}
        case 0x9f:	/* lahf */
	  _eax &= ~(TF << 8);
	  break;
      }
      dpmi_mhp_TF=0;
#endif
      return DPMI_RET_TRAP_DB;
    }
  }
#endif
  if (_trapno == 13) {
    Bit32u org_eip;
    int pref_seg;
    int done,is_rep,prefix66,prefix67;

    if (CheckSelectors(scp, 1) == 0)
      leavedos(36);
    if (dpmi_gpf_simple(scp, csp, sp, &ret))
      return ret;

    /* DANG_BEGIN_REMARK
     * Here we handle all prefixes prior switching to the appropriate routines
     * The exception CS:EIP will point to the first prefix that effects the
     * the faulting instruction, hence, 0x65 0x66 is same as 0x66 0x65.
     * So we collect all prefixes and remember them.
     * - Hans Lermen
     * DANG_END_REMARK
     */

    done=0;
    is_rep=0;
    prefix66=prefix67=0;
    pref_seg=-1;

    do {
      switch (*(csp++)) {
         case 0x66:      /* operand prefix */  prefix66=1; break;
         case 0x67:      /* address prefix */  prefix67=1; break;
         case 0x2e:      /* CS */              pref_seg=_cs; break;
         case 0x3e:      /* DS */              pref_seg=_ds; break;
         case 0x26:      /* ES */              pref_seg=_es; break;
         case 0x36:      /* SS */              pref_seg=_ss; break;
         case 0x65:      /* GS */              pref_seg=_gs; break;
         case 0x64:      /* FS */              pref_seg=_fs; break;
         case 0xf2:      /* repnz */
         case 0xf3:      /* rep */             is_rep=1; break;
         default: done=1;
      }
    } while (!done);
    csp--;
    org_eip = _eip;
    _eip += (csp-lina);

#ifdef X86_EMULATOR
    if (config.cpuemu>3) {
	switch (*csp) {
	case 0x6c: case 0x6d: case 0x6e: case 0x6f: /* insb/insw/outsb/outsw */
	case 0xe4: case 0xe5: case 0xe6: case 0xe7: /* inb/inw/outb/outw imm */
	case 0xec: case 0xed: case 0xee: case 0xef: /* inb/inw/outb/outw dx */
	case 0xfa: case 0xfb: /* cli/sti */
	    break;
	default: /* int/hlt/0f/cpu_exception */
	    ret = DPMI_RET_DOSEMU;
	    break;
	}
    }
#endif

    switch (*csp++) {

    case 0x6c:                    /* [rep] insb */
      if (debug_level('M')>=9)
        D_printf("DPMI: insb\n");
      /* NOTE: insb uses ES, and ES can't be overwritten by prefix */
      /* WARNING: no test for (E)DI wrapping! */
      if (ASIZE_IS_32)		/* a32 insb */
	_edi += port_rep_inb(_LWORD(edx), (Bit8u *)SEL_ADR(_es,_edi),
	        _LWORD(eflags)&DF, (is_rep?_LWECX:1));
      else			/* a16 insb */
	_LWORD(edi) += port_rep_inb(_LWORD(edx), (Bit8u *)SEL_ADR(_es,_LWORD(edi)),
        	_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      if (is_rep) set_LWECX(0);
      LWORD32(eip,++);
      break;

    case 0x6d:			/* [rep] insw/d */
      if (debug_level('M')>=9)
        D_printf("DPMI: ins%s\n", OSIZE_IS_32 ? "d" : "w");
      /* NOTE: insw/d uses ES, and ES can't be overwritten by prefix */
      /* WARNING: no test for (E)DI wrapping! */
      if (OSIZE_IS_32) {	/* insd */
	if (ASIZE_IS_32)	/* a32 insd */
	  _edi += port_rep_ind(_LWORD(edx), (Bit32u *)SEL_ADR(_es,_edi),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	else			/* a16 insd */
	  _LWORD(edi) += port_rep_ind(_LWORD(edx), (Bit32u *)SEL_ADR(_es,_LWORD(edi)),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      }
      else {			/* insw */
	if (ASIZE_IS_32)	/* a32 insw */
	  _edi += port_rep_inw(_LWORD(edx), (Bit16u *)SEL_ADR(_es,_edi),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
	else			/* a16 insw */
	  _LWORD(edi) += port_rep_inw(_LWORD(edx), (Bit16u *)SEL_ADR(_es,_LWORD(edi)),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      }
      if (is_rep) set_LWECX(0);
      LWORD32(eip,++);
      break;

    case 0x6e:			/* [rep] outsb */
      if (debug_level('M')>=9)
        D_printf("DPMI: outsb\n");
      if (pref_seg < 0) pref_seg = _ds;
      /* WARNING: no test for (E)SI wrapping! */
      if (ASIZE_IS_32)		/* a32 outsb */
	_esi += port_rep_outb(_LWORD(edx), (Bit8u *)SEL_ADR(pref_seg,_esi),
	        _LWORD(eflags)&DF, (is_rep?_LWECX:1));
      else			/* a16 outsb */
	_LWORD(esi) += port_rep_outb(_LWORD(edx), (Bit8u *)SEL_ADR(pref_seg,_LWORD(esi)),
	        _LWORD(eflags)&DF, (is_rep?_LWECX:1));
      if (is_rep) set_LWECX(0);
      LWORD32(eip,++);
      break;

    case 0x6f:			/* [rep] outsw/d */
      if (debug_level('M')>=9)
        D_printf("DPMI: outs%s\n", OSIZE_IS_32 ? "d" : "w");
      if (pref_seg < 0) pref_seg = _ds;
      /* WARNING: no test for (E)SI wrapping! */
      if (OSIZE_IS_32) {	/* outsd */
        if (ASIZE_IS_32)	/* a32 outsd */
	  _esi += port_rep_outd(_LWORD(edx), (Bit32u *)SEL_ADR(pref_seg,_esi),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
        else			/* a16 outsd */
	  _LWORD(esi) += port_rep_outd(_LWORD(edx), (Bit32u *)SEL_ADR(pref_seg,_LWORD(esi)),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      }
      else {			/* outsw */
        if (ASIZE_IS_32)	/* a32 outsw */
	  _esi += port_rep_outw(_LWORD(edx), (Bit16u *)SEL_ADR(pref_seg,_esi),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
        else			/* a16 outsw */
	  _LWORD(esi) += port_rep_outw(_LWORD(edx), (Bit16u *)SEL_ADR(pref_seg,_LWORD(esi)),
		_LWORD(eflags)&DF, (is_rep?_LWECX:1));
      }
      if (is_rep) set_LWECX(0);
      LWORD32(eip,++);
      break;

    case 0xe5:			/* inw xx, ind xx */
      if (debug_level('M')>=9)
        D_printf("DPMI: in%s xx\n", OSIZE_IS_32 ? "d" : "w");
      if (OSIZE_IS_32) _eax = ind((int) csp[0]);
      else _LWORD(eax) = inw((int) csp[0]);
      LWORD32(eip, += 2);
      break;
    case 0xe4:			/* inb xx */
      if (debug_level('M')>=9)
        D_printf("DPMI: inb xx\n");
      _LWORD(eax) &= ~0xff;
      _LWORD(eax) |= inb((int) csp[0]);
      LWORD32(eip, += 2);
      break;
    case 0xed:			/* inw dx */
      if (debug_level('M')>=9)
        D_printf("DPMI: in%s dx\n", OSIZE_IS_32 ? "d" : "w");
      if (OSIZE_IS_32) _eax = ind(_LWORD(edx));
      else _LWORD(eax) = inw(_LWORD(edx));
      LWORD32(eip,++);
      break;
    case 0xec:			/* inb dx */
      if (debug_level('M')>=9)
        D_printf("DPMI: inb dx\n");
      _LWORD(eax) &= ~0xff;
      _LWORD(eax) |= inb(_LWORD(edx));
      LWORD32(eip, += 1);
      break;
    case 0xe7:			/* outw xx */
      if (debug_level('M')>=9)
        D_printf("DPMI: out%s xx\n", OSIZE_IS_32 ? "d" : "w");
      if (OSIZE_IS_32) outd((int)csp[0], _eax);
      else outw((int)csp[0], _LWORD(eax));
      LWORD32(eip, += 2);
      break;
    case 0xe6:			/* outb xx */
      if (debug_level('M')>=9)
        D_printf("DPMI: outb xx\n");
      outb((int) csp[0], _LO(ax));
      LWORD32(eip, += 2);
      break;
    case 0xef:			/* outw dx */
      if (debug_level('M')>=9)
        D_printf("DPMI: out%s dx\n", OSIZE_IS_32 ? "d" : "w");
      if (OSIZE_IS_32) outd(_LWORD(edx), _eax);
      else outw(_LWORD(edx), _LWORD(eax));
      LWORD32(eip, += 1);
      break;
    case 0xee:			/* outb dx */
      if (debug_level('M')>=9)
        D_printf("DPMI: outb dx\n");
      outb(_LWORD(edx), _LO(ax));
      LWORD32(eip, += 1);
      break;

    case 0x0f: {
      uint32_t *reg32[8] = { &_eax, &_ecx, &_edx, &_ebx, &_esp, &_ebp, &_esi, &_edi };
      uint16_t *reg16[8] = { &_LWORD(eax), &_LWORD(ecx), &_LWORD(edx), &_LWORD(ebx),
                      &_LWORD(esp), &_LWORD(ebp), &_LWORD(esi), &_LWORD(edi) };
      if (debug_level('M')>=9)
        D_printf("DPMI: 0f opcode %x\n", csp[0]);
      switch (csp[0]) {
        case 0: // SLDT, STR ...
        case 1: // SGDT, SIDT, SMSW ...
          switch (csp[1] & 0xc0) {
            case 0xc0: // register dest
              /* just write 0 */
              if (OSIZE_IS_32)
                *reg32[csp[1] & 7] = 0;
              else
                *reg16[csp[1] & 7] = 0;
              LWORD32(eip, += 3);
              break;
            default:
              error("DPMI: unsupported SLDT dest %x\n", csp[1]);
              LWORD32(eip, += 3);
              break;
          }
          break;
        case 0x20:
          switch (csp[1] & 0xc0) {
            case 0xc0: // register dest
              /* just write 0 */
#define CR0_PE         0x00000001
#define CR0_PG         0x80000000
              *reg32[csp[1] & 7] = CR0_PE | CR0_PG;
              LWORD32(eip, += 3);
              break;
            default:
              error("DPMI: unsupported mov xx,cr0 dest %x\n", csp[1]);
              LWORD32(eip, += 3);
              break;
          }
          break;
        case 0x22:
          /* mov cr0, xx - ignore */
          LWORD32(eip, += 3);
          break;
        default:
          error("%s", DPMI_show_state(scp));
          goto out;
      }
      break;
    }
out:
    default:
      _eip = org_eip;
      do_cpu_exception(scp);
    } /* switch */
  } /* _trapno==13 */
  else {
    if (_trapno == 0x0c) {
      if (Segments[_cs >> 3].is_32 && !Segments[_ss >> 3].is_32 &&
	  _esp > 0xffff) {
	unsigned char *p = csp;
	unsigned int *regs[8] = { &_eax, &_ecx, &_edx, &_ebx,
				  &_esp, &_ebp, &_esi, &_edi };
	unsigned int *reg;
        D_printf("DPMI: Stack Fault, ESP corrupted due to a CPU bug, "
		 "trying to recover.\n");
	/* There are a few ways to recover:
	 * If the instruction was a normal push or a pop we wouldn't be here!
	 * Assume a modr/m instruction
	 * First decode what is the likely register that caused mayhem,
	   this is not 100% correct but works for known cases.
	   - Native 64-bit on __x86_64__:
	     simply zero out high parts of the offending register and ESP
	     and try again: the iret trampoline avoids recorruption.
	   - 32-bit DOSEMU:
	     If ESP did not cause the stack fault, then zero the high part
	     of the other register and try again,
	     else try to return to DOSEMU and retry via direct_dpmi_switch
	     if the trap flag is not set, so it won't recorrupt ESP,
	     else we're lost :(
	     but then this won't happen on i386 kernels >= 2.6.12 :)
	*/
	if (*p == 0x66) p++; /* operand size override */
	if (*p == 0x36) p++; /* ss: override */
	if (*p == 0x66) p++; /* operand size override */
	if (*p == 0x0f) p++; /* instruction shift */
	p++; /* skip instruction, modr/m byte follows */
	if ((*p & 7) == 4) p++; /* sib byte */
	D_printf("DPMI: stack fault was caused by register %d\n", *p & 7);
	reg = regs[*p & 7];
	_esp &= 0xffff;
#ifdef __x86_64__
	if (*reg > 0xffff) {
	  *reg &= 0xffff;
	  return ret;
	}
#else
	if (reg != &_esp) {
	  if (*reg > 0xffff) {
	    *reg &= 0xffff;
	    return ret;
	  }
	}
#endif
        error("Stack Fault, ESP corrupted due to a CPU bug.\n"
	      "For more details on that problem and possible work-arounds,\n"
	      "please read EMUfailure.txt, section 1.6.2.\n");
#if 0
	_HWORD(ebp) = 0;
	_HWORD(esp) = 0;
	if (instr_emu(scp, 1, 1)) {
	  error("Instruction emulated\n");
	  return ret;
	}
	error("Instruction emulation failed!\n"
	      "%s\n"
	      "Now we will force a B bit for a stack segment in hope this will help.\n"
	      "This is not reliable and your program may now crash.\n"
	      "In that case try to convince Intel to fix that bug.\n"
	      "Now patching the B bit, get your fingers crossed...\n",
	      DPMI_show_state(scp));
	SetSelector(_ss, Segments[_ss >> 3].base_addr, Segments[_ss >> 3].limit,
	  1, Segments[_ss >> 3].type, Segments[_ss >> 3].readonly,
	  Segments[_ss >> 3].is_big,
	  Segments[_ss >> 3].not_present, Segments[_ss >> 3].useable
	  );
	return ret;
#endif
      }
    } else if (_trapno == 0x0e) {
      if (msdos_ldt_pagefault(scp))
        return ret;
    }
    do_cpu_exception(scp);
  }

  if (dpmi_is_cli && isset_IF())
    dpmi_is_cli = 0;
  return ret;
}

int dpmi_fault(sigcontext_t *scp)
{
  /* If this is an exception 0x11, we have to ignore it. The reason is that
   * under real DOS the AM bit of CR0 is not set.
   * Also clear the AC flag to prevent it from re-occuring.
   */
  if (_trapno == 0x11) {
    g_printf("Exception 0x11 occured, clearing AC\n");
    _eflags &= ~AC;
    return DPMI_RET_CLIENT;
  }

  return DPMI_RET_FAULT;	// process the rest in dosemu context
}

void dpmi_realmode_hlt(unsigned int lina)
{
  sigcontext_t *scp;
  if (!in_dpmi) {
    D_printf("ERROR: DPMI call while not in dpmi!\n");
    LWORD(eip)++;
    return;
  }
  scp = &DPMI_CLIENT.stack_frame;
#ifdef TRACE_DPMI
  if ((debug_level('t')==0)||(lina!=DPMI_ADD + HLT_OFF(DPMI_return_from_dos)))
#endif
  D_printf("DPMI: realmode hlt: %#x, in_dpmi=%i\n", lina, in_dpmi);
  if (lina == DPMI_ADD + HLT_OFF(DPMI_return_from_dos)) {

#ifdef TRACE_DPMI
    if ((debug_level('t')==0)||(lina!=DPMI_ADD + HLT_OFF(DPMI_return_from_dos)))
#endif
    D_printf("DPMI: Return from DOS Interrupt without register translation\n");
    restore_rm_regs();
    dpmi_set_pm(1);

  } else if (lina == DPMI_ADD + HLT_OFF(DPMI_return_from_rmint)) {
    D_printf("DPMI: Return from RM Interrupt\n");
    rm_to_pm_regs(&DPMI_CLIENT.stack_frame, ~0);
    restore_rm_regs();
    dpmi_set_pm(1);

  } else if (lina >= DPMI_ADD + HLT_OFF(DPMI_return_from_realmode) &&
      lina < DPMI_ADD + HLT_OFF(DPMI_return_from_realmode) +
      DPMI_MAX_CLIENTS) {
    int i = lina - (DPMI_ADD + HLT_OFF(DPMI_return_from_realmode));
    D_printf("DPMI: Return from Real Mode Procedure, clnt=%i\n", i);
#ifdef SHOWREGS
    show_regs();
#endif
    post_rm_call(i);
    scp = &DPMI_CLIENT.stack_frame;	// refresh after post_rm_call()
    /* remove passed arguments */
    LWORD(esp) += 2 * _LWORD(ecx);
    DPMI_save_rm_regs(SEL_ADR_X(_es, _edi));
    restore_rm_regs();
    dpmi_set_pm(1);

  } else if (lina == DPMI_ADD + HLT_OFF(DPMI_return_from_dos_memory)) {
    unsigned long length, base;
    unsigned short begin_selector, num_descs;
    int i;

    D_printf("DPMI: Return from DOS memory service, CARRY=%d, AX=0x%04X, BX=0x%04x, DX=0x%04x\n",
	     LWORD(eflags) & CF, LWORD(eax), LWORD(ebx), LWORD(edx));

    dpmi_set_pm(1);

    begin_selector = LO_WORD(_edx);

    _eflags &= ~CF;
    if(LWORD(eflags) & CF) {	/* error */
	switch (LO_WORD(_eax)) {
	  case 0x100:
	    ResizeDescriptorBlock(&DPMI_CLIENT.stack_frame,
		begin_selector, 0);
	    break;

	  case 0x102:
	    if (!ResizeDescriptorBlock(&DPMI_CLIENT.stack_frame,
		begin_selector, LO_WORD(_ebx) << 4))
		error("Unable to resize descriptor block\n");
	}
	if (LO_WORD(_eax) != 0x101)
	    LO_WORD(_ebx) = LWORD(ebx); /* max para avail */
	LO_WORD(_eax) = LWORD(eax); /* get error code */
	_eflags |= CF;
	goto done;
    }

    base = 0;
    length = 0;
    switch (LO_WORD(_eax)) {
      case 0x0100:	/* allocate block */
	LO_WORD(_eax) = LWORD(eax);
	base = LWORD(eax) << 4;
	length = GetSegmentLimit(begin_selector) + 1;
	LO_WORD(_ebx) = length >> 4;
	num_descs = (length ? (DPMI_CLIENT.is_32 ? 1 : (length/0x10000 +
					((length%0x10000) ? 1 : 0))) : 0);
	for (i = 0; i < num_descs; i++) {
	    if (SetSegmentBaseAddress(begin_selector + (i<<3), base+i*0x10000))
		error("Set segment base failed\n");
	}
	break;

      case 0x0101:	/* free block */
	break;

      case 0x0102:	/* resize block */
        if (ValidAndUsedSelector(begin_selector)) {
	  length = GetSegmentLimit(begin_selector) + 1;
	}
	LO_WORD(_ebx) = length >> 4;
	break;
    }

done:
    restore_rm_regs();

  } else if (lina == DPMI_ADD + HLT_OFF(DPMI_raw_mode_switch_rm)) {
    if (!Segments[LWORD(esi) >> 3].used) {
      error("DPMI: PM switch to unused segment %x\n", LWORD(esi));
      leavedos(61);
    }
    D_printf("DPMI: switching from real to protected mode\n");
#ifdef SHOWREGS
    show_regs();
#endif
    dpmi_set_pm(1);
    if (DPMI_CLIENT.is_32) {
      _eip = REG(edi);
      _esp = REG(ebx);
    } else {
      _eip = LWORD(edi);
      _esp = LWORD(ebx);
    }
    _cs	 = LWORD(esi);
    _ss	 = LWORD(edx);
    _ds	 = LWORD(eax);
    _es	 = LWORD(ecx);
    _fs	 = 0;
    _gs	 = 0;
    _ebp = REG(ebp);
    _eflags = 0x0202 | (0x0dd5 & REG(eflags));
    /* zero out also the "undefined" registers? */
    _eax = 0;
    _ebx = 0;
    _ecx = 0;
    _edx = 0;
    _esi = 0;
    _edi = 0;

  } else if (lina == DPMI_ADD + HLT_OFF(DPMI_save_restore_rm)) {
    unsigned int buf = SEGOFF2LINEAR(SREG(es), LWORD(edi));
    unsigned short *buffer = LINEAR2UNIX(buf);
    if (LO(ax)==0) {
      D_printf("DPMI: save protected mode registers\n");
      *(unsigned long *)buffer = _eflags, buffer += 2;
      *(unsigned long *)buffer = _eax, buffer += 2;
      *(unsigned long *)buffer = _ebx, buffer += 2;
      *(unsigned long *)buffer = _ecx, buffer += 2;
      *(unsigned long *)buffer = _edx, buffer += 2;
      *(unsigned long *)buffer = _esi, buffer += 2;
      *(unsigned long *)buffer = _edi, buffer += 2;
      *(unsigned long *)buffer = _esp, buffer += 2;
      *(unsigned long *)buffer = _ebp, buffer += 2;
      *(unsigned long *)buffer = _eip, buffer += 2;
      /* 10 regs, 40 bytes */
      *buffer++ = _cs;
      *buffer++ = _ds;
      *buffer++ = _ss;
      *buffer++ = _es;
      *buffer++ = _fs;
      *buffer++ = _gs;
      /* 6 more regs, +12 bytes, 52 bytes total.
       * note that the buffer should not exceed 60 bytes
       * so the segment regs are saved as 2-bytes. */
    } else {
      D_printf("DPMI: restore protected mode registers\n");
      _eflags = *(unsigned long *)buffer, buffer += 2;
      _eax = *(unsigned long *)buffer, buffer += 2;
      _ebx = *(unsigned long *)buffer, buffer += 2;
      _ecx = *(unsigned long *)buffer, buffer += 2;
      _edx = *(unsigned long *)buffer, buffer += 2;
      _esi = *(unsigned long *)buffer, buffer += 2;
      _edi = *(unsigned long *)buffer, buffer += 2;
      _esp = *(unsigned long *)buffer, buffer += 2;
      _ebp = *(unsigned long *)buffer, buffer += 2;
      _eip = *(unsigned long *)buffer, buffer += 2;
      /* 10 regs, 40 bytes */
      _cs =  *buffer++;
      _ds =  *buffer++;
      _ss =  *buffer++;
      _es =  *buffer++;
      _fs =  *buffer++;
      _gs =  *buffer++;
      /* 6 more regs, +12 bytes, 52 bytes total.
       * note that the buffer should not exceed 60 bytes
       * so the segment regs are saved as 2-bytes. */
    }
    REG(eip) += 1;            /* skip halt to point to FAR RET */

  } else if (lina == DPMI_ADD + HLT_OFF(DPMI_int1c)) {
    REG(eip) += 1;
    run_pm_dos_int(0x1c);
  } else if (lina == DPMI_ADD + HLT_OFF(DPMI_int23)) {
    REG(eip) += 1;
    run_pm_dos_int(0x23);
  } else if (lina == DPMI_ADD + HLT_OFF(DPMI_int24)) {
    REG(eip) += 1;
    run_pm_dos_int(0x24);

  } else if (lina == DPMI_ADD + HLT_OFF(DPMI_return_from_dosext)) {
    D_printf("DPMI: Return from DOS for registers translation\n");
    dpmi_set_pm(1);
    _esp -= sizeof(struct RealModeCallStructure);
    DPMI_save_rm_regs(SEL_ADR(_ss, _esp));
    restore_rm_regs();

  } else {
    D_printf("DPMI: unhandled HLT: lina=%#x\n", lina);
  }
}

int DPMIValidSelector(unsigned short selector)
{
  /* does this selector refer to the LDT? */
  return Segments[selector >> 3].used != 0xfe && (selector & 4);
}

uint8_t *dpmi_get_ldt_buffer(void)
{
    return ldt_buffer;
}

int dpmi_segment_is32(int sel)
{
  return (Segments[sel >> 3].is_32);
}

#ifdef USE_MHPDBG   /* dosdebug support */

int dpmi_mhp_regs(void)
{
  sigcontext_t *scp;
  if (!in_dpmi || !in_dpmi_pm()) return 0;
  scp=&DPMI_CLIENT.stack_frame;
  mhp_printf("\nEAX: %08x EBX: %08x ECX: %08x EDX: %08x eflags: %08x",
     _eax, _ebx, _ecx, _edx, _eflags);
  mhp_printf("\nESI: %08x EDI: %08x EBP: %08x", _esi, _edi, _ebp);
  mhp_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n", _ds, _es, _fs, _gs);
  mhp_printf(" CS:EIP= %04x:%08x SS:ESP= %04x:%08x\n", _cs, _eip, _ss, _esp);
  return 1;
}

void dpmi_mhp_getcseip(unsigned int *seg, unsigned int *off)
{
  sigcontext_t *scp = &DPMI_CLIENT.stack_frame;

  *seg = _cs;
  *off = _eip;
}

void dpmi_mhp_modify_eip(int delta)
{
  sigcontext_t *scp = &DPMI_CLIENT.stack_frame;
  _eip +=delta;
}

void dpmi_mhp_getssesp(unsigned int *seg, unsigned int *off)
{
  sigcontext_t *scp = &DPMI_CLIENT.stack_frame;

  *seg = _ss;
  *off = _esp;
}

int dpmi_mhp_getcsdefault(void)
{
  sigcontext_t *scp = &DPMI_CLIENT.stack_frame;
  return dpmi_segment_is32(_cs);
}

void dpmi_mhp_GetDescriptor(unsigned short selector, unsigned int *lp)
{
  memcpy(lp, &ldt_buffer[selector & 0xfff8], 8);
}

unsigned long dpmi_mhp_getreg(regnum_t regnum)
{
  sigcontext_t *scp;

  assert(in_dpmi && in_dpmi_pm());

  scp = &DPMI_CLIENT.stack_frame;
  switch (regnum) {
    case _SSr: return _ss;
    case _CSr: return _cs;
    case _DSr: return _ds;
    case _ESr: return _es;
    case _FSr: return _fs;
    case _GSr: return _gs;
    case _AXr: return _eax;
    case _BXr: return _ebx;
    case _CXr: return _ecx;
    case _DXr: return _edx;
    case _SIr: return _esi;
    case _DIr: return _edi;
    case _BPr: return _ebp;
    case _SPr: return _esp;
    case _IPr: return _eip;
    case _FLr: return _eflags;
    case _EAXr: return _eax;
    case _EBXr: return _ebx;
    case _ECXr: return _ecx;
    case _EDXr: return _edx;
    case _ESIr: return _esi;
    case _EDIr: return _edi;
    case _EBPr: return _ebp;
    case _ESPr: return _esp;
    case _EIPr: return _eip;
  }

  assert(0);
  return -1; // keep compiler happy, control never reaches here
}

void dpmi_mhp_setreg(regnum_t regnum, unsigned long val)
{
  sigcontext_t *scp;

  assert(in_dpmi && in_dpmi_pm());

  scp = &DPMI_CLIENT.stack_frame;
  switch (regnum) {
    case _SSr: _ss = val; break;
    case _CSr: _cs = val; break;
    case _DSr: _ds = val; break;
    case _ESr: _es = val; break;
    case _FSr: _fs = val; break;
    case _GSr: _gs = val; break;
    case _AXr: _eax = val; break;
    case _BXr: _ebx = val; break;
    case _CXr: _ecx = val; break;
    case _DXr: _edx = val; break;
    case _SIr: _esi = val; break;
    case _DIr: _edi = val; break;
    case _BPr: _ebp = val; break;
    case _SPr: _esp = val; break;
    case _IPr: _eip = val; break;
    case _FLr: _eflags = (_eflags & ~0x0dd5) | (val & 0x0dd5); break;
    case _EAXr: _eax = val; break;
    case _EBXr: _ebx = val; break;
    case _ECXr: _ecx = val; break;
    case _EDXr: _edx = val; break;
    case _ESIr: _esi = val; break;
    case _EDIr: _edi = val; break;
    case _EBPr: _ebp = val; break;
    case _ESPr: _esp = val; break;
    case _EIPr: _eip = val; break;

    default:
      assert(0);
  }
}

static int dpmi_mhp_intxx_pending(sigcontext_t *scp, int intno)
{
  if (dpmi_mhp_intxxtab[intno] & 1) {
    if (dpmi_mhp_intxxtab[intno] & 0x80) {
      if (mhp_getaxlist_value( (_eax & 0xFFFF) | (intno<<16), -1) <0) return -1;
    }
    return 1;
  }
  return 0;
}

static int dpmi_mhp_intxx_check(sigcontext_t *scp, int intno)
{
  switch (dpmi_mhp_intxx_pending(scp,intno)) {
    case -1: return DPMI_RET_CLIENT;
    case 1:
      dpmi_mhp_intxxtab[intno] &=~1;
      return DPMI_RET_INT+intno;
    default:
      if (!(dpmi_mhp_intxxtab[intno] & 2)) dpmi_mhp_intxxtab[intno] |=3;
      return DPMI_RET_CLIENT;
  }
}

int dpmi_mhp_setTF(int on)
{
  sigcontext_t *scp;
  if (!in_dpmi_pm())
    return 0;
  scp=&DPMI_CLIENT.stack_frame;
  if (on) _eflags |=TF;
  else _eflags &=~TF;
  return 1;
}

#endif /* dosdebug support */

void add_cli_to_blacklist(void)
{
  if (!dpmi_is_cli || !in_dpmi_pm())
    return;
  if (cli_blacklisted < CLI_BLACKLIST_LEN) {
    if (debug_level('M') > 5)
      D_printf("DPMI: adding cli to blacklist: lina=%p\n", current_cli);
    cli_blacklist[cli_blacklisted++] = current_cli;
  }
  else
    D_printf("DPMI: Warning: cli blacklist is full!\n");
  dpmi_is_cli = 0;
}

static int find_cli_in_blacklist(unsigned char * cur_cli)
{
  int i;
  if (debug_level('M') > 8)
    D_printf("DPMI: searching blacklist (%d elements) for cli (lina=%p)\n",
      cli_blacklisted, cur_cli);
  for (i=0; i<cli_blacklisted; i++) {
    if (cli_blacklist[i] == cur_cli)
      return 1;
  }
  return 0;
}

void dpmi_return_request(void)
{
  return_requested = 1;
}

int dpmi_check_return(void)
{
  return return_requested ? DPMI_RET_DOSEMU : DPMI_RET_CLIENT;
}

int in_dpmi_pm(void)
{
  return dpmi_pm;
}

int dpmi_active(void)
{
  return in_dpmi;
}

void dpmi_done(void)
{
  D_printf("DPMI: finalizing\n");

  current_client = in_dpmi - 1;
  while (in_dpmi) {
    if (in_dpmi_pm())
      dpmi_set_pm(0);
    dpmi_cleanup();
  }

  if (in_dpmi_thr && !dpmi_thr_running)
    co_delete(dpmi_tid);
  co_thread_cleanup(co_handle);

  DPMI_freeAll(&host_pm_block_root);
  dpmi_free_pool();
}

/* for debug only */
sigcontext_t *dpmi_get_scp(void)
{
  if (!in_dpmi)
    return NULL;
  return &DPMI_CLIENT.stack_frame;
}

static uint16_t decode_selector(sigcontext_t *scp)
{
    int done, pref_seg;
    uint8_t *csp;

    csp = (uint8_t *) SEL_ADR(_cs, _eip);
    done = 0;
    pref_seg = -1;

    do {
      switch (*(csp++)) {
         case 0x66:      /* operand prefix */  /* prefix66=1; */ break;
         case 0x67:      /* address prefix */  /* prefix67=1; */ break;
         case 0x2e:      /* CS */              pref_seg=_cs; break;
         case 0x3e:      /* DS */              pref_seg=_ds; break;
         case 0x26:      /* ES */              pref_seg=_es; break;
         case 0x36:      /* SS */              pref_seg=_ss; break;
         case 0x65:      /* GS */              pref_seg=_gs; break;
         case 0x64:      /* FS */              pref_seg=_fs; break;
         case 0xf2:      /* repnz */
         case 0xf3:      /* rep */             /* is_rep=1; */ break;
         default: done=1;
      }
    } while (!done);

    if (pref_seg == -1)
	return _ds;	// may be also _ss
    return pref_seg;
}

char *DPMI_show_state(sigcontext_t *scp)
{
    static char buf[4096];
    int pos = 0;
    unsigned char *csp2, *ssp2;
    dosaddr_t daddr, saddr;
    pos += sprintf(buf + pos, "eip: 0x%08x  esp: 0x%08x  eflags: 0x%08x\n"
	     "\ttrapno: 0x%02x  errorcode: 0x%08x  cr2: 0x%08"PRI_RG"\n"
	     "\tcs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x  fs: 0x%04x  gs: 0x%04x\n",
	     _eip, _esp, _eflags, _trapno, _err, _cr2, _cs, _ds, _es, _ss, _fs, _gs);
    pos += sprintf(buf + pos, "EAX: %08x  EBX: %08x  ECX: %08x  EDX: %08x\n",
	     _eax, _ebx, _ecx, _edx);
    pos += sprintf(buf + pos, "ESI: %08x  EDI: %08x  EBP: %08x\n",
	     _esi, _edi, _ebp);
    /* display the 10 bytes before and after CS:EIP.  the -> points
     * to the byte at address CS:EIP
     */
    if (!((_cs) & 0x0004)) {
      /* GTD */
#if 0
      csp2 = (unsigned char *) _rip;
      daddr = 0;
#else
      return buf;
#endif
    }
    else {
      /* LDT */
      csp2 = SEL_ADR(_cs, _eip);
      daddr = GetSegmentBase(_cs) + D_16_32(_eip);
    }
    /* We have a problem here, if we get a page fault or any kind of
     * 'not present' error and then we try accessing the code/stack
     * area, we fall into another fault which likely terminates dosemu.
     */
#ifdef X86_EMULATOR
    if (config.cpu_vm != CPUVM_EMU || (_trapno!=0x0b && _trapno!=0x0c))
#endif
    {
      int i;
      #define CSPP (csp2 - 10)
      pos += sprintf(buf + pos, "OPS  : ");
      if ((CSPP >= &mem_base[0] && CSPP + 10 < &mem_base[0x110000]) ||
	  ((mapping_find_hole((uintptr_t)CSPP, (uintptr_t)CSPP + 10, 1) == MAP_FAILED) &&
	   dpmi_is_valid_range(daddr - 10, 10))) {
	for (i = 0; i < 10; i++)
	  pos += sprintf(buf + pos, "%02x ", CSPP[i]);
      } else {
	pos += sprintf(buf + pos, "<invalid memory> ");
      }
      if ((csp2 >= &mem_base[0] && csp2 + 10 < &mem_base[0x110000]) ||
	  ((mapping_find_hole((uintptr_t)csp2, (uintptr_t)csp2 + 10, 1) == MAP_FAILED) &&
	   dpmi_is_valid_range(daddr, 10))) {
	pos += sprintf(buf + pos, "-> ");
	for (i = 0; i < 10; i++)
	  pos += sprintf(buf + pos, "%02x ", *csp2++);
	pos += sprintf(buf + pos, "\n");
      } else {
	pos += sprintf(buf + pos, "CS:EIP points to invalid memory\n");
      }
      if (!((_ss) & 0x0004)) {
        /* GDT */
#if 0
        ssp2 = (unsigned char *) _rsp;
        saddr = 0;
#else
        return buf;
#endif
      }
      else {
        /* LDT */
	ssp2 = SEL_ADR(_ss, _esp);
	saddr = GetSegmentBase(_ss) + D_16_32(_esp);
      }
      #define SSPP (ssp2 - 10)
      pos += sprintf(buf + pos, "STACK: ");
      if ((SSPP >= &mem_base[0] && SSPP + 10 < &mem_base[0x110000]) ||
	  ((mapping_find_hole((uintptr_t)SSPP, (uintptr_t)SSPP + 10, 1) == MAP_FAILED) &&
	   dpmi_is_valid_range(saddr - 10, 10))) {
	for (i = 0; i < 10; i++)
	  pos += sprintf(buf + pos, "%02x ", SSPP[i]);
      } else {
	pos += sprintf(buf + pos, "<invalid memory> ");
      }
      if ((ssp2 >= &mem_base[0] && ssp2 + 10 < &mem_base[0x110000]) ||
	  ((mapping_find_hole((uintptr_t)ssp2, (uintptr_t)ssp2 + 10, 1) == MAP_FAILED) &&
	   dpmi_is_valid_range(saddr, 10))) {
	pos += sprintf(buf + pos, "-> ");
	for (i = 0; i < 10; i++)
	  pos += sprintf(buf + pos, "%02x ", *ssp2++);
	pos += sprintf(buf + pos, "\n");
      } else {
	pos += sprintf(buf + pos, "SS:ESP points to invalid memory\n");
      }
    }

    if (_trapno == 0xd && _err == 0) {
      const char *msd_dsc;
      uint16_t sel = decode_selector(scp);
      pos += sprintf(buf + pos, "GPF on selector 0x%x base=%08x lim=%x\n",
          sel, GetSegmentBase(sel), GetSegmentLimit(sel));
#if WITH_DPMI
      msd_dsc = msdos_describe_selector(sel);
      if (msd_dsc)
        pos += sprintf(buf + pos, "MSDOS selector: %s\n", msd_dsc);
#endif
    }

    return buf;
}
