/* 	MS-DOS API translator for DOSEMU\'s DPMI Server
 *
 * DANG_BEGIN_MODULE msdos.h
 *
 * MS-DOS API translator allows DPMI programs to call DOS service directly
 * in protected mode.
 *
 * DANG_END_MODULE
 *
 * First Attempted by Dong Liu,  dliu@rice.njit.edu
 *
 * $Log$
 */

#ifndef lint
static char *rcsid = "$Header$";
#endif

enum { ES_INDEX = 0, CS_INDEX = 1, SS_INDEX = 2,  DS_INDEX = 3,
       FS_INDEX = 4, GS_INDEX = 5 };

static struct vm86_regs SAVED_REGS;
static struct vm86_regs MOUSE_SAVED_REGS;

#define S_REG(reg) (SAVED_REGS.##reg)

/* these are used like:  S_LO(ax) = 2 (sets al to 2) */
#define S_LO(reg)  (*(unsigned char *)&S_REG(e##reg))
#define S_HI(reg)  (*((unsigned char *)&S_REG(e##reg) + 1))

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define S_LWORD(reg)	(*((unsigned short *)&S_REG(reg)))
#define S_HWORD(reg)	(*((unsigned short *)&S_REG(reg) + 1))

/* used when passing a DTA higher than 1MB */
static void * DTA_over_1MB;
static void * DTA_under_1MB;

static inline unsigned long GetSegmentBaseAddress(unsigned short seg);
static inline int ConvertSegmentToDescriptor(unsigned short seg);
static inline int SetSegmentBaseAddress(unsigned short selector,
					unsigned long baseaddr);
static inline int SetSegmentLimit(unsigned short, unsigned int);
static inline int SetSelector(unsigned short, unsigned long,
                              unsigned int, unsigned char, unsigned char,
                              unsigned char, unsigned char,
                              unsigned char, unsigned char);
static inline void save_pm_regs();
static inline void restore_pm_regs();
static inline unsigned short AllocateDescriptors(int);

/* We use static varialbes because DOS in non-reentrant, but maybe a */
/* better way? */
static unsigned short DS_MAPPED;
static unsigned short ES_MAPPED;
static char READ_DS_COPIED;
static unsigned short CURRENT_PSP;
static unsigned short CURRENT_ENV_SEL;
static unsigned short PARENT_PSP;
static unsigned short PARENT_ENV_SEL;
static int in_dos_21 = 0;
static inline int old_dos_terminate(struct sigcontext_struct *scp)
{
    REG(cs)  = CURRENT_PSP;
    REG(eip) = 0x100;
    if (in_win31) {
	int i;
	unsigned short parent_seg;
	unsigned short parent_off;
#if 0	
	/* is seems that winos2 uses a PSP that not allocate via dos, */
	/* so we can\'t let dos to free it. just restore some DOS int */
	/* vectors and return. */

	/* restore terminate address address */
	((unsigned long *)0)[0x22] =
	             *(unsigned long *)((char *)(CURRENT_PSP<<4) + 0xa);
	/* restore Ctrl-Break address */
	((unsigned long *)0)[0x23] =
	             *(unsigned long *)((char *)(CURRENT_PSP<<4) + 0xe);
	/* restore critical error  address */
	((unsigned long *)0)[0x24] =
	             *(unsigned long *)((char *)(CURRENT_PSP<<4) + 0x12);
#endif
	DTA_over_1MB = 0;
	parent_off = *(unsigned short *)((char *)(CURRENT_PSP<<4) + 0xa);
	parent_seg = *(unsigned short *)((char *)(CURRENT_PSP<<4) + 0xa+2);
#if 1
	return 1;
#else	

        /* find the code selector */
	for (i=0; i < MAX_SELECTORS; i++)
	    if (Segments[i].used &&
		(Segments[i].type & MODIFY_LDT_CONTENTS_CODE) &&
		(Segments[i].base_addr == (unsigned long)(parent_seg << 4)) &&
		(Segments[i].limit > parent_off))
		break;
	if (i>=MAX_SELECTORS) {
	    error("DPMI: WARNNING !!! unvalid termninate address found in PSP\n");
	    return 1;
	} else
	    _cs = (i << 3)|7;
	_eip = parent_off;
	return 1;
#endif	
    }
    return 0;
}

