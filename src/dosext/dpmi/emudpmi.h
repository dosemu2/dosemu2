/* this is for the DPMI support */
#ifndef EMUDPMI_H
#define EMUDPMI_H

#define WITH_DPMI 1

#if WITH_DPMI

#if 0
#define DPMI_VERSION   		0x00	/* major version 0 */
#define DPMI_DRIVER_VERSION	0x5a	/* minor version 0.90 */
#else
#define DPMI_VERSION   		1	/* major version 1 */
#define DPMI_MINOR_VERSION	0	/* minor version 0 */
#endif

#define DPMI_MAX_CLIENTS	32	/* maximal number of clients */

#ifndef __ASSEMBLER__

#include <stdarg.h>
#include "cpu.h"
#include "emu-ldt.h"
#include "emm.h"
#include "dmemory.h"
#define DPMI_MAX_RMCBS		32

#define DPMI_page_size		4096	/* 4096 bytes per page */

#define DPMI_pm_stack_size	0xf000	/* locked protected mode stack for exceptions, */
					/* hardware interrupts, software interrups 0x1c, */
					/* 0x23, 0x24 and real mode callbacks */

#define DPMI_rm_stacks		6	/* RM stacks per DPMI client */
#define DPMI_rm_stack_size	0x0200	/* real mode stack size */

#define DPMI_private_paragraphs	((DPMI_rm_stacks * DPMI_rm_stack_size)>>4)
					/* private data for DPMI server */

#ifdef __linux__
int modify_ldt(int func, void *ptr, unsigned long bytecount);
#endif
#define LDT_READ      0
#define LDT_WRITE_OLD 1
#define LDT_WRITE     0x11

/* this is used like: SEL_ADR(_ss, _esp) */
void *SEL_ADR(unsigned short sel, unsigned int reg);
void *SEL_ADR_CLNT(unsigned short sel, unsigned int reg, int is_32);

typedef struct pmaddr_s
{
    unsigned int	offset;
    unsigned short	selector;
} __attribute__((packed)) INTDESC;
typedef struct
{
    unsigned int	offset32;
    unsigned short	selector;
} __attribute__((packed)) DPMI_INTDESC;

typedef struct segment_descriptor_s
{
    unsigned int	base_addr;	/* Pointer to segment in flat memory */
    unsigned int	limit;		/* Limit of Segment */
    unsigned int	type:2;
    unsigned int	is_32:1;	/* one for is 32-bit Segment */
    unsigned int	readonly:1;	/* one for read only Segments */
    unsigned int	is_big:1;	/* Granularity */
    unsigned int	not_present:1;
    unsigned int	useable:1;
    unsigned int	cstd:1;		/* from Convert Seg to Desc */
    unsigned int	used;		/* Segment in use by client # */
					/* or Linux/GLibc (0xfe) */
					/* or DOSEMU (0xff) */
} SEGDESC;

struct sel_desc_s {
  unsigned short selector;
  unsigned int descriptor[2];
} __attribute__((packed));

struct RealModeCallStructure {
  unsigned int edi;
  unsigned int esi;
  unsigned int ebp;
  unsigned int esp_reserved;
  unsigned int ebx;
  unsigned int edx;
  unsigned int ecx;
  unsigned int eax;
  unsigned short flags;
  unsigned short es;
  unsigned short ds;
  unsigned short fs;
  unsigned short gs;
  unsigned short ip;
  unsigned short cs;
  unsigned short sp;
  unsigned short ss;
} __attribute__((packed));

typedef struct {
    unsigned short selector;
    unsigned int  offset;
    unsigned short rmreg_selector;
    unsigned int  rmreg_offset;
    struct RealModeCallStructure *rmreg;
    unsigned rm_ss_selector;
} RealModeCallBack;

struct SHM_desc {
  uint32_t req_len;
  uint32_t ret_len;
  uint32_t handle;
  uint32_t addr;
  uint32_t name_offset32;
  uint16_t name_selector;
#define SHM_NOEXEC 1
#define SHM_EXCL 2
#define SHM_NEW_NS 4
#define SHM_NS 8
  uint16_t flags;
  uint32_t opaque;
};

enum { DPMI_RET_FAULT=-3, DPMI_RET_EXIT=-2, DPMI_RET_DOSEMU=-1,
       DPMI_RET_CLIENT=0, DPMI_RET_TRAP_DB=1, DPMI_RET_TRAP_BP=3,
       DPMI_RET_INT=0x100 };

extern unsigned char dpmi_mhp_intxxtab[256];

extern unsigned int dpmi_total_memory; /* total memory  of this session */
extern unsigned int pm_block_handle_used;       /* tracking handle */
extern unsigned char *ldt_buffer;

typedef enum {
   _SSr, _CSr, _DSr, _ESr, _FSr, _GSr,
   _AXr, _BXr, _CXr, _DXr, _SIr, _DIr, _BPr, _SPr, _IPr, _FLr,
  _EAXr,_EBXr,_ECXr,_EDXr,_ESIr,_EDIr,_EBPr,_ESPr,_EIPr
} regnum_t;

