/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* this is for the DPMI support */
#ifndef DPMI_H
#define DPMI_H

#define DPMI_VERSION   		0x00	/* major version 0 */
#define DPMI_DRIVER_VERSION	0x5a	/* minor version 0.90 */

#define DPMI_MAX_CLIENTS	32	/* maximal number of clients */

#define DPMI_page_size		4096	/* 4096 bytes per page */

#define DPMI_pm_stack_size	0x1000	/* locked protected mode stack for exceptions, */
					/* hardware interrupts, software interrups 0x1c, */
					/* 0x23, 0x24 and real mode callbacks */

#define DPMI_max_rec_rm_func	16	/* max number of recursive real mode functions */
#define DPMI_rm_stack_size	0x0200	/* real mode stack size */

#define DPMI_private_paragraphs	((DPMI_max_rec_rm_func * DPMI_rm_stack_size)>>4)
					/* private data for DPMI server */
#define current_client (in_dpmi-1)
#define DPMI_CLIENT (DPMIclient[current_client])
#define PREV_DPMI_CLIENT (DPMIclient[current_client-1])

/* Aargh!! Is this the only way we have to know if a signal interrupted
 * us in DPMI server or client code? */
#ifdef __linux__
#define UCODESEL 0x23
#define UDATASEL 0x2b
#endif

/* DANG_BEGIN_REMARK
 * Handling of the virtual interrupt flag is still not correct and there
 * are many open questions since DPMI specifications are unclear in this
 * point.
 * An example: If IF=1 in protected mode and real mode code is called
 * which is disabling interrupts via cli and returning to protected
 * mode, is IF then still one or zero?
 * I guess I have to think a lot about this and to write a small dpmi
 * client running under a commercial dpmi server :-).
 * DANG_END_REMARK
 */

#define dpmi_cli() 	({ dpmi_eflags &= ~IF; pic_cli(); })

#define dpmi_sti() 	({ dpmi_eflags |= IF; is_cli = 0; pic_sti(); })

#ifdef __linux__
int modify_ldt(int func, void *ptr, unsigned long bytecount);
#define LDT_WRITE 0x11
#endif

/* this is used like: SEL_ADR(_ss, _esp) */
#define SEL_ADR(seg, reg) \
({ unsigned long __res; \
  if (!((seg) & 0x0004)) { \
    /* GDT */ \
    __res = (unsigned long) reg; \
  } else { \
    /* LDT */ \
    if (Segments[seg>>3].is_32) \
      __res = (unsigned long) (GetSegmentBaseAddress(seg) + reg ); \
    else \
      __res = (unsigned long) (GetSegmentBaseAddress(seg) + *((unsigned short *)&(reg)) ); \
  } \
__res; })

#define HLT_OFF(addr) ((unsigned long)addr-(unsigned long)DPMI_dummy_start)

typedef struct interrupt_descriptor_s
{
    unsigned long	offset;
    unsigned short	selector, __selectorh;
} INTDESC;

typedef struct segment_descriptor_s
{
    unsigned long	base_addr;	/* Pointer to segment in flat memory */
    unsigned int	limit;		/* Limit of Segment */
    unsigned int	type:2;
    unsigned int	is_32:1;	/* one for is 32-bit Segment */
    unsigned int	readonly:1;	/* one for read only Segments */	
    unsigned int	is_big:1;	/* Granularity */
    unsigned int	not_present:1;		
    unsigned int	useable:1;		
    unsigned int	used;		/* Segment in use by client # */
} SEGDESC;

struct RealModeCallStructure {
  unsigned long edi;
  unsigned long esi;
  unsigned long ebp;
  unsigned long esp;
  unsigned long ebx;
  unsigned long edx;
  unsigned long ecx;
  unsigned long eax;
  unsigned short flags;
  unsigned short es;
  unsigned short ds;
  unsigned short fs;
  unsigned short gs;
  unsigned short ip;
  unsigned short cs;
  unsigned short sp;
  unsigned short ss;
};

typedef struct {
    unsigned short selector;
    unsigned long  offset;
    unsigned short rmreg_selector;
    unsigned long  rmreg_offset;
    struct RealModeCallStructure *rmreg;
    unsigned rm_ss_selector;
} RealModeCallBack;

