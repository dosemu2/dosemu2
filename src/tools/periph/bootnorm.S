#
# normal DOS boot sector
#

.intel_syntax noprefix
.text
.code16

  .globl  bootnormal_code
bootnormal_code:

#-----------------------------------------------------------------------
#   ENTRY (copied from freedos bootsector)
#
# IN: DL = boot drive
#OUT: DL = boot drive
#
#-----------------------------------------------------------------------

real_start:     cli
                cld
                xor     ax, ax
                mov     ss, ax          # initialize stack
                mov     ds, ax
                mov     bp, 0x7c00
                lea     sp, [bp-0x20]
                sti

                mov     ax, 0x1FE0
                mov     es, ax
                mov     si, bp
                mov     di, bp
                mov     cx, 0x0100
                rep     movsw

                jmp     0x1FE0:0x7c00+ cont-real_start

cont:           mov     ds, ax
                mov     ss, ax
                xor     ax,ax
                mov     es,ax

#               call    print
#               db      "FreeDOS MBR...",0


                                                                                 # search for active partition
                lea di, [bp+0x1be] # start of partition table
test_next_for_active:
                test byte ptr [di],0x80
                jne  active_partition_found
                add  di,0x10                    # next table
                cmp  di, 0x7c00+0x1fe# scanned beyond end of table ??
                jb  test_next_for_active

#*****************************************************************
                call print
                .asciz "no active partition found"

WAIT_FOR_REBOOT:
                jmp $


#*****************************************************************
trouble_reading_drive:
                call print
                .asciz "read error while reading drive"
                jmp WAIT_FOR_REBOOT

#*****************************************************************

invalid_partition_code:
                call print
                .asciz "partition signature != 55AA"

                jmp WAIT_FOR_REBOOT


#*****************************************************************

active_partition_found:
#                               call print
#                               db 'loading active partition',0

                call read_boot_sector

                jc  trouble_reading_drive

                cmp word ptr [es:0x7c00+0x1fe],0xaa55
                jne invalid_partition_code

#               call print
#               db '.jump DOS..',0

                jmp 0x0:0x7c00             # and jump to boot sector code


#*****************************
# read_boot_sector
#
# IN: DI--> partition info
#OUT:CARRY
#*****************************

read_boot_sector:
                #  /* check for LBA support */
                mov bx,0x55aa
                mov ah,0x41
                int 0x13

                jc  StandardBios    #  if (regs.b.x != 0xaa55 || (regs.flags & 0x01))
                cmp bx,0xaa55       #    goto StandardBios#
                jne StandardBios

                              #  /* if DAP cannot be used, don't use LBA */
                              #  /* if ((regs.c.x & 1) == 0) */
                              #  /*  goto StandardBios# */
                test cl,1
                jz StandardBios

        jmp short LBABios



#struct _bios_LBA_address_packet            /* Used to access a hard disk via LBA */
#{
#  unsigned char packet_size#    /* size of this packet...set to 16  */
#  unsigned char reserved_1#     /* set to 0...unused                */
#  unsigned char number_of_blocks#       /* 0 < number_of_blocks < 128       */
#  unsigned char reserved_2#     /* set to 0...unused                */
#  UBYTE far *buffer_address#    /* addr of transfer buffer          */
#  unsigned long block_address#  /* LBA address                      */
#  unsigned long block_address_high#     /* high bytes of LBA addr...unused  */
#}#

_bios_LBA_address_packet:
        .byte 0x10
        .byte 0
        .byte 4                            # read four sectors - why not
        .byte 0
        .word 0x7c00                       # fixed boot address for DOS sector
        .word 0x0000
_bios_LBA_low:
        .word 0
_bios_LBA_high:
        .word 0
        .word 0,0


LBABios:
                                                # copy start address of partition to DAP
        mov ax,[di+8]
        mov [0x7c00+ (_bios_LBA_low-real_start)],ax
        mov ax,[di+8+2]
        mov [0x7c00+ (_bios_LBA_high-real_start)],ax

    mov ax,0x4200               #  regs.a.x = LBA_READ#
    mov si,0x7c00+ (_bios_LBA_address_packet-real_start)# regs.si = FP_OFF(&dap)#

        int 0x13
        ret

#*****************************************************************
# read disk, using standard BIOS
#
StandardBios:
#       call print
#       .byte 'standard BIOS',0


    mov ax,0x0204                       #  regs.a.x = 0x0201#
    mov bx,0x7c00                       #  regs.b.x = FP_OFF(buffer)#
        mov cx,[di+2]                   #      regs.c.x =
                                                # ((chs.Cylinder & 0xff) << 8) + ((chs.Cylinder & 0x300) >> 2) +
                                                # chs.Sector#
                                                # that was easy #-)
    mov dh,[di+1]                       #  regs.d.b.h = chs.Head#
                                                #  regs.es = FP_SEG(buffer)#
        int 0x13
        ret



#****** PRINT
# prints text after call to this function.

print_1char:
                xor   bx, bx                   # video page 0
                mov   ah, 0x0E                 # else print it
                int   0x10                     # via TTY mode
print:          pop   si                       # this is the first character
print1:         lodsb                          # get token
                push  si                       # stack up potential return address
                cmp   al, 0                    # end of string?
                jne   print_1char              # until done
                ret                            # and jump to it

  .globl  bootnormal_code_end
bootnormal_code_end:
