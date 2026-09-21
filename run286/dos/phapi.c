/*
 * PHAPI on top of DPMI.
 *
 * Every function here is entered from a 16bit stub through gate_entry(),
 * with the program's arguments still on its own stack. The Pascal calling
 * convention puts them there left to right, so the last one is nearest the
 * return address; the offsets below are counted from CALL_ARGS.
 *
 * Free software, GPL v2 or later.
 */
#include <stdio.h>
#include <string.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include "run286.h"

/* the error codes these functions return in AX */
#define ERROR_INVALID_PARAMETER	87
#define ERROR_NOT_ENOUGH_MEMORY	8

/* access rights of a 16bit, DPL 3, present data segment */
#define AR_DATA16	0x00f2

uint16_t call_argw(struct call *c, unsigned off)
{
    return _farpeekw(c->ss, c->sp + CALL_ARGS + off);
}

uint32_t call_argd(struct call *c, unsigned off)
{
    return _farpeekl(c->ss, c->sp + CALL_ARGS + off);
}

/* A far pointer argument is a selector in the high half and an offset in
 * the low one, so it can be poked at directly. */
void call_setw(uint32_t fp, uint16_t val)
{
    _farpokew(fp >> 16, fp & 0xffff, val);
}

void call_setd(uint32_t fp, uint32_t val)
{
    _farpokel(fp >> 16, fp & 0xffff, val);
}

/* Hands out one descriptor covering [base, base + size). A size of zero
 * means the full 64K a 16bit selector can reach: the programs get there by
 * pushing a 16bit count, so a 64K request arrives as 0. */
static uint16_t map_seg(uint32_t base, uint32_t size, uint32_t selp)
{
    uint16_t sel;

    if (!size)
	size = 0x10000;
    sel = __dpmi_allocate_ldt_descriptors(1);
    if (sel == (uint16_t)-1)
	return ERROR_NOT_ENOUGH_MEMORY;
    if (__dpmi_set_segment_base_address(sel, base) == -1 ||
	    __dpmi_set_segment_limit(sel, size - 1) == -1 ||
	    __dpmi_set_descriptor_access_rights(sel, AR_DATA16) == -1) {
	__dpmi_free_ldt_descriptor(sel);
	return ERROR_INVALID_PARAMETER;
    }
    call_setw(selp, sel);
    return 0;
}

/* USHORT DosCreateDSAlias(SEL sel, PSEL aselp) */
static uint16_t dos_create_ds_alias(struct call *c)
{
    uint32_t aselp = call_argd(c, 0);
    uint16_t sel = call_argw(c, 4);
    int alias = __dpmi_create_alias_descriptor(sel);

    if (alias == -1)
	return ERROR_INVALID_PARAMETER;
    call_setw(aselp, alias);
    return 0;
}

/* USHORT DosMapRealSeg(USHORT rm_para, ULONG size, PSEL selp) */
static uint16_t dos_map_real_seg(struct call *c)
{
    uint32_t selp = call_argd(c, 0);
    uint32_t size = call_argd(c, 4);
    uint16_t para = call_argw(c, 8);

    return map_seg((uint32_t)para << 4, size, selp);
}

uint32_t ldt_lin;
uint32_t ldt_size;
uint16_t ldt_sel_reg;

/*
 * Origin's wrapper looks for its descriptor table the way a 286 extender
 * would: sgdt for the GDT, sldt for the index of the LDT inside it, then
 * the base and limit out of that entry. dosemu2 tells a client that both
 * tables are at zero, so we hand the wrapper a page of our own with one
 * real entry in it: the LDT, as dosemu2 makes it reachable. Everything it
 * indexes after that lands in the LDT, where dosemu2 picks the writes up
 * on the alias page and applies them itself.
 */
static __dpmi_meminfo fake_gdt;
static uint16_t fake_gdt_sel;

static int fake_gdt_init(void)
{
    uint32_t off;

    if (fake_gdt.size)
	return 0;
    if (!ldt_lin || !ldt_size)
	return -1;
    fake_gdt.size = 0x10000;
    if (__dpmi_allocate_memory(&fake_gdt) == -1) {
	fake_gdt.size = 0;
	return -1;
    }
    fake_gdt_sel = __dpmi_allocate_ldt_descriptors(1);
    if (fake_gdt_sel == (uint16_t)-1 ||
	    __dpmi_set_segment_base_address(fake_gdt_sel,
		fake_gdt.address) == -1 ||
	    __dpmi_set_segment_limit(fake_gdt_sel, fake_gdt.size - 1) == -1 ||
	    __dpmi_set_descriptor_access_rights(fake_gdt_sel, AR_DATA16) == -1)
	return -1;
    for (off = 0; off < fake_gdt.size; off += 4)
	_farpokel(fake_gdt_sel, off, 0);

    off = ldt_sel_reg & 0xfff8;
    _farpokew(fake_gdt_sel, off + 0, (ldt_size - 1) & 0xffff);
    _farpokew(fake_gdt_sel, off + 2, ldt_lin & 0xffff);
    _farpokeb(fake_gdt_sel, off + 4, (ldt_lin >> 16) & 0xff);
    _farpokeb(fake_gdt_sel, off + 5, 0x82);	/* present, LDT */
    _farpokeb(fake_gdt_sel, off + 6, ((ldt_size - 1) >> 16) & 0x0f);
    _farpokeb(fake_gdt_sel, off + 7, (ldt_lin >> 24) & 0xff);
    printf("run286: descriptor table at %#lx, ldt entry %#x -> %#x/%#x\n",
	    (unsigned long)fake_gdt.address, off, ldt_lin, ldt_size);
    return 0;
}

