/* bcc.h      -- Dong Liu <dliu@rice.njit.edu> Sat Feb 11 1995
 *
 * Special hacks for running Borland\'s C++ compiler.
 *
 * $Log: bcc.h,v $
 * Revision 1.1  1995/02/25  21:54:01  root
 * Initial revision
 *
 * Revision 1.1  1995/02/25  21:54:01  root
 * Initial revision
 *
 */

#ifndef lint
static char *rcsid = "$Header: /home/src/dosemu0.60/dpmi/RCS/bcc.h,v 1.1 1995/02/25 21:54:01 root Exp $";
#endif

int in_bcc = 0;       /* Set to 1 when running DPMILOAD */
int in_dpmires = 0;		/* set 1 when dpmires is loaded */

enum { ES_INDEX = 0, CS_INDEX = 1, SS_INDEX = 2,  DS_INDEX = 3,
       FS_INDEX = 4, GS_INDEX = 5 };

static struct vm86_regs SAVED_REGS;

#define S_REG(reg) (SAVED_REGS.##reg)

/* these are used like:  S_LO(ax) = 2 (sets al to 2) */
#define S_LO(reg)  (*(unsigned char *)&S_REG(e##reg))
#define S_HI(reg)  (*((unsigned char *)&S_REG(e##reg) + 1))

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define S_LWORD(reg)	(*((unsigned short *)&S_REG(reg)))
#define S_HWORD(reg)	(*((unsigned short *)&S_REG(reg) + 1))

typedef struct {
    unsigned short flags;
    unsigned long size __attribute__ ((packed));
    unsigned long handle __attribute__ ((packed));
    unsigned long linear_address __attribute__ ((packed));
    unsigned short error_code;
    unsigned short code_selector;
    unsigned short code_alias;
} dpmiload_mem_info;

/* the bits of flags of dpmiload_mem_info */
#define SEGMENT_DATA        0x1
#define SEGMENT_VALID       0x2
#define SEGMENT_DOS         0x4
#define SEGMENT_MOVABLE     0x10
#define SEGMENT_PRELOAD     0x40
#define SEGMENT_DISCARDABLE 0x1000

/* used when bcc passing a DTA higher than 1MB */
static void * DTA_over_1MB;
static void * DTA_under_1MB;

#undef inline /*for gdb*/
#define inline

static inline unsigned long GetSegmentBaseAddress(unsigned short seg);
static inline int ConvertSegmentToDescriptor(unsigned short seg);
static inline int SetSegmentBaseAddress(unsigned short selector,
					unsigned long baseaddr);
static inline int SetSegmentLimit(unsigned short, unsigned long);
static inline int SetSelector(unsigned short, unsigned long,
                              unsigned long, unsigned char, unsigned char,
                              unsigned char, unsigned char);
static inline void save_pm_regs();
static inline void restore_pm_regs();

/* fortunately when dpmiload.exe calling dpmi_get_entry_point, ES=PSP */
/* so, we check the enviroment segment to see if it is real dpmiload.exe*/

static inline int bcc_check_dpmiload(unsigned short old_es)
{
    char *env;
    char *dpmiload = "dpmiload.exe";
    char *dpmires = "dpmires.exe";

    env = (char *)(((unsigned long)old_es)<<4);
    if ( *(unsigned short *)env != 0x20cd /* int 0x20 */)
	return 0;
    env = (char *)(((unsigned long)
		   (*(unsigned short *)(env+0x2c)))<<4); /* get envr. */
    /* scan for 0 0 */
    while (*(unsigned short *)env != 0)
	env++;
    env += 4;			/* now env points to the exe file */

    if (strlen(env)> strlen(dpmires)) {
	if (strcasecmp(env + strlen(env)-strlen(dpmires), dpmires) == 0)
	    in_dpmires = 1;
    }
    if (strlen(env)> strlen(dpmiload)) {
	env += strlen(env)-strlen(dpmiload);
	if (strcasecmp(env, dpmiload) == 0)
	    return 1;
    }
    return 0;
}

/* when BCC call real mode interrupt, its ds and/or es segments may be */
/* on the area over 1MB, we first copy them to memory under 1MB, then */
/* copy them back when back from real mode interrupt. Should consider */
/* use mmap for speed, but how =(? */

