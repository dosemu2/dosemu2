import re

# What a real cpu produces, three bytes per case, in the order the probe
# below runs them: first the eight FCMOVcc opcodes over the sixteen flag
# settings (was the move made, and the flags as the instruction left them),
# then the four F(U)COMI(P) opcodes over eleven operand pairs and two flag
# presets (the flags it set, and the exception flags it raised).
FCMOV_REF = (
    "000000010100000400010500004000014100004400014500"
    "00900801910800940801950800d00801d10800d40801d508"
    "000000000100000400000500014000014100014400014500"
    "00900800910800940800950801d00801d10801d40801d508"
    "000000010100000400010500014000014100014400014500"
    "00900801910800940801950801d00801d10801d40801d508"
    "000000000100010400010500004000004100014400014500"
    "00900800910801940801950800d00800d10801d40801d508"
    "010000000100010400000500014000004100014400004500"
    "01900800910801940800950801d00800d10801d40800d508"
    "010000010100010400010500004000004100004400004500"
    "01900801910801940801950800d00800d10800d40800d508"
    "010000000100010400000500004000004100004400004500"
    "01900800910801940800950800d00800d10800d40800d508"
    "010000010100000400000500014000014100004400004500"
    "01900801910800940800950801d00801d10800d40800d508"
    "010000010000400000400000000000000000000000000000"
    "010000010000400000400000450001450001450001450001"
    "450001450001010000010000000000000000010000010000"
    "400000400000000000000000000000000000010000010000"
    "400000400000450000450000450000450000450000450000"
    "010000010000000000000000010000010000400000400000"
    "000000000000000000000000010000010000400000400000"
    "450001450001450001450001450001450001010000010000"
    "000000000000010000010000400000400000000000000000"
    "000000000000010000010000400000400000450000450000"
    "450000450000450000450000010000010000000000000000"
)


def cpu_fcmov(self, cpusim):
    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_cpuemu = (%i)
""" % cpusim

    self.mkfile("testit.bat", """\
c:\\fcmov
rem end
""", newline="\r\n")

    self.mkcom_with_nasm("fcmov", r"""

bits 16
cpu 386

org 100h

%define NFLAGS	16
%define NPAIRS	11

section .text

	push	cs
	pop	ds
	push	cs
	pop	es

	mov	di, rbuf
	xor	si, si			; opcode index

fcmov_op:
	xor	bx, bx			; flag setting index (word table)
fcmov_fl:
	mov	bp, si
	shl	bp, 1
	mov	bp, [fcmovtab+bp]	; stub address, worked out before popf
	finit
	fld	dword [valA]
	fld	dword [valB]		; st(0)=B, st(1)=A
	mov	ax, [flagtab+bx]
	push	ax
	popf
	call	bp
	pushf
	pop	dx			; the flags as the instruction left them
	fstp	dword [res]
	mov	ax, [res+2]
	cmp	ax, 0x3f80		; high word of 1.0, i.e. st(1) was moved
	mov	al, 0
	jne	.nm
	mov	al, 1
.nm:	mov	[di], al
	and	dx, 0x8d5
	mov	[di+1], dl
	mov	[di+2], dh
	add	di, 3
	add	bx, 2
	cmp	bx, NFLAGS*2
	jb	fcmov_fl
	inc	si
	cmp	si, 8
	jb	fcmov_op

	xor	si, si			; opcode index
fcomi_op:
	xor	bx, bx			; operand pair index
fcomi_pair:
	xor	cx, cx			; flag preset index
fcomi_pre:
	push	bx
	push	cx
	mov	bp, si
	shl	bp, 1
	mov	bp, [fcomitab+bp]
	mov	ax, bx
	shl	ax, 3			; two dwords per pair
	mov	bx, ax
	finit
	fld	dword [pairs+bx+4]	; st(0)=b
	fld	dword [pairs+bx]	; st(0)=a, st(1)=b
	shl	cx, 1
	mov	bx, cx
	mov	ax, [presets+bx]
	push	ax
	popf
	call	bp
	pushf
	pop	dx
	fnstsw	ax			; fnstsw leaves the flags alone
	and	dx, 0x8d5
	mov	[di], dl
	mov	[di+1], dh
	and	al, 0x3f		; the six exception flags
	mov	[di+2], al
	add	di, 3
	pop	cx
	pop	bx
	inc	cx
	cmp	cx, 2
	jb	fcomi_pre
	inc	bx
	cmp	bx, NPAIRS
	jb	fcomi_pair
	inc	si
	cmp	si, 4
	jb	fcomi_op

