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


#define MAP_SEGMENT
#ifdef MAP_SEGMENT

static unsigned short DS_MAPPED;
static unsigned short ES_MAPPED;

static inline int
bcc_pre_extender(struct sigcontext_struct *scp, int intr)
{
    /* first see if we don\'t need to go to real mode */
    switch (intr) {
    case 0x21:
	switch (_HI(ax)) {
	case 0x25:		/* set vector */
	    Interrupt_Table[current_client][_LO(bx)].selector = _ds;
	    Interrupt_Table[current_client][_LO(bx)].offset = _edx;
	    return 1;
	case 0x35:	/* Get Interrupt Vector */
	    _es = Interrupt_Table[current_client][_LO(bx)].selector;
	    _ebx = Interrupt_Table[current_client][_LO(bx)].offset;
	    return 1;
	}
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
	SAVED_REGS = REGS;
	switch (HI(ax)) {
	case 0x50:		/* set PSP */
	    if (((LWORD(ebx) >> 3) < MAX_SELECTORS) &&
		Segments[LWORD(ebx) >> 3].used)
		REG(ebx) = (long) GetSegmentBaseAddress(LWORD(ebx)) >> 4;
	    break;
	}
    }
    return 0;
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
	default:
	    return;
	}
	break;
    default:
	return;
    }
}

#else /* MAP_SEGMENT */    

static inline
bcc_pre_extender(struct sigcontext_struct *scp, int intr)
{
    switch (intr) {
    case 0x21:
	SAVED_REGS = REGS;
	switch (HI(ax)) {
	case 0x09:		/* Dos print string */
	    if ( Segments[ _ds >> 3].base_addr > 0xffff0) {
		void *des , *src, *p;

		D_printf("DPMI: bcc passing pointer > 1MB to dos print\n");

		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;

		des = (void *)((REG(ds) << 4)+LWORD(edx));
		src = (void *)GetSegmentBaseAddress(_ds)+_LWORD(edx);
		
		p = memchr(src, '$',  0xffff);
		if (!p) 
		    *(char  *)des = '$';
		else
		    memcpy(des, src, p - src + 1);
	    }
	    return;
	case 0x1a:		/* set DTA */
	    if ( Segments[ _ds >> 3].base_addr > 0xffff0) {
		D_printf("DPMI: bcc passing pointer > 1MB to dos set DTA\n"); 
		
		DTA_over_1MB = (void *)GetSegmentBaseAddress(_ds)
		                                   + _LWORD(edx); 
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs+
		                  0x1000 - 0x8;
		REG(edx)=0;
		DTA_under_1MB = (void *)(REG(ds) << 4);
	    } else
		DTA_over_1MB = 0;
	    return;
	case 0x50:		/* set PSP */
	    if (((LWORD(ebx) >> 3) < MAX_SELECTORS) &&
		Segments[LWORD(ebx) >> 3].used)
		REG(ebx) = (long) GetSegmentBaseAddress(LWORD(ebx)) >> 4;
	    return;
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
	    return;
	case 0x3f:		/* dos read */
	    /* bcc sometime pass address > 1MB to dos to read, so */
	    /* frist read to our buffer, then copy it to client */
	    if ( Segments[ _ds >> 3].base_addr > 0xffff0)
		D_printf("DPMI: bcc passing pointer > 1MB to dos read\n"); 
	    S_REG(ds) = _ds;
	    REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
	    REG(edx) = 0;
	    return;
	case 0x40:		/* DOS Write */
	    if ( Segments[ _ds >> 3].base_addr > 0xffff0)
		D_printf("DPMI: bcc passing pointer > 1MB to dos write\n"); 
	    S_REG(ds) = _ds;
	    REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
	    REG(edx) = 0;
	    memcpy((void *)(REG(ds) << 4),
		   (void *)GetSegmentBaseAddress(S_REG(ds))+S_LWORD(edx),
		   LWORD(ecx));
	    return;
	case 0x47:		/* dos getcwd */
	    /* bcc sometime pass address > 1MB to dos to read, so */
	    /* frist read to our buffer, then copy it to client */
	    if ( Segments[ _ds >> 3].base_addr > 0xffff0)
		D_printf("DPMI: bcc passing pointer > 1MB to dos getcwd\n"); 
	    S_REG(ds) = _ds;
	    REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
	    return;
	}
	break;
    case 0x3f:			/* for gdb */
	D_printf("DPMI: int $0x3f called\n");
	break;
    }
}

