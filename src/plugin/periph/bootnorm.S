#
# normal DOS boot sector
#

.text
.code16

.global  _start
_start:

#-----------------------------------------------------------------------
#   ENTRY (copied from freedos bootsector)
#
# IN: DL = boot drive
#OUT: DL = boot drive
#
#-----------------------------------------------------------------------

real_start:     cli
                cld
                xorw    %ax,%ax
                movw    %ax,%ds
                movw    $0x7c00,%bp
                movw    %ax,%ss         # initialize stack
                leaw    -0x20(%bp),%sp
                sti

                movw    $0x1FE0,%ax
                movw    %ax,%es
                movw    %bp,%si
                movw    %bp,%di
                movw    $0x0100,%cx
                rep
                movsw

                jmp     $0x1FE0,$0x7c00+ cont-real_start

cont:           movw    %ax,%ds
                movw    %ax,%ss
                xorw    %ax,%ax
                movw    %ax,%es

#               call    print
#               db      "FreeDOS MBR...",0


                                                                                 # search for active partition
                leaw 0x1be(%bp),%di # start of partition table
test_next_for_active:
                testb $0x80,(%di)
                jne  active_partition_found
                addw $0x10,%di                  # next table
                cmpw $0x7c00+0x1fe,%di # scanned beyond end of table ??
                jb  test_next_for_active

#*****************************************************************
                call print
                .asciz "no active partition found\r\n"

WAIT_FOR_REBOOT:
                hlt
                jmp WAIT_FOR_REBOOT


#*****************************************************************
trouble_reading_drive:
                call print
                .asciz "read error while reading drive\r\n"
                jmp WAIT_FOR_REBOOT

#*****************************************************************

invalid_partition_code:
                call print
                .asciz "partition signature != 55AA\r\n"

                jmp WAIT_FOR_REBOOT


#*****************************************************************

active_partition_found:
#                               call print
#                               db 'loading active partition',0

                call read_boot_sector

                jc  trouble_reading_drive

                cmpw $0xaa55,%es:0x7c00+0x1fe
                jne invalid_partition_code

#               call print
#               db '.jump DOS..',0

                movw %di,%si               # ds:si -> booted partition entry
                jmp $0x0,$0x7c00             # and jump to boot sector code


#*****************************
# read_boot_sector
#
# IN: DI--> partition info
#OUT:CARRY
#*****************************

read_boot_sector:
                #  /* check for LBA support */
                movw $0x55aa,%bx
                movb $0x41,%ah
                int $0x13

                jc  StandardBios    #  if (regs.b.x != 0xaa55 || (regs.flags & 0x01))
                cmpw $0xaa55,%bx    #    goto StandardBios;
                jne StandardBios

                              #  /* if DAP cannot be used, don't use LBA */
                              #  /* if ((regs.c.x & 1) == 0) */
                              #  /*  goto StandardBios; */
                testb $1,%cl
                jz StandardBios

        jmp LBABios



#struct _bios_LBA_address_packet            /* Used to access a hard disk via LBA */
#{
#  unsigned char packet_size;    /* size of this packet...set to 16  */
#  unsigned char reserved_1;     /* set to 0...unused                */
#  unsigned char number_of_blocks;       /* 0 < number_of_blocks < 128       */
#  unsigned char reserved_2;     /* set to 0...unused                */
#  UBYTE far *buffer_address;    /* addr of transfer buffer          */
#  unsigned long block_address;  /* LBA address                      */
#  unsigned long block_address_high;     /* high bytes of LBA addr...unused  */
#};

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
        movw 8(%di),%ax
        movw %ax,0x7c00+ _bios_LBA_low-real_start
        movw 8+2(%di),%ax
        movw %ax,0x7c00+ _bios_LBA_high-real_start

        movw $0x4200,%ax            #  regs.a.x = LBA_READ;
        mov $0x7c00+ _bios_LBA_address_packet-real_start,%si

        int $0x13
        ret

#*****************************************************************
# read disk, using standard BIOS
#
StandardBios:
#       call print
#       .byte 'standard BIOS',0


    movw $0x0204,%ax                    #  regs.a.x = 0x0201;
    movw $0x7c00,%bx                    #  regs.b.x = FP_OFF(buffer);
        movw 2(%di),%cx                 #      regs.c.x =
                                                # ((chs.Cylinder & 0xff) << 8) + ((chs.Cylinder & 0x300) >> 2) +
                                                # chs.Sector;
                                                # that was easy ;-)
    movb 1(%di),%dh                     #  regs.d.b.h = chs.Head;
                                                #  regs.es = FP_SEG(buffer);
        int $0x13
        ret



#****** PRINT
# prints text after call to this function.

print_1char:
                xorw  %bx,%bx                  # video page 0
                movb  $0x0E,%ah                # else print it
                int   $0x10                    # via TTY mode
print:          popw  %si                      # this is the first character
print1:         lodsb                          # get token
                pushw %si                      # stack up potential return address
                cmpb  $0,%al                   # end of string?
                jne   print_1char              # until done
                ret                            # and jump to it

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
