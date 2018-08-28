/* this is for the DPMI support */
#ifndef DPMI_H
#define DPMI_H

#define WITH_DPMI 1

#if WITH_DPMI

#include "emu-ldt.h"
#include "emm.h"
#include "dmemory.h"

#define DPMI_VERSION   		0x00	/* major version 0 */
#define DPMI_DRIVER_VERSION	0x5a	/* minor version 0.90 */

#define DPMI_MAX_CLIENTS	32	/* maximal number of clients */
#define DPMI_MAX_RMCBS		32

#define DPMI_page_size		4096	/* 4096 bytes per page */

#define DPMI_pm_stack_size	0xf000	/* locked protected mode stack for exceptions, */
					/* hardware interrupts, software interrups 0x1c, */
					/* 0x23, 0x24 and real mode callbacks */

#define DPMI_rm_stacks		6	/* RM stacks per DPMI client */
#define DPMI_max_rec_rm_func	(DPMI_MAX_CLIENTS * DPMI_rm_stacks)	/* max number of recursive real mode functions */
#define DPMI_rm_stack_size	0x0200	/* real mode stack size */

#define DPMI_private_paragraphs	((DPMI_rm_stacks * DPMI_rm_stack_size)>>4)
					/* private data for DPMI server */

#ifdef __linux__
int modify_ldt(int func, void *ptr, unsigned long bytecount);
#define LDT_WRITE 0x11
#endif

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
    unsigned int	usable:1;
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

struct DPMIclient_struct {
  sigcontext_t stack_frame;
  int is_32;
  dpmi_pm_block_root pm_block_root;
  unsigned short private_data_segment;
  int in_dpmi_rm_stack;
  dpmi_pm_block *pm_stack;
  int in_dpmi_pm_stack;
  /* for real mode call back, DPMI function 0x303 0x304 */
  RealModeCallBack realModeCallBack[DPMI_MAX_RMCBS];
  Bit16u rmcb_seg;
  Bit16u rmcb_off;
  INTDESC Interrupt_Table[0x100];
  INTDESC Exception_Table[0x20];
  unsigned short PMSTACK_SEL;	/* protected mode stack selector */
  /* used for RSP calls */
  unsigned short RSP_cs[DPMI_MAX_CLIENTS], RSP_ds[DPMI_MAX_CLIENTS];
  int RSP_state, RSP_installed;
  int ext__thunk_16_32;	// thunk extension
};

struct RSPcall_s {
  unsigned char data16[8];
  unsigned char code16[8];
  unsigned short ip;
  unsigned short reserved;
  unsigned char data32[8];
  unsigned char code32[8];
  unsigned int eip;
};
struct RSP_s {
  struct RSPcall_s call;
  dpmi_pm_block_root pm_block_root;
};

enum { DPMI_RET_FAULT=-3, DPMI_RET_EXIT=-2, DPMI_RET_DOSEMU=-1,
       DPMI_RET_CLIENT=0, DPMI_RET_TRAP_DB=1, DPMI_RET_TRAP_BP=3,
       DPMI_RET_INT=0x100 };

extern unsigned char dpmi_mhp_intxxtab[256];

extern unsigned long dpmi_total_memory; /* total memory  of this session */
extern unsigned long dpmi_free_memory; /* how many bytes memory client */
				       /* can allocate */
extern unsigned long pm_block_handle_used;       /* tracking handle */
extern unsigned char *ldt_buffer;

typedef enum {
   _SSr, _CSr, _DSr, _ESr, _FSr, _GSr,
   _AXr, _BXr, _CXr, _DXr, _SIr, _DIr, _BPr, _SPr, _IPr, _FLr,
  _EAXr,_EBXr,_ECXr,_EDXr,_ESIr,_EDIr,_EBPr,_ESPr,_EIPr
} regnum_t;

void dpmi_get_entry_point(void);
#ifdef __x86_64__
extern void dpmi_iret_setup(sigcontext_t *scp);
extern void dpmi_iret_unwind(sigcontext_t *scp);
#else
#define dpmi_iret_setup(x)
#endif
#ifdef __linux__
int dpmi_fault(sigcontext_t *scp);
#endif
void dpmi_realmode_hlt(unsigned int lina);
void run_pm_int(int inum);
void fake_pm_int(void);
int in_dpmi_pm(void);
int dpmi_active(void);

#ifdef __linux__
int dpmi_mhp_regs(void);
void dpmi_mhp_getcseip(unsigned int *seg, unsigned int *off);
void dpmi_mhp_getssesp(unsigned int *seg, unsigned int *off);
int dpmi_segment_is32(int sel);
int dpmi_mhp_getcsdefault(void);
int dpmi_mhp_setTF(int on);
void dpmi_mhp_GetDescriptor(unsigned short selector, unsigned int *lp);
unsigned long dpmi_mhp_getreg(regnum_t regnum);
void dpmi_mhp_setreg(regnum_t regnum, unsigned long val);
void dpmi_mhp_modify_eip(int delta);
#endif

void add_cli_to_blacklist(void);
dpmi_pm_block DPMImalloc(unsigned long size);
dpmi_pm_block DPMImallocLinear(unsigned long base, unsigned long size, int committed);
int DPMIfree(unsigned long handle);
dpmi_pm_block DPMIrealloc(unsigned long handle, unsigned long size);
dpmi_pm_block DPMIreallocLinear(unsigned long handle, unsigned long size,
  int committed);
void DPMIfreeAll(void);
int DPMIMapConventionalMemory(unsigned long handle, unsigned long offset,
			  unsigned long low_addr, unsigned long cnt);
int DPMISetPageAttributes(unsigned long handle, int offs, u_short attrs[], int count);
int DPMIGetPageAttributes(unsigned long handle, int offs, u_short attrs[], int count);
void GetFreeMemoryInformation(unsigned int *lp);
int GetDescriptor(u_short selector, unsigned int *lp);
unsigned int GetSegmentBase(unsigned short sel);
unsigned int GetSegmentLimit(unsigned short sel);
int CheckSelectors(sigcontext_t *scp, int in_dosemu);
int ValidAndUsedSelector(unsigned short selector);
int dpmi_is_valid_range(dosaddr_t addr, int len);

extern char *DPMI_show_state(sigcontext_t *scp);
extern void dpmi_sigio(sigcontext_t *scp);

extern int ConvertSegmentToDescriptor(unsigned short segment);
extern int SetSegmentBaseAddress(unsigned short selector,
					unsigned long baseaddr);
extern int SetSegmentLimit(unsigned short, unsigned int);
extern DPMI_INTDESC dpmi_get_interrupt_vector(unsigned char num);
extern void dpmi_set_interrupt_vector(unsigned char num, DPMI_INTDESC desc);
extern far_t DPMI_get_real_mode_interrupt_vector(int vec);
extern int DPMI_allocate_specific_ldt_descriptor(unsigned short selector);
extern unsigned short AllocateDescriptors(int);
extern unsigned short CreateAliasDescriptor(unsigned short selector);
extern int SetDescriptorAccessRights(unsigned short selector, unsigned short acc_rights);
extern int SetSelector(unsigned short selector, dosaddr_t base_addr, unsigned int limit,
                       unsigned char is_32, unsigned char type, unsigned char readonly,
                       unsigned char is_big, unsigned char seg_not_present, unsigned char usable);
extern int SetDescriptor(unsigned short selector, unsigned int *lp);
extern int FreeDescriptor(unsigned short selector);
extern void FreeSegRegs(sigcontext_t *scp, unsigned short selector);
extern far_t DPMI_allocate_realmode_callback(u_short sel, int offs, u_short rm_sel,
	int rm_offs);
extern int DPMI_free_realmode_callback(u_short seg, u_short off);
extern int DPMI_get_save_restore_address(far_t *raddr, struct pmaddr_s *paddr);

extern void dpmi_setup(void);
extern void dpmi_reset(void);
extern void dpmi_done(void);
extern int get_ldt(void *buffer);
void dpmi_return(sigcontext_t *scp, int retcode);
void dpmi_return_request(void);
int dpmi_check_return(void);
void dpmi_init(void);
extern void copy_context(sigcontext_t *d,
    sigcontext_t *s, int copy_fpu);
extern unsigned short dpmi_sel(void);
unsigned long dpmi_mem_size(void);
void dpmi_set_mem_bases(void *rsv_base, void *main_base);
void dump_maps(void);

int DPMIValidSelector(unsigned short selector);
uint8_t *dpmi_get_ldt_buffer(void);

sigcontext_t *dpmi_get_scp(void);

#else

static inline void dpmi_realmode_hlt(unsigned int lina)
{
    leavedos(1);
}

static inline int dpmi_is_valid_range(dosaddr_t addr, int len)
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

static inline void dpmi_set_mem_bases(void *rsv_base, void *main_base)
{
}

static inline unsigned long dpmi_mem_size(void)
{
    return 0;
}

static inline int dpmi_fault(sigcontext_t *scp)
{
    return 0;
}

static inline int dpmi_check_return(sigcontext_t *scp)
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

static inline sigcontext_t *dpmi_get_scp(void)
{
    return NULL;
}

static inline void dpmi_sigio(sigcontext_t *scp)
{
}

static inline unsigned int GetSegmentLimit(unsigned short sel)
{
    return 0;
}

static inline int dpmi_segment_is32(int sel)
{
    return 0;
}

static inline void add_cli_to_blacklist(void)
{
}

static inline void dpmi_get_entry_point(void)
{
}

static inline void dpmi_iret_unwind(sigcontext_t *scp)
{
}

static inline char *DPMI_show_state(sigcontext_t *scp)
{
    return "";
}

static inline void dpmi_iret_setup(sigcontext_t *scp)
{
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

#endif

#endif /* DPMI_H */
