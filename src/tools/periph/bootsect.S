# 
# All modifications in this file to the original code are
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

#  Boot record program (C) Copyright Peter Norton 1986
#  modified by Robert Sanders, gt8134b@prism.gatech.edu
#   for dosemu 0.49, 4/1/93
#

.code16	
.text
	.globl	_start16
_start16:

head:

# I have modified this for hdimage...
# actually, this will all be overwritten when you fdisk & format, but
# I thought it educational (and I used it in an earlier version)

      jmp   begin           # EB 2A
      nop                   # 90 as per normal
      .ascii  " DOSEMU "    # 8-byte system id
      .word   512           # sector size in bytes
      .byte   8             # sectors per cluster
      .word   1             # reserved clusters
      .byte   2             # numbers of fats
      .word   512           # root directory entries
      .word   2703          # total sectors
      .byte   0xF8          # format id
      .word   1             # sectors per fat
      .word   17            # sectors per track
      .word   4             # sides
      .word   17            # special hidden sectors
# now at address 0x1e...fill 2 to 0x20
      .word   00

# this is where the DOS 4.0+ header begins...

      .org     0x20

      .word    0,0               # big total # sectors
      .byte    0x80              # physical drive number (hd's start at 0x80)
      .byte    0                 # reserved
      .byte    0x29              # signature byte
      .long    0x12345678        # serial number
      .ascii   "HDIMAGEDIST"     # 11-byte volume label
      .byte    0,0,0,0,0,0,0,0   # 8 reserved bytes

# the actual boot code

      .org   0x3e
begin:
      movw  $0x7c0,%ax           # boot record location
      pushw %ax
      popw  %ds
      movw  $message_offset, %bx
      movw  $message_length, %cx

cont1:
      movb  $14, %ah             # BIOS write teletype
      movb  (%bx), %al
      pushw %ds
      pushw %cx
      pushw %bx
      movw  $0, %bx
      int   $0x10
      popw  %bx
      popw  %cx
      popw  %ds
      incw  %bx
      loop  cont1

      movb  $0,%ah               # BIOS read next keyboard character
      int   $0x16

      # exit dosemu
      movw    $0xffff, %ax
      int     $0xe6

beg_message:
      .byte    0xd,0xa           # carriage return, line feed
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
      .ascii   "    press any key to return to Linux...\r\n"
end_message:

message_offset    =      beg_message - head
message_length    =      end_message - beg_message

# WARNING! the message above cannot extend past 0x1bd, or it will
#          overwrite the partition table.  Also, I have seen ld86
#          go into an infinite loop when a .org overlapped already-extant
#          data.
# -Robert

# show an empty partition, which terminates partition table

partition_table:
      .align   0

      .org     0x1be
      .byte    0        # boot indicator (0x80 for active)
      .byte    0        # beginning head number
      .byte    0        # beginning sector & 2 high bits of ending cyl
      .byte    0        # 8 low bits of beginning cyl
      .byte    0        # system indicator (partition type)
      .byte    0        # ending head number
      .byte    0        # ending sector & 2 high bits of ending cyl
      .byte    0        # 8 low bits of ending cyl
      .long    0        # number of sectors preceding partition
      .long    0        # number of sectors in partition

      .org     510
      .byte    0x55, 0xaa             # boot id for DOS (mandatory)




