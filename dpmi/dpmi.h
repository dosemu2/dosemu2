/* this is for the DPMI support */
#ifndef DPMI_H
#define DPMI_H

#define DPMI_VERSION   		0x00	/* major version 0 */
#define DPMI_DRIVER_VERSION	0x5a	/* minor version 0.90 */

#define DPMI_MAX_CLIENTS	8	/* maximal number of clients */

#define DPMI_page_size		4096	/* 4096 bytes per page */

#define DPMI_pm_stack_size	0x1000	/* locked protected mode stack for exceptions, */
					/* hardware interrupts, software interrups 0x1c, */
					/* 0x23, 0x24 and real mode callbacks */

#define DPMI_max_rec_rm_func	16	/* max number of recursive real mode functions */
#define DPMI_rm_stack_size	0x0200	/* real mode stack size */

#define DPMI_private_paragraphs	((DPMI_max_rec_rm_func * DPMI_rm_stack_size)>>4)
					/* private data for DPMI server */

#define UCODESEL 0x23
#define UDATASEL 0x2b

extern int in_dpmi;
#define current_client (in_dpmi-1)
extern int in_win31;
extern int dpmi_eflags;
extern int in_dpmi_dos_int;

void dpmi_get_entry_point();

void dpmi_fault(struct sigcontext_struct *);
void dpmi_realmode_hlt(unsigned char *);
void run_pm_int(int);

/* this is used like: SEL_ADR(_ss, _esp) */
#define SEL_ADR(seg, reg) \
({ unsigned long __res; \
  if (!((seg) & 0x0004)) { \
    /* GTD */ \
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
    unsigned long	limit;		/* Limit of Segment */
    unsigned char	type;
    unsigned char	is_32;		/* one for is 32-bit Segment */
    unsigned char	readonly;	/* one for read only Segments */	
    unsigned char	is_big;		/* Granularity */
    unsigned int	used;		/* Segment in use by client # */
} SEGDESC;

#define MAX_SELECTORS	0x0400
#define MODIFY_LDT_CONTENTS_DATA        0
#define MODIFY_LDT_CONTENTS_STACK       1
#define MODIFY_LDT_CONTENTS_CODE        2

extern SEGDESC Segments[];

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

typedef struct dpmi_pm_block_stuct {
  struct   dpmi_pm_block_stuct *next;
  unsigned long handle;
  void     *base;
} dpmi_pm_block;

#define DPMI_show_state \
    D_printf("eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%08lx\n" \
	     "trapno: 0x%02lx  errorcode: 0x%08lx  cr2: 0x%08lx\n" \
	     "cs: 0x%04x  ds: 0x%04x  es: 0x%04x  ss: 0x%04x  fs: 0x%04x  gs: 0x%04x\n", \
	     _eip, _esp, _eflags, _trapno, scp->err, scp->cr2, _cs, _ds, _es, _ss, _fs, _gs); \
    D_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx\n", \
	     _eax, _ebx, _ecx, _edx); \
    D_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n", \
	     _esi, _edi, _ebp); \
    /* display the 10 bytes before and after CS:EIP.  the -> points \
     * to the byte at address CS:EIP \
     */ \
    if (!((_cs) & 0x0004)) { \
      /* GTD */ \
      csp2 = (unsigned char *) _eip - 10; \
    } \
    else { \
      /* LDT */ \
      csp2 = (unsigned char *) (GetSegmentBaseAddress(_cs) + _eip) - 10; \
    } \
    D_printf("OPS  : "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *csp2++); \
    D_printf("-> "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *csp2++); \
    D_printf("\n"); \
    if (!((_ss) & 0x0004)) { \
      /* GTD */ \
      ssp2 = (unsigned char *) _esp - 10; \
    } \
    else { \
      /* LDT */ \
      if (Segments[_ss>>3].is_32) \
	ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + _esp ) - 10; \
      else \
	ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + _LWORD(esp) ) - 10; \
    } \
    D_printf("STACK: "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *ssp2++); \
    D_printf("-> "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *ssp2++); \
    D_printf("\n");

#endif /* DPMI_H */