static unsigned short DS_MAPPED;
static unsigned short ES_MAPPED;
static char READ_DS_COPIED;
static inline int
bcc_pre_extender(struct sigcontext_struct *scp, int intr)
{
    DS_MAPPED = 0;
    ES_MAPPED = 0;
    /* first see if we don\'t need to go to real mode */
    switch (intr) {
    case 0x21:
	SAVED_REGS = REGS;
	switch (_HI(ax)) {
	case 0x25:		/* set vector */
	    Interrupt_Table[current_client][_LO(ax)].selector = _ds;
	    Interrupt_Table[current_client][_LO(ax)].offset = _edx;
	    D_printf("BCC: int 21,ax=0x%04x, ds=0x%04x. dx=0x%04x\n",
		     _LWORD(eax), _ds, _LWORD(edx));
	    return 1;
	case 0x35:	/* Get Interrupt Vector */
	    _es = Interrupt_Table[current_client][_LO(ax)].selector;
	    _ebx = Interrupt_Table[current_client][_LO(ax)].offset;
	    D_printf("BCC: int 21,ax=0x%04x, es=0x%04x. bx=0x%04x\n",
		     _LWORD(eax), _es, _LWORD(ebx));
	    return 1;
	case 0x1a:		/* set DTA */
	    if ( Segments[ _ds >> 3].base_addr > 0xffff0) {
		D_printf("DPMI: bcc passing pointer > 1MB to dos set DTA\n"); 
		
		DTA_over_1MB = (void *)GetSegmentBaseAddress(_ds)
		                                   + _LWORD(edx); 
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs+
		                  0x2000;
		REG(edx)=0;
		DTA_under_1MB = (void *)(REG(ds) << 4);
	    } else
		DTA_over_1MB = 0;
	    return 0;
	case 0x4b:		/* EXEC */
	    D_printf("BCC: call dos exec.\n");
	    /* we must copy all data from higher 1MB to lower 1MB */
	    {
		unsigned short segment =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		void *p;
		unsigned short sel,off;
		
		if ( Segments[ _ds >> 3].base_addr > 0xffff0) {
		    /* must copy command line */
		    REG(ds) = segment;
		    REG(edx) = 0;
		    p = (void *)GetSegmentBaseAddress(_ds) +
			_LWORD(edx);
		    strcpy((char *)(REG(ds)<<4), p);
		    segment += (strlen(p)>>4) + 1;
		}
		if ( Segments[ _es >> 3].base_addr > 0xffff0) {
		    /* must copy parameter block */
		    REG(es) = segment;
		    REG(ebx) = 0;
		    memcpy((void *)(REG(es)<<4),
			   (void*)GetSegmentBaseAddress(_es) + _LWORD(ebx),
			   0x20);
		    segment += 2;
		}
		/* now the envrionment segment */
		sel = *(unsigned short *)((REG(es)<<4)+LWORD(ebx));
		if (Segments[sel>>3].base_addr> 0xffff0) {
		    *(unsigned short *)((REG(es)<<4)+LWORD(ebx)) =
			segment;
		    memcpy((void *)(segment <<4),           /* 4K envr. */
			   (void *)GetSegmentBaseAddress(sel),
			   0x1000);
		    segment += 0x100;
		} else 
		    *(unsigned short *)((REG(es)<<4)+LWORD(ebx)) =
			GetSegmentBaseAddress(sel)>>4;

		/* now the tail of the command line */
		off = *(unsigned short *)((REG(es)<<4)+LWORD(ebx)+2);
		sel = *(unsigned short *)((REG(es)<<4)+LWORD(ebx)+4);
		if (Segments[sel>>3].base_addr > 0xffff0) {
		    *(unsigned short *)((REG(es)<<4)+LWORD(ebx)+4)
			= segment;
		    *(unsigned short *)((REG(es)<<4)+LWORD(ebx)+2) = 0;
		    memcpy((void *)(segment<<4),
			   (void *)GetSegmentBaseAddress(sel) + off,
			   0x80);
		} else
		    *(unsigned short *)((REG(es)<<4)+LWORD(ebx)+4)
			= GetSegmentBaseAddress(sel)>>4;

		/* make tow FCB\'s zero */
		*(unsigned long *)((REG(es)<<4)+LWORD(ebx)+6) = 0;
		*(unsigned long *)((REG(es)<<4)+LWORD(ebx)+0xA) = 0;
	    }
	    REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos_exec);
	    /* save context for child exits */
	    memcpy(&dpmi_stack_frame[current_client], scp,
		   sizeof(struct sigcontext_struct));
	    save_pm_regs();
	    return 0;
	case 0x50:		/* set PSP */
	    if (((LWORD(ebx) & 0x7) == 0x7) &&
		((LWORD(ebx) >> 3) < MAX_SELECTORS) &&
		Segments[LWORD(ebx) >> 3].used) {
		unsigned short envp;

		REG(ebx) = (long) GetSegmentBaseAddress(LWORD(ebx)) >> 4;
		envp = *(unsigned short *)(((char *)(LWORD(ebx)<<4)) + 0x2c);
		if (((envp & 0x7 == 0x7) &&
		     ((envp >> 3) < MAX_SELECTORS) &&
		     Segments[envp >> 3].used))
		    *(unsigned short *)(((char *)(LWORD(ebx)<<4))+0x2c) =
			(long) GetSegmentBaseAddress(envp) >> 4;
	    }
	    return 0;
	case 0x39:		/* mkdir */
	case 0x3a:		/* rmdir */
	case 0x3b:		/* chdir */
	case 0x3c:		/* creat */
	case 0x3d:		/* Dos OPEN */
	case 0x41:		/* unlink */
	case 0x43:		/* change attr */
	case 0x4e:		/* find first */
	case 0x4f:		/* find next */
	    if ( Segments[ _ds >> 3].base_addr > 0xffff0) {
		D_printf("DPMI: bcc passing ASCIIZ > 1MB to dos\n"); 
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		strcpy((void *)((REG(ds) << 4)+LWORD(edx)),
		       (void *)GetSegmentBaseAddress(_ds)+_LWORD(edx));
	    }
	    return 0;
	case 0x3f:		/* dos read */
	    /* bcc sometime pass address > 1MB to dos to read, so */
	    /* frist read to our buffer, then copy it to client */
	    if ( Segments[ _ds >> 3].base_addr > 0xffff0) {
		D_printf("DPMI: bcc passing pointer > 1MB to dos read\n"); 
		S_REG(ds) = _ds;
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		REG(edx) = 0;
		READ_DS_COPIED = 1;
	    } else
		READ_DS_COPIED = 0;
	    return 0;
	case 0x40:		/* DOS Write */
	    if ( Segments[ _ds >> 3].base_addr > 0xffff0)
		D_printf("DPMI: bcc passing pointer > 1MB to dos write\n"); 
	    S_REG(ds) = _ds;
	    REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
	    REG(edx) = 0;
	    memcpy((void *)(REG(ds) << 4),
		   (void *)GetSegmentBaseAddress(S_REG(ds))+S_LWORD(edx),
		   LWORD(ecx));
	    return 0;
	case 0x01 ... 0x08:	/* These are dos functions which */
	case 0x0b ... 0x0e:	/* are not required memory copy. */
	case 0x19:
	case 0x2a ... 0x2e:
	case 0x30 ... 0x31:
	case 0x33:
	case 0x36 ... 0x37:
	case 0x3e:
	case 0x42:
	case 0x45 ... 0x46:
	case 0x48 ... 0x4a:
	case 0x4d:
	    return 0;
	}
	break;
    case 0x08:			/* timer, hardware */
    case 0x09:			/* keyborad HW */
	return 0;
    case 0x10:			/* video */
	switch (_HI(ax)) {
	case 0x00 ... 0x0f:
	    return 0;
	}
	break;
    case 0x11:			/* bios equipment flags */
    case 0x12:			/* bios memory */
    case 0x16:			/* keyboard */
    case 0x17:			/* printer */
    case 0x1c:			/* BIOS timer */
    case 0x23:			/* DOS Ctrl-C */
    case 0x24:			/* DOS critical error */
    case 0x28:			/* DOS idle */
	return 0;
    default:
	break;
    }

    /* envetually, we should use mmap, now just copy it */
    if (Segments[ _ds >> 3].base_addr > 0xffff0) {
	D_printf("DPMI: BCC pass ds segment > 1MB to real mode\n");
	DS_MAPPED = _ds;
	REG(ds) =   DPMI_private_data_segment+DPMI_private_paragraphs;
	memcpy ((void *)(REG(ds)<<4), (void *)GetSegmentBaseAddress(_ds),
		 Segments[_ds >> 3].limit);
    } else
	DS_MAPPED = 0;

    if ( (_es == _ds) && DS_MAPPED) {
	REG(es) = REG(ds);
	ES_MAPPED = 0;
    }  else if (Segments[ _es >> 3].base_addr > 0xffff0) {
	D_printf("DPMI: BCC pass es segment > 1MB to real mode\n");
	ES_MAPPED = _es;
	REG(es) =   DPMI_private_data_segment + DPMI_private_paragraphs
	                                 + 0x1000;
	memcpy ((void *)(REG(es)<<4), (void *)GetSegmentBaseAddress(_es),
		 Segments[_es >> 3].limit);
    } else
	ES_MAPPED = 0;

    switch (intr) {
    case 0x21:
	switch (HI(ax)) {
	}
    }
    return 0;
}

