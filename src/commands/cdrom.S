# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#



.code16
.text
	.globl	_start16
_start16:

LinuxCD	=	0xe6		# int 0xe6 for cdrom server
Helper  =       0x40		# code to reach cdrom server
cr	=	0x0d
lf	=	0x0a
eom	=	'$'


# header of a console device
ConDev:
	.long	-1		# pointer to next driver
	.word	0xc800		# driver attributes
        .word	Strategy        # pointer to strategy routine
        .word   Entry		# pointer to service routine
        .ascii  "MSCD000"	# logical device name
Index:	.ascii	"1"
	.word	0		# reserved
        .byte   0		# drive letter
        .byte   1		# number of units supported by driver
        .ascii  "MSCD00"	# signature

# command jump table
ConTbl:
	.word	coninit		# initialize driver
	.word	cmderr		# media check
	.word	cmderr		# build BPB
	.word	ioctlin		# input of control strings
	.word	cmderr		# input
	.word	cmderr		# input no wait
	.word 	cmderr		# input status
	.word	input_flush	# input flush
	.word 	cmderr		# output
	.word	cmderr		# output verify
	.word	cmderr		# output status
	.word	cmderr		# output flush
	.word	ioctlout	# output of control strings
	.word	dev_open	# device open
	.word 	dev_close	# device close
	.word 	cmderr		# check if media is removed
	.word  	cmderr		# out until busy
# now the commands defined by the cdrom extensions
MscdTbl:
	.word	read_long	# read data from cdrom drive
	.word	cmderr		# reserved
	.word	cmderr          # read long prefetch
        .word   seek            # position drives head
        .word   play            # play audio
        .word   stop            # stop playing audio
        .word   cmderr          # write long
        .word 	cmderr          # write verify
        .word   resume          # resume playing audio

ptrsav:	.long	0		# saved pointer to request header
arg:	.byte	0		# Number of CDROM drive 0 ... 3


# structure of command block header (13 bytes)
CmdLen	= 	0		# length of header
Unit   	=	1		# sub unit id
CmdCode	=	2		# describes command to be executed
Status	=	3		# status code
Reserved=	5		# bytes 5 to 12 are reserved

Strategy:
				# save adress of command block
	mov	%bx, %cs:ptrsav	# in ptrsav
	mov 	%es, %cs:ptrsav+2
	lret

Entry:
	pushw	%si
	pushw	%ax
	pushw	%cx
	pushw	%dx
	pushw	%di
	pushw	%bp
	pushw	%ds
	pushw	%es
	pushw	%bx

	pushw	%cs
	popw	%ds
	les	ptrsav,%di	# es:di now points to the command block

	movb	%es:CmdCode(%di), %al # get command code
	movb	$0,%ah
	test	$0x80,%al	# test if highes bit is set
	jz	StdCommand
	andb	$0x7F,%al	# AL = AL-128
	cmpb	$8,%al
	ja	cmderr
	movw	$MscdTbl,%si
	jmp	EntryCon

StdCommand:
	cmpb	$16,%al
	ja 	cmderr
	movw	$ConTbl,%si
EntryCon:
	addw	%ax,%si
	addw	%ax,%si
# now we have in es:di the adress of the command block and cs = ds
# now call command handler
	jmpw    *(%si)


cmderr:
	movw	$3,%ax		# unknown command
	movb    $0x80,%ah	# set error bit
exit:
# here we expect that %ax contains a valid status code
	orb	$0x01,%ah	# set done flag
	pushw	%cs
	popw	%ds
	les	ptrsav,%di      # pointer to command block to es:di

	mov 	%ax,%es:Status(%di)

	popw	%bx
	popw	%es
	popw	%ds
	popw	%bp
	popw	%di
	popw	%dx
	popw	%cx
	popw	%ax
	popw	%si
	lret


#-----------------------------------------------------------------------
# the device functions
#-----------------------------------------------------------------------


#-----------------------------------------
# device open / device close / input flush

dev_open:
	movw	$0,%ax
	jmp	exit
dev_close:
	movw	$0,%ax
	jmp	exit
input_flush:
	movw	$0,%ax
	jmp	exit


#----------------------------------------
# play / stop / resume

play:
	movb	$Helper, %al
	movb	$0x04, %ah
        orb	%cs:arg, %ah
	int	$LinuxCD
	cmp	$0, %al
	jne	play_fail

	movb	$0, %al
	movb	$0x02, %ah 	# set busy bit
	jmp	exit
play_fail:
	movb	$0x0B, %al	# set error code
	movb	$0x80, %ah 	# set error bit
	jmp	exit