/* DOS selector is a selector whose base address is less than 0xffff0 */
/* and para. aligned.                                                 */
static int inline is_dos_selector(unsigned short sel)
{
    unsigned long base = Segments[sel >> 3].base_addr;
    if ( (base > 0xffff0) || (base & 0xf)) {
      D_printf("DPMI: base adddress > 1MB or not para. aligned.\n");
      return 0;
    } else
      return 1;
}
		
/*
 * DANG_BEGIN_FUNCTION msdos_pre_extender
 *
 * This function is called before a protected mode client goes to real
 * mode for DOS service. All protected mode selector is changed to
 * real mode segment register. And if client\'s data buffer is above 1MB,
 * necessary buffer copying is performed. This function returns 1 if
 * it does not need to go to real mode, otherwise returns 0.
 *
 * DANG_END_FUNCTION
 */

static inline int
msdos_pre_extender(struct sigcontext_struct *scp, int intr)
{
    /* only consider DOS services */
    switch (intr) {
    case 0x20 ... 0x21:
    case 0x25 ... 0x26:
    case 0x33:			/* mouse function */
	break;
    default:
	return 0;
    }

    if (DTA_over_1MB && intr == 0x21 ) {
	switch (_HI(ax)) {	/* functions use DTA */
	case 0x0f ... 0x17:	/* FCB functions */
	case 0x21 ... 0x24:
	case 0x27 ... 0x29:
	case 0x4e ... 0x4f:	/* find first/next */
	    memcpy(DTA_under_1MB, DTA_over_1MB, 0x80);
	    break;
	}
    }

    DS_MAPPED = 0;
    ES_MAPPED = 0;
    switch (intr) {
    case 0x20:			/* DOS terminate */
	return old_dos_terminate(scp);
    case 0x21:
	if (in_dos_21) 
	    dbug_printf("DPMI: int 0x21 called recursively in_dos_21=%d.\n",in_dos_21);
	if (_HI(ax) != 0x4b )
	    in_dos_21++;
	SAVED_REGS = REGS;
	READ_DS_COPIED = 0;
	switch (_HI(ax)) {
	    /* first see if we don\'t need to go to real mode */
	case 0x25:		/* set vector */
	    Interrupt_Table[current_client][_LO(ax)].selector = _ds;
	    if (DPMIclient_is_32)
	      Interrupt_Table[current_client][_LO(ax)].offset = _edx;
	    else
	      Interrupt_Table[current_client][_LO(ax)].offset = _LWORD(edx);
	    D_printf("DPMI: int 21,ax=0x%04x, ds=0x%04x. dx=0x%04x\n",
		     _LWORD(eax), _ds, _LWORD(edx));
	    in_dos_21--;
	    return 1;
	case 0x35:	/* Get Interrupt Vector */
	    _es = Interrupt_Table[current_client][_LO(ax)].selector;
	    _ebx = Interrupt_Table[current_client][_LO(ax)].offset;
	    D_printf("DPMI: int 21,ax=0x%04x, es=0x%04x. bx=0x%04x\n",
		     _LWORD(eax), _es, _LWORD(ebx));
	    in_dos_21--;
	    return 1;
	case 0x01 ... 0x08:	/* These are dos functions which */
	case 0x0b ... 0x0e:	/* are not required memory copy, */
	case 0x19:		/* and segment register translation. */
	case 0x2a ... 0x34:
	case 0x36 ... 0x37:
	case 0x3e:
	case 0x42:
	case 0x45 ... 0x46:
	case 0x48 ... 0x4a:
	case 0x4d:
	case 0x54:
	case 0x58 ... 0x59:
	case 0x5c:		/* lock */
	case 0x66 ... 0x68:	
	case 0xF8:		/* OEM SET vector */
	    return 0;
	case 0x00:		/* DOS terminate */
	    in_dos_21--;
	    return old_dos_terminate(scp);
	case 0x09:		/* Print String */
	    if ( !is_dos_selector(_ds)) {
		REG(ds) =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		memcpy ((void *)(REG(ds)<<4),
			(void *)GetSegmentBaseAddress(_ds),
			Segments[_ds >> 3].limit);
	    }
	    return 0;
	case 0x0a:		/* buffered keyboard input */
	case 0x0f ... 0x18:	/* FCB functions */
	case 0x1b ... 0x24:
	case 0x27 ... 0x28:
	case 0x38:
	case 0x5a:		/* mktemp */
	case 0x5d:		/* Critical Error Information  */
	case 0x69:
	    DS_MAPPED = 1;
	    break;
	case 0x1a:		/* set DTA */
	    if ( !is_dos_selector(_ds)) {
		DTA_over_1MB = (void *)GetSegmentBaseAddress(_ds)
                                  + (DPMIclient_is_32 ? _edx : (_LWORD(edx))); 
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs+
		                  0x1000;
		REG(edx)=0;
		DTA_under_1MB = (void *)(REG(ds) << 4);
		memcpy(DTA_under_1MB, DTA_over_1MB, 0x80);
	    } else
		DTA_over_1MB = 0;
	    return 0;
	case 0x29:		/* Parse a file name for FCB */
	    {
		unsigned short seg =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		S_REG(es) = _es;
		S_REG(ds) = _ds;
		if ( !is_dos_selector(_ds)) {
		    REG(ds) = seg;
		    REG(esi) = 0;
		    memcpy ((void *)(REG(ds)<<4),
			    (void *)GetSegmentBaseAddress(_ds) +
			    (DPMIclient_is_32 ? _esi : (_LWORD(esi))),
			    0x100);
		    seg += 10;
		}
		if (!is_dos_selector(_es)) {
		    REG(es) = seg;
		    memcpy ((void *)((REG(es)<<4) + _LWORD(edi)),
			    (void *)GetSegmentBaseAddress(_es) +
			    (DPMIclient_is_32 ? _edi : (_LWORD(edi))),
			    0x20);
		}
	    }
	    return 0;
	case 0x44:		/* IOCTL */
	    switch (_LO(ax)) {
	    case 0x02 ... 0x05:
	    case 0x0c ... 0x0d:
		DS_MAPPED = 1;
		break;
	    default:
		return 0;
	    }
	    break;
	case 0x47:		/* GET CWD */
	    if ( !is_dos_selector(_ds)) {
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		READ_DS_COPIED = 1;
		S_REG(ds) = _ds;
	    }
	    return 0;
	case 0x4b:		/* EXEC */
	    D_printf("BCC: call dos exec.\n");
	    /* we must copy all data from higher 1MB to lower 1MB */
	    {
		unsigned short segment =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		void *p;
		unsigned short sel,off;
		
		if ( !is_dos_selector(_ds)) {
		    /* must copy command line */
		    REG(ds) = segment;
		    REG(edx) = 0;
		    p = (void *)GetSegmentBaseAddress(_ds) +
			(DPMIclient_is_32 ? _edx : (_LWORD(edx)));
		    strcpy((char *)(REG(ds)<<4), p);
		    segment += (strlen(p)>>4) + 1;
		}
		if ( !is_dos_selector( _es)) {
		    /* must copy parameter block */
		    REG(es) = segment;
		    REG(ebx) = 0;
		    memcpy((void *)(REG(es)<<4),
			   (void*)GetSegmentBaseAddress(_es) +
			   (DPMIclient_is_32 ? _ebx : (_LWORD(ebx))),
			   0x20);
		    segment += 2;
		}
		/* now the envrionment segment */
		sel = *(unsigned short *)((REG(es)<<4)+LWORD(ebx));
		if ( !is_dos_selector(sel)) {
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
		if ( !is_dos_selector(sel)) {
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
	    /* then the enviroment seg */
	    if (CURRENT_ENV_SEL)
 		*(unsigned short *)((char *)(CURRENT_PSP<<4) + 0x2c) =
 		    (unsigned long)(GetSegmentBaseAddress(CURRENT_ENV_SEL))
 				>> 4;
	    PARENT_ENV_SEL = CURRENT_ENV_SEL;
	    PARENT_PSP = CURRENT_PSP;
	    REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos_exec);
	    /* save context for child exits */
	    memcpy(&dpmi_stack_frame[current_client], scp,
		   sizeof(struct sigcontext_struct));
	    save_pm_regs();
	    return 0;
	case 0x50:		/* set PSP */
	    if (((LWORD(ebx) & 0x4) == 0x4) &&
		((LWORD(ebx) >> 3) < MAX_SELECTORS) &&
		Segments[LWORD(ebx) >> 3].used &&
		(Segments[LWORD(ebx) >> 3].base_addr < 0xffff0)) {
		unsigned short envp;

		REG(ebx) = (long) GetSegmentBaseAddress(LWORD(ebx)) >> 4;
		envp = *(unsigned short *)(((char *)(LWORD(ebx)<<4)) + 0x2c);
		if (((envp & 0x4) == 0x4) &&
		    ((envp >> 3) < MAX_SELECTORS) &&
		    Segments[envp >> 3].used && 
		    (Segments[envp >> 3].base_addr < 0xffff0)) {
		    CURRENT_ENV_SEL = envp;
		} else 
		    CURRENT_ENV_SEL = 0;
	    }
	    CURRENT_PSP = LWORD(ebx);
	    return 0;
	case 0x26:
	case 0x55:		/* create PSP */
	    if (((LWORD(edx) & 0x4) == 0x4) &&
		((LWORD(edx) >> 3) < MAX_SELECTORS) &&
		Segments[LWORD(edx) >> 3].used &&
		(Segments[LWORD(edx) >> 3].base_addr < 0xffff0))
		REG(edx) = (long) GetSegmentBaseAddress(LWORD(edx)) >> 4;
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
	case 0x5b:		/* Create */
	    if ( !is_dos_selector(_ds)) {
		D_printf("DPMI: passing ASCIIZ > 1MB to dos\n"); 
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		strcpy((void *)((REG(ds) << 4)+LWORD(edx)),
		       (void *)GetSegmentBaseAddress(_ds)+
		       (DPMIclient_is_32 ? _edx : (_LWORD(edx))));
	    }
	    return 0;
	case 0x3f:		/* dos read */
	    if ( !is_dos_selector(_ds)) {
		S_REG(ds) = _ds;
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		REG(edx) = 0;
		READ_DS_COPIED = 1;
	    } else
		READ_DS_COPIED = 0;
	    return 0;
	case 0x40:		/* DOS Write */
	    if ( !is_dos_selector(_ds)) {
		S_REG(ds) = _ds;
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		REG(edx) = 0;
		memcpy((void *)(REG(ds) << 4),
		       (void *)GetSegmentBaseAddress(S_REG(ds))+
		       (DPMIclient_is_32 ? _edx : (_LWORD(edx))),
		       LWORD(ecx));
		READ_DS_COPIED = 1;
	    } else
		READ_DS_COPIED = 0;
	    return 0;
	case 0x53:		/* Generate Drive Parameter Table  */
	    {
		unsigned short seg =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		S_REG(es) = _es;
		S_REG(ds) = _ds;
		if ( !is_dos_selector(_ds)) {
		    REG(ds) = seg;
		    REG(esi) = 0;
		    memcpy ((void *)(REG(ds)<<4),
			    (void *)GetSegmentBaseAddress(_ds) +
			    (DPMIclient_is_32 ? _edx : (_LWORD(esi))),
			    0x30);
		    seg += 30;
		}
		if ( !is_dos_selector(_es)) {
		    REG(es) = seg;
		    memcpy ((void *)((REG(es)<<4) + _LWORD(ebp)),
			    (void *)GetSegmentBaseAddress(_es) +
			    (DPMIclient_is_32 ? _ebp : (_LWORD(ebp))),
			    0x60);
		}
	    }
	    return 0;
	case 0x56:		/* rename file */
	    {
		unsigned short seg =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		if ( !is_dos_selector(_ds)) {
		    REG(ds) = seg;
		    REG(edx) = 0;
		    strcpy ((void *)(REG(ds)<<4),
			    (void *)GetSegmentBaseAddress(_ds) +
			    (DPMIclient_is_32 ? _edx : (_LWORD(edx))));
		    seg += 200;
		}
		if ( !is_dos_selector(_es)) {
		    REG(es) = seg;
		    strcpy ((void *)((REG(es)<<4) + _LWORD(edi)),
			    (void *)GetSegmentBaseAddress(_es) +
			    (DPMIclient_is_32 ? _edi : (_LWORD(edi))));
		}
	    }
	    return 0;
	case 0x57:		/* Get/Set File Date and Time Using Handle */
	    if ((_LO(ax) == 0) || (_LO(ax) == 1))
		return 0;
	    ES_MAPPED = 1;
	    break;
	case 0x5e:
	    if (_LO(ax) == 0x03)
		ES_MAPPED = 1;
	    else
		DS_MAPPED = 1;
	    break;
	case 0x5f:		/* redirection */
	    switch (_LO(ax)) {
	    case 0 ... 1:
		return 0;
	    case 2 ... 6:
		REG(ds) = DPMI_private_data_segment + DPMI_private_paragraphs;
		S_REG(ds) = _ds;
		REG(esi) = 0;
		memcpy ((void *)(REG(ds)<<4),
			(void *)GetSegmentBaseAddress(_ds) +
			(DPMIclient_is_32 ? _esi : (_LWORD(esi))),
			0x100);
		REG(es) = DPMI_private_data_segment + DPMI_private_paragraphs
		    + 100;
		S_REG(es) = _es;
		REG(edi) = 0;
		memcpy ((void *)(REG(es)<<4),
			(void *)GetSegmentBaseAddress(_es) +
			(DPMIclient_is_32 ? _edi : (_LWORD(edi))),
			0x100);
		return 0;
	    }
	case 0x60:		/* Get Fully Qualified File Name */
	    REG(ds) = DPMI_private_data_segment + DPMI_private_paragraphs;
	    S_REG(ds) = _ds;
	    REG(esi) = 0;
	    memcpy ((void *)(REG(ds)<<4),
		    (void *)GetSegmentBaseAddress(_ds) +
		    (DPMIclient_is_32 ? _esi : (_LWORD(esi))),
		    0x100);
	    REG(es) = DPMI_private_data_segment + DPMI_private_paragraphs
		+ 100;
	    S_REG(es) = _es;
	    REG(edi) = 0;
	    memcpy ((void *)(REG(es)<<4),
		    (void *)GetSegmentBaseAddress(_es) +
		    (DPMIclient_is_32 ? _edi : (_LWORD(edi))),
		    0x100);
	    return 0;
	case 0x6c:		/*  Extended Open/Create */
	    if ( !is_dos_selector(_ds)) {
		D_printf("DPMI: passing ASCIIZ > 1MB to dos\n"); 
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		strcpy((void *)((REG(ds) << 4)+LWORD(esi)),
		       (void *)GetSegmentBaseAddress(_ds)+
		       (DPMIclient_is_32 ? _esi : (_LWORD(esi))));
	    }
	    return 0;
	default:
	    break;
	}
	break;
    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk write */
	DS_MAPPED = 1;
	D_printf("DPMI: msdos Absolute Disk Read/Write called.\n");
	break;
    case 0x33:			/* mouse */
	MOUSE_SAVED_REGS = REGS;
	switch (_LWORD(eax)) {
	case 0x09:		/* Set Mouse Graphics Cursor */
	    if(!is_dos_selector(_es)) {
		REG(es) = DPMI_private_data_segment+DPMI_private_paragraphs;
		memcpy ((void *)(REG(es)<<4) +
				 (DPMIclient_is_32 ? _edx : (_LWORD(edx))),
			         (void *)GetSegmentBaseAddress(_es) +
				 (DPMIclient_is_32 ? _edx : (_LWORD(edx))),
				 16);
	    }
	    return 0;
	case 0x0c:		/* set call back */
	case 0x14:		/* swap call back */
	    if ( _es != 0 || _edx != 0) {
		D_printf("DPMI: set mouse callback\n");
		mouseCallBack.selector = _es;
		mouseCallBack.offset = (DPMIclient_is_32 ? _edx : (_LWORD(edx))); 
		REG(es) = DPMI_SEG;
		REG(edx) = DPMI_OFF + HLT_OFF(DPMI_mouse_callback);
	    } else {
		D_printf("DPMI: reset mouse callback\n");
		REG(es) =0;
		REG(edx) = 0;
	    }
	    return 0;
	case 0x16:		/* save state */
	case 0x17:		/* restore */
	    ES_MAPPED = 1;
	    break;
	default:
	    return 0;
	}
    default:
	return 0;
    }

    if (DS_MAPPED) {
	if ( !is_dos_selector(_ds)) {
	    DS_MAPPED = _ds;
	    REG(ds) =   DPMI_private_data_segment+DPMI_private_paragraphs;
	    memcpy ((void *)(REG(ds)<<4), (void *)GetSegmentBaseAddress(_ds),
		    (Segments[_ds >> 3].limit > 0xffff) ?
		    0xffff : Segments[_ds >> 3].limit);
	} else
	    DS_MAPPED = 0;
    }

    if (ES_MAPPED) {
	if ( !is_dos_selector(_es)) {
	    DS_MAPPED = _es;
	    REG(ds) =   DPMI_private_data_segment+DPMI_private_paragraphs;
	    memcpy ((void *)(REG(es)<<4), (void *)GetSegmentBaseAddress(_es),
		    (Segments[_es >> 3].limit > 0xffff) ?
		    0xffff : Segments[_es >> 3].limit);
	} else
	    ES_MAPPED = 0;
    }
    return 0;
}

static inline
msdos_post_exec()
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
    if (PARENT_ENV_SEL)
	*(unsigned short *)((char *)(PARENT_PSP<<4) + 0x2c) =
	                     PARENT_ENV_SEL;
    CURRENT_PSP = PARENT_PSP;
    return;
}

/*
 * DANG_BEGIN_FUNCTION msdos_post_extender
 *
 * This function is called after retrun from real mode DOS service
 * All real mode segment register is changed to protected mode selector
 * And if client\'s data buffer is above 1MB, necessary buffer copying
 * is performed.
 *
 * DANG_END_FUNCTION
 */

static inline void
msdos_post_extender(int intr)
{
    switch (intr) {
    case 0x15:
      switch(S_HI(ax)) {
        case 0xc0:      /* Get Configuartion */
                if (REG(eflags)&CF)
                        return;
                if (!(dpmi_stack_frame[current_client].es =
                         ConvertSegmentToDescriptor(REG(es))))
                return;
        default:
                return;
      }
    /* only copy buffer for dos services */
    case 0x21:
    case 0x25 ... 0x26:
    case 0x33:			/* mouse */
	break;
    default:
	return;
    }

    if (DTA_over_1MB && intr == 0x21 ) {
	switch (S_HI(ax)) {	/* functions use DTA */
	case 0x0f ... 0x17:	/* FCB functions */
	case 0x21 ... 0x24:
	case 0x27 ... 0x29:
	case 0x4e ... 0x4f:	/* find first/next */
	    memcpy(DTA_over_1MB, DTA_under_1MB, 0x80);
	    break;
	}
    }

    if (DS_MAPPED) {
	unsigned short my_ds;
	my_ds =   DPMI_private_data_segment+DPMI_private_paragraphs;
	memcpy ((void *)GetSegmentBaseAddress(DS_MAPPED), (void *)(my_ds<<4), 
		(Segments[DS_MAPPED >> 3].limit > 0xffff) ?
		0xffff : Segments[DS_MAPPED >> 3].limit);
    } 
    DS_MAPPED = 0;

    if (ES_MAPPED) {
	unsigned short my_es;
	my_es =   DPMI_private_data_segment + DPMI_private_paragraphs;
	memcpy ((void *)GetSegmentBaseAddress(ES_MAPPED), (void *)(my_es<<4), 
		(Segments[ES_MAPPED >> 3].limit > 0xffff) ?
		0xffff : Segments[ES_MAPPED >> 3].limit);
    } 
    ES_MAPPED = 0;
	       
    switch (intr) {
    case 0x21:
	in_dos_21--;
	switch (S_HI(ax)) {
	case 0x29:		/* Parse a file name for FCB */
	    {
		unsigned short seg =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		if ( Segments[ S_REG(ds) >> 3].base_addr > 0xffff0) {
		    memcpy ((void *)GetSegmentBaseAddress(S_REG(ds)) +
			    (DPMIclient_is_32 ? S_REG(esi) : (S_LWORD(esi))),
			    (void *)(REG(ds)<<4),
			    0x100);
		    dpmi_stack_frame[current_client].esi =
			(DPMIclient_is_32 ? S_REG(esi) : (S_LWORD(esi))) +
			LWORD(esi); 
		    seg += 10;
		}
		if (Segments[ S_REG(es) >>3].base_addr > 0xffff0) {
		    memcpy ((void *)GetSegmentBaseAddress(S_REG(es)) +
			    LWORD(edi),
			    (void *)((REG(es)<<4) + LWORD(edi)),  0x20);
		}
	    }
	    return;
	case 0x2f:		/* GET DTA */
	case 0x34:		/* Get Address of InDOS Flag */
	case 0x35:		/* GET Vector */
	case 0x52:		/* Get List of List */
        case 0x59:		/* Get EXTENDED ERROR INFORMATION */
	    dpmi_stack_frame[current_client].es =
	               ConvertSegmentToDescriptor(REG(es));
	    break;
	case 0x38:
	    if (S_LO(ax) == 0x00) { /* get contry info */
		dpmi_stack_frame[current_client].ds =
	               ConvertSegmentToDescriptor(REG(ds));
	    }
	    break;
	case 0x47:		/* get CWD */
	    if (LWORD(eflags) & CF)
		break;
	    if (READ_DS_COPIED)
		strcpy((void *)(GetSegmentBaseAddress
				(dpmi_stack_frame[current_client].ds) +
			(DPMIclient_is_32 ? S_REG(esi) : (S_LWORD(esi)))),
		        (void *)((REG(ds) << 4) + LWORD(esi)));
	    READ_DS_COPIED = 0;
	    break;
	case 0x48:		/* allocate memory */
	    if (LWORD(eflags) & CF)
		break;
	    dpmi_stack_frame[current_client].eax =
		ConvertSegmentToDescriptor(LWORD(eax));
	    break;
#if 0
	case 0x4e:		/* find first */
	case 0x4f:		/* find next */
	    if (LWORD(eflags) & CF)
		break;
	    if (DTA_over_1MB)
		memcpy(DTA_over_1MB, DTA_under_1MB, 43);
	    break;
#endif	    
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
	case 0x53:		/* Generate Drive Parameter Table  */
	    {
		unsigned short seg =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		if ( !is_dos_selector(S_REG(ds))) {
		    seg += 30;
		    dpmi_stack_frame[current_client].esi = S_REG(esi);
		}
		if ( !is_dos_selector(S_REG(es))) {
		    memcpy ((void *)GetSegmentBaseAddress(S_REG(es)) +
			    (DPMIclient_is_32 ? S_REG(ebp) : (S_LWORD(ebp))),
			    (void *)((REG(es)<<4) + LWORD(ebp)),
			    0x60);
		}
	    }
	    return ;
	case 0x56:		/* rename */
	    dpmi_stack_frame[current_client].edx = S_REG(edx);
	    dpmi_stack_frame[current_client].edi = S_REG(edi);
	    return ;
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
		memcpy((void *)(GetSegmentBaseAddress
				(dpmi_stack_frame[current_client].ds)
			 + (DPMIclient_is_32 ? S_REG(edx) : (S_LWORD(edx)))),
				(void *)(REG(ds) << 4), LWORD(eax));
	    }
	    READ_DS_COPIED = 0;
	    break;
	case 0x40:
	    if (READ_DS_COPIED)
		dpmi_stack_frame[current_client].edx = S_REG(edx);
	    READ_DS_COPIED = 0;
	    break;
	case 0x5f:		/* redirection */
	    switch (S_LO(ax)) {
	    case 0 ... 1:
		return ;
	    case 2 ... 6:
		dpmi_stack_frame[current_client].esi = S_REG(esi);
		memcpy ((void *)GetSegmentBaseAddress
			(dpmi_stack_frame[current_client].ds)
			+ (DPMIclient_is_32 ? S_REG(esi) :  (S_LWORD(esi))),
			(void *)(REG(ds)<<4),
			0x100);
		dpmi_stack_frame[current_client].edi = S_REG(edi);
		memcpy ((void *)GetSegmentBaseAddress
			(dpmi_stack_frame[current_client].es)
			+ (DPMIclient_is_32 ? S_REG(edi) :  (S_LWORD(edi))),
			(void *)(REG(es)<<4),
			0x100);
	    }
	    break;
	default:
	    return;
	}
	break;
    case 0x25:			/* Absolute Disk Read */
    case 0x26:			/* Absolute Disk Write */
	/* the flags should be pushed to stack */
	if (DPMIclient_is_32) {
	    dpmi_stack_frame[current_client].esp -= 4;
	    *(unsigned long *)(GetSegmentBaseAddress(
		dpmi_stack_frame[current_client].ss) +
	      dpmi_stack_frame[current_client].esp) = REG(eflags);
	} else {
	    dpmi_stack_frame[current_client].esp -= 2;
	    *(unsigned short *)(GetSegmentBaseAddress(
		dpmi_stack_frame[current_client].ss) +
	      (dpmi_stack_frame[current_client].esp&0xffff)) =
		 LWORD(eflags);
	}
	return;
    case 0x33:			/* mouse */
	switch (MOUSE_SAVED_REGS.eax & 0xffff) {
	case 0x14:		/* swap call back */
	    dpmi_stack_frame[current_client].es =
                  	    ConvertSegmentToDescriptor(REG(es)); 
	    return;
	case 0x19:		/* Get User Alternate Interrupt Address */
	    dpmi_stack_frame[current_client].ebx =
                  	    ConvertSegmentToDescriptor(LWORD(ebx)); 
	    return;
	default:
	    return;
	}
    default:
	return;
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
	
    
static inline int msdos_fix_cs_prefix (struct sigcontext_struct *scp)
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

static  inline int msdos_fault(struct sigcontext_struct *scp)
{
    unsigned char reg;
    unsigned short segment, desc;
    unsigned long len, base_addr;

    if ((_err & 0xffff) == 0) {	/*  not a selector error */
	/* although dpmiload allocates data alias selector for every */
	/* code selector, but it insists to write on code segment :=(.*/
	char fix_alias = 0;
	unsigned char *csp;
#if 0
	if (in_bcc) {
	    /* see if it is a cs prefix problem */
	    if (msdos_fix_cs_prefix(scp))
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
	}
#endif 	

	csp = (unsigned char *) SEL_ADR(_cs, _eip);
	switch (*csp) {
	case 0x2e:		/* cs: */
	    break;		/* do nothing */
	case 0x36:		/* ss: */
	    break;		/* do nothing */
	case 0x26:		/* es: */
	    if (_es == 0) {
		D_printf("DPMI: client tries to use use gdt 0 as es\n");
		_es = ConvertSegmentToDescriptor(0);
		fix_alias = 1;
	    }
	    break;
	case 0x64:		/* fs: */
	    if (_fs == 0) {
		D_printf("DPMI: client tries to use use gdt 0 as fs\n");
		_fs = ConvertSegmentToDescriptor(0);
		fix_alias = 1;
	    }
	    break;
	case 0x65:		/* gs: */
	    if (_gs == 0) {
		D_printf("DPMI: client tries to use use gdt 0 as es\n");
		_gs = ConvertSegmentToDescriptor(0);
		fix_alias = 1;
	    }
	    break;
	case 0xf2:		/* REPNE prefix */
	case 0xf3:		/* REP, REPE */
	    /* this might be a string insn */
	    switch (*(csp+1)) {
	    case 0xaa ... 0xab:		/* stos */
	    case 0xae ... 0xaf:	        /* scas */
		/* only use es */
		if (_es == 0) {
		    D_printf("DPMI: client tries to use use gdt 0 as es\n");
		    _es = ConvertSegmentToDescriptor(0);
		    fix_alias = 1;
		}
		break;
	    case 0xa4 ... 0xa5:		/* movs */
	    case 0xa6 ... 0xa7:         /* cmps */
		/* use both ds and es */
		if (_es == 0) {
		    D_printf("DPMI: client tries to use use gdt 0 as es\n");
		    _es = ConvertSegmentToDescriptor(0);
		    fix_alias = 1;
		}
		if (_ds == 0) {
		    D_printf("DPMI: client tries to use use gdt 0 as ds\n");
		    _ds = ConvertSegmentToDescriptor(0);
		    fix_alias = 1;
		}
		break;
	    }
	    break;
	case 0x3e:		/* ds: */
	default:		/* assmue default is using ds, but if the */
				/* client sets ss to 0, it is totally broken */
	    if (_ds == 0) {
		D_printf("DPMI: client tries to use use gdt 0 as ds\n");
		_ds = ConvertSegmentToDescriptor(0);
		fix_alias = 1;
	    }
	    break;
	}
	return fix_alias;
    }

    /* now it is a invalid selector error, try to fix it if it is */
    /* caused by an instruction mov Sreg,m/r16                    */

    len = decode_modify_segreg_insn(scp, &segment, &reg);
    if (len == 0) 
	return 0;

    D_printf("DPMI: try mov to a invalid selector 0x%04x\n", segment);

#if 1
    /* olny allow using some special GTD\'s */
    if ((segment != 0x0040) && (segment != 0xa000) &&
	(segment != 0xb000) && (segment != 0xb800) &&
	(segment != 0xc000) && (segment != 0xe000) && (segment != 0xf000))
	return 0;

    if (!(desc = ConvertSegmentToDescriptor(segment)))
	return 0;
    desc = desc >>3;

#else						     
    base_addr = ((unsigned long)segment) << 4;
    for(desc=0; desc < MAX_SELECTORS; desc++)
	if ((Segments[desc].base_addr == base_addr)&&
	    Segments[desc].used)
	    break;
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