static inline
bcc_post_exec()
{
    /* restore parent\'s context */
    restore_pm_regs();
    dpmi_stack_frame[current_client].eflags = 0x0202 | (0x0dd5 & REG(eflags));
    dpmi_stack_frame[current_client].eax = REG(eax);
    dpmi_stack_frame[current_client].ebx = REG(ebx);
    dpmi_stack_frame[current_client].ecx = REG(ecx);
    dpmi_stack_frame[current_client].edx = REG(edx);
    dpmi_stack_frame[current_client].esi = REG(esi);
    dpmi_stack_frame[current_client].edi = REG(edi);
    dpmi_stack_frame[current_client].ebp = REG(ebp);

    if (LWORD(eflags) & CF) {
	dpmi_stack_frame[current_client].edx = S_REG(edx);
	dpmi_stack_frame[current_client].ebx = S_REG(ebx);
    }
    return;
}

static inline
bcc_post_extender(int intr)
{
    if (DS_MAPPED) {
	unsigned short my_ds;
	my_ds =   DPMI_private_data_segment+DPMI_private_paragraphs;
	memcpy ((void *)GetSegmentBaseAddress(DS_MAPPED), (void *)(my_ds<<4), 
		 Segments[DS_MAPPED >> 3].limit);
    } 
    DS_MAPPED = 0;

    if (ES_MAPPED) {
	unsigned short my_es;
	my_es =   DPMI_private_data_segment + DPMI_private_paragraphs
	                                 + 0x1000;
	memcpy ((void *)GetSegmentBaseAddress(ES_MAPPED), (void *)(my_es<<4), 
		 Segments[ES_MAPPED >> 3].limit);
    } 
    ES_MAPPED = 0;
	       
    switch (intr) {
    case 0x21:
	switch (S_HI(ax)) {
	case 0x2f:		/* GET DTA */
	case 0x35:		/* GET Vector */
	    dpmi_stack_frame[current_client].es =
	               ConvertSegmentToDescriptor(REG(es));
	    break;
	case 0x38:
	    if (S_LO(ax) == 0x00) { /* get contry info */
		dpmi_stack_frame[current_client].ds =
	               ConvertSegmentToDescriptor(REG(ds));
	    }
	    break;
	case 0x48:		/* allocate memory */
	    if (LWORD(eflags) & CF)
		break;
	    dpmi_stack_frame[current_client].eax =
		ConvertSegmentToDescriptor(LWORD(eax));
	    break;
	case 0x4e:		/* find first */
	case 0x4f:		/* find next */
	    if (LWORD(eflags) & CF)
		break;
	    if (DTA_over_1MB)
		memcpy(DTA_over_1MB, DTA_under_1MB, 43);
	    break;
	case 0x51:		/* get PSP */
	case 0x62:
	    {/* convert environment pointer to a descriptor*/
		unsigned short envp, psp;
		psp = LWORD(ebx);
#if 0		
		envp = *(unsigned short *)(((char *)(psp<<4))+0x2c);
		envp = ConvertSegmentToDescriptor(envp);
		*(unsigned short *)(((char *)(psp<<4))+0x2c) = envp;
#endif 		
		dpmi_stack_frame[current_client].ebx = ConvertSegmentToDescriptor(psp);
	    }
	    return;
	case 0x34:	/* Get Address of InDOS Flag */
	    if (!(dpmi_stack_frame[current_client].es =
		  ConvertSegmentToDescriptor(REG(es))))
		D_printf("DPMI: can't allocate descriptor for InDOS pointer\n");
	    return;
	case 0x5d:
	    if (S_LO(ax) == 0x06) /* get address of DOS swappable area */
				/*        -> DS:SI                     */
		dpmi_stack_frame[current_client].ds = ConvertSegmentToDescriptor(REG(ds));
	    return;
	case 0x3f:
	    if (READ_DS_COPIED) {
		dpmi_stack_frame[current_client].edx = S_REG(edx);
		if (LWORD(eflags) & CF)
		    return;
		memcpy((void *)(GetSegmentBaseAddress(S_REG(ds)) +
				S_LWORD(edx)),
				(void *)(REG(ds) << 4), LWORD(eax));
	    }
	    READ_DS_COPIED = 0;
	    break;
	case 0x40:
	    dpmi_stack_frame[current_client].edx = S_REG(edx);
	    break;
	default:
	    return;
	}
	break;
    default:
	return;
    }
}