static inline
bcc_post_extender(int intr)
{
    switch (intr) {
    case 0x21:
	switch (S_HI(ax)) {
	case 0x1a:		/* set DTA */
	    dpmi_stack_frame[current_client].edx = S_REG(edx);
	    break;
	case 0x2f:		/* GET DTA */
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
	    dpmi_stack_frame[current_client].edx = S_REG(edx);
	    if (LWORD(eflags) & CF)
		return;
	    memcpy((void *)(GetSegmentBaseAddress(S_REG(ds)) + S_LWORD(edx)),
			    (void *)(REG(ds) << 4), LWORD(eax));
	    break;
	case 0x40:
	    dpmi_stack_frame[current_client].edx = S_REG(edx);
	    break;
	case 0x47:		/* getcwd */
	    if (LWORD(eflags) & CF)
		return;
	    strcpy((char *)(GetSegmentBaseAddress(S_REG(ds)) + S_LWORD(esi)),
			    (char *)(REG(ds) << 4)+ LWORD(esi));
	    return;
	default:
	    return;
	}
	break;
    default:
	return;
    }
}
#endif /* MAP_SEGMENT */

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
#if 1
    if ((desc == _ds) && ((base_addr&0xffff) == _LWORD(esi))) {
	dpmiload_mem_info *info = (dpmiload_mem_info *)(
	    GetSegmentBaseAddress(_ds) + _LWORD(esi));

	D_printf("DPMI: bcc uses ds:si as base address\n");
#if 0
	if (info -> handle == 0)  /* no need to setdescriptor */
	    return 0;
	base_addr = info->linear_address;
	limit = info->size;
#endif
	base_addr = Segments[_ds>>3].base_addr;
	limit = info->size;
	fix_code_alias = 0;
    }
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

static inline unsigned char *
decode_8e_index(struct sigcontext_struct *scp, unsigned char *prefix,
		int rm)
{
    switch (rm) {
    case 0:
	if (prefix)
	    return prefix + _LWORD(ebx) + _LWORD(esi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ds)+
				     _LWORD(ebx) + _LWORD(esi));
    case 1:
	if (prefix)
	    return prefix + _LWORD(ebx) + _LWORD(edi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ds)+
				     _LWORD(ebx) + _LWORD(edi));
    case 2:
	if (prefix)
	    return prefix + _LWORD(ebp) + _LWORD(esi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ss)+
				     _LWORD(ebp) + _LWORD(esi));
    case 3:
	if (prefix)
	    return prefix + _LWORD(ebp) + _LWORD(edi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ss)+
				     _LWORD(ebp) + _LWORD(edi));
    case 4:
	if (prefix)
	    return prefix + _LWORD(esi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ds)+
				     _LWORD(esi));
    case 5:
	if (prefix)
	    return prefix + _LWORD(edi);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ds)+
				     _LWORD(edi));
    case 6:
	if (prefix)
	    return prefix + _LWORD(ebp);
	else
	    return (unsigned char *)(GetSegmentBaseAddress(_ss)+
				     _LWORD(ebp));
    case 7:
	if (prefix)
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

    prefix = 0;
    switch (*csp) {
    case 0x2e:
	prefix = (unsigned char *)GetSegmentBaseAddress(_cs);
	break;
    case 0x36:
	prefix = (unsigned char *)GetSegmentBaseAddress(_ss);
	break;
    case 0x3e:
	prefix = (unsigned char *)GetSegmentBaseAddress(_ds);
	break;
    case 0x26:
	prefix = (unsigned char *)GetSegmentBaseAddress(_es);
	break;
    case 0x64:
	prefix = (unsigned char *)GetSegmentBaseAddress(_fs);
	break;
    case 0x65:
	prefix = (unsigned char *)GetSegmentBaseAddress(_gs);
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

    csp = (unsigned char *) SEL_ADR(_cs, _eip);

    if ((Segments[_cs>>3].is_32) && (*csp == 0x66)) { /* Operand-Size prefix */
	csp++;
	len++;
    }

    prefix = check_prefix(scp);
    if (prefix) {
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
	    if(prefix)
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
				   (int)(*(csp+1)));
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

    csp = (unsigned char *) SEL_ADR(_cs, _eip);

    prefix = check_prefix(scp);
    if (prefix) {
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
	    if(prefix)
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
				   (int)(*(csp+1)));
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
    int len ;

    /* first try mov sreg, .. */
    if ((len = decode_8e(scp, segment, sreg)))
	return len;
    /* then try lds, les ... */
    if ((len = decode_load_descriptor(scp, segment, sreg)))
	return len;

    /* then try pop sreg */
    len = 0;
    csp = (unsigned char *) SEL_ADR(_cs, _eip);
    ssp = (unsigned short *) SEL_ADR(_ss, _esp);
    switch (*csp) {
    case 0x1f:			/* pop ds */
	len = 1;
	*segment = *ssp;
	*sreg = DS_INDEX;
	_esp += 2;
	break;
    case 0x07:			/* pop es */
	len = 1;
	*segment = *ssp;
	*sreg = ES_INDEX;
	_esp += 2;
	break;
    case 0x17:			/* pop ss */
	len = 1;
	*segment = *ssp;
	*sreg = SS_INDEX;
	_esp += 2;
	break;
    case 0x0f:		/* two byte opcode */
	csp++;
	switch (*csp) {
	case 0xa1:		/* pop fs */
	    len = 2;
	    *segment = *ssp;
	    *sreg = FS_INDEX;
	    _esp += 2;
	    break;
	case 0xa9:		/* pop gs */
	    len = 2;
	    *segment = *ssp;
	    *sreg = FS_INDEX;
	    _esp += 2;
	break;
	}
    }
    return len;
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
	if (Segments[desc].base_addr == base_addr)
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
