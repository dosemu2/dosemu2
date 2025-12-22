
def fpu_bart_exceptions_fpex(self):
    if self.use_cpu == "kvm":
        if not self.have_kvm:
            self.skipTest("requires KVM")
        cpu_vm = 'kvm'
    elif self.use_cpu == "emu":
        cpu_vm = 'emulated'
    else:
        raise ValueError('invalid self.use_cpu')

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "%s"
""" % cpu_vm

    self.mkfile("testit.bat", """\
c:\\fpex
rem end
""", newline="\r\n")

    # compile sources
    self.mkexe_with_watcom("fpex", r"""
#include <stdio.h>
#include <float.h>
#include <dos.h>

char *status[2] = { "disabled", "enabled" };

void main()
{
    unsigned int fp_cw = 0;
    unsigned int fp_mask = 0x3f;
    unsigned int bits;
    double a = 1.0;

    fp_cw = _controlfp( fp_cw,
                         fp_mask );

    printf( "Interrupt Exception Masks\n" );
    bits = fp_cw & MCW_EM;
    printf( "  Invalid Operation exception %s\n",
            status[ (bits & EM_INVALID) == 0 ] );
    printf( "  Denormalized exception %s\n",
            status[ (bits & EM_DENORMAL) == 0 ] );
    printf( "  Divide-By-Zero exception %s\n",
            status[ (bits & EM_ZERODIVIDE) == 0 ] );
    printf( "  Overflow exception %s\n",
            status[ (bits & EM_OVERFLOW) == 0 ] );
    printf( "  Underflow exception %s\n",
            status[ (bits & EM_UNDERFLOW) == 0 ] );
    printf( "  Precision exception %s\n",
            status[ (bits & EM_PRECISION) == 0 ] );

    printf( "Infinity Control = " );
    bits = fp_cw & MCW_IC;
    if( bits == IC_AFFINE )     printf( "affine\n" );
    if( bits == IC_PROJECTIVE ) printf( "projective\n" );

    printf( "Rounding Control = " );
    bits = fp_cw & MCW_RC;
    if( bits == RC_NEAR )       printf( "near\n" );
    if( bits == RC_DOWN )       printf( "down\n" );
    if( bits == RC_UP )         printf( "up\n" );
    if( bits == RC_CHOP )       printf( "chop\n" );

    printf( "Precision Control = " );
    bits = fp_cw & MCW_PC;
    if( bits == PC_24 )         printf( "24 bits\n" );
    if( bits == PC_53 )         printf( "53 bits\n" );
    if( bits == PC_64 )         printf( "64 bits\n" );
    printf("%g\n", a/0.0);
    printf("FAIL:\n");
}
""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn('Floating point exception', results)
    self.assertNotIn('FAIL:', results)