static inline int
bcc_SetDescriptor(struct sigcontext_struct *scp)
{
    unsigned selector;
    unsigned long *lp;
    unsigned long base_addr, limit, desc;
    char fix_code_alias = 1;
    
    selector = _LWORD(ebx);
    lp = (unsigned long *)(GetSegmentBaseAddress(_es) +
			   (DPMIclient_is_32 ? _edi : _LWORD(edi)));

    D_printf("DPMI: bcc_SetDescriptor[0x%04lx] 0x%08lx%08lx\n", selector>>3, *(lp+1), *lp);

    if (selector >= (MAX_SELECTORS << 3))
	return -1;
    base_addr = (*lp >> 16) & 0x0000FFFF;
    limit = *lp & 0x0000FFFF;
    lp++;
    base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
    limit |= (*lp & 0x000F0000);
 
#if 1
    /* this is getting ridiculous, dpmiload.exe sometime uses a */
    /* selector (DS) as the higher 16-value of a descriptor when making */
    /* dpmi service 0x000c call, I dont what it want here */
    desc = ((base_addr>>16)&0xffff);
    if ((desc == _ds) && ((base_addr&0xffff) == _LWORD(esi))) {
	dpmiload_mem_info *info = (dpmiload_mem_info *)(
	    GetSegmentBaseAddress(_ds) + _LWORD(esi));

	fix_code_alias = 0;

	/* after we figure out that ds:si is the segment info block, */
	/* we make all segments fixed, preload and non-discardable */
	D_printf("DPMI: bcc uses ds:si as base address flags 0x%04x\n", info->flags);
	/* make sure it is a valid memory block info struct */
	if(((info->flags & SEGMENT_DOS) == 0)
	   /*   && ((info->flags & SEGMENT_DATA) == 0)*/
	   && (info->code_selector == selector)) {
	    /* make all segment fixed, preload and non-discardable, */
	    /* because memory is not the issue here :=) */
	    if (info -> flags & SEGMENT_MOVABLE)
		info -> flags &= ~SEGMENT_MOVABLE;
	    if ((info -> flags & SEGMENT_PRELOAD) == 0)
		info -> flags |= SEGMENT_PRELOAD;
	    if (info -> flags & SEGMENT_DISCARDABLE)
		info -> flags &= ~SEGMENT_DISCARDABLE;
	}

	base_addr = GetSegmentBaseAddress(_ds);
    }
#endif    
    if (SetSelector(selector, base_addr, limit, (*lp >> 22) & 1,
		    (*lp >> 10) & 7, ((*lp >> 9) & 1) ? 0 : 1, (*lp >> 23) & 1))
	return -1;
    if (fix_code_alias) {
	dpmiload_mem_info *info = (dpmiload_mem_info *)(
	    GetSegmentBaseAddress(_ds) + _LWORD(esi));
	/* for a valid code dos memoey block */
	if ( ((info->flags & 0x3) == 0x2) &&
	     (info->code_alias == selector) &&
	     (Segments[info->code_selector>>3].used)&&
	     (Segments[info->code_selector>>3].type &
	      MODIFY_LDT_CONTENTS_CODE) &&
	     (Segments[info->code_selector>>3].base_addr !=
		 Segments[selector>>3].base_addr)) {
	    D_printf("DPMI: BCC fix code segment by code alias segment\n");
	    SetSegmentBaseAddress(info->code_selector, base_addr);
	    if (SetSegmentLimit(info->code_selector, limit))
		return -1;
	}
    }
    {
	unsigned long *lp2 = (unsigned long *) &ldt_buffer[(selector>>3)*LDT_ENTRY_SIZE];
	*(lp2++) = *(lp-1);
	*lp2 = *lp;
    }
    return 0;
}

