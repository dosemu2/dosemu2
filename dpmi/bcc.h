/* bcc.h	-- Dong Liu <dliu@rice.njit.edu> Sat Feb 11 1995
 *
 * Special hacks for running Borland\'s C++ compiler.
 *
 * $Log: bcc.h,v $
 * Revision 1.1  1995/02/25  21:54:01  root
 * Initial revision
 *
 */

/* fortunately when dpmiload.exe calling dpmi_get_entry_point, ES=PSP */
/* so, we check the enviroment segment to see if it is real dpmiload.exe*/

static int bcc_check_dpmiload(unsigned short old_es)
{
    char *env;
    char *dpmiload = "dpmiload.exe";

    env = (char *)(((unsigned long)old_es)<<4);
    if ( *(unsigned short *)env != 0x20cd /* int 0x20 */)
	return 0;
    env = (char *)(((unsigned long)
		   (*(unsigned short *)(env+0x2c)))<<4); /* get envr. */
    /* scan for 0 0 */
    while (*(unsigned short *)env != 0)
	env++;
    env += 4;			/* now env points to the exe file */

    if (strlen(env)> strlen(dpmiload)) {
	env += strlen(env)-strlen(dpmiload);
	if (strcasecmp(env, dpmiload) == 0)
	    return 1;
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

#if 0
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
	
	if(info->code_selector == selector) {
	    if (info->flags & SEGMENT_DOS)
		info->flags &= ~SEGMENT_DOS;
	}

	if(((info->flags & SEGMENT_DOS) == 0)
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
	/* make this segment present */
	if (!(*lp & 0x8000))
	    *lp |= 0x8000;
    }
    if (SetSelector(selector, base_addr, limit, (*lp >> 22) & 1,
		    (*lp >> 10) & 7, ((*lp >> 9) & 1) ^1,
                    (*lp >> 23) & 1, ((*lp >> 15) & 1) ^1, (*lp >> 20) & 1))
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
#endif
