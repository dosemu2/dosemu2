/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*************************************************************************/
/**                                                                     **/
/** This is the virtual PC's Bios (F000:0 .. F000:FFFF)                 **/
/**                                                                     **/
/** We must compile this with gcc/as/ld as follows:	                **/
/**       gcc -nostdlib -Wl,-Ttext,0,-e,0 -traditional bios.S -o bios.o **/
/**       strip -N _edata -N __bss_start -N _end bios.o                 **/
/** modified from as86 to gas by Bart Oldeman Jan 2001			**/
/**                                                                     **/
/*************************************************************************/

#define __ASM__
#include "config.h"
#include "memory.h"
#include "macros86.h"
#include "doshelpers.h"

#include "keyb_server.h"

/* some other useful definitions */

#define DOSHELPER_INT 0xe6

#define BIOS_DATA 0x40
#define KEYBUF_READ_PTR  0x1a
#define KEYBUF_WRITE_PTR 0x1c
#define KEYBUFFER_START 0x80
#define KEYBUFFER_END 0x82
#define KEYSHIFT_FLAGS 0x17
#define KEYBOARD_FLAGS_2 0x18
#define KEYBOARD_STATUS_3 0x96
#define BIOS_TIMER 0x46c
#define BIOS_TIMER_OVERFLOW 0x470
#define HOUR24_ADJUST 0x1800B0
#define DISKETTE_MOTOR_TIMEOUT 0x440
#define BIOS_VIDEO_MODE 0x49

/* NOTE: The following definition need to be in memory.h, but at this
 *       moment they aren't, so I define them here.
 *       NEED TO BE CLEANED UP !
 */
		/* out of xms.h */
#define INT2F_XMS_MAGIC         0x43  /* AH for all int 2f XMS calls */

.code16
.text
	.globl	CISH(bios_f000)
CISH(bios_f000):

	.org	((DOSEMU_LMHEAP_SEG - BIOSSEG) << 4) + DOSEMU_LMHEAP_OFF
/* ======================= Addr = F000:4000 (F4000) */
	/* 32K Heap here for the dosemu internal needs */
	.REPT DOSEMU_LMHEAP_SIZE
	    .byte 0
	.ENDR

	.org	BIOS_HLT_BLK - (BIOSSEG << 4)
bios_hlt_blk:
/* ======================= Addr = F000:C000 (FC000) */
	FILL_OPCODE (XMSControl_ADD-BIOS_HLT_BLK),hlt
        jmp     (.+2+3) /* jmp short forward 3 */
        FILL_OPCODE 3,nop
	FILL_OPCODE (BIOS_HLT_BLK+BIOS_HLT_BLK_SIZE-XMSControl_ADD-5),hlt
        
/* ----------------------------------------------------------------- */   
	.org	((DPMI_SEG-BIOSSEG) << 4)+DPMI_OFF
/* ======================= Addr = F800:4800 (FC800) */
	.globl	CISH(DPMI_dummy_start)
CISH(DPMI_dummy_start):
	pushw	%bx
	pushw	%ax
	movb	$0x62,%ah
	int	$0x21			/* Get PSP */
	popw	%ax
/* ======================= Addr = F800:4807 (FC807) */
	.globl	CISH(DPMI_dpmi_init)
CISH(DPMI_dpmi_init):
	hlt
	popw	%bx
	lret
/* ======================= Addr = F800:480A (FC80A) */
	.globl	CISH(DPMI_return_from_dos)
CISH(DPMI_return_from_dos):
	hlt
/* ======================= Addr = F800:480B (FC80B) */
	.globl	CISH(DPMI_reserved12)
CISH(DPMI_reserved12):
	hlt
/* ======================= Addr = F800:480C (FC80C+intno) */
	.globl	CISH(DPMI_return_from_dosint)
CISH(DPMI_return_from_dosint):
	FILL_OPCODE 256,hlt
/* ======================= Addr = F800:490C (FC90C) */
	.globl	CISH(DPMI_return_from_realmode)
CISH(DPMI_return_from_realmode):
	hlt
/* ======================= Addr = F800:490D (FC90D) */
	.globl	CISH(DPMI_return_from_dos_memory)
CISH(DPMI_return_from_dos_memory):
	hlt
/* ======================= Addr = F800:490E (FC90E) */
CISH(DPMI_reserved):
	hlt
	lret
/* ======================= Addr = F800:4910 (FC910) */
	.globl	CISH(DPMI_raw_mode_switch_rm)
CISH(DPMI_raw_mode_switch_rm):
	hlt
/* ======================= Addr = F800:4911 (FC911) */
	.globl	CISH(DPMI_save_restore_rm)