static void inline
bcc_fixed_realloc_descriptor(void *old_base, dpmi_pm_block *block,
			     unsigned long new_size)
{
    unsigned desc;
    for (desc=0; desc < MAX_SELECTORS; desc++)
	if((Segments[desc].base_addr == (unsigned long)old_base) &&
	   Segments[desc].used && (Segments[desc].limit <= 0xffff) &&
	   (Segments[desc].is_big==0) &&
	   (Segments[desc].is_32 == DPMIclient_is_32))
	    break;
    if(desc < MAX_SELECTORS) {
	SetSegmentBaseAddress(desc<<3, (unsigned long)block->base);
	SetSegmentLimit(desc<<3, new_size);
    }
}

static char decode_use_16bit;
static char use_prefix;
static inline unsigned char *
decode_8e_index(struct sigcontext_struct *scp, unsigned char *prefix,
		int rm)
{
    switch (rm) {
    case 0:
	if (use_prefix)
	    return prefix + _LWORD(ebx) + _LWORD(esi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ds)+
				     _LWORD(ebx) + _LWORD(esi));
    case 1:
	if (use_prefix)
	    return prefix + _LWORD(ebx) + _LWORD(edi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ds)+
				     _LWORD(ebx) + _LWORD(edi));
    case 2:
	if (use_prefix)
	    return prefix + _LWORD(ebp) + _LWORD(esi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ss)+
				     _LWORD(ebp) + _LWORD(esi));
    case 3:
	if (use_prefix)
	    return prefix + _LWORD(ebp) + _LWORD(edi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ss)+
				     _LWORD(ebp) + _LWORD(edi));
    case 4:
	if (use_prefix)
	    return prefix + _LWORD(esi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ds)+
				     _LWORD(esi));
    case 5:
	if (use_prefix)
	    return prefix + _LWORD(edi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ds)+
				     _LWORD(edi));
    case 6:
	if (use_prefix)
	    return prefix + _LWORD(ebp);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ss)+
				     _LWORD(ebp));
    case 7:
	if (use_prefix)
	    return prefix + _LWORD(ebx);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ds)+
				     _LWORD(ebx));
    }
}

