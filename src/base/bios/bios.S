/*************************************************************************/
/**                                                                     **/
/** This is the virtual PC's Bios (F000:0 .. F000:FFFF)                 **/
/**                                                                     **/
/** modified from as86 to gas by Bart Oldeman Jan 2001			**/
/**                                                                     **/
/*************************************************************************/

#define __ASM__
#include "dosemu_config.h"
#include "version.h"
#include "memory.h"
#include "macros86.h"
#include "doshelpers.h"

#include "keyb_server.h"

/* some other useful definitions */

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
	.globl	bios_f000
bios_f000:

	.org	((DOSEMU_LMHEAP_SEG - BIOSSEG) << 4) + DOSEMU_LMHEAP_OFF
/* ======================= Addr = F000:4000 (F4000) */
	/* 32K Heap here for the dosemu internal needs */
	.REPT DOSEMU_LMHEAP_SIZE
	    .byte 0
	.ENDR

	.org	BIOS_HLT_BLK - (BIOSSEG << 4)
bios_hlt_blk:
/* ======================= Addr = F000:C000 (FC000) */
	FILL_OPCODE BIOS_HLT_BLK_SIZE,hlt
/* ----------------------------------------------------------------- */
	.org	((DPMI_SEG-BIOSSEG) << 4)+DPMI_OFF
/* ======================= Addr = F800:4800 (FC800) */
	.globl	DPMI_dummy_start
DPMI_dummy_start:
/* ======================= Addr = F800:4807 (FC807) */
	.globl	DPMI_dpmi_init
DPMI_dpmi_init:
	hlt
	lret
/* ======================= Addr = F800:480A (FC80A) */
	.globl	DPMI_return_from_dos
DPMI_return_from_dos:
	hlt
/* ======================= Addr = F800:480B (FC80B) */
	.globl	DPMI_return_from_rmint
DPMI_return_from_rmint:
	hlt
/* ======================= Addr = F800:480C (FC80C) */
	.globl	DPMI_return_from_realmode
DPMI_return_from_realmode:
	hlt
/* ======================= Addr = F800:480D (FC80D) */
	.globl	DPMI_return_from_dos_memory
DPMI_return_from_dos_memory:
	hlt
/* ======================= Addr = F800:480E (FC80E) */
	.globl DPMI_int1c
DPMI_int1c:
	hlt
	iret
/* ======================= Addr = F800:4810 (FC810) */
	.globl	DPMI_int23
DPMI_int23:
	hlt
	iret
/* ======================= Addr = F800:4812 (FC812) */
	.globl	DPMI_int24
DPMI_int24:
	hlt
	iret
/* ======================= Addr = F800:4814 (FC814) */
	.globl	DPMI_raw_mode_switch_rm
DPMI_raw_mode_switch_rm:
	hlt
/* ======================= Addr = F800:4816 (FC816) */
	.globl	DPMI_save_restore_rm
DPMI_save_restore_rm:
	hlt
	lret
/* ======================= Addr = F800:4817 (FC817) */
	.globl	DPMI_return_from_dosext
DPMI_return_from_dosext:
	hlt
/* ======================= Addr = F800:4818 (FC818) */
	.globl	DPMI_dummy_end
DPMI_dummy_end:


/* ----------------------------------------------------------------- */
	.org	((XMSControl_SEG - BIOSSEG) << 4) + XMSControl_OFF
/* ======================= Addr = F800:4C40 (FCC40) */
        jmp     (.+2+3) /* jmp short forward 3 */
        FILL_OPCODE 3,nop
        hlt

/******************************************************************
 * DATA BLOCK
 ******************************************************************/

.org	0xd000

	.globl	bios_f000_int10ptr
bios_f000_int10ptr:
	.long	0xc0000003

	.globl	bios_f000_int10_old
bios_f000_int10_old:
	.long	0

	.globl	bios_in_int10_callback
bios_in_int10_callback:
	.byte	0

	.globl	bios_f000_bootdrive
bios_f000_bootdrive:
	.byte	0

	/* this is the paramblock, we told DOS to use for INT21 AX=4B01 */
	.align	16,0
	.globl	DBGload_parblock