stop:
	movb	$Helper, %al
	movb	$0x05, %ah
        orb	%cs:arg, %ah
	int	$LinuxCD
 	movw	$0,%ax
	jmp	exit
	
resume:
	movb	$Helper, %al
	movb	$0x06, %ah
        orb	%cs:arg, %ah
	int	$LinuxCD
	cmpb	$0, %al
	jne	resume_fail

	movb	$0, %al
	movb	$0x02, %ah
	jmp	exit
resume_fail:
	movb	$0x0C, %al	# set error code
	movb	$0x80, %ah	# set error bit
	jmp	exit

	
#----------------------------------------
# read long / seek

seek:
	movb	$Helper, %al
	movb	$0x03, %ah
        orb	%cs:arg, %ah
	int	$LinuxCD
	movw	$0,%ax
	jmp	exit

READLONG_TRANSFER = 14

read_long:
	movb	$Helper, %al
	movb	$0x02, %ah
        orb	%cs:arg, %ah
	lds	%es:READLONG_TRANSFER(%di), %si 
	int	$LinuxCD
	cmpb	$0, %al
	jne	read_long_fail

	movw	$0,%ax
	jmp	exit

read_long_fail:
        movb	$0x0F, %al	# set error code
	movb	$0x80, %ah	# set error bit
	jmp	exit


#----------------------------------------
# ioctlout

# structure of an ioctlout request
IOCTLOUT_MDB	= 13		# media descriptor byte (not used)
IOCTLOUT_TRANS	= 14		# transfer adress
IOCTLOUT_TRANSLEN= 18		# number of bytes to be transfered
IOCTLOUT_SEKTOR  = 20		# not used
IOCTLOUT_VOL     = 22		# not used (4 bytes)	

# ioctlout jump table
IoctloutTbl:	.word	eject
		.word	lockunlock
		.word	resetdrive
		.word	audio_ctrl
		.word	cmderr
		.word	closetray

ioctlout:
	lds	%es:IOCTLOUT_TRANS(%di),%si

# now we have:
#   es:di points to command control block
#   ds:si points to request header
#   cs!=ds

	movb	(%si),%al	# get request code
	movb	$0,%ah
	cmpw	$5,%ax
	ja	ioctlout_err

	pushw	%cs
	popw	%es
	movw	$IoctloutTbl,%di
	add	%ax,%di
	add	%ax,%di

	jmpw	*%es:(%di)

ioctlout_err:
	jmp	cmderr

# eject disk
eject:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x0D,%ah
	orb	%cs:arg,%ah
	int	$LinuxCD

	mov	$0,%ax
	jmp	exit

# function lock - unlock drive
lockunlock:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x0C,%ah
	orb	%cs:arg,%ah
        movb	1(%si),%bl
	int	$LinuxCD

	mov	$0,%ax
	jmp	exit

# resetdrive
resetdrive:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x0B,%ah
	orb	%cs:arg,%ah
	int	$LinuxCD

	mov	$0,%ax
	jmp	exit

# function closetray
closetray:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x0E,%ah
	orb	%cs:arg,%ah
	int	$LinuxCD

	mov	$0,%ax
	jmp	exit

# alter audio control
audio_ctrl:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x0F,%ah
	orb	%cs:arg,%ah
	int	$LinuxCD

	jmp 	check_busy


#----------------------------------------
# ioctlin

# structure of an ioctlin request
IOCTLIN_MDB	= 13		# media descriptor byte (not used)
IOCTLIN_TRANS	= 14		# transfer adress
IOCTLIN_TRANSLEN= 18		# number of bytes to be transfered
IOCTLIN_SEKTOR  = 20		# not used
IOCTLIN_VOL     = 22		# not used (4 bytes)	

# ioctlin jump table
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
	lds	%es:IOCTLIN_TRANS(%di),%si

# now we have:
#   es:di points to command control block
#   ds:si points to request header
#   cs!=ds

	movb	(%si),%al	# get request code
	movb	$0,%ah
	cmp	$15,%ax
	ja	ioctlin_err

	pushw	%cs
	popw	%es
	mov	$IoctlinTbl,%di
	add	%ax,%di
	add	%ax,%di

	jmp	*%es:(%di)

ioctlin_err:
	jmp	cmderr

# function get device header adress
HEADER_ADRESS	= 1

devheader_adress:
	movw	$ConDev,%ax
	pushw	%cs
	popw	%bx
	movw	%ax, HEADER_ADRESS(%si)
	movw	%bx, HEADER_ADRESS+2(%si)
	movw	$0, %ax
	jmp	exit

# get device status
DEVICE_PARMS = 1

