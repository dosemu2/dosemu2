/* bugy-bcc.c	-- Dong Liu <dliu@ricenjit.edu> Sat Feb 11 1995
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

static struct vm86_regs SAVED_REGS;

#define S_REG(reg) (SAVED_REGS.##reg)

/* these are used like:  S_LO(ax) = 2 (sets al to 2) */
#define S_LO(reg)  (*(unsigned char *)&S_REG(e##reg))
#define S_HI(reg)  (*((unsigned char *)&S_REG(e##reg) + 1))

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define S_LWORD(reg)	(*((unsigned short *)&S_REG(reg)))
#define S_HWORD(reg)	(*((unsigned short *)&S_REG(reg) + 1))

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

/* fortunately when dpmiload.exe calling dpmi_get_entry_point, ES=PSP */
/* so, we check the enviroment segment to see if it is real dpmiload.exe*/

static inline int bcc_check_dpmiload(unsigned short old_es)
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
    
static inline
bcc_pre_extender(struct sigcontext_struct *scp, int intr)
{
    switch (intr) {
    case 0x21:
	SAVED_REGS = REGS;
	switch (HI(ax)) {
	case 0x50:		/* set PSP */
	    if (((LWORD(ebx) >> 3) < MAX_SELECTORS) &&
		Segments[LWORD(ebx) >> 3].used)
		REG(ebx) = (long) GetSegmentBaseAddress(LWORD(ebx)) >> 4;
	    return;
	case 0x3f:		/* dos read */
	    /* bcc sometime pass address > 1MB to dos to read, so */
	    /* frist read to our buffer, then copy it to client */
	    S_REG(ds) = _ds;
	    REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
	    REG(edx) = 0;
	    return;
	case 0x40:		/* DOS Write */
	    S_REG(ds) = _ds;
	    REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
	    REG(edx) = 0;
	    memcpy((void *)(REG(ds) << 4),
		   (void *)GetSegmentBaseAddress(S_REG(ds))+S_LWORD(edx),
		   LWORD(ecx));
	    return;
	}
	break;
    }
}

static inline
bcc_post_extender(int intr)
{
    switch (intr) {
    case 0x21:
	switch (S_HI(ax)) {
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
	    dpmi_stack_frame[current_client].edx = S_REG(edx);
	    if (LWORD(eflags) & CF)
		return;
	    memcpy((void *)(GetSegmentBaseAddress(S_REG(ds)) + S_LWORD(edx)),
			    (void *)(REG(ds) << 4), LWORD(eax));
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
#if 1
    if (desc == _ds)
	base_addr = Segments[desc>>3].base_addr + (base_addr&0xffff);
#else
    desc = desc>>3;
    if((desc < MAX_SELECTORS) && Segments[desc].used &&
       (Segments[desc].limit <= 0xffff) &&
       (Segments[desc].is_big==0) &&
       (Segments[desc].is_32 == DPMIclient_is_32)) {
	base_addr = Segments[desc].base_addr + (base_addr&0xffff);
    }
#endif    
    if (SetSelector(selector, base_addr, limit, (*lp >> 22) & 1,
		    (*lp >> 10) & 7, ((*lp >> 9) & 1) ? 0 : 1, (*lp >> 23) & 1))
	return -1;
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
