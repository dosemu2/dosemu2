BATCHFILE = """\
c:\\%s
rem end
"""


def memory_xms_pages(self):

    self.mkfile("testit.bat", BATCHFILE % 'xmspage', newline="\r\n")

    self.mkexe_with_djgpp("xmspage", r"""
#include <stdio.h>
#include <stdlib.h>
#include <dpmi.h>

static __dpmi_paddr xms;

/* fn 8: EAX = largest free block in KB, EDX = total free in KB */
static int xms_query(unsigned *largest, unsigned *total)
{
    unsigned ax, dx, bx;

    asm volatile("lcall *%[x]\n"
        : "=a"(ax), "=d"(dx), "=b"(bx)
        : [x]"m"(xms), "a"(0x0800)
        : "cc", "memory");
    *largest = ax & 0xffff;
    *total = dx & 0xffff;
    return 1;
}

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

/* fn 0xc: DX = handle -> DX:BX = address */
static int xms_lock(unsigned handle)
{
    int rc, err;
    unsigned dx;

    asm volatile("lcall *%[x]\n"
        : "=a"(rc), "=b"(err), "=d"(dx)
        : [x]"m"(xms), "a"(0x0c00), "d"(handle)
        : "cc", "memory");
    if (!(rc & 0xffff))
        printf("lock of handle %u refused, err %x\n", handle, err & 0xff);
    return rc & 0xffff;
}

int main()
{
    unsigned largest, total, handle;
    __dpmi_regs r = {};
    int i;

    r.x.ax = 0x4300;
    __dpmi_int(0x2f, &r);
    if (r.h.al != 0x80) {
        printf("FAILURE: XMS unsupported\n");
        exit(1);
    }
    asm volatile(
        "int $0x2f\n"
        "mov %%es, %0\n"
        : "=r"(xms.selector), "=b"(xms.offset32)
        : "a"(0x4310));

    /* An EMB costs whole pages.  Ask for sizes that are not a multiple of
     * a page, so that every block wastes part of its last one, and check
     * that XMS never hands out a block it cannot then lock. */
    for (i = 0; i < 64; i++) {
        unsigned kb;

        xms_query(&largest, &total);
        printf("free: largest %u KB, total %u KB\n", largest, total);
        if (!largest)
            break;
        kb = largest > 8 ? largest - 3 : largest;
        if (!xms_alloc(kb, &handle))
            break;
        if (!xms_lock(handle)) {
            printf("FAILURE: locking an EMB that XMS just handed out\n");
            exit(1);
        }
    }
    printf("Test OK\n");
    return 0;
}
""")

    results = self.runDosemu("testit.bat")

    self.assertNotIn("FAILURE:", results)
    self.assertIn("Test OK", results)
