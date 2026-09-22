def memory_jemm_aperture(self):
    """JEMM shows the video aperture where a client above the 1M line looks."""

    # JEMM translates the whole first megabyte page by page and points the
    # page above the 1M line at the card's window, so that a client whose
    # EMS windows cover 0xa0000 can still put a picture on the screen.
    # Privateer relies on it: it reads the window array's segment from
    # int 67h ah=41h, sees it below 0xb000 and moves its front surface to
    # ffff:0010.  This paints through that address and looks for the paint
    # both at 0xa0000 and on the screen itself.
    self.mkcom_with_nasm("apert", r"""
bits 16
cpu 386

org 100h

section .text
start:
	; ffff:0010 must reach 0x100000 rather than wrap into low memory
	mov	ax, 4310h		; XMS driver entry
	int	2Fh
	mov	[xmsp], bx
	mov	[xmsp + 2], es
	mov	ah, 03h			; global enable A20
	call	far [xmsp]
	mov	[a20], ax

	mov	ah, 41h			; the page frame, as Privateer asks
	int	67h
	mov	[frame], bx

	mov	ax, 0013h		; 320x200x256
	int	10h

	; paint through the aperture above the 1M line
	mov	ax, 0ffffh
	mov	es, ax
	mov	di, 0010h
	mov	cx, 32000
	mov	ax, 2525h
	cld
	rep	stosw

	; and read it back where the card's window is
	mov	ax, 0a000h
	mov	es, ax
	xor	di, di
	mov	cx, 32000
	mov	ax, 2525h
	repe	scasw
	mov	[left], cx

	push	cs
	pop	ds
	push	cs
	pop	es

	mov	si, mleft
	mov	ax, [left]
	call	report
	mov	si, ma20
	mov	ax, [a20]
	call	report
	mov	si, mframe
	mov	ax, [frame]
	call	report

	mov	ah, 3Ch			; the file the test reads back
	xor	cx, cx
	mov	dx, fname
	int	21h
	jc	idle
	mov	bx, ax
	mov	ah, 40h
	mov	cx, [outlen]
	mov	dx, out
	int	21h
	mov	ah, 3Eh
	int	21h

	; never return to DOS: the screen must keep the paint until the test
	; has taken it, and a key is never coming
idle:
	xor	ah, ah
	int	16h
	jmp	idle

; si -> caption, ax = value; appends "caption=hhhh\r\n" to out
report:
	push	ax
	mov	di, [outlen]
	add	di, out
.copy:
	lodsb
	or	al, al
	jz	.value
	stosb
	jmp	.copy
.value:
	pop	ax
	call	hex16
	mov	al, 13
	stosb
	mov	al, 10
	stosb
	sub	di, out
	mov	[outlen], di
	ret

hex16:
	push	ax
	mov	al, ah
	call	hex8
	pop	ax
hex8:
	push	ax
	shr	al, 4
	call	hex4
	pop	ax
hex4:
	and	al, 0fh
	add	al, '0'
	cmp	al, '9'
	jbe	.put
	add	al, 7
.put:
	stosb
	ret

section .data
xmsp:	dd	0
a20:	dw	0
frame:	dw	0
left:	dw	0ffffh
outlen:	dw	0
fname:	db	"C:\APERT.TXT", 0
mleft:	db	"LEFT=", 0
ma20:	db	"A20=", 0
mframe:	db	"FRAME=", 0
out:	times 64 db 0
""")

    screen = self.runDosemuGraphics(r"c:\apert", "apert.txt", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_ems = (8192)
$_jemm = (on)
""")

    results = (self.workdir / "apert.txt").read_text()

    self.assertIn("A20=0001", results)       # else ffff:0010 is 0x0010
    self.assertIn("FRAME=A000", results)     # the layout Privateer sees
    self.assertIn("LEFT=0000", results)      # every word came back

    # and the card drew it: without the aperture the screen stays black
    hist = self.readXWD(screen)
    colour, count = max(((c, n) for c, n in hist.items() if c != (0, 0, 0)),
                        key=lambda cn: cn[1], default=((0, 0, 0), 0))
    self.assertGreater(count, 50000,
                       "screen stayed dark, top colours %r" % hist.most_common(3))
