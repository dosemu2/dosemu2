/* Test instructions loading floating-point constants.  */

#include <stdint.h>
#include <stdio.h>

volatile long double ld_res;

int main(void)
{
    short cw;
    int ret = 0;

    /* Round to nearest.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x000;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldl2t" : "=t" (ld_res));
    if (ld_res != 0x3.5269e12f346e2bf8p+0L) {
        printf("FAIL: fldl2t N %Lf\n", ld_res);
        ret = 1;
    }
    /* Round downward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x400;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldl2t" : "=t" (ld_res));
    if (ld_res != 0x3.5269e12f346e2bf8p+0L) {
        printf("FAIL: fldl2t D %Lf\n", ld_res);
        ret = 1;
    }
    /* Round toward zero.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0xc00;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldl2t" : "=t" (ld_res));
    if (ld_res != 0x3.5269e12f346e2bf8p+0L) {
        printf("FAIL: fldl2t Z %Lf\n", ld_res);
        ret = 1;
    }
    /* Round upward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x800;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldl2t" : "=t" (ld_res));
    if (ld_res != 0x3.5269e12f346e2bfcp+0L) {
        printf("FAIL: fldl2t U %Lf\n", ld_res);
        ret = 1;
    }

    /* Round to nearest.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x000;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldl2e" : "=t" (ld_res));
    if (ld_res != 0x1.71547652b82fe178p+0L) {
        printf("FAIL: fldl2e N %Lf\n", ld_res);
        ret = 1;
    }
    /* Round downward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x400;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldl2e" : "=t" (ld_res));
    if (ld_res != 0x1.71547652b82fe176p+0L) {
        printf("FAIL: fldl2e D, %Lf\n", ld_res);
        ret = 1;
    }
    /* Round toward zero.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0xc00;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldl2e" : "=t" (ld_res));
    if (ld_res != 0x1.71547652b82fe176p+0L) {
        printf("FAIL: fldl2e Z %Lf\n", ld_res);
        ret = 1;
    }
    /* Round upward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x800;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldl2e" : "=t" (ld_res));
    if (ld_res != 0x1.71547652b82fe178p+0L) {
        printf("FAIL: fldl2e U %Lf\n", ld_res);
        ret = 1;
    }

    /* Round to nearest.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x000;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldpi" : "=t" (ld_res));
    if (ld_res != 0x3.243f6a8885a308d4p+0L) {
        printf("FAIL: fldpi N %Lf\n", ld_res);
        ret = 1;
    }
    /* Round downward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x400;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldpi" : "=t" (ld_res));
    if (ld_res != 0x3.243f6a8885a308dp+0L) {
        printf("FAIL: fldpi D %Lf\n", ld_res);
        ret = 1;
    }
    /* Round toward zero.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0xc00;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldpi" : "=t" (ld_res));
    if (ld_res != 0x3.243f6a8885a308dp+0L) {
        printf("FAIL: fldpi Z %Lf\n", ld_res);
        ret = 1;
    }
    /* Round upward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x800;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldpi" : "=t" (ld_res));
    if (ld_res != 0x3.243f6a8885a308d4p+0L) {
        printf("FAIL: fldpi U %Lf\n", ld_res);
        ret = 1;
    }

    /* Round to nearest.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x000;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldlg2" : "=t" (ld_res));
    if (ld_res != 0x4.d104d427de7fbcc8p-4L) {
        printf("FAIL: fldlg2 N %Lf\n", ld_res);
        ret = 1;
    }
    /* Round downward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x400;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldlg2" : "=t" (ld_res));
    if (ld_res != 0x4.d104d427de7fbccp-4L) {
        printf("FAIL: fldlg2 D %Lf\n", ld_res);
        ret = 1;
    }
    /* Round toward zero.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0xc00;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldlg2" : "=t" (ld_res));
    if (ld_res != 0x4.d104d427de7fbccp-4L) {
        printf("FAIL: fldlg2 Z %Lf\n", ld_res);
        ret = 1;
    }
    /* Round upward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x800;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldlg2" : "=t" (ld_res));
    if (ld_res != 0x4.d104d427de7fbcc8p-4L) {
        printf("FAIL: fldlg2 U %Lf\n", ld_res);
        ret = 1;
    }

    /* Round to nearest.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x000;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldln2" : "=t" (ld_res));
    if (ld_res != 0xb.17217f7d1cf79acp-4L) {
        printf("FAIL: fldln2 N %Lf\n", ld_res);
        ret = 1;
    }
    /* Round downward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x400;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldln2" : "=t" (ld_res));
    if (ld_res != 0xb.17217f7d1cf79abp-4L) {
        printf("FAIL: fldln2 D %Lf\n", ld_res);
        ret = 1;
    }
    /* Round toward zero.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0xc00;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldln2" : "=t" (ld_res));
    if (ld_res != 0xb.17217f7d1cf79abp-4L) {
        printf("FAIL: fldln2 Z %Lf\n", ld_res);
        ret = 1;
    }
    /* Round upward.  */
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    cw = (cw & ~0xc00) | 0x800;
    __asm__ volatile ("fldcw %0" : : "m" (cw));
    __asm__ volatile ("fldln2" : "=t" (ld_res));
    if (ld_res != 0xb.17217f7d1cf79acp-4L) {
        printf("FAIL: fldln2 U %Lf\n", ld_res);
        ret = 1;
    }

    if (ret == 0)
        printf("PASS: all tests\n");

    return ret;
}