CISH(DPMI_save_restore_rm):
	hlt
	lret
/* ======================= Addr = F800:4913 (FC913) */
	.globl	CISH(MSDOS_srm_start)
CISH(MSDOS_srm_start):
/* ======================= Addr = F800:4913 (FC913) */
	.globl	CISH(MSDOS_mouse_callback)
CISH(MSDOS_mouse_callback):
	hlt
	lret
/* ======================= Addr = F800:4915 (FC915) */
	.globl	CISH(MSDOS_PS2_mouse_callback)
CISH(MSDOS_PS2_mouse_callback):
	hlt
	lret
/* ======================= Addr = F800:4917 (FC917) */
	.globl	CISH(MSDOS_srm_end)
CISH(MSDOS_srm_end):

	.globl	CISH(MSDOS_rpm_start)
CISH(MSDOS_rpm_start):
/* ======================= Addr = F800:4917 (FC917) */
	.globl	CISH(MSDOS_return_from_rm)
CISH(MSDOS_return_from_rm):
	hlt
/* ======================= Addr = F800:4918 (FC918) */
	.globl	CISH(MSDOS_rpm_end)
CISH(MSDOS_rpm_end):
/* ======================= Addr = F800:4918 (FC918) */
	.globl	CISH(DPMI_dummy_end)
CISH(DPMI_dummy_end):

/* ======================= Addr = F800:4B00 (FCB00) */
	.org	((DOS_LONG_READ_SEG - BIOSSEG) << 4) + DOS_LONG_READ_OFF
	pushl	%esi
	pushl	%edi
	pushl	%ecx
	xorl	%edi, %edi
start_read:
	movl	%ecx, %esi
	cmpl	$0x10000, %esi
	jb	do_read
	xorl	%ecx, %ecx
	decw	%cx
do_read:
	movb	$0x3f, %ah
	int	$0x21
	jc	done_read
	movzwl	%ax, %eax
	pushl	%ecx
	pushl	%eax
	movl	%eax, %ecx
	movb	$DOS_HELPER_PUT_DATA, %al
	int	$DOS_HELPER_INT
	popl	%eax
	popl	%ecx
	addl	%eax, %edi
	cmpw	%ax, %cx
	jnz	done_read
	movl	%esi, %ecx
	subl	%eax, %ecx
	jnz	start_read
done_read:
	movl	%edi, %eax
	popl	%ecx
	popl	%edi
	popl	%esi
	lret

/* ======================= Addr = F800:4BA0 (FCBA0) */
	.org	((DOS_LONG_WRITE_SEG - BIOSSEG) << 4) + DOS_LONG_WRITE_OFF
	pushl	%esi
	pushl	%edi
	pushl	%ecx
	xorl	%edi, %edi
start_write:
	movl	%ecx, %esi
	cmpl	$0x10000, %esi
	jb	do_write
	xorl	%ecx, %ecx
	decw	%cx
do_write:
	movb	$DOS_HELPER_GET_DATA, %al
	int	$DOS_HELPER_INT
	movb	$0x40, %ah
	int	$0x21
	jc	done_write
	movzwl	%ax, %eax
	addl	%eax, %edi
	cmpw	%ax, %cx
	jnz	done_write
	movl	%esi, %ecx
	subl	%eax, %ecx
	jnz	start_write
done_write:
	movl	%edi, %eax
	popl	%ecx
	popl	%edi
	popl	%esi
	lret

/******************************************************************
 * DATA BLOCK
 ******************************************************************/

.org	0xd000

	.globl	CISH(bios_f000_int10ptr)
CISH(bios_f000_int10ptr):
	.long	0xc0000003

	.globl	CISH(bios_f000_int10_old)
CISH(bios_f000_int10_old):
	.long	0

	.globl	CISH(bios_in_int10_callback)
CISH(bios_in_int10_callback):
	.byte	0

	.globl	CISH(bios_f000_bootdrive)
CISH(bios_f000_bootdrive):
	.byte	0

	/* this is the paramblock, we told DOS to use for INT21 AX=4B01 */
	.align	16,0
	.globl	CISH(DBGload_parblock)
CISH(DBGload_parblock):	.word	0
			.long	0,0,0
DBGload_SSSP:		.long	0
	.globl	CISH(DBGload_CSIP)
CISH(DBGload_CSIP):	.long	0

	/* parameter packet, filled in by pkt_init */
	.globl	CISH(PKTDRV_param)
CISH(PKTDRV_param):
	.byte	0,0,0,0
	.word	0,0,0,0,0

	/* driver statistics structure */
	.globl	CISH(PKTDRV_stats)	  
CISH(PKTDRV_stats):
	.long	0,0,0,0,0,0,0

	.globl	CISH(LFN_string)
