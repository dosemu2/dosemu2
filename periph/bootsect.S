!  Boot record program (C) Copyright Peter Norton 1986
!  modified by Robert Sanders, gt8134b@prism.gatech.edu
!   for dosemu 0.49, 4/1/93
!
! $Date: 1994/06/12 23:17:32 $
! $Source: /home/src/dosemu0.60/periph/RCS/bootsect.S,v $
! $Revision: 2.1 $
! $State: Exp $
!

.text
.data
.bss

.globl _main              ! for ld86
_main:

      .org       0

head:

! I have modified this for hdimage...
! actually, this will all be overwritten when you fdisk & format, but
! I thought it educational (and I used it in an earlier version)

      jmp   begin           ! EB 2A 90 as per normal
      .ascii  " DOSEMU "    ! 8-byte system id
      .word   512           ! sector size in bytes
      .byte   8             ! sectors per cluster
      .word   1             ! reserved clusters
      .byte   2             ! numbers of fats
      .word   512           ! root directory entries
      .word   2703          ! total sectors
      .byte   0xF8          ! format id
      .word   1             ! sectors per fat
      .word   17            ! sectors per track
      .word   4             ! sides
      .word   17            ! special hidden sectors
! now at address 0x1e...fill 2 to 0x20
      .word   00

! this is where the DOS 4.0+ header begins...

      .org     0x20

      .word    0,0               ! big total # sectors
      .byte    0x80              ! physical drive number (hd's start at 0x80)
      .byte    0                 ! reserved
      .byte    0x29              ! signature byte
      .long    0x12345678        ! serial number
      .ascii   "HDIMAGEDIST"     ! 11-byte volume label
      .byte    0,0,0,0,0,0,0,0   ! 8 reserved bytes

! the actual boot code

      .org   0x3e
begin:
      mov   ax, #0x7c0           ! boot record location
      push  ax
      pop   ds
      mov   bx, #message_offset
      mov   cx, #message_length

cont1:
      mov   ah, #14              ! BIOS write teletype
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

      mov   ah,#0                ! BIOS read next keyboard character
      int   0x16

      ! exit dosemu
      mov     ax, #0xffff
      int     0x21

beg_message:
      .byte    0xd,0xa           ! carriage return, line feed
      .byte    0xd,0xa
      .ascii   "         the Linux DOS Emulator, 4/1/93"
      .byte    0xd,0xa
      .ascii   "             Version 0.49"
      .byte    0xd,0xa
      .byte    0xd,0xa
      .ascii   "         Robert Sanders"
      .byte    0xd,0xa
      .ascii   "         gt8134b@prism.gatech.edu"
      .byte    0xd,0xa
      .byte    0xd,0xa
      .ascii   "    See README.first for instructions"
      .byte    0xd,0xa
      .byte    0xd,0xa
      .ascii   "    press any key to return to Linux ... "
end_message:

message_offset    equ      beg_message - head
message_length    equ      end_message - beg_message

! WARNING! the message above cannot extend past 0x1bd, or it will
!          overwrite the partition table.  Also, I have seen ld86
!          go into an infinite loop when a .org overlapped already-extant
!          data.
! -Robert

! show an empty partition, which terminates partition table

partition_table:
      .align   0

      .org     0x1be
      .byte    0        ! boot indicator (0x80 for active)
      .byte    0        ! beginning head number
      .byte    0        ! beginning sector & 2 high bits of ending cyl
      .byte    0        ! 8 low bits of beginning cyl
      .byte    0        ! system indicator (partition type)
      .byte    0        ! ending head number
      .byte    0        ! ending sector & 2 high bits of ending cyl
      .byte    0        ! 8 low bits of ending cyl
      .long    0        ! number of sectors preceding partition
      .long    0        ! number of sectors in partition

      .org     510
      .byte    0x55, 0xaa             ! boot id for DOS (mandatory)