devstatus:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x0A,%ah
	orb	%cs:arg, %ah
	int	$LinuxCD

	movw	%bx, DEVICE_PARMS(%si)
	movw	$0, DEVICE_PARMS+2(%si)
	jmp	check_busy

# return sectorsize
RET_SECTORSIZE_READMODE	= 1
RET_SECTORSIZE_SIZE	= 2

ret_sectorsize:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x08,%ah
	orb	%cs:arg, %ah
	int	$LinuxCD

	movw	%bx,RET_SECTORSIZE_SIZE(%si)
	movw	$0,%ax
	jmp	exit

# return volumesize
ret_volumesize:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x12,%ah
	orb	%cs:arg, %ah
	int	$LinuxCD

	movw	$0,%ax
	jmp	exit

# media changed ?
MEDIA_BYTE = 1

media_changed:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x9,%ah
	orb	%cs:arg, %ah
	int	$LinuxCD

	cmp	$0,%bl
	jne	media_changed_dnr
	movb	$1,%bh			# media not changed/disk ready
	movb	%bh,MEDIA_BYTE(%si)
	jmp	check_busy
media_changed_dnr:
	movb	$0xff,%bh                # media changed
	movb	%bh,MEDIA_BYTE(%si)
	jmp	check_busy

# location of head
lochead:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x7,%ah
	orb	%cs:arg, %ah
	int	$LinuxCD

	jmp	check_busy

# get status of audio
audiostatus:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x14,%ah
	orb	%cs:arg, %ah
	int	$LinuxCD

	jmp	check_busy

# get q-channel information
qchannel:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x13,%ah
	orb	%cs:arg, %ah
	int	$LinuxCD

	jmp	check_busy

# get information about tracks
trackinfo:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x11,%ah
	orb	%cs:arg, %ah
	int	$LinuxCD

	jmp	check_busy

# get information about tracks
diskinfo:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x10,%ah
	orb	%cs:arg,%ah
	int	$LinuxCD

	jmp	check_busy

# get information about channels
getchaninfo:
	les	%cs:ptrsav,%di
	movb	$Helper,%al
	movb	$0x15,%ah
	orb	%cs:arg, %ah
	int	$LinuxCD

	jmp	check_busy


# audio playing in progress ?
# hier noch irgendwie eine Fehlerbehandlung einbauen....


check_busy:
        cmp     $1,%al
        je      error
	cmp	$1,%ah
	je	busy
	movw	$0,%ax
	jmp	exit
busy:
	movb	$0,%al
	movb	$0x02, %ah
	jmp	exit
error:
	movb	$0x0C, %al
	movb	$0x80, %ah	# set error bit
	jmp	exit


#----------------------------------------
# init (must be at the end!)

# structure of an init request:
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
cont:	mov	$Message, %dx
	movb	$0x09, %ah
	int	$0x21

 	movb	$0x01, %ah
	orb	%cs:arg, %ah
	movb	$Helper, %al
	int	$LinuxCD
	cmp	$0, %al
	je	InitEnde
	movw	$FailMsg, %dx
	movb	$0x09, %ah
	int     $0x21
InitEnde:
	movw	$Message, %es:INIT_EndAdress(%di) 
        movw	%cs, %ax
	movw	%ax, %es:INIT_EndAdress+2(%di)
# future calls to init must return in error
	movw	$cmderr, ConTbl

	movw	$0, %ax
	jmp     exit

ParseArgs:
	pushw 	%ds 		#On entry es:di points to the command line
	pushw 	%si		#the relevant data start at offset 0x12
	pushw	%ax		#If we have _no_ parameter, the default
				#will be drive 0 and name MSCD0001
	pushw 	%es
	popw	%ds
	lds 	0x012(%di),%si	#ds:si now pointing to our paramter
	
	jmp IniParse

EndParseArgs:
	popw	%ax
	popw	%si
	popw	%ds
	jmp 	cont

IniParse:
	cmpb	$0x20,(%si)
	jb	EndParseArgs
	jz	Label
	incw	%si
	jmp	IniParse


Label:	incw 	%si

StartParse:
	cmpb 	$' ',(%si)
	jb 	EndParseArgs
	jz 	NextChar

	cmpb	$'1',(%si)		#lowest allowed value is '1'; 
	jb	NextChar		#if not get next character
	cmpb	$'4',(%si)		#highest allowed value is '4'
	ja	NextChar		#if higher get next character
					#if lower all OK
	movb	(%si),%al
	movb	%al,%cs:Index		#modify the device name ('1'...'4')
	subb 	$0x31,%al		#calculate the index 0..3
	shlb	$6,%al
	movb	%al,%cs:arg

	jmp	EndParseArgs

NextChar:
	incw 	%si
	jmp	StartParse
