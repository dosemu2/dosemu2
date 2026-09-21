/*
 * Sweep of the encodings that the emulator's opcode tables kept losing:
 * the two byte 0f map and the x87 escapes. The same binary runs on the
 * host processor and under every cpuemu backend, so a case label that
 * went missing shows up as a difference against the reference file
 * rather than as a silent wrong answer in a game.
 */

/* 0f ba, the immediate forms of the bit instructions. The register
 * forms (0f a3/ab/b3/bb) are covered by test_bt() and friends. */
#define TEST_BTI(op, size, val, bit)\
{\
    long res, cf;\
    res = val;\
    asm(#op " $" #bit ", %" size "0 ; setc %b1"\
        : "+r" (res), "=q" (cf));\
    printf("%-8s A=" FMTLX " bit=%-2d R=" FMTLX " CF=%ld\n",\
           #op " $" #bit, (long)(val), (int)(bit), res, cf & 1);\
}

/* 0f a4 and 0f ac, the immediate forms of the double shifts. The cl
 * forms (0f a5/ad) are covered by test_shld()/test_shrd(). */
#define TEST_DSHIFTI(op, size, a, b, n)\
{\
    long res = a;\
    asm(#op " $" #n ", %" size "1, %" size "0"\
        : "+r" (res) : "r" ((long)(b)));\
    printf("%-8s A=" FMTLX " B=" FMTLX " n=%-2d R=" FMTLX "\n",\
           #op " $" #n, (long)(a), (long)(b), (int)(n), res);\
}

/* 0f b6/b7/be/bf */
#define TEST_EXTEND(op, ssize, dsize, val)\
{\
    long res;\
    asm(#op " %" ssize "1, %" dsize "0" : "=r" (res) : "q" ((long)(val)));\
    printf("%-8s A=" FMTLX " R=" FMTLX "\n", #op, (long)(val), res);\
}

/* 0f c8..cf, one opcode per register. %esp and %ebp are left out: the
 * operands of the asm are addressed through them. */
#define TEST_BSWAP(reg, val)\
{\
    long res = val;\
    asm("bswap %0" : "+" reg (res));\
    printf("bswap %-4s A=" FMTLX " R=" FMTLX "\n", reg, (long)(val), res);\
}

static void test_bitops_imm(void)
{
    TEST_BTI(btw, "w", 0x12345678, 3);
    TEST_BTI(btw, "w", 0x12345678, 12);
    TEST_BTI(btsw, "w", 0x12345678, 3);
    TEST_BTI(btrw, "w", 0x12345678, 12);
    TEST_BTI(btcw, "w", 0x12345678, 12);
    TEST_BTI(btl, "k", 0x12345678, 3);
    TEST_BTI(btl, "k", 0x12345678, 28);
    TEST_BTI(btsl, "k", 0x12345678, 3);
    TEST_BTI(btrl, "k", 0x12345678, 28);
    TEST_BTI(btcl, "k", 0x12345678, 28);
}

static void test_dshift_imm(void)
{
    TEST_DSHIFTI(shldw, "w", 0x12345678, 0x0f0f0f0f, 5);
    TEST_DSHIFTI(shrdw, "w", 0x12345678, 0x0f0f0f0f, 5);
    TEST_DSHIFTI(shldl, "k", 0x12345678, 0x0f0f0f0f, 5);
    TEST_DSHIFTI(shrdl, "k", 0x12345678, 0x0f0f0f0f, 5);
    TEST_DSHIFTI(shldl, "k", 0x12345678, 0x0f0f0f0f, 31);
    TEST_DSHIFTI(shrdl, "k", 0x12345678, 0x0f0f0f0f, 31);
}

static void test_extend(void)
{
    TEST_EXTEND(movzbl, "b", "k", 0x80);
    TEST_EXTEND(movsbl, "b", "k", 0x80);
    TEST_EXTEND(movzbw, "b", "w", 0x80);
    TEST_EXTEND(movsbw, "b", "w", 0x80);
    TEST_EXTEND(movzwl, "w", "k", 0x8000);
    TEST_EXTEND(movswl, "w", "k", 0x8000);
}

static void test_bswap(void)
{
    TEST_BSWAP("a", 0x12345678);
    TEST_BSWAP("b", 0x12345678);
    TEST_BSWAP("c", 0x12345678);
    TEST_BSWAP("d", 0x12345678);
    TEST_BSWAP("S", 0x12345678);
    TEST_BSWAP("D", 0x12345678);
}

/* 0f 1f, the multi byte nop, in every length the encoding allows.
 * Written out as bytes because the assembler is free to pick a
 * different padding instruction for a bare "nopl". */
static void test_long_nop(void)
{
    asm volatile(
        ".byte 0x0f,0x1f,0x00\n\t"
        ".byte 0x0f,0x1f,0x40,0x00\n\t"
        ".byte 0x0f,0x1f,0x44,0x00,0x00\n\t"
        ".byte 0x66,0x0f,0x1f,0x44,0x00,0x00\n\t"
        ".byte 0x0f,0x1f,0x80,0x00,0x00,0x00,0x00\n\t"
        ".byte 0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00\n\t"
        ".byte 0x66,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00\n\t"
        ::: "memory");
    printf("long nop: all seven lengths executed\n");
}

/* x87. The register forms of d8, dc and de, which is where a lost case
 * label sent FDIVR down the FDIV arm. st(0) holds a, st(1) holds b. */
#define TEST_FARITH_D8(mn, a, b)\
{\
    double res;\
    long fpus;\
    fpu_clear_exceptions();\
    asm volatile("fldl %3\n\t"\
                 "fldl %2\n\t"\
                 mn " %%st(1), %%st\n\t"\
                 "fstsw %%ax\n\t"\
                 "fstpl %0\n\t"\
                 "fstp %%st(0)\n\t"\
                 : "=m" (res), "=a" (fpus) : "m" (a), "m" (b) : "memory");\
    printf("d8 %-6s a=%f b=%f -> %f fpus=%04lx\n", mn, a, b, res,\
           fpus & (0x4500 | FPUS_EMASK));\
}

#define TEST_FARITH_DC(mn, a, b)\
{\
    double res;\
    long fpus;\
    fpu_clear_exceptions();\
    asm volatile("fldl %3\n\t"\
                 "fldl %2\n\t"\
                 mn " %%st, %%st(1)\n\t"\
                 "fstsw %%ax\n\t"\
                 "fstp %%st(0)\n\t"\
                 "fstpl %0\n\t"\
                 : "=m" (res), "=a" (fpus) : "m" (a), "m" (b) : "memory");\
    printf("dc %-6s a=%f b=%f -> %f fpus=%04lx\n", mn, a, b, res,\
           fpus & (0x4500 | FPUS_EMASK));\
}

#define TEST_FARITH_DE(mn, a, b)\
{\
    double res;\
    long fpus;\
    fpu_clear_exceptions();\
    asm volatile("fldl %3\n\t"\
                 "fldl %2\n\t"\
                 mn " %%st, %%st(1)\n\t"\
                 "fstsw %%ax\n\t"\
                 "fstpl %0\n\t"\
                 : "=m" (res), "=a" (fpus) : "m" (a), "m" (b) : "memory");\
    printf("de %-6s a=%f b=%f -> %f fpus=%04lx\n", mn, a, b, res,\
           fpus & (0x4500 | FPUS_EMASK));\
}

/* da and de, the integer memory forms */
#define TEST_FIARITH(mn, sfx, type, a, i)\
{\
    double res;\
    long fpus;\
    type iv = i;\
    fpu_clear_exceptions();\
    asm volatile("fldl %2\n\t"\
                 mn sfx " %3\n\t"\
                 "fstsw %%ax\n\t"\
                 "fstpl %0\n\t"\
                 : "=m" (res), "=a" (fpus) : "m" (a), "m" (iv) : "memory");\
    printf("%-8s a=%f i=%ld -> %f fpus=%04lx\n", mn sfx, a, (long)(i), res,\
           fpus & (0x4500 | FPUS_EMASK));\
}

static void test_farith_regs(double a, double b)
{
    TEST_FARITH_D8("fadd", a, b);
    TEST_FARITH_D8("fmul", a, b);
    TEST_FARITH_D8("fsub", a, b);
    TEST_FARITH_D8("fsubr", a, b);
    TEST_FARITH_D8("fdiv", a, b);
    TEST_FARITH_D8("fdivr", a, b);

    TEST_FARITH_DC("fadd", a, b);
    TEST_FARITH_DC("fmul", a, b);
    TEST_FARITH_DC("fsub", a, b);
    TEST_FARITH_DC("fsubr", a, b);
    TEST_FARITH_DC("fdiv", a, b);
    TEST_FARITH_DC("fdivr", a, b);

    TEST_FARITH_DE("faddp", a, b);
    TEST_FARITH_DE("fmulp", a, b);
    TEST_FARITH_DE("fsubp", a, b);
    TEST_FARITH_DE("fsubrp", a, b);
    TEST_FARITH_DE("fdivp", a, b);
    TEST_FARITH_DE("fdivrp", a, b);
}

static void test_farith_int(double a)
{
    TEST_FIARITH("fiadd", "l", long, a, 7);
    TEST_FIARITH("fimul", "l", long, a, 7);
    TEST_FIARITH("fisub", "l", long, a, 7);
    TEST_FIARITH("fisubr", "l", long, a, 7);
    TEST_FIARITH("fidiv", "l", long, a, 7);
    TEST_FIARITH("fidivr", "l", long, a, 7);

    TEST_FIARITH("fiadd", "s", short, a, -3);
    TEST_FIARITH("fimul", "s", short, a, -3);
    TEST_FIARITH("fisub", "s", short, a, -3);
    TEST_FIARITH("fisubr", "s", short, a, -3);
    TEST_FIARITH("fidiv", "s", short, a, -3);
    TEST_FIARITH("fidivr", "s", short, a, -3);
}

static void test_optable(void)
{
    test_bitops_imm();
    test_dshift_imm();
    test_extend();
    test_bswap();
    test_long_nop();
    test_farith_regs(3.0, 2.0);
    test_farith_regs(-1.5, 0.25);
    test_farith_int(3.0);
    test_farith_int(-1.5);
}
