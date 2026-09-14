import re


def pktdriver_api(self):
    """Exercise the packet driver API, including the functions that
    spec 1.10 and 1.11 added.  Uses slirp so that the receive path can
    be tested by bouncing an ARP request off the gateway (10.0.2.2)
    without needing an external network or a tap device."""

    self.mkfile("testit.bat", """\
c:\\pkttest
rem end
""", newline="\r\n")

    self.mkcom_with_nasm("pkttest", r"""
; Packet driver conformance test for the dosemu2 built-in driver.
; Prints one "TAG=hhhh" line per checked item; the python side asserts
; on those.  Needs $_vnet="slirp" for the receive phase, which bounces
; an ARP request off slirp's gateway (10.0.2.2).

bits 16
cpu 386
org 100h

%define PKTINT 0x60

; capture the packet driver error return into AX (0 == success)
%macro GETERR 0
    mov     ax, 0
    jnc     %%ok
    mov     al, dh
%%ok:
%endmacro

; report a constant tag with the value currently in AX
%macro REPORT 1
    mov     dx, %1
    call    report
%endmacro

section .text

start:
    push    cs
    pop     ds
    push    cs
    pop     es

; ---------------- driver signature ----------------
    mov     ax, 0x3500 + PKTINT
    int     0x21                    ; ES:BX = handler
    mov     di, bx
    add     di, 3
    mov     si, sigstr
    mov     cx, 8
    cld
    repe cmpsb
    mov     ax, 0
    jne     .nosig
    inc     ax
.nosig:
    push    cs
    pop     es
    push    ax
    REPORT  t_sig
    pop     ax
    test    ax, ax
    jnz     .havesig
    jmp     done
.havesig:

; ---------------- driver_info ----------------
    mov     ax, 0x01ff
    xor     bx, bx
    int     PKTINT
    push    cx                      ; CH = class
    mov     ah, 0
    push    ax                      ; AL = functionality
    push    cs
    pop     ds
    pop     ax
    REPORT  t_info
    pop     ax
    mov     al, ah
    mov     ah, 0
    REPORT  t_class

; ---------------- get_parameters ----------------
    mov     ah, 10
    int     PKTINT
    jnc     .parmok
    mov     ax, 0xffff
    REPORT  t_pmaj
    jmp     .parmdone
.parmok:
    ; copy the struct somewhere we control before touching segments
    mov     si, di
    push    cs
    pop     ax
    push    es
    pop     ds
    mov     es, ax
    mov     di, params
    mov     cx, 14
    cld
    rep movsb
    push    cs
    pop     ds
    push    cs
    pop     es
    mov     al, [params]            ; major_rev
    mov     ah, 0
    REPORT  t_pmaj
    mov     al, [params+1]          ; minor_rev
    mov     ah, 0
    REPORT  t_pmin
    mov     al, [params+2]          ; length
    mov     ah, 0
    REPORT  t_plen
    mov     al, [params+3]          ; addr_len
    mov     ah, 0
    REPORT  t_palen
    mov     ax, [params+6]          ; multicast_aval
    REPORT  t_pmcav
.parmdone:

; ---------------- access_type (ARP) ----------------
    mov     ax, 0x0201              ; AH=2, AL=class 1
    mov     bx, 0xffff              ; any type
    xor     dx, dx                  ; if_number 0
    mov     cx, 2
    mov     si, arptype
    push    cs
    pop     es
    mov     di, receiver
    int     PKTINT
    jc      .accfail
    mov     [handle], ax
    xor     ax, ax
    jmp     .accrep
.accfail:
    mov     al, dh
    mov     ah, 0
.accrep:
    push    ax
    REPORT  t_acc
    pop     ax
    test    ax, ax
    jz      .accok
    jmp     done
.accok:

; ---------------- get_address ----------------
    push    cs
    pop     es
    mov     di, myaddr
    mov     cx, 6
    mov     ah, 6
    int     PKTINT
    GETERR
    REPORT  t_getadr

; ---------------- set_address ----------------
    push    cs
    pop     es
    mov     di, newaddr
    mov     cx, 6
    mov     ah, 25
    int     PKTINT
    GETERR
    REPORT  t_setadr
    ; read it back and compare
    push    cs
    pop     es
    mov     di, rdaddr
    mov     cx, 6
    mov     ah, 6
    int     PKTINT
    push    cs
    pop     ds
    push    cs
    pop     es
    mov     si, newaddr
    mov     di, rdaddr
    mov     cx, 6
    cld
    repe cmpsb
    mov     ax, 0
    jne     .adrne
    inc     ax
.adrne:
    REPORT  t_adrchg
    ; put the original address back
    push    cs
    pop     es
    mov     di, myaddr
    mov     cx, 6
    mov     ah, 25
    int     PKTINT
    GETERR
    REPORT  t_adrrst

; ---------------- set/get_rcv_mode ----------------
    mov     bx, [handle]
    mov     cx, 3
    mov     ah, 20
    int     PKTINT
    GETERR
    REPORT  t_setrcv
    mov     bx, [handle]
    mov     ah, 21
    int     PKTINT
    jnc     .grmok
    mov     ax, 0xffff
.grmok:
    REPORT  t_getrcv
    ; back to promiscuous so the receive phase is not filtered out
    mov     bx, [handle]
    mov     cx, 6
    mov     ah, 20
    int     PKTINT

; ---------------- set/get_multicast_list ----------------
    push    cs
    pop     es
    mov     di, mclist
    mov     cx, 12
    mov     ah, 22
    int     PKTINT
    GETERR
    REPORT  t_setmc
    mov     ah, 23
    int     PKTINT
    jnc     .mcok
    mov     ax, 0xffff
    REPORT  t_mclen
    jmp     .mcdone
.mcok:
    ; copy the returned list out before touching segments
    push    cx
    mov     si, di
    push    cs
    pop     ax
    push    es
    pop     ds
    mov     es, ax
    mov     di, mcread
    mov     cx, 12
    cld
    rep movsb
    push    cs
    pop     ds
    push    cs
    pop     es
    pop     ax
    REPORT  t_mclen
    mov     si, mclist
    mov     di, mcread
    mov     cx, 12
    cld
    repe cmpsb
    mov     ax, 0
    jne     .mcne
    inc     ax
.mcne:
    REPORT  t_mcmatch
.mcdone:

; ---------------- get_statistics ----------------
    call    readstats
    jnc     .stok
    mov     ax, 0
    REPORT  t_stats
    jmp     .stdone
.stok:
    mov     ax, 1
    REPORT  t_stats
.stdone:

; ---------------- get_structure ----------------
    mov     bx, 1                   ; STRUCT_IO_STATS
    mov     ah, 30
    int     PKTINT
    pushf
    push    cs
    pop     ds
    popf
    mov     ax, 0
    jc      .gsdone
    inc     ax
.gsdone:
    push    cs
    pop     es
    REPORT  t_struct
    mov     bx, 99                  ; unknown structure type
    mov     ah, 30
    int     PKTINT
    pushf
    push    cs
    pop     ds
    push    cs
    pop     es
    popf
    GETERR
    REPORT  t_structbad

; ---------------- functions that must be refused ----------------
    mov     ah, 11                  ; old_as_send_pkt, withdrawn in 1.10
    int     PKTINT
    GETERR
    REPORT  t_oldas

    mov     ah, 26                  ; send_raw_bytes, serial only
    int     PKTINT
    GETERR
    REPORT  t_raw

    mov     ah, 29                  ; signal, PPP only
    int     PKTINT
    GETERR
    REPORT  t_signal

    mov     ah, 99                  ; nonexistent
    int     PKTINT
    GETERR
    REPORT  t_bogus

; ============================================================
; receive phase: bounce an ARP request off the slirp gateway
; ============================================================
    ; patch our station address into the ARP request
    mov     si, myaddr
    mov     di, arpreq + 6
    mov     cx, 6
    cld
    rep movsb
    mov     si, myaddr
    mov     di, arpreq + 22
    mov     cx, 6
    cld
    rep movsb

    mov     byte [rcv_done], 0
    mov     si, arpreq
    mov     cx, 42
    mov     ah, 4                   ; send_pkt
    int     PKTINT
    GETERR
    REPORT  t_send
    call    waitpkt
    mov     al, [rcv_done]
    mov     ah, 0
    REPORT  t_rcvdone
    mov     ax, [rcv_len]
    REPORT  t_rcvlen
    mov     ax, [rcv_lah]
    REPORT  t_rcvlah
    mov     ax, [rcv_copied]
    REPORT  t_rcvcopied
    ; is it an ARP reply?
    mov     ax, 0
    cmp     word [rcvbuf+12], 0x0608    ; 0x0806 in network order
    jne     .notarp
    cmp     word [rcvbuf+20], 0x0200    ; opcode 2 in network order
    jne     .notarp
    inc     ax
.notarp:
    REPORT  t_rcvarp

; ---- a receiver that clobbers CX must not get a truncated packet ----
    call    readstats
    mov     eax, [stats+24]         ; packets_lost
    mov     [lost0], eax
    mov     byte [rcv_done], 0
    mov     byte [shrink], 1
    mov     si, arpreq
    mov     cx, 42
    mov     ah, 4
    int     PKTINT
    call    waitpkt
    mov     byte [shrink], 0
    mov     al, [rcv_done]
    mov     ah, 0
    REPORT  t_shrdone
    call    readstats
    mov     eax, [stats+24]
    sub     eax, [lost0]
    REPORT  t_shrlost

; ============================================================
; as_send_pkt with a transmitter upcall
; ============================================================
    mov     word [iocb], arpreq
    mov     [iocb+2], cs
    mov     word [iocb+4], 42
    mov     byte [iocb+6], 2        ; UPCALL, DONE clear
    mov     byte [iocb+7], 0xee     ; poison, driver must clear it
    mov     word [iocb+8], xmitter
    mov     [iocb+10], cs
    mov     byte [xmit_called], 0
    push    cs
    pop     es
    mov     di, iocb
    mov     ah, 12
    int     PKTINT
    GETERR
    REPORT  t_assend
    mov     al, [iocb+6]            ; flagbits, expect DONE|UPCALL
    mov     ah, 0
    REPORT  t_asflags
    mov     al, [iocb+7]            ; code, expect 0
    mov     ah, 0
    REPORT  t_ascode
    mov     al, [xmit_called]
    mov     ah, 0
    REPORT  t_asupcall

    push    cs
    pop     es
    mov     di, iocb
    mov     ah, 13                  ; drop_pkt
    int     PKTINT
    GETERR
    REPORT  t_drop

; ---------------- release_type ----------------
    mov     bx, [handle]
    mov     ah, 3
    int     PKTINT
    GETERR
    REPORT  t_release

    jmp     done

; ============================================================
; helpers
; ============================================================

; copy the statistics struct into "stats"; CF set on error
readstats:
    mov     bx, [handle]
    mov     ah, 24
    int     PKTINT
    jc      .err
    push    cs
    pop     ax
    push    ds
    pop     bx                      ; stats segment
    push    cs
    pop     ds
    mov     [tmpseg], bx
    push    cs
    pop     es
    mov     di, stats
    mov     ds, bx
    mov     cx, 28
    cld
    rep movsb
    push    cs
    pop     ds
    push    cs
    pop     es
    clc
    ret
.err:
    push    cs
    pop     ds
    push    cs
    pop     es
    stc
    ret

; wait up to ~2s for rcv_done
waitpkt:
    push    es
    mov     ax, 0x40
    mov     es, ax
    mov     bx, [es:0x6c]
    add     bx, 36
.l:
    cmp     byte [cs:rcv_done], 0
    jne     .out
    int     0x28
    mov     ax, 0x40
    mov     es, ax
    mov     cx, [es:0x6c]
    sub     cx, bx
    js      .l
.out:
    pop     es
    ret

report:
    push    ax
    mov     ah, 9
    int     0x21
    pop     ax
    call    puthex16
    mov     dx, crlf
    mov     ah, 9
    int     0x21
    ret

puthex16:
    push    ax
    mov     al, ah
    call    puthex8
    pop     ax
    call    puthex8
    ret

puthex8:
    push    ax
    shr     al, 4
    call    puthex4
    pop     ax
    call    puthex4
    ret

puthex4:
    and     al, 0x0f
    add     al, '0'
    cmp     al, '9'
    jbe     .ok
    add     al, 7
.ok:
    mov     dl, al
    mov     ah, 2
    int     0x21
    ret

done:
    push    cs
    pop     ds
    mov     ah, 9
    mov     dx, t_end
    int     0x21
    mov     ax, 0x4c00
    int     0x21

; ============================================================
; upcalls
; ============================================================
receiver:
    cmp     ax, 0
    jne     .copied
    mov     [cs:rcv_len], cx
    mov     [cs:rcv_lah], dx
    push    cs
    pop     es
    mov     di, rcvbuf
    cmp     byte [cs:shrink], 0
    je      .noshrink
    mov     cx, 8                   ; pretend we only have 8 bytes
.noshrink:
    retf
.copied:
    mov     [cs:rcv_copied], cx
    mov     byte [cs:rcv_done], 1
    retf

xmitter:
    mov     byte [cs:xmit_called], 1
    retf

section .data

sigstr:     db 'PKT DRVR'
arptype:    db 0x08, 0x06
newaddr:    db 0x02, 0x11, 0x22, 0x33, 0x44, 0x55
mclist:     db 0x01, 0x00, 0x5e, 0x00, 0x00, 0x01
            db 0x01, 0x00, 0x5e, 0x00, 0x00, 0xfb

arpreq:     db 0xff,0xff,0xff,0xff,0xff,0xff    ; dst
            db 0,0,0,0,0,0                      ; src (patched)
            db 0x08,0x06                        ; ethertype ARP
            db 0x00,0x01                        ; htype ethernet
            db 0x08,0x00                        ; ptype ipv4
            db 6, 4
            db 0x00,0x01                        ; op request
            db 0,0,0,0,0,0                      ; sha (patched)
            db 10,0,2,15                        ; spa
            db 0,0,0,0,0,0                      ; tha
            db 10,0,2,2                         ; tpa (slirp gateway)

handle:      dw 0
myaddr:      times 8 db 0
rdaddr:      times 8 db 0
params:      times 16 db 0
mcread:      times 12 db 0
stats:       times 28 db 0
lost0:       dd 0
tmpseg:      dw 0
iocb:        times 24 db 0
rcv_len:     dw 0
rcv_lah:     dw 0
rcv_copied:  dw 0
rcv_done:    db 0
shrink:      db 0
xmit_called: db 0

t_sig:       db 'SIG=$'
t_info:      db 'INFO=$'
t_class:     db 'CLASS=$'
t_pmaj:      db 'PMAJ=$'
t_pmin:      db 'PMIN=$'
t_plen:      db 'PLEN=$'
t_palen:     db 'PALEN=$'
t_pmcav:     db 'PMCAV=$'
t_acc:       db 'ACCESS=$'
t_getadr:    db 'GETADDR=$'
t_setadr:    db 'SETADDR=$'
t_adrchg:    db 'ADDRCHG=$'
t_adrrst:    db 'ADDRRST=$'
t_setrcv:    db 'SETRCV=$'
t_getrcv:    db 'GETRCV=$'
t_setmc:     db 'SETMC=$'
t_mclen:     db 'MCLEN=$'
t_mcmatch:   db 'MCMATCH=$'
t_stats:     db 'STATS=$'
t_struct:    db 'STRUCT=$'
t_structbad: db 'STRUCTBAD=$'
t_oldas:     db 'OLDAS=$'
t_raw:       db 'RAW=$'
t_signal:    db 'SIGNAL=$'
t_bogus:     db 'BOGUS=$'
t_send:      db 'SEND=$'
t_rcvdone:   db 'RCVDONE=$'
t_rcvlen:    db 'RCVLEN=$'
t_rcvlah:    db 'RCVLAH=$'
t_rcvcopied: db 'RCVCOPIED=$'
t_rcvarp:    db 'RCVARP=$'
t_shrdone:   db 'SHRDONE=$'
t_shrlost:   db 'SHRLOST=$'
t_assend:    db 'ASSEND=$'
t_asflags:   db 'ASFLAGS=$'
t_ascode:    db 'ASCODE=$'
t_asupcall:  db 'ASUPCALL=$'
t_drop:      db 'DROP=$'
t_release:   db 'RELEASE=$'
t_end:       db 'PKTTEST-END',13,10,'$'
crlf:        db 13,10,'$'

rcvbuf:      times 1600 db 0

""")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_pktdriver = (on)
$_vnet = "slirp"
""", timeout=60)

    self.assertIn("PKTTEST-END", results, results)

    def val(tag):
        m = re.search(r'^%s=([0-9A-F]{4})\r?$' % tag, results, re.MULTILINE)
        self.assertIsNotNone(m, "no %s line in output:\n%s" % (tag, results))
        return int(m.group(1), 16)

    # the driver is there and announces basic + high-performance + extended
    self.assertEqual(1, val("SIG"), results)
    self.assertEqual(6, val("INFO"), results)
    self.assertEqual(1, val("CLASS"), results)

    # get_parameters() must announce 1.10 or later now that the
    # receiver upcall passes a look-ahead buffer
    self.assertEqual(1, val("PMAJ"), results)
    self.assertGreaterEqual(val("PMIN"), 10, results)
    self.assertEqual(14, val("PLEN"), results)
    self.assertEqual(6, val("PALEN"), results)
    self.assertEqual(96, val("PMCAV"), results)

    self.assertEqual(0, val("ACCESS"), results)

    # set_address() takes effect and get_address() reports it back
    self.assertEqual(0, val("GETADDR"), results)
    self.assertEqual(0, val("SETADDR"), results)
    self.assertEqual(1, val("ADDRCHG"), results)
    self.assertEqual(0, val("ADDRRST"), results)

    # the receive mode is remembered per handle
    self.assertEqual(0, val("SETRCV"), results)
    self.assertEqual(3, val("GETRCV"), results)

    # the multicast list round-trips
    self.assertEqual(0, val("SETMC"), results)
    self.assertEqual(12, val("MCLEN"), results)
    self.assertEqual(1, val("MCMATCH"), results)

    self.assertEqual(1, val("STATS"), results)

    # get_structure(STRUCT_IO_STATS) works, anything else is refused
    self.assertEqual(1, val("STRUCT"), results)
    self.assertEqual(17, val("STRUCTBAD"), results)     # BAD_ARGUMENT

    # withdrawn, serial-line-only and unknown functions are all refused
    # with BAD_COMMAND rather than silently doing something
    for tag in ("OLDAS", "RAW", "SIGNAL", "BOGUS"):
        self.assertEqual(11, val(tag), results)         # BAD_COMMAND

    # the ARP request went out and the reply came back up through the
    # receiver upcall
    self.assertEqual(0, val("SEND"), results)
    self.assertEqual(1, val("RCVDONE"), results)
    self.assertEqual(1, val("RCVARP"), results)
    self.assertGreater(val("RCVLEN"), 0, results)
    self.assertEqual(val("RCVLEN"), val("RCVCOPIED"), results)
    # a non-zero look-ahead length is what makes this a 1.10 receiver
    self.assertGreater(val("RCVLAH"), 0, results)

    # a receiver that clobbers CX instead of returning its buffer size
    # must not be handed a truncated packet: the packet is dropped and
    # counted as lost
    self.assertEqual(0, val("SHRDONE"), results)
    self.assertGreaterEqual(val("SHRLOST"), 1, results)

    # as_send_pkt() completes the iocb and makes the transmitter upcall
    self.assertEqual(0, val("ASSEND"), results)
    self.assertEqual(3, val("ASFLAGS"), results)        # DONE | UPCALL
    self.assertEqual(0, val("ASCODE"), results)
    self.assertEqual(1, val("ASUPCALL"), results)

    self.assertEqual(0, val("DROP"), results)
    self.assertEqual(0, val("RELEASE"), results)