void dpmi_get_entry_point(void);
void dpmi_realmode_hlt(unsigned int lina);
void run_pm_int(int inum);
void fake_pm_int(void);
int in_dpmi_pm(void);
int dpmi_active(void);
int dpmi_segment_is32(int sel);
int dpmi_is_32(void);
int dpmi_isset_IF(void);
#define isset_IF_async() (in_dpmi_pm() ? dpmi_isset_IF() : isset_IF())
int p_direct_vstr(const char *fmt, va_list args);

/* currently eflags are stored with IF reflecting the actual interrupt state,
 * so no translation is needed */
#define dpmi_flags_to_stack(flags) (flags)
static inline unsigned dpmi_flags_from_stack_iret(const cpuctx_t *scp,
    unsigned flags)
{
  int iopl = ((_eflags_ & IOPL_MASK) >> IOPL_SHIFT);
  unsigned new_flags = (flags & 0xdd5) | 2 | (_eflags_ & IOPL_MASK);

  if (iopl == 3)
    new_flags |= (flags & IF);
  else
    new_flags |= (_eflags_ & IF);
  return new_flags;
}
#define flags_to_pm(flags) ((flags & (0xdd5|IF)) | 2 | (_eflags_ & IOPL_MASK))
#define flags_to_rm(flags) ((flags) | 2 | IOPL_MASK)

#ifdef USE_MHPDBG   /* dosdebug support */
int dpmi_mhp_regs(void);
void dpmi_mhp_getcseip(unsigned int *seg, unsigned int *off);
void dpmi_mhp_getssesp(unsigned int *seg, unsigned int *off);
int dpmi_mhp_getcsdefault(void);
int dpmi_mhp_setTF(int on);
int dpmi_mhp_issetTF(void);
void dpmi_mhp_GetDescriptor(unsigned short selector, unsigned int *lp);
uint32_t dpmi_mhp_getreg(regnum_t regnum);
void dpmi_mhp_setreg(regnum_t regnum, uint32_t val);
void dpmi_mhp_modify_eip(int delta);
#endif

void dpmi_timer(void);
dpmi_pm_block DPMImalloc(unsigned long size);
dpmi_pm_block DPMImallocLinear(dosaddr_t base, unsigned long size, int committed);
int DPMIfree(unsigned long handle);
dpmi_pm_block DPMIrealloc(unsigned long handle, unsigned long size);
dpmi_pm_block DPMIreallocLinear(unsigned long handle, unsigned long size,
  int committed);
int DPMIMapConventionalMemory(unsigned long handle, unsigned long offset,
			  dosaddr_t low_addr, unsigned long cnt);
int DPMISetPageAttributes(unsigned long handle, int offs, u_short attrs[], int count);
int DPMIGetPageAttributes(unsigned long handle, int offs, u_short attrs[], int count);
void GetFreeMemoryInformation(unsigned int *lp);
int GetDescriptor(u_short selector, unsigned int *lp);
unsigned int GetSegmentBase(unsigned short sel);
unsigned int GetSegmentLimit(unsigned short sel);
unsigned int GetSegmentType(unsigned short selector);
int CheckSelectors(cpuctx_t *scp, int in_dosemu);
int ValidAndUsedSelector(unsigned int selector);
int dpmi_is_valid_range(dosaddr_t addr, int len);
int dpmi_read_access(dosaddr_t addr);
int dpmi_write_access(dosaddr_t addr);

extern char *DPMI_show_state(cpuctx_t *scp);
extern void run_dpmi(void);

extern int ConvertSegmentToDescriptor(unsigned short segment);
extern int SetSegmentBaseAddress(unsigned short selector,
					dosaddr_t baseaddr);
extern int SetSegmentLimit(unsigned short, unsigned int);
extern DPMI_INTDESC dpmi_get_interrupt_vector(unsigned char num);
extern void dpmi_set_interrupt_vector(unsigned char num, DPMI_INTDESC desc);
extern DPMI_INTDESC dpmi_get_exception_handler(unsigned char num);
extern void dpmi_set_exception_handler(unsigned char num, DPMI_INTDESC desc);
extern far_t DPMI_get_real_mode_interrupt_vector(int vec);
extern DPMI_INTDESC dpmi_get_pm_exc_addr(int num);
extern void dpmi_set_pm_exc_addr(int num, DPMI_INTDESC addr);
extern int DPMI_allocate_specific_ldt_descriptor(unsigned short selector);
extern unsigned short AllocateDescriptors(int);
extern unsigned short CreateAliasDescriptor(unsigned short selector);
extern int SetDescriptorAccessRights(unsigned short selector, unsigned short acc_rights);
extern int SetSelector(unsigned short selector, dosaddr_t base_addr, unsigned int limit,
                       unsigned char is_32, unsigned char type, unsigned char readonly,
                       unsigned char is_big, unsigned char seg_not_present, unsigned char useable);
extern int SetDescriptor(unsigned short selector, unsigned int *lp);
extern int FreeDescriptor(unsigned short selector);
extern void FreeSegRegs(cpuctx_t *scp, unsigned short selector);
extern far_t DPMI_allocate_realmode_callback(u_short sel, int offs, u_short rm_sel,
	int rm_offs);