CISH(LFN_string):
	.REPT 128
	    .byte 0
	.ENDR

/******************************************************************
 * BIOS CODE BLOCK						  *
 ******************************************************************/

.org	0xe000
/* COMPAS FE000-FE05A	reserved */
	.ascii	"..............IBM..............."
	.ascii	"DOSEMU Custom BIOS r0.01, Copyri"
	.ascii	"ght 1992-2005.........."

.org	ROM_BIOS_SELFTEST
/* COMPAS FE05B		jmp to POST */
/* COMPAS FE05E-FE2C2	reserved */
	lcall	$0xfc00, $0x07fe
	sti

/* ----------------------------------------------------------------- */	  
/* This is for the video init - it calls in order:
   - first helper function (int0xe6,al=8)
   - the video BIOS init entry point at C000:3 or E000:3
   - second helper function (int 0xe6,al=9)
 */
	movb	$DOS_HELPER_VIDEO_INIT,%al		/*  Start Video init */
	int	$DOSHELPER_INT
	cmpb	$0, %al
	je	no_vbios_post
	/* call far 0xc000:3  or call far 0xe000:3 */
	/* More general than just c000 or e000 ??? */	
	pushw	%ds
	lcall	INDIRECT_PTR(%cs:CISH(bios_f000_int10ptr)-bios_f000)
	popw	%ds
	sti
	jmp	video_init_done

/* ----------------------------------------------------------------- */	  
/* Post-less video init
 */
no_vbios_post:
	movb	BIOS_VIDEO_MODE,%al
	int	$0x10
video_init_done:
	movb	$DOS_HELPER_VIDEO_INIT_DONE,%al		/* Finished video init */
	int	$DOSHELPER_INT

	movb	$DOS_HELPER_SHOW_BANNER,%al
	int	$DOSHELPER_INT
	movb	%cs:CISH(bios_f000_bootdrive)-bios_f000,%dl

	ljmp	$0x0, $0x7c00		/* Some boot sectors require cs=0 */

.org	ROM_BIOS_EXIT
/* ======================= Addr = F000:E2B0 (FE2B0) */
	/* set up BIOS exit routine */
	movw	$DOS_HELPER_REALLY_EXIT,%ax
	int	$DOSHELPER_INT

/* COMPAS FE2C3		jmp to NMI */
/* COMPAS FE2C6-FE3FD	reserved */

/* ----------------------------------------------------------------- */
	/* this is IRET */
	.org	((IRET_SEG - BIOSSEG) << 4) + IRET_OFF
/* ======================= Addr = F800:62CF (FE2CF) */
	iret

/* ----------------------------------------------------------------- */
	/* this is the mouse handler */
	.org	((Mouse_SEG - BIOSSEG) << 4) + Mouse_OFF
/* ======================= Addr = F000:E2D0 (FE2D0) */
   
	/* 
	mouse routine does a "pusha" before here and then pushes this
	address ...so we just "popa; EOI; iret" to get back out
	*/
	popw	%es
	popw	%ds
	popa
	jmp	Mouse_EOI

/* ----------------------------------------------------------------- */
	.org	((Mouse_SEG - BIOSSEG) << 4) + Mouse_PS2_OFF
/* ======================= Addr = F000:E2D8 (FE2D8) */
	addw	$8, %sp			/* pop PS/2 mouse information   */
	jmp	Mouse_EOI
        
        .org	((Mouse_SEG - BIOSSEG) << 4)+Mouse_ROUTINE_OFF
/* ======================= Addr = F000:E2E0 (FE2E0) */

	/* This is the int74 handler */
	pushw	%ax	/* save everything */
	pushw	%bx
	movw	$DOS_HELPER_MOUSE_HELPER,%ax	/* mouse helper */
	movw	$0xf2,%bx	/* call the user or PS/2 hook */
	int	$DOSHELPER_INT
	/* if an event handler is installed we will go to Mouse_OFF
	   or Mouse_PS2_OFF;  otherwise return here.
	   ax and bx are popped in mouse.c
	*/      

Mouse_EOI:
	pushw	%ax
        movb    $0x20,%al
        outb    %al,$0xa0		/* flag interrupt complete	*/
        outb    %al,$0x20
	popw	%ax
	iret

        .org	((Mouse_SEG - BIOSSEG) << 4)+Mouse_HLT_OFF
/* ======================= Addr = F000:E2FF (FE2FF) */
	/* vm86 mouse handlers return here to allow for the
	   hogthreshold code to take effect
	*/
	hlt        
        
/* ----------------------------------------------------------------- */
	.org	((IPX_SEG - BIOSSEG) << 4) + IPX_OFF