/*
 * USHORT DosMapLinSeg(ULONG lin_addr, ULONG size, PSEL selp)
 *
 * Origin's wrapper runs sgdt and maps whatever it reports, so that it can
 * rewrite the descriptors of its own selectors by hand: it indexes the
 * mapping by (selector & 0xfff8) and writes the base into bytes 2, 4 and
 * 7. Under dosemu2 sgdt reads back as base 0, so what it asks to map is
 * the interrupt vector table and the whole of low memory, and letting it
 * write there takes the host down with it.
 *
 * Its selectors are the ones we handed out, which live in the LDT, so the
 * table it is really indexing is the LDT. Point the mapping there and its
 * writes land in the right place: dosemu2 catches them on the alias page
 * and applies them itself, in msdos_ldt.c.
 */
static uint16_t dos_map_lin_seg(struct call *c)
{
    uint32_t selp = call_argd(c, 0);
    uint32_t size = call_argd(c, 4);
    uint32_t lin = call_argd(c, 8);

    if (lin < 0x1000) {
	if (fake_gdt_init() != 0)
	    return ERROR_NOT_ENOUGH_MEMORY;
	return map_seg(fake_gdt.address + lin, size, selp);
    }
    return map_seg(lin, size, selp);
}

/* USHORT DosGetBIOSSeg(PSEL selp) - the BIOS data area at 0040:0000 */
static uint16_t dos_get_bios_seg(struct call *c)
{
    return map_seg(0x400, 0x100, call_argd(c, 0));
}

/* USHORT DosAllocRealSeg(ULONG size, PUSHORT parap, PSEL selp) */
static uint16_t dos_alloc_real_seg(struct call *c)
{
    uint32_t selp = call_argd(c, 0);
    uint32_t parap = call_argd(c, 4);
    uint32_t size = call_argd(c, 8);
    int sel, para;

    if (size > 0x100000)
	return ERROR_INVALID_PARAMETER;
    para = __dpmi_allocate_dos_memory((size + 15) >> 4, &sel);
    if (para == -1)
	return ERROR_NOT_ENOUGH_MEMORY;
    call_setw(parap, para);
    call_setw(selp, sel);
    return 0;
}

/* DPMI frees a block by handle, PHAPI by linear address, so the handles
 * have to be kept around. */
#define MAX_LINMEM	1024
static __dpmi_meminfo linmem[MAX_LINMEM];

/* USHORT DosAllocLinMem(ULONG size, PULONG lin_addp) */
static uint16_t dos_alloc_lin_mem(struct call *c)
{
    uint32_t linp = call_argd(c, 0);
    uint32_t size = call_argd(c, 4);
    __dpmi_meminfo m = {};
    __dpmi_free_mem_info mi;
    int i;

    for (i = 0; i < MAX_LINMEM; i++) {
	if (!linmem[i].size)
	    break;
    }
    if (i == MAX_LINMEM)
	return ERROR_NOT_ENOUGH_MEMORY;
    /* The programs measure memory by asking for ever smaller blocks until
     * one is granted, so a host that hands out more than it has turns that
     * into a very long loop. Answer from what DPMI says is left. */
    if (__dpmi_get_free_memory_information(&mi) == 0 &&
	    size > mi.largest_available_free_block_in_bytes)
	return ERROR_NOT_ENOUGH_MEMORY;
    m.size = size;
    if (__dpmi_allocate_memory(&m) == -1)
	return ERROR_NOT_ENOUGH_MEMORY;
    linmem[i] = m;
    call_setd(linp, m.address);
    return 0;
}

/* USHORT DosFreeLinMem(ULONG lin_add) */
static uint16_t dos_free_lin_mem(struct call *c)
{
    uint32_t lin = call_argd(c, 0);
    int i;

    for (i = 0; i < MAX_LINMEM; i++) {
	if (linmem[i].size && linmem[i].address == lin)
	    break;
    }
    if (i == MAX_LINMEM || __dpmi_free_memory(linmem[i].handle) == -1)
	return ERROR_INVALID_PARAMETER;
    linmem[i].size = 0;
    return 0;
}

/* USHORT DosIsPharLap(void) - yes, of a sort */
static uint16_t dos_is_pharlap(struct call *c)
{
    return 1;
}

static const struct api_fn phapi[] = {
    { "DOSCREATEDSALIAS",	0, 6,	dos_create_ds_alias },
    { "DOSMAPREALSEG",		0, 10,	dos_map_real_seg },
    { "DOSMAPLINSEG",		0, 12,	dos_map_lin_seg },
    { "DOSGETBIOSSEG",		0, 4,	dos_get_bios_seg },
    { "DOSALLOCREALSEG",	0, 12,	dos_alloc_real_seg },
    { "DOSALLOCLINMEM",		0, 8,	dos_alloc_lin_mem },
    { "DOSFREELINMEM",		0, 4,	dos_free_lin_mem },
    { "DOSISPHARLAP",		0, 0,	dos_is_pharlap },
};

const struct api_fn *phapi_lookup(const char *name)
{
    unsigned i;

    for (i = 0; i < sizeof(phapi) / sizeof(phapi[0]); i++) {
	if (!strcmp(phapi[i].name, name))
	    return &phapi[i];
    }
    return NULL;
}