DBGload_parblock:	.word	0
			.long	0,0,0
DBGload_SSSP:		.long	0
	.globl	DBGload_CSIP
DBGload_CSIP:	.long	0

	/* parameter packet, filled in by pkt_init */
	.globl	PKTDRV_param
PKTDRV_param:
	.byte	0,0,0,0
	.word	0,0,0,0,0

	/* driver statistics structure */
	.globl	PKTDRV_stats
PKTDRV_stats:
	.long	0,0,0,0,0,0,0

	.globl	LFN_string
LFN_string:
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
	hlt
	sti

/* ----------------------------------------------------------------- */	
/* This is for the video init - it calls in order:
   - first helper function (int0xe6,al=8)
   - the video BIOS init entry point at C000:3 or E000:3
   - second helper function (int 0xe6,al=9)
 */
	movb	$DOS_HELPER_VIDEO_INIT,%al		/*  Start Video init */
	int	$DOS_HELPER_INT
	cmpb	$0, %al
	je	no_vbios_post
	/* call far 0xc000:3  or call far 0xe000:3 */
	/* More general than just c000 or e000 ??? */	
	pushw	%ds
	lcall	*%cs:bios_f000_int10ptr-bios_f000
	popw	%ds
	sti
	jmp	video_init_done

/* ----------------------------------------------------------------- */	
/* Post-less video init
 */
no_vbios_post:
	movb	$3,%al
	int	$0x10
video_init_done:
	movb	$DOS_HELPER_VIDEO_INIT_DONE,%al		/* Finished video init */
	int	$DOS_HELPER_INT

	movb	$DOS_HELPER_SHOW_BANNER,%al
	int	$DOS_HELPER_INT
	movb	%cs:bios_f000_bootdrive-bios_f000,%dl

	movb	$DOS_HELPER_READ_MBR,%al
	int	$DOS_HELPER_INT
	ljmp	$0x0, $0x7c00		/* Some boot sectors require cs=0 */

.org	ROM_BIOS_EXIT
/* ======================= Addr = F000:E2B0 (FE2B0) */
	/* set up BIOS exit routine */
	movw	$DOS_HELPER_REALLY_EXIT,%ax
	int	$DOS_HELPER_INT

/* COMPAS FE2C3		jmp to NMI */

.org	GET_RETCODE_HELPER
/* ======================= Addr = F000:E2C6 (FE2C6) */
	movb	$0x4d, %ah
	int	$0x21
	movw	%ax, %bx
	movb	$DOS_HELPER_SET_RETCODE, %al
	int	$DOS_HELPER_INT
	lret

/* ----------------------------------------------------------------- */
	/* this is IRET */
	.org	((IRET_SEG - BIOSSEG) << 4) + IRET_OFF
/* ======================= Addr = F800:62DF (FE2DF) */
	iret

        .org	((Mouse_SEG - BIOSSEG) << 4)+Mouse_ROUTINE_OFF
/* ======================= Addr = F000:E2E0 (FE2E0) */

	/* This is the int74 handler */
	pushw	%ax	/* save everything */
	pushw	%bx
	movw	$DOS_HELPER_MOUSE_HELPER,%ax	/* mouse helper */
	movw	$0xf2,%bx	/* call the user or PS/2 hook */
	int	$DOS_HELPER_INT
	popw	%bx
	popw	%ax
	ljmp $BIOSSEG, $EOI2_OFF

/* ----------------------------------------------------------------- */
	.org	((IPX_SEG - BIOSSEG) << 4) + IPX_OFF
/* ======================= Addr = F800:6310 (FE310) */
ipx_handler:
	int	$0x7a
	lret

/* ----------------------------------------------------------------- */
		/* This is an int e7 used for FCB opens */
	.org	((FCB_HLP_SEG-BIOSSEG) << 4)+FCB_HLP_OFF
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
#if 0
	cmpb	$0x11,%ah
	je	L9	/* character generator, possibly resize the screen */