/* ======================= Addr = F800:6310 (FE310) */
ipx_handler:
	int	$0x7a
	lret

/* ----------------------------------------------------------------- */   
		/* This is an int e7 used for FCB opens */
	.org	((INTE7_SEG-BIOSSEG) << 4)+INTE7_OFF
/* ======================= Addr = F800:6320 (FE320) */
	pushw	%es
	pushw	%di
	pushw	%ax
	movw	$0x120c,%ax
	int	$0x2f
	popw	%ax
	popw	%di
	popw	%es
	iret

/* This is installed after video init (helper fcn 0x9) when the internal
	mouse driver is in use.  It watches for mouse set commands and
	resets the mouse driver when it sees one. */
/* Was: Comments and bugs to David Etherton, etherton@netcom.com
 * Current Maintainer:	Eric W. Biederman <eric@biederman.org>
 */	

	.org	((INT10_WATCHER_SEG-BIOSSEG) << 4)+INT10_WATCHER_OFF
/* ======================= Addr = F800:6330 (FE330) */
WINT10:
	cmpb	$1, %cs:bios_in_int10_callback-((INT10_WATCHER_SEG-BIOSSEG) << 4)-bios_f000
	je	L10
	or	%ah,%ah
	jz	L9	/* normal mode set */
	cmpb	$0x11,%ah
	je	L9	/* character generator, possibly resize the screen */
	cmpw	$0x4F02,%ax
	jne	L10	/* svga mode set */
	pushw	%bx	/* vesa mode on stack */
	jmp     L9a
L9:
	pushw	%ax	/* normal mode (or 0x110?) on stack */
L9a:
	pushw 	%ax	/* save everything */
	pushw	%bx
	movw	$DOS_HELPER_MOUSE_HELPER,%ax	/* mouse helper */
	movw	$0xf0,%bx	/* start video mode set */
	int	$DOSHELPER_INT
	popw	%bx
	popw	%ax

/* fake stack frame for iret: push original flags and avoid a GPF from pushf */
	movzwl	%sp,%esp	/* make sure high of esp is zero */
	CODE32  pushw	6(%esp)
	pushw	%cs
	call	L10	/* perform the actual mode set */

/* since following code doesn't affect flags, we keep current values */
	pushw	%ax	/* remember everything from int10 call */
	pushw	%bx
	movw	$DOS_HELPER_MOUSE_HELPER,%ax	/* mouse helper */
	movw	$0xf1,%bx	/* end video mode set */
	int	$DOSHELPER_INT
	popw	%bx
	popw	%ax
	addw	$2,%sp/* pop video mode */	    
	lret	$2    /* keep current flags and avoid another GPF from iret */

L10:	/* chain to original handler (probably the video bios) */
	ljmp	INDIRECT_PTR(%cs:CISH(bios_f000_int10_old-0x8000-bios_f000))

#ifdef X86_EMULATOR
	.org	((INT10_WATCHER_SEG-BIOSSEG) << 4)+(INT10_WATCHER_OFF+0x60)
/* ======================= Addr = F800:6390 (FE390) */
/* the reason for this trick is that when SKIP_EMU_VBIOS is active we
 * switch to VBIOS into _true_ vm86 mode, and the iret is trapped by
 * the kernel, not the CPU emulator. Here we double the return stack
 * and fall back into an HLT at the end of the video code. Since cs==
 * f800, cpuemu gets control back at the right point -- AV
 */
/* fake stack frame for iret: push original flags and avoid a GPF from pushf */
	movzwl	%sp,%esp	/* make sure high of esp is zero */
	CODE32  pushw	4(%esp)
	pushw	%cs
	call	WINT10
	hlt
	lret	$2    /* keep current flags and avoid another GPF from iret */
#endif

/* ----------------------------------------------------------------- */   
/* This hlt address is used to return back from a call back */
/* It uses one byte.  see sigsegv.c */
	.org	((CBACK_SEG-BIOSSEG) << 4)+CBACK_OFF

/* ======================= Addr = F800:63EF (FE3EF) */
	.globl	CISH(CBACK_return_from_dos)
CISH(CBACK_return_from_dos):
	hlt

/* ----------------------------------------------------------------- */   
	.org	((INT70_SEG-BIOSSEG) << 4)+INT70_OFF

	.globl	CISH(INT70_dummy_start)
/* ======================= Addr = F800:63F0 (FE3F0) */
CISH(INT70_dummy_start):		/* RTC INTERRUPT ROUTINE	*/

	pushw	%ax
	int	$0x4a
        movb    $0x20,%al
        outb    %al,$0xa0		/* flag interrupt complete	*/
	outb	%al,$0x20
	popw	%ax			/* restore registers            */
	iret				/* return to interrupted code	*/
	.globl  CISH(INT70_dummy_end)
