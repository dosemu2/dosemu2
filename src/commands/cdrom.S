! 
! (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!



use16

.text
.bss
.data
.align 0

	.org	0

LinuxCD	=	0xe6		! int 0xe6 for cdrom server
Helper  =       0x40		! code to reach cdrom server
cr	=	0x0d
lf	=	0x0a
eom	=	'$'


.globl _main
_main:

! header of a console device
ConDev:
	.long	-1		! pointer to next driver
	.word	0xc800		! driver attributes
        .word	Strategy        ! pointer to strategy routine
        .word   Entry		! pointer to service routine
        .ascii  "MSCD000"	! logical device name
Index:	.ascii	"1"
	.word	0		! reserved
        .byte   0
        .byte   1		! number of units supported by driver
        .ascii  "MSCD00"	! signature

! command jump table
ConTbl:
	.word	coninit		! initialize driver
	.word	cmderr		! media check
	.word	cmderr		! build BPB
	.word	ioctlin		! input of control strings
	.word	cmderr		! input
	.word	cmderr		! input no wait
	.word 	cmderr		! input status
	.word	input_flush	! input flush
	.word 	cmderr		! output
	.word	cmderr		! output verify
	.word	cmderr		! output status
	.word	cmderr		! output flush
	.word	ioctlout	! output of control strings
	.word	dev_open	! device open
	.word 	dev_close	! device close
	.word 	cmderr		! check if media is removed
	.word  	cmderr		! out until busy
! now the commands defined by the cdrom extensions
MscdTbl:
	.word	read_long	! read data from cdrom drive
	.word	cmderr		! reserved
	.word	cmderr          ! read long prefetch
        .word   seek            ! position drives head
        .word   play            ! play audio
        .word   stop            ! stop playing audio
        .word   cmderr          ! write long
        .word 	cmderr          ! write verify
        .word   resume          ! resume playing audio

ptrsav:	.long	0		! saved pointer to request header
arg:	.byte	0		! Number of CDROM drive 0 ... 3


! structure of command block header (13 bytes)
CmdLen	= 	0		! length of header
Unit   	=	1		! sub unit id
CmdCode	=	2		! describes command to be executed
Status	=	3		! status code
Reserved=	5		! bytes 5 to 12 are reserved

Strategy:
	seg cs			! save adress of command block
	mov	[ptrsav], bx	! in ptrsav
	seg cs
	mov 	[ptrsav+2], es
	retf

Entry:
	push	si
	push	ax
	push	cx
	push	dx
	push	di
	push	bp
	push	ds
	push	es
	push	bx

	push	cs
	pop	ds
	les	di,[ptrsav]	! es:di now points to the command block

	seg es
	movb	al,[di+CmdCode] ! get command code
	movb	ah,#0
	test	al,#0x80	! test if highes bit is set
	jz	StdCommand
	and	al,#0x7F	! AL = AL-128
	cmp	al,#8	
	ja	cmderr
	mov	si,#MscdTbl
	jmp	EntryCon

StdCommand:
	cmp	al,#16
	ja 	cmderr
	mov	si,#ConTbl
EntryCon:
	add	si,ax
	add	si,ax
! now we have in es:di the adress of the command block and cs = ds
! now call command handler
	jmp     [si]


cmderr:
	mov	ax,#3		! unknown command
	movb    ah,#0x80	! set error bit
exit:
! here we expect that ax contains a valid status code
	or	ah,#0x01	! set done flag
	push	cs
	pop	ds
	les	di,[ptrsav]     ! pointer to command block to es:di

	seg es
	mov 	[di+Status],ax

	pop	bx
	pop	es
	pop	ds
	pop	bp
	pop	di
	pop	dx
	pop	cx
	pop	ax
	pop	si
	retf


!-----------------------------------------------------------------------
! the device functions
!-----------------------------------------------------------------------


!-----------------------------------------
! device open / device close / input flush

dev_open:
	mov	ax,#0
	br	exit

dev_close:
	mov	ax,#0
	br	exit

input_flush:
	mov	ax,#0
	br	exit



!----------------------------------------
! play / stop / resume

play:
	movb	al, #Helper
	movb	ah, #0x04
	seg 	cs
	orb	ah, arg
	int	LinuxCD
	cmp	al, #0
	jne	play_fail

	movb	al, #0
	movb	ah, #0x02	; set busy bit
	br	exit
play_fail:
	movb	al, #0x0B	; set error code
	movb	ah, #0x80	; set error bit
	br	exit

stop:
	movb	al, #Helper
	movb	ah, #0x05
	seg	cs
	orb	ah, arg
	int	LinuxCD
 	mov	ax,#0
	br	exit
	
resume:
	movb	al, #Helper
	movb	ah, #0x06
	seg	cs
	orb	ah, arg
	int	LinuxCD
	cmp	al, #0
	jne	resume_fail

	mov	al, #0
	mov	ah, #0x02
	br	exit
resume_fail:
	movb	al, #0x0C	; set error code
	movb	ah, #0x80	; set error bit
	br	exit

	
!----------------------------------------
! read long / seek

seek:
	movb	al, #Helper
	movb	ah, #0x03
	seg	cs
	orb	ah, arg
	int	LinuxCD
	mov	ax,#0
	br	exit

READLONG_TRANSFER = 14

read_long:
	movb	al, #Helper
	movb	ah, #0x02
	seg 	cs
        orb	ah, arg
        seg es
	lds	si, [di+READLONG_TRANSFER]
	int	LinuxCD
	cmp	al, #0
	jne	read_long_fail

	mov	ax,#0
	br	exit
read_long_fail:
        movb	al, #0x0F	! set error code
	movb	ah, #0x80	! set error bit
	br	exit


!----------------------------------------
! ioctlout

! structure of an ioctlout request
IOCTLOUT_MDB	= 13		! media descriptor byte (not used)
IOCTLOUT_TRANS	= 14		! transfer adress
IOCTLOUT_TRANSLEN= 18		! number of bytes to be transfered
IOCTLOUT_SEKTOR  = 20		! not used
IOCTLOUT_VOL     = 22		! not used (4 bytes)	

! ioctlout jump table
IoctloutTbl:	.word	eject
		.word	lockunlock
		.word	resetdrive
		.word	audio_ctrl
		.word	cmderr
		.word	closetray

ioctlout:
	seg es
	lds	si,[di+IOCTLOUT_TRANS]

! now we have:
!   es:di points to command control block
!   ds:si points to request header
!   cs!=ds

	movb	al,[si+0]	; get request code
	movb	ah,#0
	cmp	ax,#5
	ja	ioctlout_err

	push	cs
	pop	es
	mov	di,#IoctloutTbl
	add	di,ax
	add	di,ax

	seg es
	jmp	[di]

ioctlout_err:
	br	cmderr

! eject disk
eject:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x0D
	seg	cs
	orb	ah,arg
	int	LinuxCD

	mov	ax,#0
	br	exit
	

! function lock - unlock drive
lockunlock:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x0C
	seg	cs
        orb	ah,arg
        movb	bl,[si+1]
	int	LinuxCD

	mov	ax,#0
	br	exit

! resetdrive
resetdrive:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x0B
	seg	cs
	orb	ah,arg
	int	LinuxCD

	mov	ax,#0
	br	exit

! function closetray
closetray:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x0E
	seg	cs
	orb	ah,arg
	int	LinuxCD

	mov	ax,#0
	br	exit

! alter audio control
audio_ctrl:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x0F
	seg	cs
	orb	ah,arg
	int	LinuxCD

	br 	check_busy


!----------------------------------------
! ioctlin

! structure of an ioctlin request
IOCTLIN_MDB	= 13		! media descriptor byte (not used)
IOCTLIN_TRANS	= 14		! transfer adress
IOCTLIN_TRANSLEN= 18		! number of bytes to be transfered
IOCTLIN_SEKTOR  = 20		! not used
IOCTLIN_VOL     = 22		! not used (4 bytes)	

! ioctlin jump table
IoctlinTbl:	.word	devheader_adress
		.word	lochead
		.word	cmderr
		.word	cmderr
		.word	getchaninfo
		.word	cmderr
		.word	devstatus
		.word	ret_sectorsize
		.word	ret_volumesize
		.word	media_changed
		.word	diskinfo
		.word	trackinfo
		.word	qchannel
		.word	cmderr
		.word 	cmderr
		.word	audiostatus

ioctlin:
	seg es
	lds	si,[di+IOCTLIN_TRANS]

! now we have:
!   es:di points to command control block
!   ds:si points to request header
!   cs!=ds

	movb	al,[si+0]	; get request code
	movb	ah,#0
	cmp	ax,#15
	ja	ioctlin_err

	push	cs
	pop	es
	mov	di,#IoctlinTbl
	add	di,ax
	add	di,ax

	seg es
	jmp	[di]

ioctlin_err:
	br	cmderr

! function get device header adress
HEADER_ADRESS	= 1

devheader_adress:
	mov	ax,#ConDev
	push	cs
	pop	bx
	mov	[si+HEADER_ADRESS+0],ax
	mov	[si+HEADER_ADRESS+2],bx
	mov	ax,#0
	br	exit
	
! get device status
DEVICE_PARMS = 1

devstatus:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x0A
	seg	cs
	orb	ah,arg
	int	LinuxCD

	mov	[si+DEVICE_PARMS+0],bx
	mov	[si+DEVICE_PARMS+2],#0
	br	check_busy

! return sectorsize
RET_SECTORSIZE_READMODE	= 1
RET_SECTORSIZE_SIZE	= 2

ret_sectorsize:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x08
	seg	cs
	orb	ah,arg
	int	LinuxCD

	mov	[si+RET_SECTORSIZE_SIZE+0],bx
	mov	ax,#0
	br	exit

! return volumesize
ret_volumesize:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x12
	seg	cs
	orb	ah,arg
	int	LinuxCD

	mov	ax,#0
	br	exit

! media changed ?
MEDIA_BYTE = 1

media_changed:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x09
	seg	cs
	orb	ah,arg
	int	LinuxCD

	cmp	bl,#0
	jne	media_changed_dnr
	movb	bh,#1			! media not changed/disk ready
	movb	[si+MEDIA_BYTE], bh
	br	check_busy
media_changed_dnr:
	movb	bh,#0xff                ! media changed
	movb	[si+MEDIA_BYTE], bh
	br	check_busy

! location of head
lochead:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x07
	seg	cs
	orb	ah,arg
	int	LinuxCD

	br	check_busy

! get status of audio
audiostatus:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x14
	seg	cs
	orb	ah,arg
	int	LinuxCD

	br	check_busy

! get q-channel information
qchannel:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x13
	seg	cs
	orb	ah,arg
	int	LinuxCD

	br	check_busy

! get information about tracks
trackinfo:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x11
	seg	cs
	orb	ah,arg
	int	LinuxCD

	br	check_busy

! get information about tracks
diskinfo:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x10
	seg	cs
	orb	ah,arg
	int	LinuxCD

	br	check_busy

! get information about channels
getchaninfo:
	seg cs
	les	di,[ptrsav]
	movb	al,#Helper
	movb	ah,#0x15
	seg	cs
	orb	ah,arg
	int	LinuxCD

	br	check_busy



! audio playing in progress ?
! hier noch irgendwie eine Fehlerbehandlung einbauen....


check_busy:
        cmp     al,#1
        je      error
	cmp	ah,#1
	je	busy
	mov	ax,#0
	br	exit
busy:
	movb	al,#0
	movb	ah,#0x02
	br	exit
error:
	movb	al,#0x0C
	movb	ah,#0x80	; set error bit
	br	exit


!----------------------------------------
! init (must be at the end!)

! structure of an init request:
INIT_NumOfUnits	= 13
INIT_EndAdress	= 14
INIT_BPB	= 18
INIT_BDN	= 22


Message:
	.ascii	"[dosemu cdrom driver installed (V0.2)]"
	.byte	cr,lf,eom
FailMsg:
	.byte	cr,lf,lf
	.ascii  "Access to cdrom denied."
	.byte	cr,lf
	.ascii	"Installation aborted."
	.byte	cr,lf,lf,eom

coninit:
	jmp 	ParseArgs
cont:	mov	dx, #Message
	movb	ah, #0x09
	int	0x21

 	movb	ah, #0x01
	seg	cs
	orb	ah, arg
	movb	al, #Helper
	int	LinuxCD
	cmp	al, #0
	JE	InitEnde
	mov	dx, #FailMsg
	movb	ah, #0x09
	int     0x21
InitEnde:
	seg es
	mov	[di+INIT_EndAdress+0], #Message
        mov	ax,cs
        seg es
	mov	[di+INIT_EndAdress+2], ax
! future calls to init must return in error
	mov	[ConTbl], #cmderr

	mov	ax, #0
	br      exit

ParseArgs:
	push 	ds 		!On entry es:di points to the command line
	push 	si		!the relevant data start at offset 0x12
	push	ax		!If we have _no_ parameter, the default
				!will be drive 0 and name MSCD0001
	push 	es
	pop	ds
	lds 	si,[di+0x012]	!ds:si now pointing to our paramter
	
	jmp IniParse

EndParseArgs:
	pop	ax
	pop	si
	pop	ds
	jmp 	cont

IniParse:
	cmpb	[si],#0x0d
	jz	EndParseArgs
	cmpb	[si],#0x20
	jz	Label
	inc	si
	jmp	IniParse


Label:	inc 	si

StartParse:
	cmpb 	[si],#0x0d	
	jz 	EndParseArgs
	cmpb 	[si],#0x20
	jz 	NextChar

	cmpb	[si],#0x31		!lowest allowed value is '1'
	jb	NextChar		!if not get next character
	cmpb	[si],#0x34		!highest allowed value is '4'
	ja	NextChar		!if higher get next character
					!if lower all OK
	movb	al,[si]
	seg	cs
	movb	Index,al		!modify the device name ('1'...'4')
	sub 	al,#0x31		!calculate the index 0..3
	shl	al,6
	seg	cs
	mov	arg,al	

	jmp	EndParseArgs

NextChar:
	inc 	si
	jmp	StartParse


