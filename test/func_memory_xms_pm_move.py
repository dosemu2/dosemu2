BATCHFILE = """\
c:\\%s
rem end
"""


def memory_xms_pm_move(self):

    self.mkfile("testit.bat", BATCHFILE % 'xmspmmov', newline="\r\n")

    self.mkexe_with_djgpp("xmspmmov", r"""
#include <stdio.h>
#include <string.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/movedata.h>

static __dpmi_paddr xms;

struct EMM {
    unsigned long Length;
    unsigned short SourceHandle;
    unsigned long SourceOffset;
    unsigned short DestHandle;
    unsigned long DestOffset;
} __attribute__((packed));

#define LEN 32

/* fn 9: DX = size in KB -> DX = handle */
static int xms_alloc(unsigned kb, unsigned *handle)
{
    int rc, err;
    unsigned dx;

    asm volatile("lcall *%[x]\n"
        : "=a"(rc), "=d"(dx), "=b"(err)
        : [x]"m"(xms), "a"(0x0900), "d"(kb)
        : "cc", "memory");
    *handle = dx & 0xffff;
    if (!(rc & 0xffff))
        printf("alloc of %u KB refused, err %x\n", kb, err & 0xff);
    return rc & 0xffff;
}

/* fn 0xa: DX = handle */
static void xms_free(unsigned handle)
{
    int rc;

    asm volatile("lcall *%[x]\n"
        : "=a"(rc)
        : [x]"m"(xms), "a"(0x0a00), "d"(handle)
        : "cc", "memory");
}

/* fn 0xb: DS:SI -> move structure */
static int xms_move(struct EMM *e, unsigned *err)
{
    int rc, bx;

    asm volatile("lcall *%[x]\n"
        : "=a"(rc), "=b"(bx)
        : [x]"m"(xms), "a"(0x0b00), "S"(e)
        : "cc", "memory");
    *err = bx & 0xff;
    return rc & 0xffff;
}

/* A block move with handle 0 names conventional memory by a real-mode
 * segment:offset pair, whatever entry point the call went through.  Fill
 * one DOS block, push it through an EMB into a second one, and see that
 * the bytes come back. */
int main(void)
{
    unsigned char pattern[LEN], back[LEN];
    struct EMM e;
    unsigned handle, err;
    int seg_a, seg_b, sel_a, sel_b;
    __dpmi_regs r = {};
    int i;

    r.x.ax = 0x4300;
    __dpmi_int(0x2f, &r);
    if (r.h.al != 0x80) {
        printf("FAILURE: XMS unsupported\n");
        return 1;
    }
    asm volatile(
        "int $0x2f\n"
        "mov %%es, %0\n"
        : "=r"(xms.selector), "=b"(xms.offset32)
        : "a"(0x4310));

    seg_a = __dpmi_allocate_dos_memory((LEN + 15) / 16, &sel_a);
    seg_b = __dpmi_allocate_dos_memory((LEN + 15) / 16, &sel_b);
    if (seg_a == -1 || seg_b == -1) {
        printf("FAILURE: no DOS memory\n");
        return 1;
    }
    for (i = 0; i < LEN; i++)
        pattern[i] = 0x5a ^ i;
    dosmemput(pattern, LEN, seg_a * 16);
    memset(back, 0, LEN);
    dosmemput(back, LEN, seg_b * 16);

    if (!xms_alloc(1, &handle)) {
        printf("FAILURE: cannot allocate an EMB\n");
        return 1;
    }

    /* conventional -> EMB */
    memset(&e, 0, sizeof e);
    e.Length = LEN;
    e.SourceHandle = 0;
    e.SourceOffset = (unsigned long)seg_a << 16;
    e.DestHandle = handle;
    e.DestOffset = 0;
    if (!xms_move(&e, &err)) {
        printf("FAILURE: move to EMB failed, err %x\n", err);
        return 1;
    }

    /* EMB -> conventional */
    memset(&e, 0, sizeof e);
    e.Length = LEN;
    e.SourceHandle = handle;
    e.SourceOffset = 0;
    e.DestHandle = 0;
    e.DestOffset = (unsigned long)seg_b << 16;
    if (!xms_move(&e, &err)) {
        printf("FAILURE: move from EMB failed, err %x\n", err);
        return 1;
    }

    dosmemget(seg_b * 16, LEN, back);
    if (memcmp(pattern, back, LEN) != 0) {
        printf("FAILURE: EMB round trip changed the data\n");
        for (i = 0; i < LEN; i++)
            printf(" %02x/%02x", pattern[i], back[i]);
        printf("\n");
        return 1;
    }

    /* conventional -> conventional, both handles 0 */
    memset(back, 0, LEN);
    dosmemput(back, LEN, seg_b * 16);
    memset(&e, 0, sizeof e);
    e.Length = LEN;
    e.SourceHandle = 0;
    e.SourceOffset = (unsigned long)seg_a << 16;
    e.DestHandle = 0;
    e.DestOffset = (unsigned long)seg_b << 16;
    if (!xms_move(&e, &err)) {
        printf("FAILURE: move between DOS blocks failed, err %x\n", err);
        return 1;
    }
    dosmemget(seg_b * 16, LEN, back);
    if (memcmp(pattern, back, LEN) != 0) {
        printf("FAILURE: move between DOS blocks changed the data\n");
        for (i = 0; i < LEN; i++)
            printf(" %02x/%02x", pattern[i], back[i]);
        printf("\n");
        return 1;
    }

    xms_free(handle);
    __dpmi_free_dos_memory(sel_a);
    __dpmi_free_dos_memory(sel_b);
    printf("Test OK\n");
    return 0;
}
""")

    results = self.runDosemu("testit.bat")

    self.assertNotIn("FAILURE:", results)
    self.assertIn("Test OK", results)
