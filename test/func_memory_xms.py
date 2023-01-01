BATCHFILE = """\
c:\\%s
rem end
"""

CONFIG = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""


def memory_xms(self):

# Note: Not sure if I need this
    if 'FDPP' in self.version:
        self.mkfile("userhook.sys", """DOS=HIGH\n""", newline="\r\n")

    self.mkfile("testit.bat", BATCHFILE % 'xmstest', newline="\r\n")

    self.mkexe_with_djgpp("xmstest", r"""
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dpmi.h>
#include <sys/nearptr.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

struct __attribute__ ((__packed__)) EMM {
   unsigned int Length;
   unsigned short SourceHandle;
   unsigned int SourceOffset;
   unsigned short DestHandle;
   unsigned int DestOffset;
};

static __dpmi_paddr xms;
static struct EMM e;

static int call_xms(unsigned fn, unsigned arg, unsigned *ret)
{
    int rc, err;

    asm volatile("lcall *%[x]\n"
        : "=a"(rc), "=d"(*ret), "=b"(err)
        : [x]"m"(xms), "a"(fn << 8), "d"(arg)
        : "cc", "memory");
    if (!rc)
        printf("xms error %x\n", err & 0xff);
    return rc;
}

static int call_xms0(unsigned fn, unsigned arg)
{
    int rc, err;

    asm volatile("lcall *%[x]\n"
        : "=a"(rc), "=b"(err)
        : [x]"m"(xms), "a"(fn << 8), "d"(arg)
        : "cc", "memory");
    if (!rc)
        printf("xms error %x\n", err & 0xff);
    return rc;
}

static int call_xms_dxbx(unsigned fn, unsigned arg, unsigned *ret)
{
    int rc;
    unsigned dx, bx;

    asm volatile("lcall *%[x]\n"
        : "=a"(rc), "=d"(dx), "=b"(bx)
        : [x]"m"(xms), "a"(fn << 8), "d"(arg)
        : "cc", "memory");
    *ret = (dx << 16) | bx;
    return rc;
}

static int call_xms_dssi(unsigned fn, void *ptr)
{
    int rc, err;

    asm volatile("lcall *%[x]\n"
        : "=a"(rc), "=b"(err)
        : [x]"m"(xms), "a"(fn << 8), "S"(ptr)
        : "cc", "memory");
    if (!rc)
        printf("xms error %x\n", err & 0xff);
    return rc;
}

int main()
{
    unsigned handle;
    unsigned addr;
    char *ptr;
    char buf[PAGE_SIZE];
    int err;
    __dpmi_meminfo dm = {};
    __dpmi_regs r = {};
    static const char *str1 = "request";
    static const char *str2 = "response";
    const int offs1 = 35;
    const int offs2 = 174;

    /* check xms */
    r.x.ax = 0x4300;
    __dpmi_int(0x2f, &r);
    if (r.h.al != 0x80) {
        printf("FAILURE: XMS unsupported\n");
        exit(1);
    }
    printf("XMS present\n");

    /* get entry */
    asm volatile(
        "int $0x2f\n"
        "mov %%es, %0\n"
        : "=r"(xms.selector), "=b"(xms.offset32)
        : "a"(0x4310));
    printf("XMS at %x:%lx\n", xms.selector, xms.offset32);
    /* alloc one page emb */
    if (!call_xms(9, 4, &handle)) {
        printf("FAILURE: EMB alloc failed\n");
        exit(1);
    }
    printf("XMS alloc ok\n");

    /* put test string there - length must be even */
    e.Length = (strlen(str1) + 1 + 1) & ~1;
    e.SourceHandle = 0;
    e.SourceOffset = (uintptr_t)str1;
    e.DestHandle = handle;
    e.DestOffset = offs1;
    if (!call_xms_dssi(0xb, &e)) {
        printf("FAILURE: XMS move failed\n");
        exit(1);
    }
    printf("XMS move ok\n");

    /* map EMB */
    if (!call_xms_dxbx(0xc, handle, &addr)) {
        printf("FAILURE: XMS map failed\n");
        exit(1);
    }
    printf("XMS map ok\n");

    dm.address = addr;
    dm.size = PAGE_SIZE;
    err = __dpmi_physical_address_mapping(&dm);
    if (err) {
        printf("FAILURE: failed to get XMS page\n");
        exit(1);
    }
    printf("DPMI map ok, addr=0x%08lx\n", dm.address);
    /* enable 4G wrap-around */
    if (!__djgpp_nearptr_enable()) {
        printf("FAILURE: 4G wrap-around failed\n");
        exit(1);
    }
    ptr = (char *)(dm.address + __djgpp_conventional_base);
    printf("DPMI wrap-around ok, ptr=%p, base=%08x\n", ptr,
	    -__djgpp_conventional_base);
    printf("Got this string: %s\n", ptr + offs1);
    if (strcmp(str1, ptr + offs1) != 0)
        printf("FAILURE: Test 1 FAILURE\n");
    else
        printf("Test 1 OK\n");

    strcpy(ptr + offs2, str2);
    /* disable wrap */
    __djgpp_nearptr_disable();
    /* unmap */
    err =  __dpmi_free_physical_address_mapping(&dm);
    if (err) {
        printf("FAILURE: failed to put XMS page\n");
        exit(1);
    }
    if (!call_xms0(0xd, handle)) {
        printf("FAILURE: XMS unmap failed\n");
        exit(1);
    }
    printf("XMS unmap ok\n");
    /* copy back our page */
    e.Length = PAGE_SIZE;
    e.SourceHandle = handle;
    e.SourceOffset = offs2;
    e.DestHandle = 0;
    e.DestOffset = (uintptr_t)buf;
    if (!call_xms_dssi(0xb, &e)) {
        printf("XMS bad move2 failed, OK\n");
//        exit(1);
    }
    e.Length -= offs2;
    if (!call_xms_dssi(0xb, &e)) {
        printf("FAILURE: XMS move2 failed\n");
        exit(1);
    }
    printf("XMS move2 ok\n");
    printf("Got back this string: %s\n", buf);
    if (strcmp(str2, buf) != 0)
        printf("FAILURE: Test 2 FAILURE\n");
    else
        printf("Test 2 OK\n");
    return 0;
}
""")

    results = self.runDosemu("testit.bat", config=CONFIG)

    self.assertIn("Test 1 OK", results)
    self.assertIn("Test 2 OK", results)
    self.assertNotIn("FAILURE:", results)