#endif
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
	movw	$DOS_SUBHELPER_MOUSE_START_VIDEO_MODE_SET,%bx	/* start video mode set */
	int	$DOS_HELPER_INT
	popw	%bx
	popw	%ax

/* fake stack frame for iret: push original flags and avoid a GPF from pushf */
	movzwl	%sp,%esp	/* make sure high of esp is zero */
	pushw	6(%esp)
	pushw	%cs
	call	L10	/* perform the actual mode set */

/* since following code doesn't affect flags, we keep current values */
	pushw	%ax	/* remember everything from int10 call */
	pushw	%bx
	movw	$DOS_HELPER_MOUSE_HELPER,%ax	/* mouse helper */
	movw	$DOS_SUBHELPER_MOUSE_END_VIDEO_MODE_SET,%bx	/* end video mode set */
	int	$DOS_HELPER_INT
	popw	%bx
	popw	%ax
	addw	$2,%sp/* pop video mode */	
	lret	$2    /* keep current flags and avoid another GPF from iret */

L10:	/* chain to original handler (probably the video bios) */
	ljmp	*%cs:bios_f000_int10_old-0x8000-bios_f000

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
	pushw	4(%esp)
	pushw	%cs
	call	WINT10
	hlt
	lret	$2    /* keep current flags and avoid another GPF from iret */

/* ----------------------------------------------------------------- */
	.org	((INT70_SEG-BIOSSEG) << 4)+INT70_OFF

	.globl	INT70_dummy_start
/* ======================= Addr = F800:63F0 (FE3F0) */
INT70_dummy_start:		/* RTC INTERRUPT ROUTINE	*/

	pushw	%ax
	int	$0x4a
        movb    $0x20,%al
        outb    %al,$0xa0		/* flag interrupt complete	*/
	outb	%al,$0x20
	popw	%ax			/* restore registers            */
	iret				/* return to interrupted code	*/
	.globl  INT70_dummy_end
INT70_dummy_end:


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
	.org	((INT09_SEG-BIOSSEG) << 4)+INT09_OFF
#if 0
	.globl	INT09_dummy_start
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
	movb	$0xad,%al
	outb	%al,$0x64
	inb	$0x60,%al
	movb	$0xae,%ah
	xchg	%al,%ah
	outb	%al,$0x64
	xchg	%al,%ah

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
	int	$DOS_HELPER_INT		/* call get_bios_key helper       */
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
kbd_done2:
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
	call	kbd_EOI
	sti
1:
	movw    $0x1680,%ax		/* magic machine idle function  */
	int     $0x2f
	testb 	$PAUSE_MASK,KEYBOARD_FLAGS_2 /* is pause bit still set? */
	jnz 	1b
	cli
	jmp	kbd_done2

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
	.globl  INT09_dummy_end
INT09_dummy_end:
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
	.byte	0xff	/* maximum track number */
	.byte	0xff	/* data transfer rate */
	.byte	0	/* drive type */

/* COMPAS FEFD2		jmp to INT17 */
	.org	((INT1E_SEG - BIOSSEG) << 4) + INT1E_OFF + 14
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
	.byte	0xff	/* maximum track number */
	.byte	0xff	/* data transfer rate */
	.byte	0	/* drive type */

/* ----------------------------------------------------------------- */
	.org	((INT42HOOK_SEG - BIOSSEG) << 4) + INT42HOOK_OFF
/* ======================= Addr = FF065 */
/* COMPAS FF065		jmp to INT10 */
/* COMPAS FF068-FF0A3	reserved */
	/* relocated video handler (interrupt 0x42) */
        /* Note:	 A conforming video-bios will redirect int 42 here if
          it doesn't find anything else.  Another fun suprise :	(
          I'll have to implement this in my video-bios. --EB  13 Jan 97 */
	hlt

/* COMPAS FF0A4		video param table */
/* COMPAS FF0FC-FF840	reserved */

	.org	EOI_OFF
/* ======================= Addr = F000:F100 (FF100) */
	pushw	%ax
        movb    $0x20,%al
        outb    %al,$0x20		/* flag interrupt complete	*/
	popw	%ax
	iret

	.org	EOI2_OFF
