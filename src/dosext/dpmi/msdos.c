/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 	MS-DOS API translator for DOSEMU\'s DPMI Server
 *
 * DANG_BEGIN_MODULE msdos.c
 *
 * REMARK
 * MS-DOS API translator allows DPMI programs to call DOS service directly
 * in protected mode.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * First Attempted by Dong Liu,  dliu@rice.njit.edu
 *
 */

#include <string.h>

#include "emu.h"
#include "emu-ldt.h"
#include "bios.h"
#include "dpmi.h"
#include "msdos.h"

enum { ES_INDEX = 0, CS_INDEX = 1, SS_INDEX = 2,  DS_INDEX = 3,
       FS_INDEX = 4, GS_INDEX = 5 };

#ifdef __linux__
static struct vm86_regs SAVED_REGS;
static struct vm86_regs MOUSE_SAVED_REGS;
static struct vm86_regs VIDEO_SAVED_REGS;
static struct vm86_regs INT15_SAVED_REGS;
#define S_REG(reg) (SAVED_REGS.reg)
#endif


/* these are used like:  S_LO(ax) = 2 (sets al to 2) */
#define S_LO(reg)  (*(unsigned char *)&S_REG(e##reg))
#define S_HI(reg)  (*((unsigned char *)&S_REG(e##reg) + 1))

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define S_LWORD(reg)	(*((unsigned short *)&S_REG(reg)))
#define S_HWORD(reg)	(*((unsigned short *)&S_REG(reg) + 1))

#define DTA_Para_ADD 0x1000

#define DTA_over_1MB (void*)(GetSegmentBaseAddress(USER_DTA_SEL) + USER_DTA_OFF)
#define DTA_under_1MB (void*)((DPMI_private_data_segment + \
    DPMI_private_paragraphs + DTA_Para_ADD) << 4)

#define MAX_DOS_PATH 128

/* used when passing a DTA higher than 1MB */
unsigned short USER_DTA_SEL;
static unsigned long USER_DTA_OFF;
unsigned short USER_PSP_SEL;
static void *user_FCB;

/* We use static varialbes because DOS in non-reentrant, but maybe a */
/* better way? */
static unsigned short DS_MAPPED;
static unsigned short ES_MAPPED;
static char READ_DS_COPIED;
unsigned short CURRENT_PSP;
unsigned short CURRENT_ENV_SEL;
static unsigned short PARENT_PSP;
static unsigned short PARENT_ENV_SEL;
static int in_dos_21 = 0;
static int last_dos_21 = 0;

static int old_dos_terminate(struct sigcontext_struct *scp)
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
	/*DTA_over_1MB = 0;*/
	parent_off = *(unsigned short *)((char *)(CURRENT_PSP<<4) + 0xa);
	parent_seg = *(unsigned short *)((char *)(CURRENT_PSP<<4) + 0xa+2);
#if 0
	return 1;
#else	

#if 1	
        /* find the code selector */
	for (i=0; i < MAX_SELECTORS; i++)
	    if (Segments[i].used &&
		(Segments[i].type & MODIFY_LDT_CONTENTS_CODE) &&
		(Segments[i].base_addr == (unsigned long)(parent_seg << 4)) &&
		(Segments[i].limit > parent_off))
		break;
	if (i>=MAX_SELECTORS) {
	    error("DPMI: WARNING !!! invalid terminate address found in PSP\n");
	    return 1;
	} else
	    _cs = (i << 3)|7;
	_eip = parent_off;
#endif	
	/* set parent\'s PSP to itself, so dos won\'t free memory */
	/* I believe winkernel puts the selector of parent psp there */
	PARENT_PSP = *(unsigned short *)((char *)(CURRENT_PSP<<4) + 0x16);
	*(unsigned short *)((char *)(CURRENT_PSP<<4) + 0x16) = CURRENT_PSP;
	
	/* put our return address there */
	*(unsigned short *)((char *)(CURRENT_PSP<<4) + 0xa) =
	     DPMI_OFF + HLT_OFF(DPMI_return_from_dosint) + 0x21;
	*(unsigned short *)((char *)(CURRENT_PSP<<4) + 0xa+2) = DPMI_SEG;
	return 0;
#endif	
    }
    return 0;
}

