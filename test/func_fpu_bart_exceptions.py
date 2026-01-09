from common_framework import DOSEMU_CONF_DEFAULT


def fpu_bart_exceptions_fpex(self):
    if self.use_cpu == "kvm":
        if not self.have_kvm:
            self.skipTest("requires KVM")
        cpu_vm = 'kvm'
    elif self.use_cpu == "emu":
        cpu_vm = 'emulated'
    else:
        raise ValueError('invalid self.use_cpu')

    config = DOSEMU_CONF_DEFAULT
    config += f"""$_cpu_vm = "{cpu_vm}"\n"""

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


def fpu_bart_exceptions_fpexes(self):
    if self.use_cpu == "kvm":
        if not self.have_kvm:
            self.skipTest("requires KVM")
        cpu_vm = 'kvm'
    elif self.use_cpu == "emu":
        cpu_vm = 'emulated'
    else:
        raise ValueError('invalid self.use_cpu')

    config = DOSEMU_CONF_DEFAULT
    config += f"""$_cpu_vm = "{cpu_vm}"\n"""

    self.mkfile("testit.bat", """\
c:\\fpexes
rem end
""", newline="\r\n")

    self.mkcom_with_nasm("fpexes", r"""
org 100h
    mov ax, 3502h
    int 21h
    push es
    push bx
    mov dx, int2_handler
    mov ax, 2502h
    int 21h
    fninit
    fnstcw [cw]
    and word [cw], ~0x3f
    fldcw [cw]
    fld1
    fldz
    fdiv
    fwait
    pop dx
    pop ds
    mov ax, 2502h
    int 21h
    fninit
    ret

int2_handler:
    sti
    push ax
    push dx
    push ds
    fstsw ax
    fnclex
    test ax, 80h
    mov ah, 9
    push cs
    pop ds
    mov dx, esclearstr
    jz esclear
    mov dx, essetstr
esclear:
    int 21h
    pop ds
    pop dx
    pop ax
    iret

essetstr:
    db 'PASS:ES set',13,10,'$'
esclearstr:
    db 'FAIL:ES clear',13,10,'$'
cw:
""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn('PASS:', results)
    self.assertNotIn('FAIL:', results)