/* ======================= Addr = F000:F110 (FF110) */
	pushw	%ax
	movb    $0x20,%al
	outb    %al,$0xa0
	outb    %al,$0x20		/* flag interrupt complete	*/
	popw	%ax
	iret

/* ----------------------------------------------------------------- */
	/* the packet driver */
	.org	((PKTDRV_SEG - BIOSSEG) << 4) + PKTDRV_OFF
/* ======================= Addr = F000:F140 (FF140) */

	.globl	PKTDRV_start
PKTDRV_start:
/*	jmp to entry point is also used as signature, and therefore
	it have to be jmp with word displacement. I've found no way
	to tell gas that I want jmp with word displacement, so hardcode
	it as 0xe9 */
	.byte 0xe9
	.word PKTDRV_entry-.-2

/* The packet driver signature will be written here when the packet
   driver is initialized. */
	.asciz	"PKT DRVR"

	.globl	PKTDRV_driver_name
PKTDRV_driver_name:
	.asciz	"Linux$"
	.byte	0

PKTDRV_entry:
	ljmp	*%cs:PKTDRV_driver_entry-bios_f000

	.align 4,0
PKTDRV_driver_entry:
	.globl	PKTDRV_driver_entry_ip
PKTDRV_driver_entry_ip:
	.word 0
	.globl	PKTDRV_driver_entry_cs
PKTDRV_driver_entry_cs:
	.word 0

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
        movb    $0, %al
        jnz     do_int21
        movb    $1, %dl        /* then transform to open (dl=1) */
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
	.globl	DBGload
DBGload:
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
	pushf
	/* set TF */
	orw	$0x100, %ss:(%esp)
	/* can use iret here but the trap will be generated by the
	 * _second_ instruction after iret, not the first one. Since we
	 * dont want to skip the first instruction, we use 2 instructions
	 * for control transfer. Then the trap happens at the right place.
	 */
	popf
	/* and give control to the program */
	ljmp    *%cs:DBGload_CSIP-bios_f000

/* ======================= Addr = F000:F400 (FF400) */
	.org	((DOS_LONG_READ_SEG - BIOSSEG) << 4) + DOS_LONG_READ_OFF
	.globl MSDOS_lr_start
MSDOS_lr_start:
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
	jc	read_set_cf
	movzwl	%ax, %eax
	pushl	%ecx
	pushl	%eax
	movl	%eax, %ecx
	movb	$0, %al		/* read */
	lcall   *%cs:MSDOS_lr_entry-bios_f000
	popl	%eax
	popl	%ecx
	addl	%eax, %edi
	cmpw	%ax, %cx
	jnz	done_read
	movl	%esi, %ecx
	subl	%eax, %ecx
	jnz	start_read
	movl	%edi, %eax
	jmp done_read
read_set_cf:
	orl	%edi, %edi
	jz 1f
	movl	%edi, %eax
	jmp	done_read
1:
	movl	%eax, %ecx
	movb	$2, %al		/* set CF */
	lcall   *%cs:MSDOS_lr_entry-bios_f000
#	jmp	done_read
done_read:
	popl	%ecx
	popl	%edi
	popl	%esi
	iret

        .align 4,0
MSDOS_lr_entry:
        .globl  MSDOS_lr_entry_ip
MSDOS_lr_entry_ip:
        .word 0
        .globl  MSDOS_lr_entry_cs
MSDOS_lr_entry_cs:
        .word 0

/* ======================= Addr = F000:F4A0 (FF4A0) */
	.org	((DOS_LONG_WRITE_SEG - BIOSSEG) << 4) + DOS_LONG_WRITE_OFF
	.globl MSDOS_lw_start
MSDOS_lw_start:
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
	movb	$1, %al		/* write */
	lcall   *%cs:MSDOS_lw_entry-bios_f000
	movb	$0x40, %ah
	int	$0x21
	jc	write_set_cf
	movzwl	%ax, %eax
	addl	%eax, %edi
	cmpw	%ax, %cx
	jnz	done_write
	movl	%esi, %ecx
	subl	%eax, %ecx
	jnz	start_write
	movl	%edi, %eax
	jmp	done_write