/* DOS selector is a selector whose base address is less than 0xffff0 */
/* and para. aligned.                                                 */
static int inline is_dos_selector(unsigned short sel)
{
    unsigned long base = Segments[sel >> 3].base_addr;

    if (base > 0x10ffef) {	/* ffff:ffff for DOS high */
      D_printf("DPMI: base address %#lx of sel %#x > DOS limit\n", base, sel);
      return 0;
    } else
    if (base & 0xf) {
      D_printf("DPMI: base address %#lx of sel %#x not para. aligned.\n", base, sel);
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

int msdos_pre_extender(struct sigcontext_struct *scp, int intr)
{
    /* only consider DOS and some BIOS services */
    switch (intr) {
    case 0x10: case 0x15:
    case 0x20: case 0x21:
    case 0x25: case 0x26:
    case 0x33:			/* mouse function */
	break;
    case 0x2f:
	if (_LWORD(eax) == 0x1684) {
	    D_printf("DPMI: Get VxD entry point BX = 0x%04x\n",
		     _LWORD(ebx));
	    /* no entry point */
	    _es = _edi = 0;
	    return 1;
	}
	return 0;
    case 0x41:			/* win debug */
	return 1;
    default:
	return 0;
    }

    if (USER_DTA_SEL && intr == 0x21 ) {
	switch (_HI(ax)) {	/* functions use DTA */
	case 0x11: case 0x12:	/* find first/next using FCB */
	case 0x4e: case 0x4f:	/* find first/next */
	    memmove(DTA_under_1MB, DTA_over_1MB, 0x80);
	    break;
	}
    }

    D_printf("DPMI: pre_extender: int 0x%x, ax=0x%x\n", intr, _LWORD(eax));
    DS_MAPPED = 0;
    ES_MAPPED = 0;
    switch (intr) {
    case 0x10:			/* video */
	VIDEO_SAVED_REGS = REGS;
	switch (_HI(ax)) {
	case 0x10:		/* Set/Get Palette Registers (EGA/VGA) */
	    switch(_LO(ax)) {
	    case 0x2:		/* set all palette registers and border */
	    case 0x09:		/* ead palette registers and border (PS/2) */
	    case 0x12:		/* set block of DAC color registers */
	    case 0x17:		/* read block of DAC color registers */
		ES_MAPPED = 1;
		break;
	    default:
		return 0;
	    }
	    break;
	case 0x11:		/* Character Generator Routine (EGA/VGA) */
	    switch (_LO(ax)) {
	    case 0x0:		/* user character load */
	    case 0x10:		/* user specified character definition table */
	    case 0x20: case 0x21:
		ES_MAPPED = 1;
		break;
	    default:
		return 0;
	    }
	    break;
	case 0x13:		/* Write String */
	case 0x15:		/*  Return Physical Display Parms */
	case 0x1b:
	    ES_MAPPED = 1;
	    break;
	case 0x1c:
	    if (_LO(ax) == 1 || _LO(ax) == 2)
		ES_MAPPED = 1;
	    else
		return 0;
	    break;
	default:
	    return 0;
	}
	break;
    case 0x15:			/* misc */
	INT15_SAVED_REGS = REGS;
	return 0;
    case 0x20:			/* DOS terminate */
	return old_dos_terminate(scp);
    case 0x21:
	if (in_dos_21) 
	dbug_printf("DPMI: int21 AX=%#04x called recursively "
		    "from inside %#04x, in_dos_21=%d\n",
		_LWORD(eax), last_dos_21, in_dos_21);
	last_dos_21 = _LWORD(eax);

	SAVED_REGS = REGS;
	READ_DS_COPIED = 0;
	switch (_HI(ax)) {
	    /* first see if we don\'t need to go to real mode */
	case 0x25:		/* set vector */
	    Interrupt_Table[_LO(ax)].selector = _ds;
	    if (DPMIclient_is_32)
	      Interrupt_Table[_LO(ax)].offset = _edx;
	    else
	      Interrupt_Table[_LO(ax)].offset = _LWORD(edx);
	    D_printf("DPMI: int 21,ax=0x%04x, ds=0x%04x. dx=0x%04x\n",
		     _LWORD(eax), _ds, _LWORD(edx));
	    return 1;
	case 0x35:	/* Get Interrupt Vector */
	    _es = Interrupt_Table[_LO(ax)].selector;
	    _ebx = Interrupt_Table[_LO(ax)].offset;
	    D_printf("DPMI: int 21,ax=0x%04x, es=0x%04x. bx=0x%04x\n",
		     _LWORD(eax), _es, _LWORD(ebx));
	    return 1;
	case 0x48:		/* allocate memory */
	    {
		dpmi_pm_block *bp = DPMImalloc(_LWORD(ebx)<<4);
		if (!bp) {
		    _eflags |= CF;
		    _LWORD(ebx) = dpmi_free_memory >> 4;
		    _LWORD(eax) = 0x08;
		} else {
		    unsigned short sel = AllocateDescriptors(1);
		    SetSegmentBaseAddress(sel, (unsigned long)bp->base);
		    SetSegmentLimit(sel, bp -> size - 1);
		    _LWORD(eax) = sel;
		    _eflags &= ~CF;
		}
		return 1;
	    }
	case 0x49:		/* free memory */
	    {
		unsigned long h =
		    base2handle((void *)GetSegmentBaseAddress(_es));
		if (!h) 
		    _eflags |= CF;
		else {
		    _eflags &= ~CF;
		    DPMIfree(h);
		    FreeDescriptor(_es);
		    _es = 0;
		}
		return 1;
	    }
	case 0x4a:		/* reallocate memory */
	    {
		unsigned long h;
		dpmi_pm_block *bp;

		h = base2handle((void *)GetSegmentBaseAddress(_es));
		if (!h) {
		    _eflags |= CF;
		    return 1;
		}
		bp = DPMIrealloc(h, _LWORD(ebx)<<4);
		if (!bp) {
		    _eflags |= CF;
		    _LWORD(ebx) = dpmi_free_memory >> 4;
		    _LWORD(eax) = 0x08;
		} else {
		    SetSegmentBaseAddress(_es, (unsigned long)bp->base);
		    SetSegmentLimit(_es, bp -> size - 1);
		    _eflags &= ~CF;
		}
		return 1;
	    }
	case 0x01 ... 0x08:	/* These are dos functions which */
	case 0x0b ... 0x0e:	/* are not required memory copy, */
	case 0x19:		/* and segment register translation. */
	case 0x2a ... 0x2e:
	case 0x30 ... 0x34:
	case 0x36: case 0x37:
	case 0x3e:
	case 0x42:
	case 0x45: case 0x46:
	case 0x4d:
	case 0x4f:		/* find next */
	case 0x54:
	case 0x58: case 0x59:
	case 0x5c:		/* lock */
	case 0x66 ... 0x68:	
	case 0xF8:		/* OEM SET vector */
	    in_dos_21++;
	    return 0;
	case 0x00:		/* DOS terminate */
	    return old_dos_terminate(scp);
	case 0x09:		/* Print String */
	    if ( !is_dos_selector(_ds)) {
		int i;
		char *s, *d;
		REG(ds) =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		REG(edx) = 0;
		d = (char *)(REG(ds)<<4);
		s = (char *)SEL_ADR(_ds, _edx);
		for(i=0; i<0xffff; i++, d++, s++) {
		    *d = *s;
		    if( *s == '$')
			break;
		}
	    } else {
                REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
	    }
	    in_dos_21++;
	    return 0;
	case 0x0a:		/* buffered keyboard input */
	case 0x38:
	case 0x5a:		/* mktemp */
	case 0x5d:		/* Critical Error Information  */
	case 0x69:
	    DS_MAPPED = 1;
	    break;
	case 0x1a:		/* set DTA */
	    if ( !is_dos_selector(_ds)) {
		USER_DTA_SEL = _ds;
		USER_DTA_OFF = DPMIclient_is_32 ? _edx : _LWORD(edx);
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs+
		                  DTA_Para_ADD;
		REG(edx)=0;
                memmove(DTA_under_1MB, DTA_over_1MB, 0x80);
	    } else {
                REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
                USER_DTA_SEL = 0;
            }
            in_dos_21++;
	    return 0;
            
	/* FCB functions */	    
	case 0x0f: case 0x10:	/* These are not supported by */
	case 0x14: case 0x15:	/* dosx.exe, according to Ralf Brown */
	case 0x21 ... 0x24:
	case 0x27: case 0x28:
	    _HI(ax) = 0xff;
	    return 1;
	case 0x11: case 0x12:	/* find first/next using FCB */
	case 0x13:		/* Delete using FCB */
	case 0x16:		/* Create usring FCB */
	case 0x17:		/* rename using FCB */
	    user_FCB = (void *)SEL_ADR(_ds, _edx);
	    REG(ds) =	DPMI_private_data_segment+DPMI_private_paragraphs;
	    memmove(SEG_ADR((void *), ds, dx), user_FCB, 0x50);
	    in_dos_21++;
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
		    memmove ((void *)(REG(ds)<<4),
			    (char *)GetSegmentBaseAddress(_ds) +
			    (DPMIclient_is_32 ? _esi : (_LWORD(esi))),
			    0x100);
		    seg += 10;
                } else {
                    REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
		}
		if (!is_dos_selector(_es)) {
		    REG(es) = seg;
		    memmove ((void *)((REG(es)<<4) + _LWORD(edi)),
			    (char *)GetSegmentBaseAddress(_es) +
			    (DPMIclient_is_32 ? _edi : (_LWORD(edi))),
			    0x50);
                } else {
                    REG(es) = GetSegmentBaseAddress(_es) >> 4;
		}
	    }
	    in_dos_21++;
	    return 0;
	case 0x44:		/* IOCTL */
	    switch (_LO(ax)) {
	    case 0x02 ... 0x05:
	    case 0x0c: case 0x0d:
		DS_MAPPED = 1;
		break;
	    default:
		in_dos_21++;
		return 0;
	    }
	    break;
	case 0x47:		/* GET CWD */
	    if ( !is_dos_selector(_ds)) {
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		READ_DS_COPIED = 1;
		S_REG(ds) = _ds;
	    } else {
                REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
	    }
	    in_dos_21++;
	    return 0;
	case 0x4b:		/* EXEC */
	    D_printf("BCC: call dos exec.\n");
	    /* we must copy all data from higher 1MB to lower 1MB */
	    {
		unsigned short segment =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		char *p;
		unsigned short sel,off;
		
		if ( !is_dos_selector(_ds)) {
		    /* must copy command line */
		    REG(ds) = segment;
		    REG(edx) = 0;
		    p = (char *)GetSegmentBaseAddress(_ds) +
			(DPMIclient_is_32 ? _edx : (_LWORD(edx)));
		    snprintf((char *)(REG(ds)<<4), MAX_DOS_PATH, "%s", p);
		    segment += strlen((char *)(REG(ds)>>4)) + 1;
                } else {
                    REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
		}
		if ( !is_dos_selector( _es)) {
		    /* must copy parameter block */
		    REG(es) = segment;
		    REG(ebx) = 0;
		    memmove((void *)(REG(es)<<4),
			   (char *)GetSegmentBaseAddress(_es) +
			   (DPMIclient_is_32 ? _ebx : (_LWORD(ebx))),
			   0x20);
		    segment += 2;
                } else {
                    REG(es) = GetSegmentBaseAddress(_es) >> 4;
		}
		/* now the envrionment segment */
		sel = *(unsigned short *)((REG(es)<<4)+LWORD(ebx));
		if ( !is_dos_selector(sel)) {
		    *(unsigned short *)((REG(es)<<4)+LWORD(ebx)) =
			segment;
		    memmove((void *)(segment <<4),           /* 4K envr. */
			   (char *)GetSegmentBaseAddress(sel),
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
		    memmove((void *)(segment<<4),
			   (char *)GetSegmentBaseAddress(sel) + off,
			   0x80);
		    segment += 8;
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
 		    GetSegmentBaseAddress(CURRENT_ENV_SEL) >> 4;
	    PARENT_ENV_SEL = CURRENT_ENV_SEL;
	    PARENT_PSP = CURRENT_PSP;
	    REG(eip) = DPMI_OFF + HLT_OFF(DPMI_return_from_dos_exec);
	    /* save context for child exits */
	    save_pm_regs(scp);
	    return 0;
	case 0x50:		/* set PSP */
	  {
	    unsigned short envp;
	    if ( !is_dos_selector(_LWORD(ebx))) {

		USER_PSP_SEL = _LWORD(ebx);
		LWORD(ebx) = CURRENT_PSP;
		memmove((void *)(LWORD(ebx) << 4), 
		    (char *)GetSegmentBaseAddress(_LWORD(ebx)), 0x100);
		D_printf("DPMI: PSP moved from %p to %p\n",
		    (char *)GetSegmentBaseAddress(_LWORD(ebx)),
		    (void *)(LWORD(ebx) << 4));
	    } else {
		REG(ebx) = GetSegmentBaseAddress(_LWORD(ebx)) >> 4;
		USER_PSP_SEL = 0;
	    }
	    CURRENT_PSP = LWORD(ebx);
	    envp = *(unsigned short *)(((char *)(LWORD(ebx)<<4)) + 0x2c);
	    if ( !is_dos_selector(envp)) {
		/* DANG_FIXTHIS: Please implement the ENV translation! */
		error("FIXME: ENV translation is not implemented\n");
		CURRENT_ENV_SEL = 0;
	    } else {
		CURRENT_ENV_SEL = envp;
	    }
	  }

	    in_dos_21++;
	    return 0;

	case 0x26:
	case 0x55:		/* create PSP */
	    if ( !is_dos_selector(_LWORD(edx))) {
		REG(edx) = DPMI_private_data_segment+DPMI_private_paragraphs;
	    } else {
		REG(edx) = GetSegmentBaseAddress(_LWORD(edx)) >> 4;
	    }
	    in_dos_21++;
	    return 0;
	case 0x39:		/* mkdir */
	case 0x3a:		/* rmdir */
	case 0x3b:		/* chdir */
	case 0x3c:		/* creat */
	case 0x3d:		/* Dos OPEN */
	case 0x41:		/* unlink */
	case 0x43:		/* change attr */
	case 0x4e:		/* find first */
	case 0x5b:		/* Create */
	    if ((_HI(ax) == 0x4e) && (_ecx & 0x8))
		D_printf("DPMI: MS-DOS try to find volume label\n");
	    if ( !is_dos_selector(_ds)) {
		char *src, *dst;
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		src = (char *)(GetSegmentBaseAddress(_ds)+(DPMIclient_is_32 ? _edx : (_LWORD(edx))));
		dst = (char *)((REG(ds) << 4)+LWORD(edx));
		D_printf("DPMI: passing ASCIIZ > 1MB to dos %#x\n", (int)dst); 
		D_printf("%#x: '%s'\n", (int)src, src);
                snprintf(dst, MAX_DOS_PATH, "%s", src);
	    } else {
                REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
	    }
	    in_dos_21++;
	    return 0;
	case 0x3f:		/* dos read */
	    if ( !is_dos_selector(_ds)) {
		S_REG(ds) = _ds;
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		REG(edx) = 0;
		READ_DS_COPIED = 1;
	    } else {
                REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
		READ_DS_COPIED = 0;
            }
	    in_dos_21++;
	    return 0;
	case 0x40:		/* DOS Write */
	    if ( !is_dos_selector(_ds)) {
		S_REG(ds) = _ds;
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		REG(edx) = 0;
		memmove((void *)(REG(ds) << 4),
		       (char *)GetSegmentBaseAddress(S_REG(ds))+
		       (DPMIclient_is_32 ? _edx : (_LWORD(edx))),
		       LWORD(ecx));
		READ_DS_COPIED = 1;
	    } else {
                REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
		READ_DS_COPIED = 0;
            }
	    in_dos_21++;
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
		    memmove ((void *)(REG(ds)<<4),
			    (char *)GetSegmentBaseAddress(_ds) +
			    (DPMIclient_is_32 ? _edx : (_LWORD(esi))),
			    0x30);
		    seg += 30;
                } else {
                    REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
		}
		if ( !is_dos_selector(_es)) {
		    REG(es) = seg;
		    memmove ((void *)((REG(es)<<4) + _LWORD(ebp)),
			    (char *)GetSegmentBaseAddress(_es) +
			    (DPMIclient_is_32 ? _ebp : (_LWORD(ebp))),
			    0x60);
                } else {
                    REG(es) = GetSegmentBaseAddress(_es) >> 4;
		}
	    }
	    in_dos_21++;
	    return 0;
	case 0x56:		/* rename file */
	    {
		unsigned short seg =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		if ( !is_dos_selector(_ds)) {
		    REG(ds) = seg;
		    REG(edx) = 0;
		    snprintf((char *)(REG(ds)<<4), MAX_DOS_PATH, "%s",
			     (char *)GetSegmentBaseAddress(_ds) +
			     (DPMIclient_is_32 ? _edx : (_LWORD(edx))));
		    seg += 200;
                } else {
                    REG(ds) = (long) GetSegmentBaseAddress(_ds) >> 4;
		}
		if ( !is_dos_selector(_es)) {
		    REG(es) = seg;
		    snprintf((char *)((REG(es)<<4) + _LWORD(edi)),
                             MAX_DOS_PATH, "%s",
			     (char *)GetSegmentBaseAddress(_es) +
			     (DPMIclient_is_32 ? _edi : (_LWORD(edi))));
                } else {
                    REG(es) = (long) GetSegmentBaseAddress(_es) >> 4;
		}
	    }
	    in_dos_21++;
	    return 0;
	case 0x57:		/* Get/Set File Date and Time Using Handle */
	    if ((_LO(ax) == 0) || (_LO(ax) == 1)) {
		in_dos_21++;
		return 0;
	    }
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
	    case 0: case 1:
		in_dos_21++;
		return 0;
	    case 2 ... 6:
		REG(ds) = DPMI_private_data_segment + DPMI_private_paragraphs;
		S_REG(ds) = _ds;
		REG(esi) = 0;
		memmove ((void *)(REG(ds)<<4),
			(char *)GetSegmentBaseAddress(_ds) +
			(DPMIclient_is_32 ? _esi : (_LWORD(esi))),
			0x100);
		REG(es) = DPMI_private_data_segment + DPMI_private_paragraphs
		    + 100;
		S_REG(es) = _es;
		REG(edi) = 0;
		memmove ((void *)(REG(es)<<4),
			(char *)GetSegmentBaseAddress(_es) +
			(DPMIclient_is_32 ? _edi : (_LWORD(edi))),
			0x100);
		in_dos_21++;
		return 0;
	    }
	case 0x60:		/* Get Fully Qualified File Name */
	    REG(ds) = DPMI_private_data_segment + DPMI_private_paragraphs;
	    S_REG(ds) = _ds;
	    REG(esi) = 0;
	    memmove ((void *)(REG(ds)<<4),
		    (char *)GetSegmentBaseAddress(_ds) +
		    (DPMIclient_is_32 ? _esi : (_LWORD(esi))),
		    0x100);
	    REG(es) = DPMI_private_data_segment + DPMI_private_paragraphs
		+ 100;
	    S_REG(es) = _es;
	    REG(edi) = 0;
	    memmove ((void *)(REG(es)<<4),
		    (char *)GetSegmentBaseAddress(_es) +
		    (DPMIclient_is_32 ? _edi : (_LWORD(edi))),
		    0x100);
	    in_dos_21++;
	    return 0;
	case 0x6c:		/*  Extended Open/Create */
	    if ( !is_dos_selector(_ds)) {
		char *src, *dst;
		REG(ds) = DPMI_private_data_segment+DPMI_private_paragraphs;
		src = (char *)(GetSegmentBaseAddress(_ds)+(DPMIclient_is_32 ? _esi : (_LWORD(esi))));
		dst = (char *)((REG(ds) << 4)+LWORD(esi));
		D_printf("DPMI: passing ASCIIZ > 1MB to dos %#x\n", (int)dst); 
		D_printf("%#x: '%s'\n", (int)src, src);
		snprintf(dst, MAX_DOS_PATH, "%s", src);
            } else {
                REG(ds) = GetSegmentBaseAddress(_ds) >> 4;
	    }
	    in_dos_21++;
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
		memmove ((char *)(REG(es)<<4) +
				 (DPMIclient_is_32 ? _edx : (_LWORD(edx))),
			         (char *)GetSegmentBaseAddress(_es) +
				 (DPMIclient_is_32 ? _edx : (_LWORD(edx))),
				 16);
            } else {
                REG(es) = GetSegmentBaseAddress(_es) >> 4;
	    }
	    return 0;
	case 0x0c:		/* set call back */
	case 0x14:		/* swap call back */
	    if ( _es && _edx ) {
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
	    char *src, *dst;
	    int len;
	    DS_MAPPED = _ds;
	    REG(ds) =   DPMI_private_data_segment+DPMI_private_paragraphs;
	    src = (char *)GetSegmentBaseAddress(_ds);
	    dst = (char *)(REG(ds)<<4);
	    len = ((Segments[_ds >> 3].limit > 0xffff) ||
	    	Segments[_ds >> 3].is_big) ?
		0xffff : Segments[_ds >> 3].limit;
	    D_printf("DPMI: whole segment of DS at %#x copy to DOS at %#x for %#x\n",
		(int)src, (int)dst, len);
	    memmove (dst, src, len);
        } else {
            REG(ds) = (long) GetSegmentBaseAddress(_ds) >> 4;
	    DS_MAPPED = 0;
        }
    }

    if (ES_MAPPED) {
	if ( !is_dos_selector(_es)) {
	    char *src, *dst;
	    int len;
	    ES_MAPPED = _es;
	    REG(es) =   DPMI_private_data_segment+DPMI_private_paragraphs;
	    src = (char *)GetSegmentBaseAddress(_es);
	    dst = (char *)(REG(es)<<4);
	    len = ((Segments[_es >> 3].limit > 0xffff) ||
	    	Segments[_es >> 3].is_big) ?
		0xffff : Segments[_es >> 3].limit;
	    D_printf("DPMI: whole segment of ES at %#x copy to DOS at %#x for %#x\n",
		(int)src, (int)dst, len);
	    memmove (dst, src, len);
	} else {
            REG(es) = (long) GetSegmentBaseAddress(_es) >> 4;
            ES_MAPPED = 0;
        }
    }
    if (intr==0x21) in_dos_21++;
    return 0;
}

