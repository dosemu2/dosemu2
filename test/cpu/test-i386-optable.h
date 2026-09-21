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

/* 0f 00 and 0f 01, the descriptor table group. The values these store are
 * host state - gdt base, ldt selector, task register, cr0 - so printing
 * them would differ between a native run and a run under dosemu for
 * legitimate reasons. What the emulator kept getting wrong is not the
 * value but the store itself: #3022 was the memory form of smsw being
 * skipped outright, and #3066/#3067 were forms that reached the handler
 * but never wrote their operand. So these two macros print the shape of
 * the write and nothing else, which is the same everywhere the
 * instruction is implemented correctly.
 *
 * The memory form runs twice, into a buffer of 0x00 and a buffer of 0xff.
 * A byte the instruction wrote holds the same value in both, a byte it
 * left alone still holds the fill, so 'w' marks written and '.' marks
 * untouched with no dependence on what was stored. Two bytes past the
 * expected end are printed too, so a store of the wrong length shows up
 * as a trailing 'w' rather than as silence. */
#define TEST_DT_MEM(mn, nb)\
{\
    static unsigned char b0[16], b1[16];\
    char mask[16];\
    int i;\
    memset(b0, 0x00, sizeof(b0));\
    memset(b1, 0xff, sizeof(b1));\
    asm volatile(mn " (%0)" : : "r" (b0) : "memory");\
    asm volatile(mn " (%0)" : : "r" (b1) : "memory");\
    for (i = 0; i < (nb) + 2; i++)\
        mask[i] = (b0[i] == b1[i]) ? 'w' : '.';\
    mask[i] = '\0';\
    printf("%-8s mem=%s\n", mn, mask);\
}

/* The register form with a 32 bit operand. smsw copies the whole of cr0
 * into a 32 bit destination, and sldt and str zero extend their selector,
 * so on a correct implementation the top half is written in every case.
 * Running it from two different starting values tells written from
 * preserved without printing the value itself. */
#define TEST_DT_REG(mn)\
{\
    unsigned int r0 = 0x00000000, r1 = 0xffff0000;\
    asm volatile(mn " %0" : "+r" (r0));\
    asm volatile(mn " %0" : "+r" (r1));\
    printf("%-8s reg high16=%s low16=%s\n", mn,\
           ((r0 >> 16) == (r1 >> 16)) ? "written" : "kept",\
           ((r0 & 0xffff) == (r1 & 0xffff)) ? "written" : "kept");\
}

static void test_dtables(void)
{
    TEST_DT_MEM("sgdtl", 6);
    TEST_DT_MEM("sidtl", 6);
    TEST_DT_MEM("sldtw", 2);
    TEST_DT_MEM("strw", 2);
    TEST_DT_MEM("smsww", 2);

    TEST_DT_REG("smsw");
    TEST_DT_REG("sldt");
    TEST_DT_REG("str");
}

/* 0f 31 and 0f a2. Both hand back numbers that belong to the machine -
 * a cycle counter and a feature word - so the value cannot go in the
 * reference file any more than a gdt base could. What can go in is
 * everything around the value: that the counter moves forward, that it
 * lands in both halves of edx:eax, that neither instruction disturbs
 * the flags, that cpuid gives the same answer twice for the same leaf,
 * and that the feature bits a client actually branches on are set. */

#define RDTSC(hi, lo)\
    asm volatile("rdtsc" : "=d" (hi), "=a" (lo) :: "memory")

static void test_rdtsc(void)
{
    unsigned int h1, l1, h2, l2;
    unsigned int seeded_hi = 0xdeadbeef, seeded_lo = 0xdeadbeef;
    long flags_in, flags_out;

    /* both halves are written: the seed is a value the counter cannot
     * plausibly hold, so surviving it means the half was not written */
    asm volatile("rdtsc" : "+d" (seeded_hi), "+a" (seeded_lo) :: "memory");
    printf("%-8s edx=%s eax=%s\n", "rdtsc",
           seeded_hi != 0xdeadbeef ? "written" : "kept",
           seeded_lo != 0xdeadbeef ? "written" : "kept");

    /* the counter moves forward and never backward */
    RDTSC(h1, l1);
    RDTSC(h2, l2);
    printf("%-8s monotonic=%s\n", "rdtsc",
           (h2 > h1 || (h2 == h1 && l2 >= l1)) ? "yes" : "no");

    /* and it leaves the flags alone */
    asm volatile("pushf\n\t"
                 "andl $~0x8d5, (%%esp)\n\t"   /* clear the arith flags */
                 "orl  $0x0c5, (%%esp)\n\t"    /* CF PF ZF SF set, OF/AF clear */
                 "popf\n\t"
                 "pushf\n\t"
                 "popl %0\n\t"
                 "rdtsc\n\t"
                 "pushf\n\t"
                 "popl %1\n\t"
                 : "=r" (flags_in), "=r" (flags_out) :: "eax", "edx", "cc");
    printf("%-8s flags=%s\n", "rdtsc",
           (flags_in & 0x8d5) == (flags_out & 0x8d5) ? "kept" : "clobbered");
}

