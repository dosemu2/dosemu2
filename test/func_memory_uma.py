BATCHFILE = """\
c:\\%s
rem end
"""

CONFIG = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""


def memory_uma_strategy(self):

    self.mkfile("testit.bat", BATCHFILE % 'umastrat', newline="\r\n")

    self.mkexe_with_djgpp("umastrat", r"""\
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dpmi.h>
#include <sys/movedata.h>
#include <sys/segments.h>
#include <stubinfo.h>

typedef unsigned FAR_PTR;

struct PSP {
    unsigned short	opint20;	/* 0x00 */
    unsigned short	memend_frame;	/* 0x02 */
    unsigned char	dos_reserved4;	/* 0x04 */
    unsigned char	cpm_function_entry[0xa-0x5];	/* 0x05 */
    FAR_PTR		int22_copy;	/* 0x0a */
    FAR_PTR		int23_copy;	/* 0x0e */
    FAR_PTR		int24_copy;	/* 0x12 */
    unsigned short	parent_psp;	/* 0x16 */
    unsigned char	file_handles[20];	/* 0x18 */
    unsigned short	envir_frame;	/* 0x2c */
    FAR_PTR		system_stack;	/* 0x2e */
    unsigned short	max_open_files;	/* 0x32 */
    FAR_PTR		file_handles_ptr;	/* 0x34 */
    unsigned char	dos_reserved38[0x50-0x38];	/* 0x38 */
    unsigned char	high_language_dos_call[0x53-0x50];	/* 0x50 */
    unsigned char	dos_reserved53[0x5c-0x53];	/* 0x53 */
    unsigned char	FCB1[0x6c-0x5c];	/* 0x5c */
    unsigned char	FCB2[0x80-0x6c];	/* 0x6c */
    unsigned char	cmdline_len;		/* 0x80 */
    char		cmdline[0x100-0x81];	/* 0x81 */
} __attribute__((packed));
static struct PSP psp;  // make it static to be %ds-addresable

int main()
{
    __dpmi_regs r = {};
    int err;
    unsigned long psp_addr;
    unsigned short psp_seg;
    int alloced, want_alloc;
    int ret = 0;

    err = __dpmi_get_segment_base_address(_stubinfo->psp_selector, &psp_addr);
    if (err || (psp_addr & 0xf) || psp_addr >= 0x110000) {
        printf("FAIL: get psp failed\n");
        exit(1);
    }
    psp_seg = psp_addr >> 4;
    printf("INFO: PSP at %x\n", psp_seg);
    if (psp_seg > 0xa000) {
        printf("FAIL: test loaded high, BAD. Use SHELL_LOADHIGH_DEFAULT=0 dosemu arg\n");
        exit(1);
    }
    movedata(_stubinfo->psp_selector, 0, _my_ds(), (uintptr_t)&psp, sizeof(psp));
    printf("INFO: mem end: %x\n", psp.memend_frame);
    if (psp.memend_frame > 0xa000) {
        printf("FAIL: too many memory allocated, FAILURE\n");
        exit(1);
    }

    r.h.ah = 0x48;
    r.x.bx = 1;
    __dpmi_int(0x21, &r);               /* allocate one-paragraph block */
    if ((r.x.flags & 1)) {
        printf("FAIL: alloc failed\n");
        exit(1);
    }
    printf("INFO: alloc at %x\n", r.x.ax);
    if (r.x.ax > 0xa000) {
        printf("FAIL: initial alloc at UMB, FAILURE!\n");
        exit(1);
    }

    r.x.es = r.x.ax;
    r.h.ah = 0x49;
    __dpmi_int(0x21, &r);               /* free block */
    if ((r.x.flags & 1)) {
        printf("FAIL: free failed\n");
        exit(1);
    }

    /* try UMB alloc */
    r.x.ax = 0x5801;
    r.x.bx = 0x80;
    __dpmi_int(0x21, &r);
#if 1
    r.x.ax = 0x5803;
    r.x.bx = 1;
    __dpmi_int(0x21, &r);
#endif
    r.h.ah = 0x48;
    r.x.bx = 1;
    __dpmi_int(0x21, &r);               /* allocate one-paragraph block */
    if ((r.x.flags & 1)) {
        printf("FAIL: alloc failed\n");
        exit(1);
    }
    printf("INFO: alloc at %x\n", r.x.ax);
    if (r.x.ax < 0xa000) {
        printf("FAIL: alloc at UMB FAILED\n");
        ret++;
//        exit(1);
    } else
        printf("INFO: alloc at UMB OK\n");

    r.x.es = r.x.ax;
    r.h.ah = 0x49;
    __dpmi_int(0x21, &r);               /* free block */
    if ((r.x.flags & 1)) {
        printf("FAIL: free failed\n");
        ret++;
//        exit(1);
    }

    /* try large low alloc */
    r.x.ax = 0x5801;
    r.x.bx = 0;
    __dpmi_int(0x21, &r);
    want_alloc = 0xe000;
    alloced = 0;
    while (alloced < want_alloc) {
        r.h.ah = 0x48;
        r.x.bx = 0x100;
        __dpmi_int(0x21, &r);
        if ((r.x.flags & 1))
            break;
        alloced += 0x100;
    }
    printf("INFO: allocated %i of %i\n", alloced << 4, want_alloc << 4);
    if (alloced < 0xa000) {
        printf("FAIL: UMB not allocated, FAILURE\n");
        ret++;
    }

    if (ret == 0)
        printf("PASS: Done\n");
    else
        printf("FAIL: One or more non-fatal errors occurred\n");

    return ret;
}
""")

    results = self.runDosemu("testit.bat", config=CONFIG)

    self.assertIn("PASS:", results)
    self.assertNotIn("FAIL:", results)