void msdos_post_exec(void)
{
    /* restore parent\'s context to preserve segment registers */
    restore_pm_regs(&dpmi_stack_frame[current_client]);

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
    CURRENT_ENV_SEL = PARENT_ENV_SEL;
}

/*
 * DANG_BEGIN_FUNCTION msdos_post_extender
 *
 * This function is called after return from real mode DOS service
 * All real mode segment registers are changed to protected mode selectors
 * And if client\'s data buffer is above 1MB, necessary buffer copying
 * is performed.
 *
 * DANG_END_FUNCTION
 */

void msdos_post_extender(int intr)
{
    D_printf("DPMI: post_extender: int 0x%x\n", intr);
    switch (intr) {
    case 0x10:			/* video */
	if ((VIDEO_SAVED_REGS.eax & 0xffff) == 0x1130) {
	    /* get current character generator infor */
	    dpmi_stack_frame[current_client].es =
		ConvertSegmentToDescriptor(REG(es));
	    return;
	} else
	    break;
    case 0x15:
	/* we need to save regs at int15 because AH has the return value */
	if ((INT15_SAVED_REGS.eax & 0xff00) == 0xc000) { /* Get Configuration */
                if (REG(eflags)&CF)
                        return;
                if (!(dpmi_stack_frame[current_client].es =
                         ConvertSegmentToDescriptor(REG(es)))) return;
                break;
      }
        else
                return;
    /* only copy buffer for dos services */
    case 0x21:
    case 0x25: case 0x26:
    case 0x33:			/* mouse */
	break;
    default:
	return;
    }

    if (USER_DTA_SEL && intr == 0x21 ) {
	switch (S_HI(ax)) {	/* functions use DTA */
	case 0x11: case 0x12:	/* find first/next using FCB */
	case 0x4e: case 0x4f:	/* find first/next */
	    memmove(DTA_over_1MB, DTA_under_1MB, 0x80);
	    break;
	}
    }

    if (DS_MAPPED) {
	unsigned short my_ds;
	char *src, *dst;
	int len;
	my_ds =   DPMI_private_data_segment+DPMI_private_paragraphs;
	src = (char *)(my_ds<<4);
	dst = (char *)GetSegmentBaseAddress(DS_MAPPED);
	len = ((Segments[DS_MAPPED >> 3].limit > 0xffff) ||
	    	Segments[DS_MAPPED >> 3].is_big) ?
		0xffff : Segments[DS_MAPPED >> 3].limit;
	D_printf("DPMI: DS_MAPPED seg at %#x copy back at %#x for %#x\n",
		(int)src, (int)dst, len);
	memmove (dst, src, len);
    } 

    if (ES_MAPPED) {
	unsigned short my_es;
	char *src, *dst;
	int len;
	my_es =   DPMI_private_data_segment + DPMI_private_paragraphs;
	src = (char *)(my_es<<4);
	dst = (char *)GetSegmentBaseAddress(ES_MAPPED);
	len = ((Segments[ES_MAPPED >> 3].limit > 0xffff) ||
	    	Segments[ES_MAPPED >> 3].is_big) ?
		0xffff : Segments[ES_MAPPED >> 3].limit;
	D_printf("DPMI: ES_MAPPED seg at %#x copy back at %#x for %#x\n",
		(int)src, (int)dst, len);
	memmove (dst, src, len);
    } 
	       
    switch (intr) {
    case 0x21:
	in_dos_21--;
	switch (S_HI(ax)) {
	case 0x00:
	    in_dos_21++;
	    /* restore terminate address */
	    *(unsigned short *)((char *)(CURRENT_PSP<<4) + 0xa) =
		 dpmi_stack_frame[current_client].eip & 0xffff;
	    *(unsigned short *)((char *)(CURRENT_PSP<<4) + 0xa+2) =
		 GetSegmentBaseAddress(dpmi_stack_frame[current_client].cs)>>4;
	    /* restore parent PSP */
	    *(unsigned short *)((char *)(CURRENT_PSP<<4) + 0x16) = PARENT_PSP;
	    break;
	case 0x09:		/* print String */
	case 0x1a:		/* set DTA */
	    dpmi_stack_frame[current_client].edx = S_REG(edx);
	    break;
	case 0x11: case 0x12:	/* findfirst/next using FCB */
	    memmove(user_FCB, SEG_ADR((void *), ds, dx), 0x50);
	    break ;
	case 0x29:		/* Parse a file name for FCB */
	    {
		unsigned short seg =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		if ( Segments[ S_REG(ds) >> 3].base_addr > 0xffff0) {
		    memmove ((char *)GetSegmentBaseAddress(S_REG(ds)) +
			    (DPMIclient_is_32 ? S_REG(esi) : (S_LWORD(esi))),
			    (void *)(REG(ds)<<4),
			    0x100);
		    dpmi_stack_frame[current_client].esi =
			(DPMIclient_is_32 ? S_REG(esi) : (S_LWORD(esi))) +
			LWORD(esi); 
		    seg += 10;
		}
		if (Segments[ S_REG(es) >>3].base_addr > 0xffff0) {
		    memmove ((char *)GetSegmentBaseAddress(S_REG(es)) +
			    LWORD(edi),
			    (void *)((REG(es)<<4) + LWORD(edi)),  0x50);
		}
	    }
	    break;

	case 0x2f:		/* GET DTA */
	    if (SEG_ADR((void*), es, bx) == DTA_under_1MB) {
		if (!USER_DTA_SEL)
		    error("Selector is not set for the translated DTA\n");
		dpmi_stack_frame[current_client].es = USER_DTA_SEL;
		dpmi_stack_frame[current_client].ebx = USER_DTA_OFF;
	    } else {
		dpmi_stack_frame[current_client].es = ConvertSegmentToDescriptor(REG(es));
	    }
	    break;

	case 0x34:		/* Get Address of InDOS Flag */
	case 0x35:		/* GET Vector */
	case 0x52:		/* Get List of List */
	    if (!ES_MAPPED)
	      dpmi_stack_frame[current_client].es =
	               ConvertSegmentToDescriptor(REG(es));
	    break;

	case 0x50:		/* Set PSP */
	    dpmi_stack_frame[current_client].ebx = S_REG(ebx);
	    break;
	    
	case 0x26:
	case 0x55:		/* create PSP */
	    if ( !is_dos_selector(S_LWORD(edx))) {
		memmove((void *)GetSegmentBaseAddress(S_LWORD(edx)),
		(void *)(LWORD(edx) << 4), 0x100);
	    }
	    dpmi_stack_frame[current_client].edx = S_REG(edx);
	    break;

        case 0x59:		/* Get EXTENDED ERROR INFORMATION */
	    if(LWORD(eax) == 0x22 && !ES_MAPPED) { /* only this code has a pointer */
		dpmi_stack_frame[current_client].es =
			ConvertSegmentToDescriptor(REG(es));
	    }
	    break;
	case 0x38:
	    if (S_LO(ax) == 0x00 && !DS_MAPPED) { /* get country info */
		dpmi_stack_frame[current_client].ds =
	               ConvertSegmentToDescriptor(REG(ds));
	    }
	    break;
	case 0x47:		/* get CWD */
	    if (LWORD(eflags) & CF)
		break;
	    if (READ_DS_COPIED)
		snprintf((char *)(GetSegmentBaseAddress
				(dpmi_stack_frame[current_client].ds) +
			(DPMIclient_is_32 ? S_REG(esi) : (S_LWORD(esi)))),
                         0x40, "%s", 
		        (char *)((REG(ds) << 4) + LWORD(esi)));
	    READ_DS_COPIED = 0;
	    break;
#if 0	    
	case 0x48:		/* allocate memory */
	    if (LWORD(eflags) & CF)
		break;
	    dpmi_stack_frame[current_client].eax =
		ConvertSegmentToDescriptor(LWORD(eax));
	    break;
#endif	    
#if 0
	case 0x4e:		/* find first */
	case 0x4f:		/* find next */
	    if (LWORD(eflags) & CF)
		break;
	    if (USER_DTA_SEL)
		memmove(DTA_over_1MB, DTA_under_1MB, 43);
	    break;
#endif	    
	case 0x51:		/* get PSP */
	case 0x62:
	    {/* convert environment pointer to a descriptor*/
		unsigned short 
#if 0
		envp, 
#endif
		psp;
		psp = LWORD(ebx);
#if 0
		envp = *(unsigned short *)(((char *)(psp<<4))+0x2c);
		envp = ConvertSegmentToDescriptor(envp);
		*(unsigned short *)(((char *)(psp<<4))+0x2c) = envp;
#endif
		if (psp == CURRENT_PSP && USER_PSP_SEL) {
		    dpmi_stack_frame[current_client].ebx = USER_PSP_SEL;
		} else {
		    dpmi_stack_frame[current_client].ebx = ConvertSegmentToDescriptor(psp);
		}
	    }
	    break;
	case 0x53:		/* Generate Drive Parameter Table  */
	    {
		unsigned short seg =
		    DPMI_private_data_segment+DPMI_private_paragraphs;
		if ( !is_dos_selector(S_REG(ds))) {
		    seg += 30;
		    dpmi_stack_frame[current_client].esi = S_REG(esi);
		}
		if ( !is_dos_selector(S_REG(es))) {
		    memmove ((char *)GetSegmentBaseAddress(S_REG(es)) +
			    (DPMIclient_is_32 ? S_REG(ebp) : (S_LWORD(ebp))),
			    (void *)((REG(es)<<4) + LWORD(ebp)),
			    0x60);
		}
	    }
	    break ;
	case 0x56:		/* rename */
	    dpmi_stack_frame[current_client].edx = S_REG(edx);
	    dpmi_stack_frame[current_client].edi = S_REG(edi);
	    break ;
	case 0x5d:
	    if (S_LO(ax) == 0x06) /* get address of DOS swappable area */
				/*        -> DS:SI                     */
		dpmi_stack_frame[current_client].ds = ConvertSegmentToDescriptor(REG(ds));
	    break;
	case 0x3f:
	    if (READ_DS_COPIED) {
		dpmi_stack_frame[current_client].edx = S_REG(edx);
		if (LWORD(eflags) & CF)
		    break;
		memmove((char *)(GetSegmentBaseAddress
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
	    case 0: case 1:
		break ;
	    case 2 ... 6:
		dpmi_stack_frame[current_client].esi = S_REG(esi);
		memmove ((char *)GetSegmentBaseAddress
			(dpmi_stack_frame[current_client].ds)
			+ (DPMIclient_is_32 ? S_REG(esi) :  (S_LWORD(esi))),
			(void *)(REG(ds)<<4),
			0x100);
		dpmi_stack_frame[current_client].edi = S_REG(edi);
		memmove ((char *)GetSegmentBaseAddress
			(dpmi_stack_frame[current_client].es)
			+ (DPMIclient_is_32 ? S_REG(edi) :  (S_LWORD(edi))),
			(void *)(REG(es)<<4),
			0x100);
	    }
	    break;
	default:
	    break;
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
	break;
    case 0x33:			/* mouse */
	switch (MOUSE_SAVED_REGS.eax & 0xffff) {
	case 0x14:		/* swap call back */
	    dpmi_stack_frame[current_client].es =
                  	    ConvertSegmentToDescriptor(REG(es)); 
	    break;
	case 0x19:		/* Get User Alternate Interrupt Address */
	    dpmi_stack_frame[current_client].ebx =
                  	    ConvertSegmentToDescriptor(LWORD(ebx)); 
	    break;
	default:
	    break;
	}
    default:
	break;
    }
    DS_MAPPED = 0;
    ES_MAPPED = 0;
}

static char decode_use_16bit;
static char use_prefix;
static  unsigned char *
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
    D_printf("DPMI: decode_8e_index returns with NULL\n");
    return(NULL);
}

static  unsigned char *
check_prefix (struct sigcontext_struct *scp)
{
    unsigned char *prefix, *csp;

    csp = (unsigned char *) SEL_ADR(_cs, _eip);

    prefix = NULL;
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
    default:
	break;
    }
    if (use_prefix) {
    	D_printf("DPMI: check_prefix covered for *csp=%x\n", *csp);
    }
    return prefix;
}
/*
 * this function tries to decode opcode 0x8e (mov Sreg,m/r16), returns
 * the length of the instruction.
 */

static int
decode_8e(struct sigcontext_struct *scp, unsigned short *src,
	  unsigned  char * sreg)
{
    unsigned char *prefix, *csp;
    unsigned char mod, rm, reg;
    int len = 0;

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

static  int
decode_load_descriptor(struct sigcontext_struct *scp, unsigned short
		       *segment, unsigned char * sreg)
{
    unsigned char *prefix, *csp;
    unsigned char mod, rm, reg;
    unsigned long *lp=NULL;
    unsigned offset;
    int len = 0;

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

static  int
decode_pop_segreg(struct sigcontext_struct *scp, unsigned short
		       *segment, unsigned char * sreg)
{
    unsigned short *ssp;
    unsigned char *csp;
    int len;

    csp = (unsigned char *) SEL_ADR(_cs, _eip);
    ssp = (unsigned short *) SEL_ADR(_ss, _esp);
    len = 0;
    switch (*csp) {
    case 0x1f:			/* pop ds */
	len = 1;
	*segment = *ssp;
	*sreg = DS_INDEX;
	break;
    case 0x07:			/* pop es */
	len = 1;
	*segment = *ssp;
	*sreg = ES_INDEX;
	break;
    case 0x17:			/* pop ss */
	len = 1;
	*segment = *ssp;
	*sreg = SS_INDEX;
	break;
    case 0x0f:		/* two byte opcode */
	csp++;
	switch (*csp) {
	case 0xa1:		/* pop fs */
	    len = 2;
	    *segment = *ssp;
	    *sreg = FS_INDEX;
	    break;
	case 0xa9:		/* pop gs */
	    len = 2;
	    *segment = *ssp;
	    *sreg = GS_INDEX;
	break;
	}
    }
    return len;
}

/*
 * decode_modify_segreg_insn tries to decode instructions which would modify a
 * segment register, returns the length of the insn.
 */
static  int
decode_modify_segreg_insn(struct sigcontext_struct *scp, unsigned
			  short *segment, unsigned char *sreg)
{
    unsigned char *csp;
    int len, size_prfix;

    csp = (unsigned char *) SEL_ADR(_cs, _eip);
    size_prfix = 0;
    decode_use_16bit = !Segments[_cs>>3].is_32;
    if (*csp == 0x66) { /* Operand-Size prefix */
	csp++;
	_eip++;
	decode_use_16bit ^= 1;
	size_prfix++;
    }
	
    /* first try mov sreg, .. (equal for 16/32 bit operand size) */
    if ((len = decode_8e(scp, segment, sreg))) {
      _eip += len;
      return len + size_prfix;
    }
 
    if (decode_use_16bit) {	/*  32bit decode not implemented yet */
      /* then try lds, les ... */
      if ((len = decode_load_descriptor(scp, segment, sreg))) {
        _eip += len;
	return len+size_prfix;
      }
    }

    /* now try pop sreg */
    if ((len = decode_pop_segreg(scp, segment, sreg))) {
      _esp += decode_use_16bit ? 2 : 4;
      _eip += len;
      return len+size_prfix;
    }

    return 0;
}
	
    
#if 0 /* Not USED!  JES 96/01/2x */
static  int msdos_fix_cs_prefix (struct sigcontext_struct *scp)
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
#endif


int msdos_fault(struct sigcontext_struct *scp)
{
    struct sigcontext_struct new_sct;
    unsigned char reg;
    unsigned short segment, desc;
    unsigned long len;

    D_printf("DPMI: msdos_fault, err=%#lx\n",_err);

    if ((_err & 0xffff) == 0) {	/*  not a selector error */
	char fixed = 0;
	unsigned char * csp;

	csp = (unsigned char *) SEL_ADR(_cs, _eip);

	/* see if client wants to access control registers */
	if (*csp == 0x0f) {
	  if (cpu_trap_0f(csp, scp)) return 1;	/* 1=handled */
	}
	
	switch (*csp) {
	case 0x2e:		/* cs: */
	    break;		/* do nothing */
	case 0x36:		/* ss: */
	    break;		/* do nothing */
	case 0x26:		/* es: */
	    if (_es == 0) {
		D_printf("DPMI: client tries to use use gdt 0 as es\n");
		_es = ConvertSegmentToDescriptor(0);
		fixed = 1;
	    }
	    break;
	case 0x64:		/* fs: */
	    if (_fs == 0) {
		D_printf("DPMI: client tries to use use gdt 0 as fs\n");
		_fs = ConvertSegmentToDescriptor(0);
		fixed = 1;
	    }
	    break;
	case 0x65:		/* gs: */
	    if (_gs == 0) {
		D_printf("DPMI: client tries to use use gdt 0 as es\n");
		_gs = ConvertSegmentToDescriptor(0);
		fixed = 1;
	    }
	    break;
	case 0xf2:		/* REPNE prefix */
	case 0xf3:		/* REP, REPE */
	    /* this might be a string insn */
	    switch (*(csp+1)) {
	    case 0xaa: case 0xab:		/* stos */
	    case 0xae: case 0xaf:	        /* scas */
		/* only use es */
		if (_es == 0) {
		    D_printf("DPMI: client tries to use use gdt 0 as es\n");
		    _es = ConvertSegmentToDescriptor(0);
		    fixed = 1;
		}
		break;
	    case 0xa4: case 0xa5:		/* movs */
	    case 0xa6: case 0xa7:         /* cmps */
		/* use both ds and es */
		if (_es == 0) {
		    D_printf("DPMI: client tries to use use gdt 0 as es\n");
		    _es = ConvertSegmentToDescriptor(0);
		    fixed = 1;
		}
		if (_ds == 0) {
		    D_printf("DPMI: client tries to use use gdt 0 as ds\n");
		    _ds = ConvertSegmentToDescriptor(0);
		    fixed = 1;
		}
		break;
	    }
	    break;
	case 0x3e:		/* ds: */
	default:		/* assume default is using ds, but if the */
				/* client sets ss to 0, it is totally broken */
	    if (_ds == 0) {
		D_printf("DPMI: client tries to use use gdt 0 as ds\n");
		_ds = ConvertSegmentToDescriptor(0);
		fixed = 1;
	    }
	    break;
	}
	return fixed;
    }
    
    /* now it is a invalid selector error, try to fix it if it is */
    /* caused by an instruction mov Sreg,m/r16                    */

    new_sct = *scp;
    len = decode_modify_segreg_insn(&new_sct, &segment, &reg);
    if (len == 0) 
	return 0;
    if (ValidAndUsedSelector(segment)) {
	/*
	 * The selector itself is OK, but the descriptor (type) is not.
	 * We cannot fix this! So just give up immediately and dont
	 * screw up the context.
	 */
	D_printf("DPMI: msdos_fault: Illegal use of selector %#x\n", segment);
	return 0;
    }

    D_printf("DPMI: try mov to a invalid selector 0x%04x\n", segment);

#if 0
    /* only allow using some special GTD\'s */
    if ((segment != 0x0040) && (segment != 0xa000) &&
	(segment != 0xb000) && (segment != 0xb800) &&
	(segment != 0xc000) && (segment != 0xe000) && (segment != 0xf000))
	return 0;
#endif    

    if (!(desc = ConvertSegmentToDescriptor(segment)))
	return 0;

    /* OKay, all the sanity checks passed. Now we go and fix the selector */
    switch (reg) {
    case ES_INDEX:
	new_sct.es = desc;
	break;
    case CS_INDEX:
	new_sct.cs = desc;
	break;
    case SS_INDEX:
	new_sct.ss = desc;
	break;
    case DS_INDEX:
	new_sct.ds = desc;
	break;
    case FS_INDEX:
	new_sct.fs = desc;
	break;
    case GS_INDEX:
	new_sct.gs = desc;
	break;
    default :
	/* Cannot be here */
	error("DPMI: Invalid segreg %#x\n", reg);
	return 0;
    }

    /* lets hope we fixed the thing, apply the "fix" to context and return */
    *scp = new_sct;
    return 1;
}