static inline unsigned char *
check_prefix (struct sigcontext_struct *scp)
{
    unsigned char *prefix, *csp;

    csp = (unsigned char *) SEL_ADR(_cs, _eip);

    use_prefix = 0;
    switch (*csp) {
    case 0x2e:
	prefix = (unsigned char *)GetSegmentBaseAddress(_cs);
	use_prefix = 1;
	break;
    case 0x36:
	prefix = (unsigned char *)GetSegmentBaseAddress(_ss);
	use_prefix = 1;
	break;
    case 0x3e:
	prefix = (unsigned char *)GetSegmentBaseAddress(_ds);
	use_prefix = 1;
	break;
    case 0x26:
	prefix = (unsigned char *)GetSegmentBaseAddress(_es);
	use_prefix = 1;
	break;
    case 0x64:
	prefix = (unsigned char *)GetSegmentBaseAddress(_fs);
	use_prefix = 1;
	break;
    case 0x65:
	prefix = (unsigned char *)GetSegmentBaseAddress(_gs);
	use_prefix = 1;
	break;
    }
    return prefix;
}
/*
 * this function try to decode opcode 0x8e (mov Sreg,m/r16), return
 * the length of the instructon.
 */

static inline int
decode_8e(struct sigcontext_struct *scp, unsigned short *src,
	  unsigned  char * sreg)
{
    unsigned char *prefix, *csp;
    unsigned char mod, rm, reg;
    int len = 0;

    if (!decode_use_16bit)	/*  32bit decode not implmented yet*/
	return 0;
    csp = (unsigned char *) SEL_ADR(_cs, _eip);

    prefix = check_prefix(scp);
    if (use_prefix) {
	csp++;
	len++;
    }

    if (*csp != 0x8e)
	return 0;

    csp++;
    len += 2;
    mod = (*csp>>6) & 3;
    reg = (*csp>>3) & 7;
    rm = *csp & 0x7;

    switch (mod) {
    case 0:
	if (rm == 6) {		/* disp16 */
	    if(use_prefix)
		*src = *(unsigned short *)(prefix +
					   (int)(*(short *)(csp+1)));
	    else
		*src =  *(unsigned short *)(GetSegmentBaseAddress(_ds) +
					    (int)(*(short *)(csp+1)));
	    len += 2;
	} else
	    *src = *(unsigned short *)decode_8e_index(scp, prefix, rm);
	break;
    case 1:			/* disp8 */
	*src = *(unsigned short *)(decode_8e_index(scp, prefix, rm) +
				   (int)(*(char *)(csp+1)));
	len++;
	break;
    case 2:			/* disp16 */
	*src = *(unsigned short *)(decode_8e_index(scp, prefix, rm) +
				   (int)(*(short *)(csp+1)));
	len += 2;
	break;
    case 3:			/* register */
	switch (rm) {
	case 0:
	    *src = (unsigned short)_LWORD(eax);
	    break;
	case 1:
	    *src = (unsigned short)_LWORD(ecx);
	    break;
	case 2:
	    *src = (unsigned short)_LWORD(edx);
	    break;
	case 3:
	    *src = (unsigned short)_LWORD(ebx);
	    break;
	case 4:
	    *src = (unsigned short)_LWORD(esp);
	    break;
	case 5:
	    *src = (unsigned short)_LWORD(ebp);
	    break;
	case 6:
	    *src = (unsigned short)_LWORD(esi);
	    break;
	case 7:
	    *src = (unsigned short)_LWORD(edi);
	    break;
	}
    }

    *sreg = reg;
    return len;
}

