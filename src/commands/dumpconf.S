# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

# Author: ???
# what is this good for ??

.code16
.text
	.globl	_start16
_start16:

#------------------------------------------------------------------------------

Header:	.long      -1              # link to the next driver
        .word   0x8000          # device attribute
        .word   Strat           # strategie routine
        .word   Intr            # interrupt routine
	.ascii	"DUMPCONF"      #

RHPtr:	.long	0               # Request Header Ptr
StartPos: .word 0
WaitKey: .word	0

#------------------------------------------------------------------------------

Strat:
        movw    %bx, %cs:RHPtr
        movw	%es, %cs:RHPtr+2
        lret

#------------------------------------------------------------------------------

Intr:
        pushw    %ds
        pushw    %es
        pushw    %si
        pushw    %di
        pushw    %dx
        pushw    %cx
        pushw    %bx
        pushw    %ax

        les     %cs:RHPtr,%di   # ES:DI = request header
        movb    %es:2(%di),%bl  # command
        xor     %bh,%bh
        cmp     $0,%bx          # valid ?
        jne     Intr3

        call    work

Intr3:
	les     %cs:RHPtr,%di   # ES:DI = request header (destroyed)
        movw    $0,%es:0xe(%di)
        movw    %cs,%es:0x10(%di)

        movw    $0x8100,%es:3(%di)	# transfer status bit

Intr4:  popw     %ax
        popw     %bx
        popw     %cx
        popw     %dx
        popw     %di
        popw     %si
        popw     %es
        popw     %ds
        lret

#------------------------------------------------------------------------------
DumpRawChar:
        pushw   %si
        pushw   %ax
        pushw   %cx
        pushw   %dx
        movb    %al,%dl
        movb    $2,%ah
        int     $0x21
        popw    %dx
        popw    %cx
        popw    %ax
        popw    %si
        ret

DumpAsciiChar:
        pushw   %ax
        cmp     $0x20,%al
        jae     NoAdjust
        mov     $'!',%al
NoAdjust:
        call    DumpRawChar
        popw    %ax
        ret

DumpHexChar:
        pushw   %si
        pushw   %ax
        pushw   %cx

        movb    %al, %ah
        movb    $4, %cl
        shrb    %cl,%al
        cmpb    $9,%al
        jbe     Add0_1
        subb    $10, %al
        addb    $'A',%al
        jmp     DumpNibble1
Add0_1:
	add     $'0',%al
DumpNibble1:
        call    DumpAsciiChar

        movb    %ah,%al
        andb    $15,%al
        cmpb    $9,%al
        jbe     Add0_2
        subb    $10,%al
        addb    $'A',%al
        jmp     DumpNibble2
Add0_2: addb    $'0',%al
DumpNibble2:
        call    DumpAsciiChar

        movb    $0x30,%al		#' '; 
        call    DumpAsciiChar

        popw    %cx
        popw    %ax
        popw    %si
        ret

DumpLine:
        pushw   %si
        pushw   %cx
        pushw   %si
        movw    $16,%cx
DumpHex:
        lodsb
        call    DumpHexChar
        loop    DumpHex
DumpChars:
        movb    $' ', %al
        call    DumpAsciiChar
        popw    %si
        movw    $16,%cx
DumpChar:
        lodsb
        call    DumpAsciiChar
        loop    DumpChar

        movb    $13,%al
        call    DumpRawChar
        movb    $10,%al
        call    DumpRawChar

        popw    %cx
        popw    %si
        ret

work:
	lds     %es:18(%di),%si        #load config.sys line
        movw    %si,%cs:StartPos
        cld
workfind:
        lodsb                           #load next char
        cmpb    $13,%al                 #EOL ?
        je      workret
        cmpb    $' ', %al	#search first blank after driver name
        jne     workfind
worknext:
        lodsb                           #load next char
        cmpb    $13,%al                 #EOL ?
        je      workdone
        cmpb    $24,%al                 #"!" (Read AnyKey ??)
        jne     worknext
        movw    $1,%cs:WaitKey
        jmp     worknext

workdone:
        movw    %cs:StartPos,%si
        cmpw    $64,%si
        jae     NoPosAdjust
        movw    $64,%si
NoPosAdjust:
        subw    $64,%si
        call    DumpLine
        addw    $16,%si
        call    DumpLine
        addw    $16,%si
        call    DumpLine
        addw    $16,%si
        call    DumpLine
        addw    $16,%si
        call    DumpLine
        addw    $16,%si
        call    DumpLine
        addw    $16,%si
        call    DumpLine
        addw    $16,%si
        call    DumpLine
        cmpw    $0,%cs:WaitKey
        je      workret
readkey:
        mov     $0xC07,%ax              #flush buffer, read unfiltered keyboard
                                        #without echo
        int     $0x21
        mov     $0xC00,%ax              #flush buffer
        int     $0x21
workret:
        ret