; print rbuf as hex, 24 bytes to a line
	mov	si, rbuf
	xor	cx, cx
dump:
	cmp	si, di
	jae	dumpend
	lodsb
	push	ax
	shr	al, 4
	call	puthex
	pop	ax
	and	al, 0x0f
	call	puthex
	inc	cx
	cmp	cx, 24
	jb	dump
	xor	cx, cx
	call	putnl
	jmp	dump
dumpend:
	call	putnl
	mov	ax, 4c00h
	int	21h

putnl:
	mov	dl, 13
	call	putc
	mov	dl, 10
	jmp	putc

puthex:
	add	al, '0'
	cmp	al, '9'
	jbe	.p
	add	al, 'a'-'0'-10
.p:	mov	dl, al
putc:
	push	ax
	push	cx
	push	si
	mov	ah, 2
	int	21h
	pop	si
	pop	cx
	pop	ax
	ret

; one stub per opcode, so that nothing between the popf and the
; instruction under test can disturb the flags
%macro STUB 3
%1:	db	%2, %3
	ret
%endmacro
	STUB	f_cmovb,   0xda,0xc1	; fcmovb   st(0),st(1)
	STUB	f_cmove,   0xda,0xc9	; fcmove   st(0),st(1)
	STUB	f_cmovbe,  0xda,0xd1	; fcmovbe  st(0),st(1)
	STUB	f_cmovu,   0xda,0xd9	; fcmovu   st(0),st(1)
	STUB	f_cmovnb,  0xdb,0xc1	; fcmovnb  st(0),st(1)
	STUB	f_cmovne,  0xdb,0xc9	; fcmovne  st(0),st(1)
	STUB	f_cmovnbe, 0xdb,0xd1	; fcmovnbe st(0),st(1)
	STUB	f_cmovnu,  0xdb,0xd9	; fcmovnu  st(0),st(1)
	STUB	f_comi,    0xdb,0xf1	; fcomi    st(0),st(1)
	STUB	f_ucomi,   0xdb,0xe9	; fucomi   st(0),st(1)
	STUB	f_comip,   0xdf,0xf1	; fcomip   st(0),st(1)
	STUB	f_ucomip,  0xdf,0xe9	; fucomip  st(0),st(1)

section .data

fcmovtab:
	dw	f_cmovb, f_cmove, f_cmovbe, f_cmovu
	dw	f_cmovnb, f_cmovne, f_cmovnbe, f_cmovnu
fcomitab:
	dw	f_comi, f_ucomi, f_comip, f_ucomip

; CF(0x01) PF(0x04) ZF(0x40) in every combination, each also with
; AF(0x10) SF(0x80) OF(0x800) set, which no condition may look at
flagtab:
	dw	0x002, 0x003, 0x006, 0x007, 0x042, 0x043, 0x046, 0x047
	dw	0x892, 0x893, 0x896, 0x897, 0x8d2, 0x8d3, 0x8d6, 0x8d7
presets:
	dw	0x8d7, 0x002		; all CC flags set, then all clear

valA:	dd	1.0
valB:	dd	2.0
pairs:	dd	1.0, 2.0		; a < b
	dd	2.0, 2.0		; a == b
	dd	2.0, 1.0		; a > b
	dd	2.0, -1.0
	dd	2.0, 3.0
	dd	-0.0, 0.0		; equal, opposite signs
	dd	0x7fc00000, 1.0		; a is a quiet NaN
	dd	2.0, 0x7fc00000		; b is a quiet NaN
	dd	0x7fc00000, 0x7fc00000	; both are quiet NaNs
	dd	0xff800000, -1.0	; a is -inf
	dd	0x7f800000, -1.0	; a is +inf

section .bss

res:	resd	1
rbuf:	resb	648
""")

    results = self.runDosemu("testit.bat", config=config)

    got = "".join(re.findall(r'^([0-9a-f]{48})\r?$', results, re.MULTILINE))
    want = "".join(FCMOV_REF)
    self.assertNotEqual(got, "", results)

    if got != want:
        for i in range(0, min(len(got), len(want)), 6):
            if got[i:i+6] != want[i:i+6]:
                case = i // 6
                if case < 128:
                    what = "fcmov opcode %i, flags %i" % (case // 16, case % 16)
                else:
                    case -= 128
                    what = "fcomi opcode %i, pair %i, preset %i" % (
                        case // 22, (case % 22) // 2, case % 2)
                self.fail("%s: expected %s, got %s\n%s" % (
                    what, want[i:i+6], got[i:i+6], results))
    self.assertEqual(got, want, results)