static inline int
decode_load_descriptor(struct sigcontext_struct *scp, unsigned short
		       *segment, unsigned char * sreg)
{
    unsigned char *prefix, *csp;
    unsigned char mod, rm, reg;
    unsigned long *lp, base_addr;
    unsigned offset;
    int len = 0;

    if (!decode_use_16bit)	/*  32bit decode not implmented yet*/
	return 0;
    csp = (unsigned char *) SEL_ADR(_cs, _eip);

    prefix = check_prefix(scp);
    if (use_prefix) {
	csp++;
	len++;
    }

    switch (*csp) {
    case 0xc5:
	*sreg = DS_INDEX;		/* LDS */
	break;
    case 0xc4:
	*sreg = ES_INDEX;		/* LES */
	break;
    default:
	return 0;
    }

    csp++;
    len += 2;
    mod = (*csp>>6) & 3;
    reg = (*csp>>3) & 7;
    rm = *csp & 0x7;

    switch (mod) {
    case 0:
	if (rm == 6) {		/* disp16 */
	    if(use_prefix)
		lp = (unsigned long *)(prefix +
					   (int)(*(short *)(csp+1)));
	    else
		lp =  (unsigned long *)(GetSegmentBaseAddress(_ds) +
					    (int)(*(short *)(csp+1)));
	    len += 2;
	} else
	    lp = (unsigned long *)decode_8e_index(scp, prefix, rm);
	break;
    case 1:			/* disp8 */
	lp = (unsigned long *)(decode_8e_index(scp, prefix, rm) +
				   (int)(*(char *)(csp+1)));
	len++;
	break;
    case 2:			/* disp16 */
	lp = (unsigned long *)(decode_8e_index(scp, prefix, rm) +
				   (int)(*(short *)(csp+1)));
	len += 2;
	break;
    case 3:			/* register */
				/* must be memory address */
	return 0;
    }

    offset = *lp & 0xffff;
    *segment = (*lp >> 16) & 0xffff;
    switch (reg) {
	case 0:
	    (unsigned short)_LWORD(eax) = offset;
	    break;
	case 1:
	    (unsigned short)_LWORD(ecx) = offset;
	    break;
	case 2:
	    (unsigned short)_LWORD(edx) = offset;
	    break;
	case 3:
	    (unsigned short)_LWORD(ebx) = offset;
	    break;
	case 4:
	    (unsigned short)_LWORD(esp) = offset;
	    break;
	case 5:
	    (unsigned short)_LWORD(ebp) = offset;
	    break;
	case 6:
	    (unsigned short)_LWORD(esi) = offset;
	    break;
	case 7:
	    (unsigned short)_LWORD(edi) = offset;
	    break;
	}
    return len;
}

/*
 * decode_modify_segreg_insn tries to decode instructions which would modify a
 * segment register, return the length of the insn.
 */
static inline int
decode_modify_segreg_insn(struct sigcontext_struct *scp, unsigned
			  short *segment, unsigned char *sreg)
{
    unsigned char *csp;
    unsigned short *ssp;
    int len ,size_prfix;
    unsigned long old_eip;

    old_eip = _eip;
    csp = (unsigned char *) SEL_ADR(_cs, _eip);
    size_prfix = 0;
    if (Segments[_cs>>3].is_32) {
	if (*csp == 0x66) { /* Operand-Size prefix */
	    csp++;
	    _eip++;
	    decode_use_16bit = 1;
	    size_prfix = 1;
	} else
	    decode_use_16bit = 0;
    } else {
	if (*csp == 0x66) { /* Operand-Size prefix */
	    csp++;
	    _eip++;
	    decode_use_16bit = 0;
	    size_prfix = 1;
	} else
	    decode_use_16bit = 1;
    }
	
    /* first try mov sreg, .. */
    if ((len = decode_8e(scp, segment, sreg))) {
	_eip = old_eip;
	return len + size_prfix;
    }
    /* then try lds, les ... */
    if ((len = decode_load_descriptor(scp, segment, sreg))) {
	_eip = old_eip;
	return len+size_prfix;
    }

    /* then try pop sreg */
    len = 0;
    ssp = (unsigned short *) SEL_ADR(_ss, _esp);
    switch (*csp) {
    case 0x1f:			/* pop ds */
	len = 1;
	*segment = *ssp;
	*sreg = DS_INDEX;
	_esp += decode_use_16bit ? 2 : 4;
	break;
    case 0x07:			/* pop es */
	len = 1;
	*segment = *ssp;
	*sreg = ES_INDEX;
	_esp += decode_use_16bit ? 2 : 4;
	break;
    case 0x17:			/* pop ss */
	len = 1;
	*segment = *ssp;
	*sreg = SS_INDEX;
	_esp += decode_use_16bit ? 2 : 4;
	break;
    case 0x0f:		/* two byte opcode */
	csp++;
	switch (*csp) {
	case 0xa1:		/* pop fs */
	    len = 2;
	    *segment = *ssp;
	    *sreg = FS_INDEX;
	    _esp += decode_use_16bit ? 2 : 4;
	    break;
	case 0xa9:		/* pop gs */
	    len = 2;
	    *segment = *ssp;
	    *sreg = FS_INDEX;
	    _esp += decode_use_16bit ? 2 : 4;
	break;
	}
    }
    _eip = old_eip;
    if (len)
	return len+size_prfix;
    else
	return 0;
}
	
    
static inline int bcc_fix_cs_prefix (struct sigcontext_struct *scp)
{
    unsigned char *csp;

    csp = (unsigned char *) SEL_ADR(_cs, _eip);
    if (*csp != 0x2e)		/* not cs prefix */
	return 0;

    /* bcc try to something like mov cs:[xx],ax here, we cheat it by */
    /* using mov gs:[xx],ax instead, hope bcc will never use gs :=( */

    if ((Segments[_cs>>3].type & MODIFY_LDT_CONTENTS_CODE) &&
	(Segments[(_cs>>3)+1].base_addr == Segments[_cs>>3].base_addr)
	&&((Segments[(_cs>>3)+1].type & MODIFY_LDT_CONTENTS_CODE)==0)) {
	    _gs = _cs + 8;
	    *csp = 0x65;	/* gs prefix */
	    return 1;
    }
    return 0;
}

