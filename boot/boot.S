!  Boot record program (C) Copyright Peter Norton 1986

!.globl begtext, begdata, begbss !, endtext, enddata, endbss
.text
.data
.bss

!begtext:
!begdata:
!begbss:


!entry start
!start:

.globl _main
_main:

      .org       0

! boot  proc  far

!  30-byte DOS info -- set up for 2-side, 18-sector
!  change as needed for any other format

head:
                        ! I have modified this for hdimage...
      jmp   begin       ! EB 2A 90 as per normal
      .ascii    " DOSEMU "  ! 8-byte system id
      .word   512         ! sector size in bytes
      .byte    8           ! sectors per cluster
      .word   1           ! reserved clusters
      .byte    2           ! numbers of fats
      .word   512         ! root directory entries
      .word   2703        ! total sectors
      .byte    0xF8        ! format id
      .word   1           ! sectors per fat
      .word   17          ! sectors per track
      .word   4           ! sides
      .word   17          ! special hidden sectors
! now at address 0x1e...fill 2 to 0x20
      .word   00

      .org     32
! this is where the DOS 4.0+ header begins...
!
      .word    0,0               ! big total # sectors
      .byte    0x80              ! physical drive number (hd's start at 0x80)
      .byte    0                 ! reserved
      .byte    0x29              ! signature byte
      .long    0x12345678        ! serial number
      .ascii   "HDIMAGEDIST"     ! 11-byte volume label
      .byte    0,0,0,0,0,0,0,0   ! 8 reserved bytes

!  carry on with the boot work

      .org   62
begin:
      mov   ax, #0x7c0 ! boot record location
      push  ax
      pop   ds
      mov   bx, #message_offset ! message_offset
      mov   cx, #message_length  ! message length into cx

cont1:
      mov   ah, #14   ! write teletype
      mov   al,(bx)
      push  ds
      push  cx
      push  bx
      mov   bx, #0
      int   0x10
      pop   bx
      pop   cx
      pop   ds
      inc   bx
      loop  cont1

      mov   ah,#0     ! read next keyboard character
      int   0x16

      ! exit dosemu
      mov     ax, #0xffff
      int     0x21

beg_message:
      .byte    0xd,0xa  ! return carriage, line feed
      .byte    0xd,0xa
      .ascii   "         the Linux DOS Emulator, 3/2/93"
      .byte    0xd,0xa
      .ascii   "             Version 0.49 (IPC)"
      .byte    0xd,0xa
      .byte    0xd,0xa
      .ascii   "         Robert Sanders"
      .byte    0xd,0xa
      .ascii   "         38134 Ga Tech Station"
      .byte    0xd,0xa
      .ascii   "         Atlanta, GA   30332"
      .byte    0xd,0xa
      .ascii   "         gt8134b@prism.gatech.edu"
      .byte    0xd,0xa
      .byte    0xd,0xa
      .ascii   "    See README.first and README for instructions on how to"
      .byte    0xd,0xa
      .ascii   "    setup dosemu...requires Linux 0.99pl7+ & MS-DOS 3.3+"
      .byte    0xd,0xa
      .byte    0xd,0xa
      .byte    0xd,0xa
      .ascii   "    press any key to return to Linux ... "

end_message:

! I put a copyright notice here; you do if you want to
tail:

message_offset equ beg_message - head
message_length equ end_message - beg_message

      .org     510
      .byte    0x55, 0xaa             ! boot id

	end