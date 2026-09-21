def memory_vcpi_pm_timer(self):
    # A VCPI client owns the CPU for real: it runs at ring 0 on its own
    # page tables, GDT and IDT, and the only interrupt vectors it can be
    # given are the ones the 8259As are programmed with -- which is why
    # the VCPI interface has AX=DE0Ah for the client to ask what those
    # are.  dosemu2 also has a virtual timer of its own, VTMR, whose
    # vector lives in the DOS interrupt vector table and means nothing in
    # protected mode.  Handing it to a client that is not in v86 is a bug:
    # a real client has no gate for it at all and dies on the spot.
    #
    # The client below installs a gate for every vector, so that an
    # unexpected one is reported rather than fatal, and then spins with
    # interrupts on until the timer reaches it.  It must see interrupts
    # only on the vectors AX=DE0Ah named.
    #
    # Note on the calling convention: dosemu2 takes the address of the
    # AX=DE0Ch parameter block from ESI as a linear address rather than
    # from DS:SI, so that is what the client passes here.

    if not self.have_kvm:
        self.skipTest("VCPI protected mode needs KVM")

    self.mkcom_with_nasm('vcpipm', r"""
bits 16
cpu 386

org 100h

TSS_SEL     equ 20h
PMCS_SEL    equ 38h
PMDS_SEL    equ 40h

; an upper bound on the spin: the loop leaves as soon as a timer tick
; arrives, so this only matters when none ever does
SPIN        equ 20000000

section .text

    push    cs
    pop     ds
    push    cs
    pop     es
    cld

    mov     ax, 0DE00h              ; VCPI present?
    int     67h
    or      ah, ah
    jz      .havevcpi
    mov     si, mnovcpi
    call    puts
    jmp     done
.havevcpi:

    mov     ax, 0DE0Ah              ; what the 8259As are programmed with
    int     67h
    or      ah, ah
    jz      .havepic
    mov     si, mnopic
    call    puts
    jmp     done
.havepic:
    mov     [picmaster], bl
    mov     [picslave], cl

    ; Carve a page-aligned page directory and page table out of the
    ; memory above the program.  A .COM owns its whole segment.
    xor     eax, eax
    mov     ax, ds
    shl     eax, 4
    mov     [segbase], eax
    add     eax, arena
    add     eax, 0FFFh
    and     eax, 0FFFFF000h
    mov     [pdlin], eax
    add     eax, 1000h
    mov     [ptlin], eax
    add     eax, 1000h
    mov     [idtlin], eax           ; 2K of IDT
    add     eax, 1000h
    mov     [tsslin], eax           ; TSS gets a page of its own
    add     eax, 2000h
    mov     [pmsplin], eax          ; and the stack the page above it

    ; zero the lot
    mov     eax, [pdlin]
    sub     eax, [segbase]
    mov     di, ax
    mov     cx, 6*1000h/4
    xor     eax, eax
    rep     stosd

    ; AX=DE01h: ES:DI is a page-aligned page table to be filled in with
    ; the pages the client has to map, DS:SI three GDT descriptors.
    mov     eax, [ptlin]
    sub     eax, [segbase]
    mov     di, ax
    mov     si, gdt+8
    mov     ax, 0DE01h
    int     67h
    or      ah, ah
    jz      .havepmi
    mov     si, mnopmi
    call    puts
    jmp     done
.havepmi:
    mov     [pmentry], ebx          ; offset in the first descriptor's segment

    ; page directory: one entry is enough, everything we touch and
    ; everything AX=DE01h handed us is inside the first 4M
    mov     eax, [pdlin]
    sub     eax, [segbase]
    mov     di, ax
    mov     eax, [ptlin]
    or      eax, 7
    mov     [di], eax

    ; our own descriptors, based on this segment so that the protected
    ; mode half can keep using the offsets it was assembled with
    mov     eax, [tsslin]
    mov     bx, TSS_SEL
    mov     cx, 103
    mov     dl, 89h                 ; available 32-bit TSS
    mov     dh, 0
    call    setdesc

    mov     eax, [segbase]
    mov     bx, PMCS_SEL
    mov     cx, 0FFFFh
    mov     dl, 9Bh                 ; code, read, present
    mov     dh, 40h                 ; 32-bit
    call    setdesc

    mov     eax, [segbase]
    mov     bx, PMDS_SEL
    mov     cx, 0FFFFh
    mov     dl, 93h                 ; data, write, present
    mov     dh, 40h
    call    setdesc

    ; GDTR and IDTR images
    mov     eax, [segbase]
    add     eax, gdt
    mov     [gdtr+2], eax
    mov     word [gdtr], gdtend-gdt-1
    mov     eax, [idtlin]
    mov     [idtr+2], eax
    mov     word [idtr], 256*8-1

    ; a gate for every vector, each to its own ten-byte stub
    mov     eax, [idtlin]
    sub     eax, [segbase]
    mov     di, ax
    mov     eax, istubs             ; offset of stub 0 inside our code
    mov     cx, 256
.gate:
    mov     [di], ax                ; offset 15..0
    mov     word [di+2], PMCS_SEL
    mov     word [di+4], 8E00h      ; present, DPL 0, 32-bit interrupt gate
    push    eax
    shr     eax, 16
    mov     [di+6], ax              ; offset 31..16
    pop     eax
    add     eax, 10                 ; stub stride
    add     di, 8
    loop    .gate

    ; the parameter block AX=DE0Ch reads
    mov     eax, [pdlin]
    mov     [pb_cr3], eax
    mov     eax, [segbase]
    add     eax, gdtr
    mov     [pb_gdtr], eax
    mov     eax, [segbase]
    add     eax, idtr
    mov     [pb_idtr], eax
    mov     word [pb_ldt], 0
    mov     word [pb_tr], TSS_SEL
    mov     eax, pm_start           ; offset inside PMCS_SEL, which is based
    mov     [pb_eip], eax
    mov     word [pb_cs], PMCS_SEL

    ; where the protected-mode half has to come back to
    mov     [v86_ss], ss
    mov     [v86_sp], sp
    mov     [v86_cs], cs
    mov     [v86_ds], ds
    mov     [v86_es], es

    mov     esi, [segbase]
    add     esi, pb_cr3             ; linear, see the note in the test
    mov     ax, 0DE0Ch
    int     67h

    ; AX=DE0Ch is one way: control comes back at back_in_v86, not here.
    mov     si, mnoswitch
    call    puts
    jmp     done

back_in_v86:
    push    cs
    pop     ds
    push    cs
    pop     es
    sti

    mov     si, mpic
    call    puts
    movzx   eax, byte [picmaster]
    call    puthex32
    mov     al, '/'
    call    putc
    movzx   eax, byte [picslave]
    call    puthex32
    call    crlf

    mov     si, mseen
    call    puts
    mov     cx, 8
    mov     bx, seen
.pseen:
    mov     eax, [bx]
    call    puthex32
    mov     al, ' '
    call    putc
    add     bx, 4
    loop    .pseen
    call    crlf

    mov     si, mtimer
    call    puts
    mov     eax, [n_timer]
    call    puthex32
    call    crlf

    mov     si, mother
    call    puts
    mov     eax, [n_other]
    call    puthex32
    call    crlf

    mov     si, mbad
    call    puts
    mov     eax, [n_bad]
    call    puthex32
    call    crlf

    mov     si, mbadvec
    call    puts
    mov     eax, [firstbad]
    call    puthex32
    call    crlf

done:
    mov     ax, 4C00h
    int     21h

; --- build a GDT descriptor: EAX base, BX selector, CX limit, DL access,
;     DH granularity/flags bits
setdesc:
    push    di
    mov     di, bx
    add     di, gdt
    mov     [di], cx
    mov     [di+2], ax
    shr     eax, 16
    mov     [di+4], al
    mov     [di+5], dl
    mov     [di+6], dh
    mov     [di+7], ah
    pop     di
    ret

putc:
    push    ax
    push    bx
    mov     ah, 0Eh
    xor     bx, bx
    int     10h
    pop     bx
    pop     ax
    ret

puts:
    push    ax
.l:
    lodsb
    or      al, al
    jz      .e
    call    putc
    jmp     .l
.e:
    pop     ax
    ret

crlf:
    push    ax
    mov     al, 13
    call    putc
    mov     al, 10
    call    putc
    pop     ax
    ret

puthex32:
    push    eax
    push    cx
    mov     cx, 8
.n:
    rol     eax, 4
    push    eax
    and     al, 0Fh
    cmp     al, 10
    jb      .d
    add     al, 'a'-10
    jmp     .p
.d:
    add     al, '0'
.p:
    call    putc
    pop     eax
    loop    .n
    pop     cx
    pop     eax
    ret

; ---------------------------------------------------------------- 32 bit
bits 32

pm_start:
    mov     ax, PMDS_SEL
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     esp, [pmsplin]
    sub     esp, [segbase]          ; the data segment is based, not flat

    sti
    mov     ecx, SPIN
.spin:
    cmp     dword [n_int], 6
    jae     .enough
    cmp     dword [n_bad], 0
    jne     .enough
    dec     ecx
    jnz     .spin
.enough:
pm_exit:
    cli

    ; back to v86 the way the VCPI interface wants it: the iret frame is
    ; on our stack and AX=DE0Ch goes to the protected-mode entry point
    movzx   eax, word [v86_ds]
    push    dword 0                 ; GS
    push    dword 0                 ; FS
    push    eax                     ; DS
    movzx   eax, word [v86_es]
    push    eax                     ; ES
    movzx   eax, word [v86_ss]
    push    eax                     ; SS
    movzx   eax, word [v86_sp]
    push    eax                     ; ESP
    push    dword 0                 ; EFLAGS, the monitor writes its own
    movzx   eax, word [v86_cs]
    push    eax                     ; CS
    push    dword back_in_v86       ; EIP
    mov     ax, 0DE0Ch
    call    far [pmptr]

    ; not reached
    jmp     $

; every vector lands here with its number on the stack
ivec_common:
    pop     eax
    push    ebx
    mov     ebx, [n_int]
    cmp     ebx, 8
    jae     .noroom
    mov     [seen + ebx*4], eax
.noroom:
    pop     ebx
    inc     dword [n_int]
    cmp     dword [n_int], 100000   ; a fault we cannot clear would spin
    jae     .runaway
    movzx   ebx, byte [picmaster]
    cmp     eax, ebx
    je      .timer
    mov     ecx, eax
    sub     ecx, ebx
    cmp     ecx, 8                  ; unsigned: below the base wraps high
    jb      .master
    movzx   ebx, byte [picslave]
    mov     ecx, eax
    sub     ecx, ebx
    cmp     ecx, 8
    jb      .slave
    ; a vector nobody has any business sending a VCPI client
    inc     dword [n_bad]
    cmp     dword [firstbad], -1
    jne     .out
    mov     [firstbad], eax
    jmp     .out
.runaway:
    mov     esp, [pmsplin]
    sub     esp, [segbase]
    jmp     pm_exit
.timer:
    inc     dword [n_timer]
    mov     al, 20h
    out     20h, al
    jmp     .out
.master:
    inc     dword [n_other]
    mov     al, 20h
    out     20h, al
    jmp     .out
.slave:
    inc     dword [n_other]
    mov     al, 20h
    out     0A0h, al
    out     20h, al
.out:
    iret

align 4
istubs:
%assign vec 0
%rep 256
    push    strict dword vec        ; five bytes, never the short form
    jmp     near ivec_common        ; five bytes
%assign vec vec+1
%endrep

section .data

pmptr:
pmentry     dd 0
            dw 8                    ; the first descriptor AX=DE01h gave us

gdt:
            dq 0                    ; null
            dq 0                    ; 08: VCPI code,  filled by AX=DE01h
            dq 0                    ; 10: VCPI data,  filled by AX=DE01h
            dq 0                    ; 18: VCPI spare, filled by AX=DE01h
            dq 0                    ; 20: our TSS
            dq 0                    ; 28
            dq 0                    ; 30
            dq 0                    ; 38: our code
            dq 0                    ; 40: our data
gdtend:

gdtr        dw 0
            dd 0
idtr        dw 0
            dd 0

; the AX=DE0Ch parameter block, in the order the interface reads it
pb_cr3      dd 0
pb_gdtr     dd 0
pb_idtr     dd 0
pb_ldt      dw 0
pb_tr       dw 0
pb_eip      dd 0
pb_cs       dw 0
            dw 0

segbase     dd 0
pdlin       dd 0
ptlin       dd 0
idtlin      dd 0
tsslin      dd 0
pmsplin     dd 0

picmaster   db 0
picslave    db 0

v86_ss      dw 0
v86_sp      dw 0
v86_cs      dw 0
v86_ds      dw 0
v86_es      dw 0

n_int       dd 0
seen        times 8 dd 0FFFFFFFFh
n_timer     dd 0
n_other     dd 0
n_bad       dd 0
firstbad    dd -1

mnovcpi     db 'NOVCPI',13,10,0
mnopic      db 'NODE0A',13,10,0
mnopmi      db 'NOPM',13,10,0
mnoswitch   db 'NOSWITCH',13,10,0
mpic        db 'PIC=',0
mseen       db 'SEEN=',0
mtimer      db 'TIMER=',0
mother      db 'OTHER=',0
mbad        db 'BAD=',0
mbadvec     db 'BADVEC=',0

align 16
arena:
""")

    self.mkfile("testit.bat", """\
c:\\vcpipm
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "kvm"
$_cpu_vm_dpmi = "kvm"
$_ems = (8192)
$_vcpi = (on)
$_ext_mem = (6144)
""")

    if "NOPM" in results:
        self.skipTest("VCPI protected mode not available in this build")

    self.assertNotIn("NOVCPI", results)
    self.assertNotIn("NODE0A", results)
    self.assertNotIn("NOSWITCH", results)

    # The client has to come back at all: a vector it cannot take would
    # otherwise have killed it outright on a real client with no gates.
    self.assertIn("BAD=", results)

    # and it must not have been handed anything but the 8259A vectors
    self.assertIn("BAD=00000000", results)
    self.assertIn("BADVEC=ffffffff", results)

    # the timer has to reach it, otherwise the run proves nothing
    self.assertNotIn("TIMER=00000000", results)