static  inline int bcc_fault(struct sigcontext_struct *scp)
{
    unsigned char reg;
    unsigned short segment, desc;
    unsigned long len, base_addr;

    if ((_err & 0xffff) == 0) {	/*  not a selector error */
	/* although dpmiload allocates data alias selector for every */
	/* code selector, but it insists to write on code segment :=(.*/
	char fix_alias = 0;

	/* see if it is a cs prefix problem */
	if (bcc_fix_cs_prefix(scp))
	    return 1;
	
	if ((Segments[_ds>>3].type & MODIFY_LDT_CONTENTS_CODE) &&
	    (Segments[(_ds>>3)+1].base_addr == Segments[_ds>>3].base_addr)
	    &&((Segments[(_ds>>3)+1].type & MODIFY_LDT_CONTENTS_CODE)==0)){
	    _ds += 8;
	    fix_alias=1;
	}
	if ((Segments[_es>>3].type & MODIFY_LDT_CONTENTS_CODE) &&
	    (Segments[(_es>>3)+1].base_addr == Segments[_es>>3].base_addr)
	    &&((Segments[(_es>>3)+1].type & MODIFY_LDT_CONTENTS_CODE)==0)){
	    _es += 8;
	    fix_alias=1;
	}
	/* see if bcc want use GDT 0 */
	if (_ds == 0) {
	    D_printf("BCC: try to use use gdt 0 as ds\n");
	    _ds = ConvertSegmentToDescriptor(0);
	    fix_alias = 1;
	}
	if (_es == 0) {
	    D_printf("BCC: try to use use gdt 0 as es\n");
	    _es = ConvertSegmentToDescriptor(0);
	    fix_alias = 1;
	}
	    
	return fix_alias;
    }

    /* now it is a invalid selector error, try to fix it if it is */
    /* caused by an instruction mov Sreg,m/r16                    */

    len = decode_modify_segreg_insn(scp, &segment, &reg);
    if (len == 0) 
	return 0;

    D_printf("DPMI: bcc try mov to a invalid selector 0x%04x\n", segment);

    base_addr = ((unsigned long)segment) << 4;
    for(desc=0; desc < MAX_SELECTORS; desc++)
	if ((Segments[desc].base_addr == base_addr)&&
	    Segments[desc].used)
	    break;

#if 0    
    if (desc >= MAX_SELECTORS)
	return 0;
#else
    if (desc >= MAX_SELECTORS) {
	if (!(desc = ConvertSegmentToDescriptor(segment)))
	    return 0;
	desc = desc >>3;
    }
#endif    
    _eip += len;
    switch (reg) {
    case ES_INDEX:
	_es = ( desc << 3 ) | 7;
	return 1;
    case CS_INDEX:
	_cs = ( desc << 3 ) | 7;
	return 1;
    case SS_INDEX:
	_ss = ( desc << 3 ) | 7;
	return 1;
    case DS_INDEX:
	_ds = ( desc << 3 ) | 7;
	return 1;
    case FS_INDEX:
	_fs = ( desc << 3 ) | 7;
	return 1;
    case GS_INDEX:
	_gs = ( desc << 3 ) | 7;
	return 1;
    }

    _eip -= len;
    return 0;
}