CISH(INT70_dummy_end):


/* COMPAS FE3FE		jmp to INT13 HD */
	.org	((INT41_SEG - BIOSSEG) << 4) + INT41_OFF
/* COMPAS FE401-FE6F0	HD parameter table */
	.word	50	/* cyl */
	.byte	255	/* heads */
	.word	0	/* rw_cyl */
	.word	0	/* precomp_cyl */
	.byte	0	/* max_ecc */
	.byte	0	/* contr */
	.byte	10	/* std_timeout */
	.byte	10	/* fmt_timeout */
	.byte	10	/* chk_timeout */
	.word	0	/* lnd_zone */
	.byte	63	/* spt */
	.byte	0xff	/* reserved */

/* COMPAS FE3FE		jmp to INT13 HD */
	.org	((INT46_SEG - BIOSSEG) << 4) + INT46_OFF
/* COMPAS FE401-FE6F0	HD parameter table */
	.word	50	/* cyl */
	.byte	255	/* heads */
	.word	0	/* rw_cyl */
	.word	0	/* precomp_cyl */
	.byte	0	/* max_ecc */
	.byte	0	/* contr */
	.byte	10	/* std_timeout */
	.byte	10	/* fmt_timeout */
	.byte	10	/* chk_timeout */
	.word	0	/* lnd_zone */
	.byte	63	/* spt */
	.byte	0xff	/* reserved */

/* COMPAS FE6F1		reserved */
/* COMPAS FE6F2		jmp to INT19 */
/* ----------------------------------------------------------------- */   
	.org	ROM_CONFIG_OFF /* for int15 */

/* ======================= Addr = FE6F5 */
	/* from Alan Cox's mods */
	/* we need somewhere for the bios equipment. */
	.word	8	/* 8 bytes follow */
	.byte	0xfc	/* PC AT */
	.byte	0x01	/* submodel - DOSEMU */
	.byte	0x04	/* bios revision 4 */
	.byte	0x70	/* no mca, no ebios, no wat, keybint,
			   rtc, slave 8259, no dma 3 */
	.byte	0x40	/* extended keyboard */
	.byte	0,0,0	/* nothing more is supported */

/* COMPAS FE710-FE728	reserved */
/* COMPAS FE729		baud rate init table */
/* COMPAS FE73C-FE82D	reserved */

/* ----------------------------------------------------------------- */
	.org	((INT16_SEG-BIOSSEG) << 4)+INT16_OFF
	.globl	CISH(INT16_dummy_start)
/* ======================= Addr = F800:682E (FE82E) */
/* COMPAS FE82E		jmp to INT16 */
/* COMPAS FE831-FE986	reserved */
CISH(INT16_dummy_start):			/* KEYBOARD BIOS ROUTINE        */
        /* this is all handled by int16.c, except for ah=0/0x10:
           for that special case we need to loop in vm86 while
           waiting for the key */
        sti
        testb   $0xef, %ah
        jz      readkey_loop
        ljmp    $BIOSSEG, $INT_OFF(0x16)
readkey_loop:
        pushf
        lcall   $BIOSSEG, $INT_OFF(0x16)
        jz      readkey_loop
        iret

        .globl  CISH(INT16_dummy_end)
CISH(INT16_dummy_end): 

/* ----------------------------------------------------------------- */   
	.org	((INT09_SEG-BIOSSEG) << 4)+INT09_OFF
#if 0
	.globl	CISH(INT09_dummy_start)
#endif
/* ======================= Addr = FE987 */
/* COMPAS FE987		jmp to INT09 */
/* COMPAS FE98A-FEC58	reserved */
	pushw	%ax
	pushw	%bx
	pushw	%ds
	movw	$BIOS_DATA,%ax
	movw	%ax,%ds

/* BIOS keyboard intercept */

/* get the RAW scancode (used only for int15,4f) */
	inb	$0x60,%al

/* check for Ctrl-Alt-Del */
	cmpb	$0x53, %al
	jne	1f
	movb	KEYSHIFT_FLAGS, %bl
	andb	$0x0c, %bl
	cmpb	$0x0c, %bl
	je	kbd_do_CAD
1:
	
/*ERIC	* src/base/bios/bios.S (INT09_dummy_start): 
	removed spurious 'mov al,ah' between in al,0x60 and mov ah,$0x4f
	it was trashing the value read and would serve no useful purpose.
*/
	movb	$0x4f,%ah
	stc
