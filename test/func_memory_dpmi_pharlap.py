BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

PHARLAP_CONF = DOSEMU_CONF_DEFAULT + '$_pharlap = (1)\n'


def memory_dpmi_pharlap(self):
    """Check what a phar-lap style client sees: sldt/sgdt have to answer,
    the GDT they point at has to describe the LDT alias, and a descriptor
    written straight into that alias has to become a working selector."""

    self.mkfile("testit.bat", BATCHFILE % 'plgdt', newline="\r\n")

    self.mkexe_with_djgpp("plgdt", r"""
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct dtr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

int main(void)
{
  struct dtr gdtr;
  uint16_t ldtr_m = 0xffff;
  uint32_t ldtr_r = 0xffffffff;
  uint32_t d0, d1, ldt_base, ldt_limit, acc;
  int gsel, lsel, tsel;
  __dpmi_meminfo mi;
  unsigned long base;
  uint32_t lim;

  /* 1. the three instructions phar lap starts with. Under UMIP these
   * #GP from cpl 3 and have to be emulated by the host. */
  memset(&gdtr, 0xee, sizeof(gdtr));
  __asm__ volatile("sldt %0" : "=m"(ldtr_m));
  __asm__ volatile("sldt %0" : "=r"(ldtr_r));
  __asm__ volatile("sgdt %0" : "=m"(gdtr));
  printf("sldt mem=%#x reg=%#lx sgdt base=%#lx limit=%#x\n",
      ldtr_m, (unsigned long)ldtr_r, (unsigned long)gdtr.base, gdtr.limit);

  if (ldtr_m == 0xffff || gdtr.limit == 0xeeee) {
    printf("FAIL: sldt/sgdt not emulated\n");
    return -1;
  }
  if (ldtr_m != (ldtr_r & 0xffff)) {
    printf("FAIL: sldt disagrees with itself\n");
    return -1;
  }
  if (!ldtr_m || !gdtr.base) {
    printf("FAIL: no LDT reported\n");
    return -1;
  }
  if ((ldtr_m | 7) > gdtr.limit) {
    printf("FAIL: LDT selector outside the GDT\n");
    return -1;
  }
  printf("OKAY: sldt/sgdt answered\n");

  /* 2. read the LDT descriptor out of that GDT */
  gsel = __dpmi_allocate_ldt_descriptors(1);
  if (gsel < 0) {
    printf("FAIL: cannot allocate a descriptor\n");
    return -1;
  }
  if (__dpmi_set_segment_base_address(gsel, gdtr.base) ||
      __dpmi_set_segment_limit(gsel, gdtr.limit)) {
    printf("FAIL: cannot point a selector at the GDT\n");
    return -1;
  }
  d0 = _farpeekl(gsel, ldtr_m & ~7);
  d1 = _farpeekl(gsel, (ldtr_m & ~7) + 4);
  ldt_base = ((d0 >> 16) & 0xffff) | ((d1 & 0xff) << 16) | (d1 & 0xff000000);
  ldt_limit = (d0 & 0xffff) | (((d1 >> 16) & 0xf) << 16);
  acc = (d1 >> 8) & 0xff;
  printf("ldt descriptor base=%#lx limit=%#lx acc=%#lx\n",
      (unsigned long)ldt_base, (unsigned long)ldt_limit, (unsigned long)acc);
  if ((acc & 0x9f) != 0x82) {
    printf("FAIL: not an LDT descriptor\n");
    return -1;
  }
  if (!ldt_base || ldt_limit < 0xfff) {
    printf("FAIL: implausible LDT\n");
    return -1;
  }
  printf("OKAY: GDT describes the LDT\n");

  /* 3. write a descriptor into the LDT by hand, the way phar lap does,
   * and see whether the host turned it into a real one */
  tsel = __dpmi_allocate_ldt_descriptors(1);
  if (tsel < 0) {
    printf("FAIL: cannot allocate the victim descriptor\n");
    return -1;
  }
  mi.size = 0x1000;
  if (__dpmi_allocate_memory(&mi)) {
    printf("FAIL: cannot allocate memory\n");
    return -1;
  }
  lsel = __dpmi_allocate_ldt_descriptors(1);
  if (lsel < 0) {
    printf("FAIL: cannot allocate the LDT window\n");
    return -1;
  }
  if (__dpmi_set_segment_base_address(lsel, ldt_base) ||
      __dpmi_set_segment_limit(lsel, ldt_limit)) {
    printf("FAIL: cannot point a selector at the LDT\n");
    return -1;
  }
  base = mi.address;
  d0 = 0xfff | ((base & 0xffff) << 16);
  d1 = ((base >> 16) & 0xff) | (0xf2 << 8) | (0x40 << 16) |
       ((base >> 24) << 24);
  _farpokel(lsel, tsel & ~7, d0);
  _farpokel(lsel, (tsel & ~7) + 4, d1);

  if (__dpmi_get_segment_base_address(tsel, &base)) {
    printf("FAIL: cannot read back the base\n");
    return -1;
  }
  lim = __dpmi_get_segment_limit(tsel);
  printf("after direct write: base=%#lx (wanted %#lx) limit=%#lx\n",
      base, (unsigned long)mi.address, (unsigned long)lim);
  if (base != mi.address || lim != 0xfff) {
    printf("FAIL: direct LDT write did not take\n");
    return -1;
  }
  printf("OKAY: direct LDT write accepted\n");

  /* 4. and the selector it made has to actually work */
  _farpokel(tsel, 0x40, 0xdeadbeef);
  if (_farpeekl(tsel, 0x40) != 0xdeadbeef) {
    printf("FAIL: selector from the direct write is dead\n");
    return -1;
  }
  printf("OKAY: selector usable\n");
  printf("Test OK\n");
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=PHARLAP_CONF, timeout=45)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)


def memory_dpmi_pharlap16(self):
    """The same walk from 16-bit protected mode, which is where the 286
    extender actually lives: 16-bit selectors, 16-bit addressing modes and
    a descriptor written by hand into the LDT."""

    self.mkfile("testit.bat", BATCHFILE % 'pl16', newline="\r\n")

    self.mkcom_with_nasm("pl16", r"""
bits 16
cpu 386
org 0x100

section .text
start:
	mov	ax, 0x1687		; DPMI host present?
	int	0x2f
	test	ax, ax
	jnz	near no_dpmi
	mov	[entry], di
	mov	[entry + 2], es
	mov	ax, cs			; host data area, well past us
	add	ax, 0x400
	mov	es, ax
	xor	ax, ax			; a 16-bit client, like run286
	call	far [cs:entry]
	jc	near no_pmode

; ---- from here on we are in 16-bit protected mode, cpl 3 ----
	xor	ax, ax			; .bss is not in the file, so clear
	mov	[ldtr], ax		; the answers before asking for them
	mov	[gdtr], ax
	mov	[gdtr + 2], ax
	mov	[gdtr + 4], ax
	sldt	[ldtr]
	sgdt	[gdtr]
	mov	ax, [ldtr]
	test	ax, ax
	jz	near no_ldt
	mov	ax, [gdtr + 2]
	or	ax, [gdtr + 4]
	jz	near no_ldt
	mov	ax, [gdtr]		; a table we can believe in is small,
	cmp	ax, 7			; and big enough to hold the entry
	jb	near bad_gdt		; sldt just named
	cmp	ax, 0x1000
	jae	near bad_gdt
	cmp	ax, [ldtr]
	jb	near bad_gdt
	mov	dx, msg_tables
	call	print

	xor	ax, ax			; allocate three descriptors
	mov	cx, 3
	int	0x31
	jc	near no_sel
	mov	[sel1], ax
	add	ax, 8
	mov	[sel2], ax
	add	ax, 8
	mov	[sel3], ax

	mov	bx, [sel1]		; point the first one at the GDT
	mov	cx, [gdtr + 4]
	mov	dx, [gdtr + 2]
	mov	ax, 7
	int	0x31
	jc	near no_sel
	mov	bx, [sel1]
	xor	cx, cx
	mov	dx, [gdtr]
	mov	ax, 8
	int	0x31
	jc	near no_sel

	mov	es, [sel1]		; read the LDT's own descriptor
	mov	bx, [ldtr]
	and	bx, 0xfff8
	mov	al, [es:bx + 5]
	cmp	al, 0x82		; present, dpl 0, ldt
	jne	near not_ldt
	mov	dx, [es:bx + 2]
	mov	[ldtbase], dx
	mov	al, [es:bx + 4]
	mov	ah, [es:bx + 7]
	mov	[ldtbase + 2], ax
	mov	dx, [es:bx]
	mov	[ldtlim], dx
	mov	al, [es:bx + 6]
	and	al, 0x0f
	xor	ah, ah
	mov	[ldtlim + 2], ax
	mov	dx, msg_found
	call	print

	mov	bx, [sel2]		; and point the second one at the LDT
	mov	cx, [ldtbase + 2]
	mov	dx, [ldtbase]
	mov	ax, 7
	int	0x31
	jc	near no_sel
	mov	bx, [sel2]
	mov	cx, [ldtlim + 2]
	mov	dx, [ldtlim]
	mov	ax, 8
	int	0x31
	jc	near no_sel

	mov	es, [sel2]		; write a descriptor the phar lap way
	mov	bx, [sel3]
	and	bx, 0xfff8
	mov	word [es:bx], 0x0fff
	mov	word [es:bx + 2], 0x3000
	mov	byte [es:bx + 4], 0x12
	mov	byte [es:bx + 5], 0xf2
	mov	byte [es:bx + 6], 0
	mov	byte [es:bx + 7], 0

	mov	bx, [sel3]		; did the host pick it up?
	mov	ax, 6
	int	0x31
	jc	near no_sel
	cmp	cx, 0x0012
	jne	near bad_base
	cmp	dx, 0x3000
	jne	near bad_base
	movzx	eax, word [sel3]	; and does the limit hold up?
	lsl	eax, eax
	cmp	eax, 0xfff
	jne	near bad_base
	mov	dx, msg_written
	call	print
	mov	dx, msg_ok
	call	print
	jmp	done

no_dpmi:
	mov	dx, msg_no_dpmi
	jmp	bail
no_pmode:
	mov	dx, msg_no_pmode
	jmp	bail
no_ldt:
	mov	dx, msg_no_ldt
	jmp	bail
not_ldt:
	mov	dx, msg_not_ldt
	jmp	bail
no_sel:
	mov	dx, msg_no_sel
	jmp	bail
bad_gdt:
	mov	dx, msg_bad_gdt
	jmp	bail
bad_base:
	mov	dx, msg_bad_base
bail:
	call	print
done:
	mov	ax, 0x4c00
	int	0x21

print:
	mov	ah, 9
	int	0x21
	ret

section .data
msg_tables	db "OKAY: sldt/sgdt answered", 13, 10, '$'
msg_found	db "OKAY: GDT describes the LDT", 13, 10, '$'
msg_written	db "OKAY: direct LDT write accepted", 13, 10, '$'
msg_ok		db "Test OK", 13, 10, '$'
msg_no_dpmi	db "FAIL: no DPMI host", 13, 10, '$'
msg_no_pmode	db "FAIL: cannot enter protected mode", 13, 10, '$'
msg_no_ldt	db "FAIL: no tables reported", 13, 10, '$'
msg_not_ldt	db "FAIL: not an LDT descriptor", 13, 10, '$'
msg_no_sel	db "FAIL: descriptor call failed", 13, 10, '$'
msg_bad_base	db "FAIL: direct LDT write did not take", 13, 10, '$'
msg_bad_gdt	db "FAIL: sgdt answered with someone else's table", 13, 10, '$'

section .bss
entry		resd 1
ldtr		resw 1
gdtr		resw 3
sel1		resw 1
sel2		resw 1
sel3		resw 1
ldtbase		resd 1
ldtlim		resd 1
""")

    results = self.runDosemu("testit.bat", config=PHARLAP_CONF, timeout=45)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
