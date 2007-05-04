# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

#  modified by Robert Sanders, gt8134b@prism.gatech.edu
#   for dosemu 0.49, 4/1/93
#

.code16	
.text
	.globl	boot_sect
boot_sect:

head:

# I have modified this for hdimage...
# actually, this will all be overwritten when you fdisk & format, but
# I thought it educational (and I used it in an earlier version)

      jmp   begin           # EB 2A
      nop                   # 90 as per normal
      .ascii  " DOSEMU "    # 8-byte system id

# the actual boot code

      .org   0x3e
begin:
      movw  $0x7c0,%ax           # boot record location
      pushw %ax
      popw  %ds
      movw  $message_offset, %si
      movw  $message_length, %cx

      movb  $0x0e, %ah           # BIOS write teletype
      xorw  %bx, %bx

cont1:
      lodsb
      int   $0x10
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

	.globl	boot_sect_end
boot_sect_end:

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