#if 1
		/* int 15 func 4f as per bios spec.
		 * We need however a _simulated_ INT else a DOS-space
		 * hooked INT15 would not be called,
		 * because _we_ intercept it in src/base/async/int.c
		 * As we currently don't do anything important with INT15-AH=4f,
		 * this doesn't make problems.
		 * -- Hans, June 15 1997
		 */
	SIM_INT 0x15, n_kbd_int15_return
#else
	int	$0x15
#endif
	
	/* ignore the returned keycode, only skip the pre-translated
	   bios keycode if CF=0.
	   this is not completely accurate but hard to improve while
	   keeping a clean keyboard server design.
	 */
	jnc	kbd_done

        /* get the pre-translated bios key. */
	
	movb	%al,%ah			/* pass the scancode... */
	movb	$DOS_HELPER_GET_BIOS_KEY,%al
	int	$DOSHELPER_INT		/* call get_bios_key helper       */
					/* returns ax=keycode or 0,       */
					/* also copies new shift state to */
					/* seg 0x40                       */

	testw	%ax,%ax
	jz	kbd_done		/* no keycode returned */

/* check for "special action" codes
 */
	cmpw	$SP_PAUSE, %ax
	je	kbd_do_pause

	testb   $PAUSE_MASK,KEYBOARD_FLAGS_2	/* if pause bit not set */
	jz      check_special			/* continue */
	andb    $~PAUSE_MASK,KEYBOARD_FLAGS_2	/* reset pause bit */
	jmp     kbd_done			/* don't store in buffer */

check_special:
	cmpw	$SP_BREAK, %ax
	je	kbd_do_break
	cmpw	$SP_PRTSCR, %ax
	je	kbd_do_prtscr
	cmpw	$SP_SYSRQ_MAKE, %ax
	je	kbd_do_sysrq_make
	cmpw	$SP_SYSRQ_BREAK, %ax
	je	kbd_do_sysrq_break
        call    store_key
kbd_done:
	call	kbd_EOI
	popw	%ds
	popw	%bx
	popw	%ax			/* restore registers		*/
	iret        

store_key:        
        movw    KEYBUF_WRITE_PTR,%bx
        incw    %bx
        incw    %bx
        cmpw    KEYBUFFER_END,%bx
        jne     no_wrap
        movw    KEYBUFFER_START,%bx
no_wrap:
        cmpw    KEYBUF_READ_PTR,%bx
        je      buffer_full
        xchgw   KEYBUF_WRITE_PTR,%bx    /* ok, update write pointer     */
        movw    %ax,(%bx)               /* and store key in buffer      */
buffer_full:        
        ret
        
kbd_do_pause:
	testb   $PAUSE_MASK,KEYBOARD_FLAGS_2	/* already paused? */
	jnz     kbd_done			/* do nothing */
	orb	$PAUSE_MASK,KEYBOARD_FLAGS_2	/* set pause bit */
	movb	$DOS_HELPER_PAUSE_KEY,%al
	int	$DOS_HELPER_INT			/* pause program */
	jmp	kbd_done

kbd_do_break:				/* CTRL-BREAK pressed		*/
	xorw	%ax,%ax
	call	store_key		/* put null word into buffer	*/
	int	$0x1b			/* call BREAK interrupt		*/
	jmp	kbd_done

kbd_do_prtscr:				/* PRINT SCREEN pressed		*/
	int	$0x05
	jmp	kbd_done

kbd_do_sysrq_make:			/* Alt-SYSRQ pressed		*/
	movw	$0x8500,%ax
	int	$0x15
	jmp	kbd_done

kbd_do_sysrq_break:			/* ALT-SYSRQ released		*/
	movw	$0x8501,%ax
	int	$0x15
	jmp	kbd_done

kbd_do_CAD:
	call	kbd_EOI
	movw	$0x1234, 0x72
	ljmp	$0xffff, $0

kbd_EOI:
	inb	$0x61,%al		/* KBD IRQ ACK code from the */
	movb	%al,%ah			/* Lukach & Sibiriakov book */
	orb	$0x80,%al
	outb	%al,$0x61
	xchgb	%al,%ah
	outb	%al,$0x61
	movb   	$0x20,%al
	outb    %al,$0x20		/* tell pic we're done 		*/
	ret

#if 0
	.globl  CISH(INT09_dummy_end)
CISH(INT09_dummy_end):
#endif


/* COMPAS FEC59		jmp to INT13 FDD */
/* COMPAS FEC5C-FEF56	reserved */
/* COMPAS FEF57		jmp to INT0E */
/* COMPAS FEF5A-FEFC6	reserved */
/* ----------------------------------------------------------------- */
	.org	((INT1E_SEG - BIOSSEG) << 4) + INT1E_OFF