typedef struct dpmi_pm_block_stuct {
  struct   dpmi_pm_block_stuct *next;
  unsigned long handle;
  unsigned long size;
  char     *base;
} dpmi_pm_block;

struct DPMIclient_struct {
  struct sigcontext_struct stack_frame;
  int is_32;
  dpmi_pm_block *pm_block_root;
  /* for real mode call back, DPMI function 0x303 0x304 */
  RealModeCallBack realModeCallBack[0x10];
  INTDESC Interrupt_Table[0x100];
  INTDESC Exception_Table[0x20];
};
extern struct DPMIclient_struct DPMIclient[DPMI_MAX_CLIENTS];

EXTERN int in_dpmi INIT(0);        /* Set to 1 when running under DPMI */
EXTERN int in_win31 INIT(0);       /* Set to 1 when running Windows 3.1 */
EXTERN int in_dpmi_dos_int INIT(0);
EXTERN int dpmi_mhp_TF INIT(0);
EXTERN unsigned char dpmi_mhp_intxxtab[256] INIT({0});
EXTERN int is_cli INIT(0);

extern unsigned short DPMI_private_data_segment;
extern unsigned long dpmi_free_memory; /* how many bytes memory client */
				       /* can allocate */
extern unsigned long pm_block_handle_used;       /* tracking handle */
extern SEGDESC Segments[];
extern struct sigcontext_struct _emu_stack_frame;
/* used to store the dpmi client registers */
extern RealModeCallBack mouseCallBack; /* user\'s mouse routine */
extern char *ldt_buffer;

void dpmi_get_entry_point(void);
void indirect_dpmi_switch(struct sigcontext_struct *);
#ifdef __linux__
void dpmi_fault(struct sigcontext_struct *);
#endif
void dpmi_realmode_hlt(unsigned char *);
void run_pm_int(int);
void fake_pm_int(void);

#ifdef __linux__
int dpmi_mhp_regs(void);
void dpmi_mhp_getcseip(unsigned int *seg, unsigned int *off);
void dpmi_mhp_getssesp(unsigned int *seg, unsigned int *off);
int dpmi_mhp_get_selector_size(int sel);
int dpmi_mhp_getcsdefault(void);
int dpmi_mhp_setTF(int on);
void dpmi_mhp_GetDescriptor(unsigned short selector, unsigned long *lp);
int dpmi_mhp_getselbase(unsigned short selector);
unsigned long dpmi_mhp_getreg(int regnum);
void dpmi_mhp_setreg(int regnum, unsigned long val);
void dpmi_mhp_modify_eip(int delta);
#endif

void add_cli_to_blacklist(void);
dpmi_pm_block* DPMImalloc(unsigned long size);
dpmi_pm_block* DPMImallocFixed(unsigned long base, unsigned long size);
int DPMIfree(unsigned long handle);
dpmi_pm_block *DPMIrealloc(unsigned long handle, unsigned long size);
void DPMIfreeAll(void);
unsigned long base2handle(void *);
dpmi_pm_block *lookup_pm_block(unsigned long h);
int
DPMIMapConventionalMemory(dpmi_pm_block *block, unsigned long offset,
			  unsigned long low_addr, unsigned long cnt);
unsigned long dpmi_GetSegmentBaseAddress(unsigned short selector);
unsigned long GetSegmentBaseAddress(unsigned short);
unsigned long GetSegmentLimit(unsigned short);
void CheckSelectors(struct sigcontext_struct *scp);
int ValidSelector(unsigned short selector);
int ValidAndUsedSelector(unsigned short selector);

extern void DPMI_show_state(struct sigcontext_struct *scp);
extern void dpmi_sigio(struct sigcontext_struct *scp);
extern void run_dpmi(void);

extern int ConvertSegmentToDescriptor(unsigned short segment);
extern int SetSegmentBaseAddress(unsigned short selector,
					unsigned long baseaddr);
extern int SetSegmentLimit(unsigned short, unsigned int);
extern void save_pm_regs(struct sigcontext_struct *);
extern void restore_pm_regs(struct sigcontext_struct *);
extern unsigned short AllocateDescriptors(int);
extern int FreeDescriptor(unsigned short selector);

#endif /* DPMI_H */