write_set_cf:
	orl	%edi, %edi
	jz 1f
	movl	%edi, %eax
	jmp	done_write
1:
	movl	%eax, %ecx
	movb	$2, %al		/* set CF */
	lcall   *%cs:MSDOS_lw_entry-bios_f000
#	jmp	done_write
done_write:
	popl	%ecx
	popl	%edi
	popl	%esi
	iret

        .align 4,0
MSDOS_lw_entry:
        .globl  MSDOS_lw_entry_ip
MSDOS_lw_entry_ip:
        .word 0
        .globl  MSDOS_lw_entry_cs
MSDOS_lw_entry_cs:
        .word 0

	.globl  bios_f000_endpart1
bios_f000_endpart1:

/* COMPAS FF841		jmp to INT12 */
/* COMPAS FF844-FF84C	reserved */
/* COMPAS FF84D		jmp to INT11 */
/* COMPAS FF850-FF858	reserved */
/* COMPAS FF859		jmp to INT15 */
/* COMPAS FF85C-FFA6D	reserved */
/* COMPAS FFA6E		font tables */
/* COMPAS FFE6E		jmp to INT1A */
/* COMPAS FFE71-FFEA4	reserved */

	.globl	bios_f000_part2
bios_f000_part2:

/* ----------------------------------------------------------------- */
	.org    ((INT75_SEG-BIOSSEG) << 4)+INT75_OFF
/* ======================= Addr = F800:7E98 (FFE98) */
	xorb %al, %al
	outb %al, $0xf0
	movb $0x20, %al
	outb %al, $0xa0
	outb %al, $0x20
	int $2		/* Bochs does this; RBIL says: redirected to INT 02 */
        iret		/* by the BIOS, for compatibility with the PC */

/* ----------------------------------------------------------------- */
	.org	((INT08_SEG-BIOSSEG) << 4)+INT08_OFF

	.globl	INT08_dummy_start
/* ======================= Addr = F800:7EA5 (FFEA5) */
INT08_dummy_start:		/* TIMER INTERRUPT ROUTINE	*/
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
	jb	INT08_L1
	movl	$0, BIOS_TIMER
	incb	BIOS_TIMER_OVERFLOW
INT08_L1:
			/* emulate 'diskette motor running */
			/* some old games rely on that --SW, --Hans */
	cmpb	$0, DISKETTE_MOTOR_TIMEOUT
	jz	INT08_L2
	decb	DISKETTE_MOTOR_TIMEOUT
	jnz	INT08_L2
	/* turn floppy motor off (code from Bochs) */
	pushw	%dx
	movw	$0x3f2, %dx
	inb	%dx, %al
	andb	$0xcf, %al
	outb	%al, %dx
	popw	%dx
	
INT08_L2:
	int	$0x1c		/* call int 0x1c, per bios spec	*/
				/* must do it before EOI, but after count */
        movb    $0x20,%al
        outb    %al,$0x20		/* flag interrupt complete	*/
	popw	%ax			/* restore registers            */
	popw	%ds
	iret				/* return to interrupted code	*/
	.globl  INT08_dummy_end
INT08_dummy_end:

/* ----------------------------------------------------------------- */
	.org  ((INT71_SEG-BIOSSEG) << 4)+INT71_OFF

	.globl  INT71_dummy_start
/* ======================= Addr = F800:7EE7 (FFEE7) */
INT71_dummy_start:   /* PC/AT IRQ9->IRQ2 REVECTOR ROUTINE  */
	push %ax
	/* EOI to PIC2 */
	movb $0x20, %al
	outb %al, $0xa0
	pop %ax
	/* Then invoke IRQ2 */
	int $0x0a
	iret        /* return to interrupted code */
	.globl  INT71_dummy_end
INT71_dummy_end:

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

	.globl  bios_f000_end
bios_f000_end:

/*--------------------------------------------------------------------------*/

.section .note.GNU-stack,"",%progbits