/* COMPAS FEFC7		FDD param table */
/* Win98/DOS uses it via int0x1e vector */
	.byte	0xaf	/* b7-4=step rate b3-0=head unload time */
	.byte	0x02	/* b7=1=head load time b0=0 */
	.byte	0x25	/* motor off delay in clock ticks */
	.byte	0x02	/* bytes per sector 00=128..03=1024 */
	.byte	18	/* sectors per track */
	.byte	0x1b	/* gap between sectors */
	.byte	0xff	/* ignored */
	.byte	0x6c	/* format gap length */
	.byte	0xf6	/* format filler byte */
	.byte	0x0f	/* head settle time (ms) */
	.byte	0x08	/* motor start time (1/8") */

/* COMPAS FEFD2		jmp to INT17 */
	.org	0xefd5
/* COMPAS FEFD5-FF064	reserved */
	.byte	0xdf
	.byte	0x02
	.byte	0x25
	.byte	0x02
	.byte	63
	.byte	0x1b
	.byte	0xff
	.byte	0x54
	.byte	0xf6
	.byte	0x0f
	.byte	0x08

/* ----------------------------------------------------------------- */
	.org	0xf065
/* ======================= Addr = FF065 */
/* COMPAS FF065		jmp to INT10 */
/* COMPAS FF068-FF0A3	reserved */
	/* relocated video handler (interrupt 0x42) */
        /* Note:	 A conforming video-bios will redirect int 42 here if
          it doesn't find anything else.  Another fun suprise :	(
          I'll have to implement this in my video-bios. --EB  13 Jan 97 */
	jmp bios_hlt_blk+0x42 /* redirect this back to our magic location */

/* COMPAS FF0A4		video param table */
/* COMPAS FF0FC-FF840	reserved */

	.org	EOI_OFF
/* ======================= Addr = F000:F100 (FF100) */
	pushw	%ax
        movb    $0x20,%al
        outb    %al,$0x20		/* flag interrupt complete	*/
	popw	%ax
	iret

/* ----------------------------------------------------------------- */   
	/* pause cycle */
	.org	((Pause_SEG - BIOSSEG) << 4) + Pause_OFF
/* ======================= Addr = F000:F110 (FF110) */
	pushf
	pushw 	%ds
	pushw 	%ax
	movw 	$BIOS_DATA,%ax
	movw 	%ax,%ds
idle_cycle:
	movw    $0x1680,%ax		/* magic machine idle function  */
	int     $0x2f
	testb 	$PAUSE_MASK,KEYBOARD_FLAGS_2 /* is pause bit still set? */
	jnz 	idle_cycle
	popw 	%ax
	popw	%ds
	popf
	lret

/* ----------------------------------------------------------------- */   
	/* the packet driver */
	.org	((PKTDRV_SEG - BIOSSEG) << 4) + PKTDRV_OFF
/* ======================= Addr = F000:F130 (FF130) */

	.globl	CISH(PKTDRV_start)
CISH(PKTDRV_start):
	jmp	bios_hlt_blk+0x60

/* The packet driver signature will be written here when the packet
   driver is initialized. */
	.asciz	"PKT DRVR"

	.globl	CISH(PKTDRV_driver_name)
CISH(PKTDRV_driver_name):
	.asciz	"Linux$"
	.byte	0

/* ===== LFN helper f000:f230 */
	.org	((LFN_HELPER_SEG-BIOSSEG) << 4)+LFN_HELPER_OFF
	pushw	%ds
        pushw	%dx
        pushw	%si
        movw    %cs, %si
        movw	%si, %ds
        movw	$LFN_string-bios_f000, %si
	cmpb	$0x6c, %ah
        je	do_6c
        movw	%si, %dx
        jmp     do_int21
do_6c:
        cmpb    $1, %al        /* ax=6c01 means created by lfn.c */
        jnz     do_int21
        movb    $0, %al        /* then transform to open (dl=1) */
        movb    $1, %dl
        int	$0x21
        movw    $2, %cx        /* flag creation */
        jmp     after_int21
do_int21:
        int	$0x21
after_int21:        
        popw	%si
        popw	%dx
        popw	%ds
        lret

/* ----------------------------------------------------------------- */

	.org	((DBGload_SEG - BIOSSEG) << 4) + DBGload_OFF
/* ======================= Addr = F000:F330 (FF330) */
/* we come here after we have intercepted INT21 AX=4B00
 * in order to get a breakpoint for the debugger
 * (wanting to debug a program from it's very beginning)
 */
	.globl	CISH(DBGload)