extern int DPMI_free_realmode_callback(u_short seg, u_short off);
extern int DPMI_get_save_restore_address(far_t *raddr, struct pmaddr_s *paddr);

extern int DPMIAllocateShared(struct SHM_desc *shm);
extern int DPMIFreeShared(uint32_t handle);

extern void dpmi_ext_set_ldt_monitor16(DPMI_INTDESC call, uint16_t ds);
extern void dpmi_ext_set_ldt_monitor32(DPMI_INTDESC call, uint16_t ds);
extern void dpmi_ext_ldt_monitor_enable(int on);

extern void dpmi_setup(void);
extern void dpmi_reset(void);
extern void dpmi_done(void);
extern void dpmi_done0(void);
extern int get_ldt(void *buffer, int len);
void dpmi_init(void);
extern unsigned short dpmi_sel(void);
extern unsigned short dpmi_sel16(void);
extern unsigned short dpmi_sel32(void);
unsigned long dpmi_mem_size(void);
void dump_maps(void);

int DPMIValidSelector(unsigned short selector);
uint8_t *dpmi_get_ldt_buffer(void);

cpuctx_t *dpmi_get_scp(void);

int dpmi_realmode_exception(unsigned trapno, unsigned err, dosaddr_t cr2);

struct RSPcall_s {
  unsigned char data16[8];
  unsigned char code16[8];
  unsigned short ip;
#define RSP_F_SW 1	// switch_client extension
#define RSP_F_LOWMEM 2	// extra lowmem extension
  unsigned char flags;	// extension
  unsigned char para;	// extension
  unsigned char data32[8];
  unsigned char code32[8];
  unsigned int eip;
};

int dpmi_install_rsp(struct RSPcall_s *callback);
dosaddr_t DPMIMapHWRam(unsigned addr, unsigned size);
int DPMIUnmapHWRam(dosaddr_t vbase);

#endif // __ASSEMBLER__
#else
#define DPMI_MAX_CLIENTS 0

#ifndef __ASSEMBLER__

static inline void dpmi_realmode_hlt(unsigned int lina)
{
    leavedos(1);
}

static inline int dpmi_is_valid_range(dosaddr_t addr, int len)
{
    return 0;
}

static inline int dpmi_read_access(dosaddr_t addr)
{
    return 0;
}

static inline int dpmi_write_access(dosaddr_t addr)
{
    return 0;
}

static inline unsigned int GetSegmentBase(unsigned short sel)
{
    return 0;
}

static inline void dpmi_setup(void)
{
}

static inline void dpmi_reset(void)
{
}

static inline void dpmi_done(void)
{
}

static inline void dpmi_done0(void)
{
}

static inline int in_dpmi_pm(void)
{
    return 0;
}

static inline int dpmi_active(void)
{
    return 0;
}

static inline void dpmi_init(void)
{
}

static inline int dpmi_mhp_regs(void)
{
    return 0;
}

static inline int dpmi_mhp_setTF(int on)
{
    return 0;
}

static inline uint8_t *dpmi_get_ldt_buffer(void)
{
    return NULL;
}

static inline int get_ldt(void *buffer)
{
    return -1;
}

static inline unsigned long dpmi_mem_size(void)
{
    return 0;
}

static inline int dpmi_fault(sigcontext_t *scp)
{
    return 0;
}

static inline void *SEL_ADR(unsigned short sel, unsigned int reg)
{
    return NULL;
}

static inline void *SEL_ADR_CLNT(unsigned short sel, unsigned int reg, int is_32)
{
    return NULL;
}

static inline int DPMIValidSelector(unsigned short selector)
{
    return 0;
}

static inline cpuctx_t *dpmi_get_scp(void)
{
    return NULL;
}

static inline unsigned int GetSegmentLimit(unsigned short sel)
{
    return 0;
}

static inline unsigned int GetSegmentType(unsigned short selector)
{
    return 0;
}

static inline int dpmi_segment_is32(int sel)
{
    return 0;
}

static inline void dpmi_timer(void)
{
}

static inline void dpmi_get_entry_point(void)
{
}

static inline char *DPMI_show_state(cpuctx_t *scp)
{
    return "";
}

static inline void dpmi_return_request(void)
{
}

static inline void run_pm_int(int inum)
{
}

static inline void fake_pm_int(void)
{
}

static inline unsigned long dpmi_mhp_getreg(regnum_t regnum)
{
    return 0;
}

static inline void dpmi_mhp_setreg(regnum_t regnum, unsigned long val)
{
}

static inline void dpmi_mhp_modify_eip(int delta)
{
}

static inline void dpmi_mhp_getcseip(unsigned int *seg, unsigned int *off)
{
}

static inline void dpmi_mhp_getssesp(unsigned int *seg, unsigned int *off)
{
}

static inline int dpmi_mhp_getcsdefault(void)
{
    return 0;
}

static inline int dpmi_realmode_exception(unsigned trapno, unsigned err, dosaddr_t cr2)
{
    return 0;
}

#endif // __ASSEMBLER__
#endif

#endif /* DPMI_H */