#define CPUID(leaf, a, b, c, d)\
    asm volatile("cpuid" : "=a" (a), "=b" (b), "=c" (c), "=d" (d)\
                 : "0" (leaf) : "memory")

static void test_cpuid_writes(unsigned int leaf)
{
    unsigned int a0, b0, c0, d0, a1, b1, c1, d1;

    a0 = leaf; b0 = c0 = d0 = 0x00000000;
    asm volatile("cpuid" : "+a" (a0), "+b" (b0), "+c" (c0), "+d" (d0)
                 :: "memory");
    a1 = leaf; b1 = c1 = d1 = 0xffffffff;
    asm volatile("cpuid" : "+a" (a1), "+b" (b1), "+c" (c1), "+d" (d1)
                 :: "memory");
    printf("%-8s leaf %08x written=%c%c%c%c\n", "cpuid", leaf,
           a0 == a1 ? 'a' : '.', b0 == b1 ? 'b' : '.',
           c0 == c1 ? 'c' : '.', d0 == d1 ? 'd' : '.');
}

static void test_cpuid(void)
{
    unsigned int a1, b1, c1, d1, a2, b2, c2, d2;
    unsigned char vendor[13];
    int i, printable;

    /* the same leaf twice gives the same answer */
    CPUID(0, a1, b1, c1, d1);
    CPUID(0, a2, b2, c2, d2);
    printf("%-8s leaf0 stable=%s maxleaf>=1=%s\n", "cpuid",
           (a1 == a2 && b1 == b2 && c1 == c2 && d1 == d2) ? "yes" : "no",
           a1 >= 1 ? "yes" : "no");

    /* the vendor string is twelve characters, in ebx, edx, ecx, and
     * the string itself differs from machine to machine while being
     * printable ascii on all of them */
    memcpy(vendor + 0, &b1, 4);
    memcpy(vendor + 4, &d1, 4);
    memcpy(vendor + 8, &c1, 4);
    vendor[12] = '\0';
    printable = 1;
    for (i = 0; i < 12; i++)
	if (vendor[i] < 0x20 || vendor[i] > 0x7e)
	    printable = 0;
    printf("%-8s vendor=%s\n", "cpuid", printable ? "printable" : "not-ascii");

    /* The feature bits a dos client actually branches on. Only the four
     * that must be set wherever this test can run at all are printed:
     * the rest of the word is a property of the machine. */
    CPUID(1, a1, b1, c1, d1);
    printf("%-8s leaf1 fpu=%d tsc=%d cx8=%d cmov=%d\n", "cpuid",
           !!(d1 & (1 << 0)), !!(d1 & (1 << 4)),
           !!(d1 & (1 << 8)), !!(d1 & (1 << 15)));

    /* Every leaf writes all four registers, including a leaf the
     * processor does not implement: an out of range leaf returns
     * defined values, it never hands the input back. Run each leaf
     * from two seeds and report which registers were written, the
     * same way the descriptor table stores are checked, so that no
     * machine specific value has to be printed. Leaf 1 is left out:
     * its ebx carries the initial apic id, which changes if the run
     * moves to another core between the two reads, and its edx is
     * already known to be written from the feature bits above. The
     * mark for eax is not a signal: eax carries the leaf in, so it
     * holds the same value in both runs whether or not the
     * instruction wrote it. Read ebx, ecx and edx. */
    test_cpuid_writes(0x00000000);
    test_cpuid_writes(0x00000002);
    test_cpuid_writes(0x80000000);
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
    test_dtables();
    test_rdtsc();
    test_cpuid();
}