CISH(DBGload):
	cli	/* first we set up the users stack */
	movw	%cs:DBGload_SSSP+2-bios_f000,%ss
	movw	%cs:DBGload_SSSP-bios_f000,%sp
	mov	$0x62,%ah	/* we must get the PSP of the loaded program */
	int	$0x21
        movw	%bx,%es
	movw    %bx,%ds
	xorw    %ax,%ax
	movw    %ax,%bx
	movw    %ax,%cx
	movw    %ax,%dx
	movw    %ax,%si
	movw    %ax,%di
	movw    %ax,%bp
	sti
	ljmp	INDIRECT_PTR(%cs:CISH(DBGload_CSIP)-bios_f000)
				/* and give control to the program */

	.globl  CISH(bios_f000_endpart1)
CISH(bios_f000_endpart1):

/* COMPAS FF841		jmp to INT12 */
/* COMPAS FF844-FF84C	reserved */
/* COMPAS FF84D		jmp to INT11 */
/* COMPAS FF850-FF858	reserved */
/* COMPAS FF859		jmp to INT15 */
/* COMPAS FF85C-FFA6D	reserved */
/* COMPAS FFA6E		font tables */
/* COMPAS FFE6E		jmp to INT1A */
/* COMPAS FFE71-FFEA4	reserved */

	.globl	CISH(bios_f000_part2)
CISH(bios_f000_part2):

/* ----------------------------------------------------------------- */   
	.org	((INT08_SEG-BIOSSEG) << 4)+INT08_OFF

	.globl	CISH(INT08_dummy_start)
/* ======================= Addr = F800:7EA5 (FFEA5) */
CISH(INT08_dummy_start):		/* TIMER INTERRUPT ROUTINE	*/
/* COMPAS FFEA5		jmp to INT08 */
/* COMPAS FFEA8-FFEF2	reserved */
#if 0
	int	$0x1c	
#endif
/* NOTE: The above int 0x1c is a compatibility fault, because
 *       the original IBM Bios calls the user *after* the timer is
 *       increased. So, I moved it down.
 *       THIS NEEDS TO BE CHECKED for side effects in dosemu !
 */
	pushw	%ds
	pushw	%ax
	xorw	%ax, %ax		/* set ax to segment 0		*/
        movw	%ax,%ds
	incl	BIOS_TIMER
	cmpl	$HOUR24_ADJUST, BIOS_TIMER /* 24 hour check */
	jb	CISH(INT08_L1)
	movl	$0, BIOS_TIMER
	incb	BIOS_TIMER_OVERFLOW
CISH(INT08_L1):
			/* emulate 'diskette motor running */
			/* some old games rely on that --SW, --Hans */
	cmpb	$0, DISKETTE_MOTOR_TIMEOUT
	jz	CISH(INT08_L2)
	decb	DISKETTE_MOTOR_TIMEOUT
	jnz	CISH(INT08_L2)
	/* turn floppy motor off (code from Bochs) */
	pushw	%dx
	movw	$0x3f2, %dx
	inb	%dx, %al
	andb	$0xcf, %al
	outb	%al, %dx
	popw	%dx
	
CISH(INT08_L2):
	int	$0x1c		/* call int 0x1c, per bios spec	*/
				/* must do it before EOI, but after count */
        movb    $0x20,%al
        outb    %al,$0x20		/* flag interrupt complete	*/
	popw	%ax			/* restore registers            */
	popw	%ds
	iret				/* return to interrupted code	*/
	.globl  CISH(INT08_dummy_end)
CISH(INT08_dummy_end):
        
/* COMPAS FFEF3		vector table for INT08-INT1F */
/* COMPAS FFF23		vector table for INT70-INT77 */
/* COMPAS FFF33-FFF53	reserved */
/* COMPAS FFF54		jmp to INT05 */
/* COMPAS FFF57-FFFD8	reserved */
/* COMPAS FFFD9		EISA ident string */
/* COMPAS FFFDD-FFFEF	reserved */

/* ----------------------------------------------------------------- */
	.org	0xffe0

	/* DOSEMU magic and version field */
	.ascii	"$DOSEMU$"
	.long	DOSEMU_VERSION_CODE

/* ----------------------------------------------------------------- */
	.org	0xfff0
/* COMPAS FFFF0		jmp to powerup */
	ljmp	$BIOSSEG, $ROM_BIOS_SELFTEST
/* COMPAS FFFF5		ROM BIOS date */
	.ascii	"02/25/93"  /* our bios date */
/* COMPAS FFFFD		unused */
	hlt
/* COMPAS FFFFE		system model ID */
	.byte	0xfc   /* model byte = IBM AT */
/* COMPAS FFFFF		unused */
	hlt

	.globl  CISH(bios_f000_end)
CISH(bios_f000_end):

/*--------------------------------------------------------------------------*/

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
