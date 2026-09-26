import re


def memory_ems_vcpi(self):
    # Check the partial VCPI interface used by the JEMM memory manager.
    # AX=DE00h must announce VCPI, and AX=DE06h must report the physical
    # address of the handle mapped into the page frame: a client points the
    # DMA controller at that address, so it must keep reaching the same
    # logical page after the window is remapped, and a different logical
    # page must get a different address.

    self.mkcom_with_nasm('vcpitest', r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds
    push    cs
    pop     es

    mov     ax, 4100h               ; get page frame segment
    int     67h
    or      ah, ah
    jnz     noems
    mov     [frame], bx

    mov     ax, 4300h               ; allocate handle, 2 logical pages
    mov     bx, 2
    int     67h
    or      ah, ah
    jnz     noems
    mov     [handle], dx

    xor     bx, bx                  ; logical page 0 -> window 0
    call    map_page
    mov     al, 'A'
    call    fill_frame

    mov     bx, 1                   ; logical page 1 -> window 0
    call    map_page
    mov     al, 'B'
    call    fill_frame

    mov     ax, 0DE00h              ; VCPI present?
    int     67h
    or      ah, ah
    jnz     novcpi
    mov     si, mvcpi
    call    puts
    mov     al, bh
    call    puthex8
    mov     al, '.'
    call    putc
    mov     al, bl
    call    puthex8
    call    crlf

    mov     ax, 0DE0Ah              ; 8259A interrupt vector mappings
    int     67h
    or      ah, ah
    jnz     node0a
    mov     si, mpic
    call    puts
    mov     ax, bx
    call    puthex16
    mov     al, '/'
    call    putc
    mov     ax, cx
    call    puthex16
    call    crlf

    xor     bx, bx                  ; logical page 0 mapped, ask for it
    call    map_page
    call    do_de06
    mov     ax, [physlo]
    mov     [p0lo], ax
    mov     ax, [physhi]
    mov     [p0hi], ax
    mov     si, mphys0
    call    puts
    call    show_phys

    mov     bx, 1                   ; remap, then ask again
    call    map_page
    call    do_de06
    mov     ax, [physlo]
    mov     [p1lo], ax
    mov     ax, [physhi]
    mov     [p1hi], ax
    mov     si, mphys1
    call    puts
    call    show_phys

    mov     ax, [p0lo]              ; page 0 is no longer in the window,
    mov     [physlo], ax            ; but its address must still reach it
    mov     ax, [p0hi]
    mov     [physhi], ax
    call    do_move
    mov     si, mpage0
    call    puts
    call    show_buf

    mov     ax, [p1lo]
    mov     [physlo], ax
    mov     ax, [p1hi]
    mov     [physhi], ax
    call    do_move
    mov     si, mpage1
    call    puts
    call    show_buf

release:
    mov     ax, 4500h               ; release handle
    mov     dx, [handle]
    int     67h
    jmp     done

noems:
    mov     si, mnoems
    call    puts
    jmp     done
novcpi:
    mov     si, mnovcpi
    call    puts
    jmp     release
node06:
    mov     si, mnode06
    call    puts
    jmp     release
node0a:
    mov     si, mnode0a
    call    puts
    jmp     release
done:
    mov     ax, 4C00h
    int     21h

do_de06:                            ; physical address of the frame page
    mov     cx, [frame]
    shr     cx, 8
    mov     ax, 0DE06h
    int     67h
    or      ah, ah
    jnz     node06
    mov     [physlo], dx
    shr     edx, 16
    mov     [physhi], dx
    ret

show_phys:
    mov     ax, [physhi]
    call    puthex16
    mov     ax, [physlo]
    call    puthex16
    call    crlf
    ret

map_page:                           ; map logical page BX into window 0
    push    ax
    push    dx
    mov     ax, 4400h
    mov     dx, [handle]
    int     67h
    or      ah, ah
    jz      .ok
    mov     si, mnomap
    call    puts
.ok:
    pop     dx
    pop     ax
    ret

fill_frame:                         ; fill 8 bytes of the frame with AL
    push    es
    push    di
    push    cx
    mov     es, [frame]
    xor     di, di
    mov     cx, 8
    rep     stosb
    pop     cx
    pop     di
    pop     es
    ret

do_move:                            ; int 15h/87h from [physhi:physlo] to buf
    push    ax
    push    bx
    push    cx
    push    dx
    push    di
    push    si

    mov     di, gdt                 ; clear the descriptor table
    mov     cx, 48
    xor     al, al
    rep     stosb

    mov     word [gdt+10h], 0FFFFh  ; source descriptor
    mov     ax, [physlo]
    mov     [gdt+12h], ax
    mov     ax, [physhi]
    mov     [gdt+14h], al
    mov     byte [gdt+15h], 93h
    mov     [gdt+17h], ah

    mov     word [gdt+18h], 0FFFFh  ; destination, our own buffer
    mov     ax, ds
    xor     dx, dx
    mov     bx, ax
    shr     ax, 12
    mov     dl, al
    mov     ax, bx
    shl     ax, 4
    add     ax, buf
    adc     dl, 0
    mov     [gdt+1Ah], ax
    mov     [gdt+1Ch], dl
    mov     byte [gdt+1Dh], 93h
    mov     byte [gdt+1Fh], 0

    mov     si, gdt
    mov     cx, 4                   ; 4 words
    mov     ax, 8700h
    int     15h
    jnc     .ok
    mov     si, mnomove
    call    puts
.ok:
    pop     si
    pop     di
    pop     dx
    pop     cx
    pop     bx
    pop     ax
    ret

show_buf:
    push    si
    push    cx
    mov     si, buf
    mov     cx, 8
.l:
    lodsb
    call    putc
    loop    .l
    call    crlf
    pop     cx
    pop     si
    ret

putc:
    push    ax
    push    dx
    mov     dl, al
    mov     ah, 2
    int     21h
    pop     dx
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

puthex8:
    push    ax
    push    cx
    mov     cl, al
    shr     al, 4
    call    .nyb
    mov     al, cl
    and     al, 0Fh
    call    .nyb
    pop     cx
    pop     ax
    ret
.nyb:
    cmp     al, 10
    jb      .d
    add     al, 'a'-10
    jmp     putc
.d:
    add     al, '0'
    jmp     putc

puthex16:
    push    ax
    push    bx
    mov     bx, ax
    mov     al, bh
    call    puthex8
    mov     al, bl
    call    puthex8
    pop     bx
    pop     ax
    ret

section .data

handle      dw 0
frame       dw 0
physlo      dw 0
physhi      dw 0
p0lo        dw 0
p0hi        dw 0
p1lo        dw 0
p1hi        dw 0
buf         times 8 db '?'
gdt         times 48 db 0

mvcpi       db 'VCPI=',0
mpic        db 'PIC=',0
mphys0      db 'PHYS0=',0
mphys1      db 'PHYS1=',0
mpage0      db 'PAGE0=',0
mpage1      db 'PAGE1=',0
mnoems      db 'NOEMS',13,10,0
mnovcpi     db 'NOVCPI',13,10,0
mnode06     db 'NODE06',13,10,0
mnode0a     db 'NODE0A',13,10,0
mnomap      db 'NOMAP',13,10,0
mnomove     db 'NOMOVE',13,10,0
""")

    self.mkfile("testit.bat", """\
c:\\vcpitest
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_ems = (8192)
$_vcpi = (on)
# the VCPI page pool is as large as $_ems and has to fit under 16M
# together with $_ext_mem and the megabyte XMS maps through
$_ext_mem = (6144)
""")

    self.assertNotIn("NOEMS", results)
    self.assertNotIn("NOMAP", results)
    self.assertNotIn("NOMOVE", results)
    self.assertNotIn("NOVCPI", results, "VCPI not announced with $_vcpi = (on)")
    self.assertNotIn("NODE06", results, "int 67h AX=DE06h failed")

    self.assertNotIn("NODE0A", results, "int 67h AX=DE0Ah failed")

    self.assertIn("VCPI=01.00", results)

    # The BIOS maps the PICs at 0x08 and 0x70, and so does the JEMM these
    # clients are written for.
    self.assertIn("PIC=0008/0070", results)

    # Each logical page gets its own physical address, above the first
    # megabyte, not the address of the window they share.
    phys = re.findall(r"PHYS[01]=([0-9a-f]{8})", results)
    self.assertEqual(len(phys), 2, results)
    self.assertNotEqual(phys[0], phys[1],
                        "two logical pages got the same physical address")
    for p in phys:
        self.assertGreaterEqual(int(p, 16), 0x110000,
                                "physical address is not a pinned page")

    # Reading a physical address must reach the logical page it was
    # obtained for, whatever is mapped into the window now.
    self.assertIn("PAGE0=AAAAAAAA", results)
    self.assertIn("PAGE1=BBBBBBBB", results)


def memory_ems_vcpi_disabled(self):
    # With the default configuration nothing may answer on the VCPI
    # interface, so that a DPMI client is not tempted onto the VCPI path.

    self.mkcom_with_nasm('novcpi', r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 0DE00h
    int     67h
    mov     si, mres
    call    puts
    mov     al, ah
    call    puthex8
    call    crlf

    mov     ax, 4C00h
    int     21h

putc:
    push    ax
    push    dx
    mov     dl, al
    mov     ah, 2
    int     21h
    pop     dx
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

puthex8:
    push    ax
    push    cx
    mov     cl, al
    shr     al, 4
    call    .nyb
    mov     al, cl
    and     al, 0Fh
    call    .nyb
    pop     cx
    pop     ax
    ret
.nyb:
    cmp     al, 10
    jb      .d
    add     al, 'a'-10
    jmp     putc
.d:
    add     al, '0'
    jmp     putc

section .data

mres    db 'DE00AH=',0
""")

    self.mkfile("testit.bat", """\
c:\\novcpi
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_ems = (8192)
""")

    # 0x84 is "function not supported"
    self.assertIn("DE00AH=84", results)
