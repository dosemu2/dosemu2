def memory_xms_move2(self):
    self.mkfile("testit.bat", """\
c:\\xmsmv2
c:\\xmsmv32
rem end
""", newline="\r\n")

    # XMS function 7Bh on the protected mode entry point: the move of
    # function 0Bh with the reading of an address that comes with a handle
    # of 0 named in AL, 00h the real mode far address of the XMS spec and
    # 01h a linear address, 02h a near offset in DS and 03h sel:ofs. 0Bh
    # has to guess between readings like these, and picks the last two by
    # the bitness of the client, so the same address is moved both ways
    # here and has to land in the two places it means, and each client is
    # asked for the reading 0Bh would not give it. An address type the host
    # does not have must come back as AEh rather than as the 80h of a host
    # without 7Bh.
    self.mkcom_with_nasm("xmsmv2", r"""

bits 16
cpu 386

org 100h

XMLEN	equ 16
BADMODE	equ 0AEh
FARADR	equ 00080000h           ; 0008:0000, or linear 512 KiB

section .text

start:
    pop     ax                  ; the word a COM file gets on its stack
    mov     bx, sp
    add     bx, 15
    shr     bx, 4
    jnz     .small
    mov     bx, 1000h           ; it was a full 64 KiB stack
.small:
    mov     ah, 4Ah
    int     21h                 ; hand back the memory we do not use
    xor     ax, ax
    xchg    ax, word [2Ch]
    mov     es, ax
    mov     ah, 49h
    int     21h                 ; free the environment, if any
    push    ds
    pop     es

    mov     bx, 8               ; a buffer addressable by segment, for the
    mov     ah, 48h             ;  moves that go by the spec
    int     21h
    jc      nomemory
    mov     word [dosseg], ax

    mov     ax, 1687h
    int     2Fh
    test    ax, ax
    jnz     nohost
    push    es
    push    di                  ; the mode switch entry point
    test    si, si
    jz      .nomemneeded
    mov     bx, si
    mov     ah, 48h
    int     21h
    jc      nomemory
    mov     es, ax
.nomemneeded:
    mov     bp, sp
    xor     ax, ax              ; start a 16-bit client
    call    far [bp]
    jc      initfailed

                                ; 16-bit protected mode from here on
    mov     ax, 4300h
    int     2Fh
    cmp     al, 80h
    jne     noxms
    xor     ax, ax
    mov     es, ax
    mov     ax, 4310h
    int     2Fh                 ; the protected mode entry point
    mov     ax, es
    test    ax, ax
    jz      noxms
    mov     word [xmsent], bx
    mov     word [xmsent + 2], ax
    push    ds
    pop     es

    mov     ax, 3
    int     31h                 ; how far apart selectors are
    mov     bp, ax
    xor     ax, ax
    mov     cx, 3               ; one selector on low memory, one on the
    int     31h                 ;  buffer and one where FARADR reads as
    jc      noselector          ;  a linear address
    mov     word [lowsel], ax
    add     ax, bp
    mov     word [bufsel], ax
    add     ax, bp
    mov     word [linsel], ax

    mov     bx, word [lowsel]
    xor     cx, cx
    xor     dx, dx
    call    setseg              ; lowsel at 0
    jc      noselector

    mov     bx, word [bufsel]
    movzx   eax, word [dosseg]
    shl     eax, 4
    call    setlin              ; bufsel at the buffer
    jc      noselector

    mov     bx, word [linsel]
    mov     eax, FARADR
    call    setlin              ; linsel where FARADR points as a linear
    jc      noselector          ;  address

segofs:                         ; the interrupt table into the buffer, by
                                ;  segment, which only the real mode driver
                                ;  can resolve
    mov     dword [xm + 0], XMLEN
    mov     word [xm + 4], 0
    mov     dword [xm + 6], 0   ; 0000:0000
    mov     word [xm + 10], 0
    mov     word [xm + 12], 0
    mov     ax, word [dosseg]
    mov     word [xm + 14], ax
    mov     ax, 7B00h
    call    doxms
    cmp     ax, 1
    jne     .bad
    mov     dx, word [lowsel]
    xor     si, si
    xor     di, di
    call    cmpbuf
    jne     .bad
    mov     dx, msg.segok
    jmp     short .prn
.bad:
    mov     dx, msg.segbad
.prn:
    call    print

linear:                         ; the interrupt table again, this time by
                                ;  linear address, both ends
    mov     dword [xm + 6], 0
    movzx   eax, word [dosseg]
    shl     eax, 4
    add     eax, XMLEN
    mov     dword [xm + 12], eax
    mov     ax, 7B01h
    call    doxms
    cmp     ax, 1
    jne     .bad
    mov     dx, word [lowsel]
    xor     si, si
    mov     di, XMLEN
    call    cmpbuf
    jne     .bad
    mov     dx, msg.linok
    jmp     short .prn
.bad:
    mov     dx, msg.linbad
.prn:
    call    print

noguessseg:                     ; one address, read as a far address
    mov     dword [xm + 6], FARADR
    mov     word [xm + 12], XMLEN * 2
    mov     ax, word [dosseg]
    mov     word [xm + 14], ax
    mov     ax, 7B00h
    call    doxms
    cmp     ax, 1
    jne     .bad
    mov     dx, word [lowsel]
    mov     si, (FARADR >> 16) * 16 + (FARADR & 0FFFFh)
    mov     di, XMLEN * 2
    call    cmpbuf
    jne     .bad
    mov     dx, msg.nogsegok
    jmp     short .prn
.bad:
    mov     dx, msg.nogsegbad
.prn:
    call    print

noguesslin:                     ; and the very same one as a linear address
    mov     dword [xm + 6], FARADR
    movzx   eax, word [dosseg]
    shl     eax, 4
    add     eax, XMLEN * 3
    mov     dword [xm + 12], eax
    mov     ax, 7B01h
    call    doxms
    cmp     ax, 1
    jne     .bad
    mov     dx, word [linsel]
    xor     si, si
    mov     di, XMLEN * 3
    call    cmpbuf
    jne     .bad
    mov     dx, msg.noglinok
    jmp     short .prn
.bad:
    mov     dx, msg.noglinbad
.prn:
    call    print

near32:                         ; a near offset in DS on both sides, which
                                ;  0Bh gives a 32-bit client only
    mov     dword [xm + 6], src
    mov     dword [xm + 12], dst
    mov     ax, 7B02h
    call    doxms
    cmp     ax, 1
    jne     .bad
    mov     si, src
    mov     di, dst
    mov     cx, XMLEN
    repe cmpsb
    jne     .bad
    mov     dx, msg.nearok
    jmp     short .prn
.bad:
    mov     dx, msg.nearbad
.prn:
    call    print

selofs:                         ; sel:ofs on both sides, the interrupt
                                ;  table into a buffer of ours
    mov     word [xm + 6], 0
    mov     ax, word [lowsel]
    mov     word [xm + 8], ax
    mov     word [xm + 12], dst2
    mov     ax, ds
    mov     word [xm + 14], ax
    mov     ax, 7B03h
    call    doxms
    cmp     ax, 1
    jne     .bad
    mov     es, word [lowsel]
    mov     si, dst2
    xor     di, di
    mov     cx, XMLEN
    repe cmpsb
    push    ds
    pop     es
    jne     .bad
    mov     dx, msg.selok
    jmp     short .prn
.bad:
    mov     dx, msg.selbad
.prn:
    call    print

badmode:                        ; an address type we do not have
    mov     ax, 7BFFh
    call    doxms
    test    ax, ax
    jnz     .bad
    cmp     bl, BADMODE
    jne     .bad
    mov     dx, msg.badmodeok
    jmp     short .prn
.bad:
    mov     dx, msg.badmodebad
.prn:
    call    print

    mov     dx, msg.done
    call    print
    mov     ax, 4C00h
    int     21h

nohost:
    mov     dx, msg.nohost
    jmp     short quit
nomemory:
    mov     dx, msg.nomemory
    jmp     short quit
initfailed:
    mov     dx, msg.initfailed
    jmp     short quit
noxms:
    mov     dx, msg.noxms
    jmp     short quit
noselector:
    mov     dx, msg.noselector
quit:
    call    print
    mov     ax, 4CFFh
    int     21h

setlin:                         ; selector bx at the linear address in eax
    mov     dx, ax
    shr     eax, 16
    mov     cx, ax
setseg:                         ; selector bx at cx:dx, 64 KiB of it
    mov     ax, 7
    int     31h
    jc      .out
    xor     cx, cx
    mov     dx, 0FFFFh
    mov     ax, 8
    int     31h
.out:
    retn

                                ; XMLEN bytes at dx:si against the buffer
                                ;  at bufsel:di, ZF set when they match
cmpbuf:
    mov     bp, ds              ; ds is about to go, keep it
    mov     ax, word [bufsel]
    mov     es, ax
    mov     ds, dx
    mov     cx, XMLEN
    repe cmpsb
    mov     ds, bp
    mov     es, bp
    retn

doxms:
    mov     si, xm
    call    far word [xmsent]
    retn

print:
    mov     ah, 9
    int     21h
    retn

    align 4
xmsent:     dd 0
dosseg:     dw 0
lowsel:     dw 0
bufsel:     dw 0
linsel:     dw 0

    align 4
xm:                             ; the XMS move structure
    dd      0                   ; length
    dw      0                   ; source handle
    dd      0                   ; source address
    dw      0                   ; destination handle
    dd      0                   ; destination address

src:        db "0123456789ABCDEF"
dst:        times XMLEN db 0
dst2:       times XMLEN db 0

msg:
.segok:       db "SEGOFS OK",13,10,"$"
.segbad:      db "FAIL segofs",13,10,"$"
.linok:       db "LINEAR OK",13,10,"$"
.linbad:      db "FAIL linear",13,10,"$"
.nogsegok:    db "NOGUESS SEG OK",13,10,"$"
.nogsegbad:   db "FAIL noguess seg",13,10,"$"
.noglinok:    db "NOGUESS LIN OK",13,10,"$"
.noglinbad:   db "FAIL noguess lin",13,10,"$"
.nearok:      db "NEAR OK",13,10,"$"
.nearbad:     db "FAIL near",13,10,"$"
.selok:       db "SELOFS OK",13,10,"$"
.selbad:      db "FAIL selofs",13,10,"$"
.badmodeok:   db "BADMODE OK",13,10,"$"
.badmodebad:  db "FAIL badmode",13,10,"$"
.done:        db "TEST DONE",13,10,"$"
.nohost:      db "FAIL no DPMI host",13,10,"$"
.nomemory:    db "FAIL no DOS memory",13,10,"$"
.initfailed:  db "FAIL DPMI init",13,10,"$"
.noxms:       db "FAIL no XMS entry point",13,10,"$"
.noselector:  db "FAIL no selector",13,10,"$"
""")

    # The same two readings asked for by a 32-bit client, which function
    # 0Bh gives the near offset and never sel:ofs.
    self.mkexe_with_djgpp("xmsmv32", r"""
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dpmi.h>
#include <sys/farptr.h>

#define XMLEN 16

struct __attribute__ ((__packed__)) EMM {
   unsigned int Length;
   unsigned short SourceHandle;
   unsigned int SourceOffset;
   unsigned short DestHandle;
   unsigned int DestOffset;
};

static __dpmi_paddr xms;
static struct EMM e;
static char src[XMLEN] = "0123456789ABCDE";
static char dst[XMLEN];

static int call_xms_dssi(unsigned ax, void *ptr)
{
    int rc, err;

    asm volatile("lcall *%[x]\n"
        : "=a"(rc), "=b"(err)
        : [x]"m"(xms), "a"(ax), "S"(ptr)
        : "cc", "memory");
    if (!rc)
        printf("xms error %x\n", err & 0xff);
    return rc;
}

int main(void)
{
    __dpmi_regs r = {};
    int lowsel, dosbufsel, dosseg, i, ok;

    r.x.ax = 0x4300;
    __dpmi_int(0x2f, &r);
    if (r.h.al != 0x80) {
        printf("FAIL no XMS entry point\n");
        return 1;
    }
    asm volatile(
        "int $0x2f\n"
        "mov %%es, %0\n"
        : "=r"(xms.selector), "=b"(xms.offset32)
        : "a"(0x4310));

    lowsel = __dpmi_allocate_ldt_descriptors(1);
    if (lowsel < 0 || __dpmi_set_segment_base_address(lowsel, 0) < 0 ||
            __dpmi_set_segment_limit(lowsel, 0xffff) < 0) {
        printf("FAIL no selector\n");
        return 1;
    }
    dosseg = __dpmi_allocate_dos_memory(2, &dosbufsel);
    if (dosseg < 0) {
        printf("FAIL no DOS memory\n");
        return 1;
    }

    /* a near offset in DS on both sides */
    e.Length = XMLEN;
    e.SourceHandle = 0;
    e.SourceOffset = (uintptr_t)src;
    e.DestHandle = 0;
    e.DestOffset = (uintptr_t)dst;
    if (call_xms_dssi(0x7b02, &e) && memcmp(dst, src, XMLEN) == 0)
        printf("32 NEAR OK\n");
    else
        printf("FAIL 32 near\n");

    /* sel:ofs on both sides, the interrupt table into DOS memory */
    e.SourceOffset = (unsigned)lowsel << 16;
    e.DestOffset = (unsigned)dosbufsel << 16;
    ok = call_xms_dssi(0x7b03, &e);
    for (i = 0; ok && i < XMLEN; i++) {
        if (_farpeekb(dosbufsel, i) != _farpeekb(lowsel, i))
            ok = 0;
    }
    printf(ok ? "32 SELOFS OK\n" : "FAIL 32 selofs\n");

    printf("32 DONE\n");
    return 0;
}
""")

    results = self.runDosemu("testit.bat")

    self.assertIn("SEGOFS OK", results)
    self.assertIn("LINEAR OK", results)
    self.assertIn("NOGUESS SEG OK", results)
    self.assertIn("NOGUESS LIN OK", results)
    self.assertIn("NEAR OK", results)
    self.assertIn("SELOFS OK", results)
    self.assertIn("BADMODE OK", results)
    self.assertIn("TEST DONE", results)
    self.assertIn("32 NEAR OK", results)
    self.assertIn("32 SELOFS OK", results)
    self.assertIn("32 DONE", results)
    self.assertNotIn("FAIL", results)
