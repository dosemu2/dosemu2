/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* 80x86 disassembler for real mode and 16 bit segments.
 * 32 bit address and data prefixes are partially supported; there
 * may be some missing cases.
 *
 * Copyright 1993 by John F. Carr (jfc@athena.mit.edu)
 *
 * Changes for dosemu-debugger by
 *              Hans Lermen <lermen@elserv.ffm.fgan.de>
 *
 *     27Sep98  Alexander Adams <a.r.adams@twi.tudelft.nl>
 *              - more (full?) 32 bit support
 *              - lots more fpu ops and a couple of cpu ops
 *		- better readable output
 */

#include "config.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "dis8086.h"

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

#define IBUFS 100
#define makeaddr(x,y) ((((unsigned long)x) << 4) + (unsigned long)y)
static unsigned char * linebuf;
static unsigned char * nameptr;
static unsigned int  * refaddr;

static const char *conditions[16] =
{
  "o ", "no", "b ", "ae", "e ", "ne", "be", "a ",
  "s ", "ns", "p ", "np", "l ", "ge", "le", "g "
};

static const char *cr[] =
{
  "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7"
};

static const char *dr[] =
{
  "dr0", "dr1", "dr2", "dr3", "dr4", "dr5", "dr6", "dr7"
};

static const char *tr[] =
{
  "tr0", "tr1", "tr2", "tr3", "tr4", "tr5", "tr6", "tr7"
};

static const char *farith[8] =
{
  "fadd  ", "fmul  ", "fcom  ", "fcomp ", "fsub  ", "fsubr ",
  "fdiv  ", "fdivr "
};

static const char *farithp[8] =
{
  "faddp ", "fmulp ", "", "fcompp", "fsubrp", "fsubp ",
  "fdivrp", "fdivp "
};

static const char *fiarith[8] =
{
  "fiadd ", "fimul ", "ficom ", "ficomp", "fisub ", "fisubr",
  "fidiv ", "fidivr"
};

static const char *d8_code1[5] =
{
  "feni", "fdisi", "fclex", "finit", "fsetpm"
};

static const char *d8_code2[8] =
{
  "fild  ", "", "fist  ", "fistp ", "", "fld   ", "", "fstp  "
};

static const char *dd_code1[8] =
{
  "fld   ", "", "fst   ", "fstp  ", "frstor", "", "fsave ", "fstsw "
};

static const char *dd_code2[8] =
{
  "ffree ", "", "fst   ", "fstp  ", "fucom ", "fucomp", "", ""
};

static const char *d9_e_f[32] =
{
  "fchs",   "fabs",   "",      "",        "ftst",   "fxam",   "",
  "",       "fld1",   "fldl2t", "fldl2e", "fldpi",  "fldlg2", "fldln2",
  "fldz",   "",       "f2xm1",  "fyl2x",  "fptan",  "fpatan", "fxtract",
  "fprem1", "fdecstp","fincstp","fprem",  "fyl2xp1","fsqrt",  "fsincos",
  "frndint","fscale", "fsin",   "fcos"
};

static const char *d9_code[8] =
{
  "fld   ", "fxch  ", "fst   ", "fstp  ", "fldenv", "fldcw ", "fstenv",
  "fstcw"
};

static const char *df_code1[4] =
{
  "fbld  ", "fild  ", "fbstp ", "fistp "
};

static const char *df_code2[8] =
{
  "fild  ", "ffreep", "", "fxch  ", "fist  ", "fstp  ", "fistp ", "fstp  "
};

static const char *f00[8] =
{
  "sldt", "str ", "lldt", "ltr ", "verr", "verw", "??? ", "??? "
};

static const char *f01[8] =
{
  "sgdt  ", "sidt  ", "lgdt  ", "lidt  ",
  "smsw  ", "invlpg", "lmsw  ", "invlpg"
};

static const char *shift[8] =
{
  "rol", "ror", "rcl", "rcr", "shl", "shr", "sal", "sar"
};

static const char *f7_code[8] =
{
  "test", "??? ", "not ", "neg ", "mul ", "imul", "div ", "idiv"
};

static const char *ff_code[8] =
{
  "inc     ", "dec     ", "call    ", "call    far ", "jmp     ",
  "jmp     far ", "push    ", "???     "
};

static const char *arith[8] =
{
  "add", "or ", "adc", "sbb", "and", "sub", "xor", "cmp"
};

static const char *bt_codes[4] =
{
  "bt ", "bts", "btr", "btc"
};

static const char *reg8[8] =
{
  "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"
};

static const char *reg16[8] =
{
  "ax", "cx", "dx", "bx", "sp", "bp", "si", "di"
};

static const char *reg32[8] =
{
  "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
};

static const char *sreg[8] =
{
  "es", "cs", "ss", "ds", "fs", "gs", "??", "??"
};

static const char *memref[8] =
{
  "bx+si", "bx+di", "bp+si", "bp+di", "si", "di", "bp", "bx"
};


static const char *loop_codes[] =
{
  "loopne", "loope ", "loop  ", "jcxz  "
};

#if defined(i386) || defined(vax) /* || defined(alpha) */
/* Little-endian machines which allow unaligned addresses.  */
#define IMMED16(code) *(unsigned short *)(code)
#define IMMED32(code) *(unsigned int *)(code)
#define IMMED32I(code) *(int *)(code)
#else
/* Big-endian or strict alignment.  */
#ifndef __GNUC__
#error needs gcc
#endif
#define IMMED16(code)	({const unsigned char *p=(code); unsigned short v;\
				v=p[0]|((unsigned short)(p[1])<<8); v; })
#define IMMED32(code)	({const unsigned char *p=(code); unsigned int v;\
				v=p[0]|((unsigned int)(p[1])<<8)|\
				((unsigned int)(p[2])<<16)|\
				((unsigned int)(p[3])<<24); v; })
#define IMMED32I(code)	({const unsigned char *p=(code); int v;\
				v=p[0]|((int)(p[1])<<8)|\
				((int)(p[2])<<16)|((int)(p[3])<<24); v; })
#endif

#undef index

static unsigned char * putaddr(unsigned int i1)
{
   *refaddr = i1;
   return (unsigned char *) 0;
}

static void d86_printf(const char *fmt,...)
{
  va_list args;
  char frmtbuf[100];

  va_start(args, fmt);
  vsprintf(frmtbuf, fmt, args);
  va_end(args);

  strcat (linebuf, frmtbuf);
}

static INLINE unsigned short immed16 (register const unsigned char * code)
{
   nameptr = putaddr(IMMED16(code));
   return(IMMED16(code));
}

static INLINE unsigned int   immed32 (register const unsigned char * code)
{
   nameptr = putaddr(IMMED32(code));
   return(IMMED32(code));
}

static INLINE	       int   immed32i (register const unsigned char * code)
{
   nameptr = putaddr(IMMED32(code));
   return(IMMED32I(code));
}

static INLINE unsigned int   resolva (unsigned int addr)
{
   nameptr = putaddr(addr);
   return(addr);
}

static unsigned char *modrm_disp32(int d)
{
   static char buf[16];

   if (d == 0)
     buf[0] = 0;
   else if (d > 0)
     sprintf(buf, "+%#08x", d);
   else
     sprintf(buf, "-%#08x", (-d));

   return buf;
}

static unsigned char *modrm_disp16(int d)
{
   static char buf[10];

   if (d == 0)
     buf[0] = 0;
   else if (d > 0)
     sprintf(buf, "+%#04x", d);
   else
     sprintf(buf, "-%#04x", (-d));

   return buf;
}

static unsigned char *modrm_disp8(int d)
{
   static char buf[8];

   if (d == 0)
     buf[0] = 0;
   else if (d > 0)
     sprintf(buf, "+%#02x", d);
   else
     sprintf(buf, "-%#02x", (-d));

   return buf;
}


static const unsigned char *mod_mem32(const unsigned char *code,
					const char *seg)
{
unsigned int rm, mod, base, index, scale;
const unsigned char *basestr, *indexstr, *scalestr, *disp;
unsigned char hex32[9];

    rm    =  code[0] & 7;
    mod   =  code[0] >> 6;
    base  =  code[1] & 7;
    index = (code[1] >> 3) & 7;
    scale =  code[1] >> 6;

    disp = indexstr = scalestr = "";
    basestr = reg32[rm];

    code++;
    if (rm == 4)    /* with sib */
      code++;

    if (mod == 0)
    {
        if (rm == 5)
        {
            d86_printf("%s[%08x]", seg, immed32(code));
            return code+4;
        }
        if (rm != 4)
        {
            d86_printf("%s[%s]", seg, reg32[rm]);
            return code;
        }

        if (base == 5)
        {
            sprintf(hex32, "%08x", immed32(code));
            basestr = hex32;
            code += 4;
        }
    }
    else if (mod == 1)
        disp = modrm_disp8((signed char)*code++);
    else
    {
        disp = modrm_disp32((signed int)immed32(code));
        code+= 4;
    }

    if (rm == 4)
    {
        if (!((mod == 0) && (base == 5)))
            basestr = reg32[base];
        switch (scale)
        {
            case 1 : scalestr = "+2*"; break;
            case 2 : scalestr = "+4*"; break;
            case 3 : scalestr = "+8*"; break;
            default: scalestr = "+";
        }
        if (index == 4)
            indexstr = scalestr = "";
        else
            indexstr = reg32[index];
    }
    d86_printf("%s[%s%s%s%s]", seg, basestr, scalestr, indexstr, disp);
    return code;
}

static const unsigned char *mod_mem16(const unsigned char *code,
                                      const char *seg)
{
unsigned char mod = *code & 0xc0;
unsigned char rm  = *code & 7;

  code++;

  if (mod == 0x80)
  {
      d86_printf("%s[%s%s]", seg, memref[rm], modrm_disp16((signed short)immed16(code)));
      return code+2;
  }

  if (mod == 0x40)
  {
      d86_printf("%s[%s%s]",seg,memref[rm],modrm_disp8((signed char)code[0]));
      return code+1;
  }
  /* (mod == 0) */

  if (rm == 6)
  {
      d86_printf("%s[%04x]", seg, immed16(code));
      return code+2;
  }
  else
  {
      d86_printf("%s[%s]", seg, memref[rm]);
      return code;
  }
}

static const unsigned char *fmod_rm(const unsigned char *code,
					  const char *seg, const char **regs,
					  int addr32)
{
unsigned char mod = (code[1] & 0xc0);
unsigned char reg = (code[1] & 7);

char *nw = 0;

  if (mod == 0xc0)
  {
      if (code[0] & 1)
          d86_printf("st(%d)",reg);

//      else if (reg == 1) ; /*((code[1] & 0x30) == 0x10) ;*/

      else if (code[0] & 4)
          d86_printf("st(%d),st",reg);

      else
          d86_printf("st,st(%d)",reg);

      return code+2;
  }

  if ((!(code[0] & 1)) || (!(code[1] & 0x20)))
  {
      switch (code[0] & 6)
      {
          case 0:
          case 2: nw = "d"; break;
          case 4: nw = "q"; break;
          case 6: nw = "" ; break;
      }
      d86_printf("%sword ptr ",nw);
  }
  else if ((code[0] == 0xdb) && (code[1] & 0x20))
      d86_printf("tbyte ptr ");
  else if ((code[0] == 0xdf) && (code[1] & 0x20))
      d86_printf((code[1] & 8) ? "qword ptr " : "tbyte ptr ");

  code++;

  if (addr32)
      return mod_mem32(code, seg);
  else
      return mod_mem16(code, seg);

}

static const unsigned char *mod_rm(const unsigned char *code,
					  const char *seg, const char **regs,
					  int addr32)
{
  if ((*code >> 6) == 3)
    {
      d86_printf(regs[*code & 7]);
      return code+1;
    }

  if (addr32)
      return mod_mem32(code, seg);
  else
      return mod_mem16(code, seg);
}	

static const unsigned char *mod_reg_rm(const unsigned char *code,
				   const char *seg, const char **sregs,
				   const char **regs, int addr32)
{
  d86_printf(sregs[(*code >> 3) & 7]);
  d86_printf(",");
  return mod_rm(code, seg, regs, addr32);
}

static const unsigned char *mod_reg_rm_r(const unsigned char *code,
				     const char *seg, const char **sregs,
				     const char **regs, int addr32)
{
  unsigned char rm = *code;

  code = mod_rm(code, seg, regs, addr32);
  d86_printf(",");
  d86_printf(sregs[(rm >> 3) & 7]);
  return code;
}


static INLINE const unsigned char *esc_op(const unsigned char opcode,
				const unsigned char *code, const char *seg,
				const char **regs,
				int addr32)
{

  d86_printf("esc     %02x,", ((opcode & 7) << 3) | ((*code >> 3) & 7));
  return mod_rm(code, seg, regs, addr32);
}


int  dis_8086(unsigned int org,
	      register const unsigned char *code,
	      unsigned char *outbuf,
	      int def_size,
	      unsigned int * refseg,
	      unsigned int * refoff,
	      int refsegbase, int nlines)
{
  const unsigned char *code0 = code;
  const char *seg = "";
  int data32 = ((def_size & 2)!=0);
  int addr32 = ((def_size & 1)!=0);
  int prefix = 0;
  int i;

  linebuf = outbuf;
  memset(linebuf, 0x00, IBUFS);

  #define SEG16REL(x) ((x)- ( refsegbase ? refsegbase : (*refseg<<4) ) )
  #define SEG32REL(x) ((x)- refsegbase)
  refaddr = refoff;
  *refaddr = 0;

  for(i=0;i<nlines;i++)   /* Why use a for loop? - ARA */
    {
      unsigned char opcode;
      register const char **wreg = data32 ? reg32 : reg16;
      opcode = *code++;
      switch(opcode)
	{
	case 0x00:
	case 0x08:
	case 0x10:
	case 0x18:
	case 0x20:
	case 0x28:
	case 0x30:
	case 0x38:
	  d86_printf(arith[(opcode >> 3) & 7]);
	  d86_printf("     ");
	  code = mod_reg_rm_r(code, seg, reg8, reg8, addr32);
	  break;

	case 0x01:
	case 0x09:
	case 0x11:
	case 0x19:
	case 0x21:
	case 0x29:
	case 0x31:
	case 0x39:
	  d86_printf(arith[(opcode >> 3) & 7]);
	  d86_printf("     ");
	  code = mod_reg_rm_r(code, seg, wreg, wreg, addr32);
	  break;

	case 0x02:
	case 0x0a:
	case 0x12:
	case 0x1a:
	case 0x22:
	case 0x2a:
	case 0x32:
	case 0x3a:
	  d86_printf(arith[(opcode >> 3) & 7]);
	  d86_printf("     ");
	  code = mod_reg_rm(code, seg, reg8, reg8, addr32);
	  break;

	case 0x03:
	case 0x0b:
	case 0x13:
	case 0x1b:
	case 0x23:
	case 0x2b:
	case 0x33:
	case 0x3b:
	  d86_printf(arith[(opcode >> 3) & 7]);
	  d86_printf("     ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	  break;

	case 0x04:
	case 0x0c:
	case 0x14:
	case 0x1c:
	case 0x24:
	case 0x2c:
	case 0x34:
	case 0x3c:
	  d86_printf("%s     al,%#02x", arith[opcode >> 3],
		  (unsigned char)*code++);
	  break;

	case 0x05:
	case 0x0d:
	case 0x15:
	case 0x1d:
	case 0x25:
	case 0x2d:
	case 0x35:
	case 0x3d:
	  if (data32)
	    {
	      d86_printf("%s     eax,%08x", arith[opcode >> 3], immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf("%s     ax,%04x", arith[opcode >> 3], immed16(code));
	      code += 2;
	    }
	  break;

	case 0x06:
	  d86_printf("push    es");
	  break;

	case 0x07:
	  d86_printf("pop     es");
	  break;

	case 0x0e:
	  d86_printf("push    cs");
	  break;

	case 0x0f:
	  opcode = *code++;
	  switch (opcode)
	    {
	    case 0x00:
	      d86_printf(f00[(*code >> 3) & 7]);
	      d86_printf("    ");
	      code = mod_rm(code, seg, wreg, addr32);
	      break;
	    case 0x01:
	      d86_printf(f01[(*code >> 3) & 7]);
	      d86_printf("  ");
	      code = mod_rm(code, seg, wreg, addr32);
	      break;
	    case 0x02:
	      d86_printf("lar     ");
	      code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	      break;
	    case 0x03:
	      d86_printf("lsl     ");
	      code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	      break;
	    case 0x05:
	      d86_printf("loadall");
	      break;
	    case 0x06:
	      d86_printf("clts");
	      break;
	    case 0x08:
	      d86_printf("invd");
	      break;
	    case 0x09:
	      d86_printf("wbinvd");
	      break;
	    case 0x20:
	      d86_printf("mov     ");
	      code = mod_reg_rm_r(code, seg, cr, reg32, addr32);
	      break;
	    case 0x21:
	      d86_printf("mov     ");
	      code = mod_reg_rm_r(code, seg, dr, reg32, addr32);
	      break;
	    case 0x22:
	      d86_printf("mov     ");
	      code = mod_reg_rm(code, seg, cr, reg32, addr32);
	      break;
	    case 0x23:
	      d86_printf("mov     ");
	      code = mod_reg_rm(code, seg, dr, reg32, addr32);
	      break;
	    case 0x24:
	      d86_printf("mov     ");
	      code = mod_reg_rm_r(code, seg, tr, reg32, addr32);
	      break;
	    case 0x26:
	      d86_printf("mov     ");
	      code = mod_reg_rm(code, seg, tr, reg32, addr32);
	      break;
	    case 0x31:
	      d86_printf("rdtsc");
	      break;
	    case 0x80:
	    case 0x81:
	    case 0x82:
	    case 0x83:
	    case 0x84:
	    case 0x85:
	    case 0x86:
	    case 0x87:
	    case 0x88:
	    case 0x89:
	    case 0x8a:
	    case 0x8b:
	    case 0x8c:
	    case 0x8d:
	    case 0x8e:
	    case 0x8f:
	      if (addr32) {
		d86_printf("j%s     %08x", conditions[opcode & 15],
			resolva((code-code0)+org + 4 + IMMED32I(code)));
		code += 4;
	      } else {
		d86_printf("j%s     %04x", conditions[opcode & 15],
			(code-code0)+org + 2 + (short)(code[0] + (code[1] << 8)));
		code += 2;
	      }
	      break;
	    case 0x90:
	    case 0x91:
	    case 0x92:
	    case 0x93:
	    case 0x94:
	    case 0x95:
	    case 0x96:
	    case 0x97:
	    case 0x98:
	    case 0x99:
	    case 0x9a:
	    case 0x9b:
	    case 0x9c:
	    case 0x9d:
	    case 0x9e:
	    case 0x9f:
	      d86_printf("set%s   ", conditions[opcode & 15]);
	      code = mod_rm(code, seg, reg8, addr32);
	      break;

	    case 0xa0:
	      d86_printf("push    fs");
	      break;

	    case 0xa1:
	      d86_printf("pop     fs");
	      break;
	
	    case 0xa2:
	      d86_printf("cpuid");
	      break;

	    case 0xa3:
	    case 0xab:
	    case 0xb3:
	    case 0xbb:
	      d86_printf(bt_codes[(opcode >> 3) & 3]);
	      d86_printf("     ");
	      code = mod_reg_rm_r(code, seg, wreg, wreg, addr32);
	      break;
	
	    case 0xa4:
	      d86_printf("shld    ");
	      code = mod_reg_rm_r(code, seg, wreg, wreg, addr32);
	      d86_printf(",%#02x", *code++);
	      break;

	    case 0xa5:
	      d86_printf("shld    ");
	      code = mod_reg_rm_r(code, seg, wreg, wreg, addr32);
	      d86_printf(",cl");
	      break;

	    case 0xa8:
	      d86_printf("push    gs");
	      break;

	    case 0xa9:
	      d86_printf("pop     gs");
	      break;

	    case 0xac:
	      d86_printf("shrd    ");
	      code = mod_reg_rm_r(code, seg, wreg, wreg, addr32);
	      d86_printf(",%#02x", *code++);
	      break;

	    case 0xad:
	      d86_printf("shrd    ");
	      code = mod_reg_rm_r(code, seg, wreg, wreg, addr32);
	      d86_printf(",cl");
	      break;
	
	    case 0xaf:
	      d86_printf("imul    ");
	      code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	      break;
	
	    case 0xb0:
	      d86_printf("cmpxchg ");
	      code = mod_reg_rm_r(code, seg, reg8, reg8, addr32);
	      break;
	
	    case 0xb1:
	      d86_printf("cmpxchg ");
	      code = mod_reg_rm_r(code, seg, wreg, wreg, addr32);
	      break;

	    case 0xb2:
	      d86_printf("lss     ");
	      code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	      break;

	    case 0xb4:
	      d86_printf("lfs     ");
	      code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	      break;

	    case 0xb5:
	      d86_printf("lgs     ");
	      code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	      break;

 	    case 0xb6:
	      d86_printf("movzx   %s,", wreg[(*code >> 3) & 7]);
	      if ((*code & 0xc0) != 0xc0)
	        d86_printf("byte ptr ");
	      code = mod_rm(code, seg, reg8, addr32);
	      break;

	    case 0xb7:
	      d86_printf("movzx   %s,", wreg[(*code >> 3) & 7]);
	      if ((*code & 0xc0) != 0xc0)
	        d86_printf("%cword ptr ",(data32? 'd':' '));
	      code = mod_rm(code, seg, reg16, addr32);
	      break;
	      	
	    case 0xba:
	      d86_printf(bt_codes[(*code >> 3) & 3]);
	      d86_printf("     ");
	      code = mod_rm(code, seg, wreg, addr32);
	      d86_printf(",%#02x", *code++);
	      break;

	    case 0xbc:
	      d86_printf("bsf     ");
	      code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	      break;

	    case 0xbd:
	      d86_printf("bsr     ");
	      code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	      break;

            case 0xbe:
	      d86_printf("movsx   %s,", wreg[(*code >> 3) & 7]);
	      if ((*code & 0xc0) != 0xc0)
	        d86_printf("byte ptr ");
	      code = mod_rm(code, seg, reg8, addr32);
	      break;

	    case 0xbf:
	      d86_printf("movsx   %s,", wreg[(*code >> 3) & 7]);
	      if ((*code & 0xc0) != 0xc0)
	        d86_printf("%cword ptr ",(data32? 'd':' '));
	      code = mod_rm(code, seg, reg16, addr32);
	      break;

	    case 0xc0:
	      d86_printf("xadd    ");
	      code = mod_reg_rm(code, seg, reg8, reg8, addr32);
	      break;
	
	    case 0xc1:
	      d86_printf("xadd    ");
	      code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	      break;
	                                                                           
	    case 0xc8:
	    case 0xc9:
	    case 0xca:
	    case 0xcb:
	    case 0xcc:
	    case 0xcd:
	    case 0xce:
	    case 0xcf:
	      d86_printf("bswap   %s", reg32[opcode & 7]);
	      break;

	    default:	/* Maybe better to just say "db 0F"? - ARA */
	      d86_printf("ESC 0F %02x", opcode);
	      break;
	    }
	  break;

	case 0x16:
	  d86_printf("push    ss");
	  break;

	case 0x17:
	  d86_printf("pop     ss");
	  break;

	case 0x1e:
	  d86_printf("push    ds");
	  break;

	case 0x1f:
	  d86_printf("pop     ds");
	  break;

	case 0x26:
	  seg = "es:";
	  prefix = 1; i--;
	  continue;

	case 0x27:
	  d86_printf("daa");
	  break;

	case 0x2e:
	  seg = "cs:";
	  prefix = 1; i--;
	  continue;

	case 0x2f:
	  d86_printf("das");
	  break;

	case 0x36:
	  seg = "ss:";
	  prefix = 1; i--;
	  continue;

	case 0x37:
	  d86_printf("aaa");
	  break;

	case 0x3e:
	  seg = "ds:";
	  prefix = 1; i--;
	  continue;

	case 0x3f:
	  d86_printf("aas");
	  break;

	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47:
	  d86_printf("inc     %s", wreg[opcode & 7]);
	  break;

	case 0x48:
	case 0x49:
	case 0x4a:
	case 0x4b:
	case 0x4c:
	case 0x4d:
	case 0x4e:
	case 0x4f:
	  d86_printf("dec     %s", wreg[opcode & 7]);
	  break;

	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:
	  d86_printf("push    %s", wreg[opcode & 7]);
	  break;

	case 0x58:
	case 0x59:
	case 0x5a:
	case 0x5b:
	case 0x5c:
	case 0x5d:
	case 0x5e:
	case 0x5f:
	  d86_printf("pop     %s", wreg[opcode & 7]);
	  break;

	case 0x60:
	  d86_printf("pusha");
	  break;

	case 0x61:
	  d86_printf("popa");
	  break;

	case 0x62:
	  d86_printf("bound   ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	  break;

	case 0x63:
	  d86_printf("arpl    ");
	  code = mod_reg_rm_r(code, seg, wreg, wreg, addr32);
	  break;

	case 0x64:
	  seg = "fs:";
	  prefix = 1; i--;
	  continue;

	case 0x65:
	  seg = "gs:";
	  prefix = 1; i--;
	  continue;

	case 0x66:
	  data32 = ((def_size & 2)==0);
	  prefix = 1; i--;
	  continue;

	case 0x67:
	  addr32 = ((def_size & 1)==0);
	  prefix = 1; i--;
	  continue;

	case 0x68:
	  if (data32)
	    {
	      d86_printf("push    %08x", immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf("push    %04x", immed16(code));
	      code += 2;
	    }
	  break;

	case 0x69:
	  d86_printf("imul    ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	  if (data32)
	    {
	      d86_printf(",%08x", IMMED32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf(",%04x", IMMED16(code));
	      code += 2;
	    }
	  break;

	case 0x6a:
	  if (data32)
	      d86_printf("push    %08x", (signed char)*code++);
	  else
	      d86_printf("push    %04x", (signed char)*code++);
	  break;

	case 0x6b:
	  d86_printf("imul    ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	  d86_printf(",%#02x", (unsigned char)*code++);
	  break;

	case 0x6c:
	  d86_printf("insb");
	  break;

	case 0x6d:
	  d86_printf("ins%c", data32 ? 'd' : 'w');
	  break;

	case 0x6e:
	  d86_printf("outsb   %s", seg);
	  break;

	case 0x6f:
	  d86_printf("outs%c   %s", data32 ? 'd' : 'w', seg);
	  break;

	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
	case 0x78:
	case 0x79:
	case 0x7a:
	case 0x7b:
	case 0x7c:
	case 0x7d:
	case 0x7e:
	case 0x7f:
	  if (addr32) d86_printf("j%s     %08x", conditions[opcode & 15],
		  SEG32REL(resolva((code-code0)+org + (signed char)*code + 1)));
	  else d86_printf("j%s     %04x", conditions[opcode & 15],
	          (unsigned short)SEG16REL(resolva((code-code0)+org + (signed char)*code + 1)));
	  code++;
	  break;

	case 0x80:
	case 0x82:
	  d86_printf(arith[(*code >> 3) & 7]);
	  d86_printf("     ");
	  code = mod_rm(code, seg, reg8, addr32);
	  d86_printf(",%#02x", (unsigned char)*code++);
	  break;

	case 0x81:
	  d86_printf(arith[(*code >> 3) & 7]);
	  d86_printf("     ");
	  code = mod_rm(code, seg, wreg, addr32);
	  if (data32)
	    {
	      d86_printf(",%08x", immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf(",%04x", immed16(code));
	      code += 2;
	    }
	  break;

	case 0x83:
	  d86_printf(arith[(*code >> 3) & 7]);
	  d86_printf("     ");
	  code = mod_rm(code, seg, wreg, addr32);
	  if (data32)
	      d86_printf(",%08x", (signed char)*code++);
	  else
	      d86_printf(",%04x", (unsigned short)((signed char)*code++));
	  break;

	case 0x84:
	  d86_printf("test    ");
	  code = mod_reg_rm(code, seg, reg8, reg8, addr32);
	  break;

	case 0x85:
	  d86_printf("test    ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	  break;

	case 0x86:
	  d86_printf("xchg    ");
	  code = mod_reg_rm(code, seg, reg8, reg8, addr32);
	  break;

	case 0x87:
	  d86_printf("xchg    ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	  break;

	case 0x88:
	  d86_printf("mov     ");
	  code = mod_reg_rm_r(code, seg, reg8, reg8, addr32);
	  break;

	case 0x89:
	  d86_printf("mov     ");
	  code = mod_reg_rm_r(code, seg, wreg, wreg, addr32);
	  break;

	case 0x8a:
	  d86_printf("mov     ");
	  code = mod_reg_rm(code, seg, reg8, reg8, addr32);
	  break;

	case 0x8b:
	  d86_printf("mov     ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);

	  break;

	case 0x8c:
	  d86_printf("mov     ");
	  code = mod_reg_rm_r(code, seg, sreg, reg16, addr32);
	  break;

	case 0x8d:
	  d86_printf("lea     ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	  break;

	case 0x8e:
	  d86_printf("mov     ");
	  code = mod_reg_rm(code, seg, sreg, reg16, addr32);
	  break;

	case 0x8f:
	  d86_printf("pop     ");
	  if ((*code & 0xc0) != 0xc0)
	    d86_printf("%cword ptr ",(data32? 'd':' '));
	  code = mod_rm(code, seg, wreg, addr32);
	  break;

	case 0x90:
	  d86_printf("nop");
	  break;

	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97:
	  d86_printf("xchg    %s,%s", wreg[0], wreg[opcode & 7]);
	  break;

	case 0x98:
	  d86_printf("cbw");
	  break;

	case 0x99:
	  d86_printf(data32 ? "cdq" : "cwd");
	  break;

	case 0x9a:
	  if (data32)
	    {
	      d86_printf("call    %04x:%08x", IMMED16(code+4),
	      	  IMMED32(code));
	      code += 6;
	    }
	  else
	    {
	      d86_printf("call    %04x:%04x", IMMED16(code+2),
		  IMMED16(code));
	      resolva( makeaddr( IMMED16(code+2), IMMED16(code)));
	      code += 4;
	    }
	  break;

	case 0x9b:
	  d86_printf("wait");
	  break;

	case 0x9c:
	  d86_printf("pushf%s", data32 ? "d" : "");
	  break;

	case 0x9d:
	  d86_printf("popf%s", data32 ? "d" : "");
	  break;

	case 0x9e:
	  d86_printf("sahf");
	  break;

	case 0x9f:
	  d86_printf("lahf");
	  break;

	case 0xa0:
	  if (addr32) {
	    d86_printf("mov     al,%s[%08x]", seg, immed32(code));
	    code += 4;
	  } else {
	    d86_printf("mov     al,%s[%04x]", seg, immed16(code));
	    code += 2;
	  }
	  break;

	case 0xa1:
	  if (addr32) {
	    d86_printf("mov     %s,%s[%08x]", wreg[0], seg, immed32(code));
	    code += 4;
	  } else {
	    d86_printf("mov     %s,%s[%04x]", wreg[0], seg, immed16(code));
	    code += 2;
	  }
	  break;

	case 0xa2:
	  if (addr32) {
	    d86_printf("mov     %s[%08x],al", seg, immed32(code));
	    code += 4;
	  } else {
	    d86_printf("mov     %s[%04x],al", seg, immed16(code));
	    code += 2;
	  }
	  break;

	case 0xa3:
	  if (addr32) {
	    d86_printf("mov     %s[%08x],%s", seg, immed32(code), wreg[0]);
	    code += 4;
	  } else {
	    d86_printf("mov     %s[%04x],%s", seg, immed16(code), wreg[0]);
	    code += 2;
	  }
	  break;

	case 0xa4:
	  if (seg == "")
	    d86_printf("movsb");
	  else
	    d86_printf("movsb es:[%s],%s[%s]", addr32 ? "edi" : "di",
		  seg, addr32 ? "esi" : "si");
	  break;

	case 0xa5:
	  if (seg == "")
	    d86_printf("movs%c", data32 ? 'd' : 'w');
	  else
	    d86_printf("movs%c   es:[%s],%s[%s]", data32 ? 'd' : 'w',
		  addr32 ? "edi" : "di", seg, addr32 ? "esi" : "si");
	  break;

	case 0xa6:
	  if (seg == "")
	    d86_printf("cmpsb");
	  else
	    d86_printf("cmpsb   es:[%s],%s[%s]", addr32 ? "edi" : "di",
	          seg, addr32 ? "esi" : "si");
	  break;

	case 0xa7:
	  if (seg =="")
	    d86_printf("cmps%c", data32 ? 'd' : 'w');
	  else
	    d86_printf("cmps%c  es:[%s],%s[%s]", data32 ? 'd' : 'w',
	          addr32 ? "edi" : "di", seg, addr32 ? "esi" : "si");
	  break;

	case 0xa8:
	  d86_printf("test    al,%#02x", *code++);
	  break;

	case 0xa9:
	  if (data32)
	    {
	      d86_printf("test    eax,%08x", immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf("test    ax,%04x", immed16(code));
	      code += 2;
	    }
	  break;

	case 0xaa:
	  d86_printf("stosb");
	  break;

	case 0xab:
	  d86_printf("stos%c", data32 ? 'd' : 'w');
	  break;

	case 0xac:
	  if (seg == "")
	    d86_printf("lodsb");
	  else
	    d86_printf("lodsb   %s[%s]", seg, addr32 ? "esi" : "si");
	  break;

	case 0xad:
	  if (seg == "")
	    d86_printf("lods%c", data32 ? 'd' : 'w');
	  else
	    d86_printf("lods%c   %s[%s]", data32 ? 'd' : 'w', seg,
		  addr32 ? "esi" : "si");
	  break;

	case 0xae:
	  d86_printf("scasb");
	  break;

	case 0xaf:
	  d86_printf("scas%c", data32 ? 'd' : 'w');
	  break;

	case 0xb0:
	case 0xb1:
	case 0xb2:
	case 0xb3:
	case 0xb4:
	case 0xb5:
	case 0xb6:
	case 0xb7:
	  d86_printf("mov     %s,%#02x", reg8[opcode & 7], *code++);
	  break;

	case 0xb8:
	case 0xb9:
	case 0xba:
	case 0xbb:
	case 0xbc:
	case 0xbd:
	case 0xbe:
	case 0xbf:
	  if (data32)
	    {
	      d86_printf("mov     %s,%08x", wreg[opcode & 7], immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf("mov     %s,%04x", wreg[opcode & 7], immed16(code));
	      code += 2;
	    }
	  break;

	case 0xc0:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf("     ");
	  if ((*code & 0xc0) != 0xc0)
	      d86_printf("byte ptr ");
	  code = mod_rm(code, seg, reg8, addr32);
	  d86_printf(",%d", 31 & *code++);
	  break;

	case 0xc1:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf("     ");
	  if ((*code & 0xc0) != 0xc0)
	      d86_printf("%cword ptr ",(data32? 'd':' '));
	  code = mod_rm(code, seg, wreg, addr32);
	  d86_printf(",%d", 31 & *code++);
	  break;

	case 0xc2:
	  d86_printf("ret     %04x", IMMED16(code));
	  code += 2;
	  break;

	case 0xc3:
	  d86_printf("ret");
	  break;

	case 0xc4:
	  d86_printf("les     ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	  break;

	case 0xc5:
	  d86_printf("lds     ");
	  code = mod_reg_rm(code, seg, wreg, wreg, addr32);
	  break;

	case 0xc6:
	  d86_printf("mov     ");
	  code = mod_rm(code, seg, reg8, addr32);
	  d86_printf(",%#02x", (unsigned char)*code++);
	  break;

	case 0xc7:
	  d86_printf("mov     ");
	  code = mod_rm(code, seg, wreg, addr32);
	  if (data32)
	    {
	      d86_printf(",%08x", immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf(",%04x", immed16(code));
	      code += 2;
	    }
	  break;

	case 0xc8:
	  d86_printf("enter   %04x",IMMED16(code));
	  code+=2;
	  d86_printf(",%#02x", *code++);
	
	  break;

	case 0xc9:
	  d86_printf("leave");
	  break;

	case 0xca:
	  d86_printf("retf    %04x", IMMED16(code));
	  code += 2;
	  break;

	case 0xcb:
	  d86_printf("retf");
	  break;

	case 0xcc:
	  d86_printf("int     3");
	  break;

	case 0xcd:
	  d86_printf("int     %#02x", *code++);
	  break;

	case 0xce:
	  d86_printf("into");
	  break;

	case 0xcf:
	  d86_printf("iret%s", data32 ? "d" : "");
	  break;

	case 0xd0:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf("     ");
	  if ((*code & 0xc0) != 0xc0)
	      d86_printf("byte ptr ");
	  code = mod_rm(code, seg, reg8, addr32);
	  d86_printf(",1");
	  break;

	case 0xd1:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf("     ");
	  if ((*code & 0xc0) != 0xc0)
	      d86_printf("%cword ptr ",(data32? 'd':' '));
	  code = mod_rm(code, seg, wreg, addr32);
	  d86_printf(",1");
	  break;

	case 0xd2:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf("     ");
	  if ((*code & 0xc0) != 0xc0)
	      d86_printf("byte ptr ");
	  code = mod_rm(code, seg, reg8, addr32);
	  d86_printf(",cl");
	  break;

	case 0xd3:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf("     ");
	  if ((*code & 0xc0) != 0xc0)
	      d86_printf("%cword ptr ",(data32? 'd':' '));
	  code = mod_rm(code, seg, wreg, addr32);
	  d86_printf(",cl");
	  break;

	case 0xd4:
	  d86_printf("aam     %02d", *code++);
	  break;

	case 0xd5:
	  d86_printf("aad     %02d", *code++);
	  break;
	
	case 0xd6:
	  d86_printf("setalc");
	  break;

	case 0xd7:
	  d86_printf("xlatb %s", seg);
	  break;
	
        case 0xd8:
        case 0xda:
        case 0xdc:
        case 0xde:
          {
            int op = (*code >> 3) & 7;
            if ((*code & 0xc0) == 0xc0)
              {
                if (opcode == 0xda)
                  {
                    code = esc_op(opcode,code,seg,reg8,addr32);
                    break;
                  }
                d86_printf("%s  ", ((opcode & 2) ? farithp[op] : farith[op]));
              }
            else
              d86_printf("%s  ", ((opcode & 2) ? fiarith[op] : farith[op]));

            code = fmod_rm(code-1,seg,wreg,addr32);
          }
          break;

        case 0xdb:
          if ((*code & 0xc0) == 0xc0)
            {
              if ((*code & 0x38) == 0x20 && (*code & 7) <= 4)
                {
                  d86_printf(d8_code1[*code++ & 7]);
                  break;
                }
              code = esc_op(opcode, code, seg, wreg, addr32);
              break;
            }
          else
            {
              register int op = (*code >> 3) & 7;
              if (d8_code2[op][0] == 0)
                {
  	          code = esc_op(opcode, code, seg, wreg, addr32);
  	          break;
  	        }
  	      d86_printf("%s  ",d8_code2[op]);
              code = fmod_rm(code-1, seg, wreg, addr32);
            }
          break;

	case 0xd9:
	  if ((*code & 0xe0) == 0xe0)
	    {
	      if (d9_e_f[*code & 0x1f][0] == 0)
  	        {
  	          code = esc_op(opcode, code, seg, wreg, addr32);
  	          break;
  	        }
  	      else
  	        {
  	          d86_printf(d9_e_f[*code++ & 0x1f]);
  	          break;
  	        }
  	    }
	  else if ((*code & 0xf8) == 0xd0)
	    {
	      d86_printf("fnop");
	      code++;
	      break;
	    }
	  else if ((*code & 0xf8) == 8)
	    {
              code = esc_op(opcode, code, seg, wreg, addr32);
              break;
            }	
	case 0xdd:
	  {
	    const char **ctbl;
	    int op = (*code >> 3) & 7;
	
	    ctbl = (opcode == 0xd9) ? d9_code :
	           ((*code & 0xc0) == 0xc0) ? dd_code2 : dd_code1;
	    if (ctbl[op][0] == 0)
	      {
	        code = esc_op(opcode, code, seg, wreg, addr32);
	        break;
	      }
	    d86_printf("%s  ",ctbl[op]);
	    code = fmod_rm(code-1,seg,wreg,addr32);
	  }
	  break;
	
	case 0xdf:
          if (((*code & 0xe0) == 0xe0) || ((*code & 0xf8) == 8))    
            {
	      if (*code == 0xe0)
		{
		  d86_printf("fnstsw  ax");
		  code++;
		}
              else
                  code = esc_op(opcode, code, seg, wreg, addr32);
              break;
    	    }
          if (*code & 0x20)
            d86_printf("%s  ",df_code1[(*code >> 3) & 3]);
          else
            d86_printf("%s  ",df_code2[((*code >> 2) & 6) | ((*code & 0xc0) == 0xc0)]);

	  code = fmod_rm(code-1,seg,wreg,addr32);
	  break;

	case 0xe0:
	case 0xe1:
	case 0xe2:
	case 0xe3:
	  if (addr32)
              d86_printf("%s  %08x",
                         loop_codes[opcode & 3],
                         SEG32REL((code-code0)+org + (signed char)*code + 1));
	  else
              d86_printf("%s  %04x",
                         loop_codes[opcode & 3],
                         SEG16REL((code-code0)+org + (signed char)*code + 1));
	  code++;
	  break;

	case 0xe4:
	  d86_printf("in      al,%#02x", *code++);
	  break;

	case 0xe5:
	  d86_printf("in      ax,%#02x", *code++);
	  break;

	case 0xe6:
	  d86_printf("out     %#02x,al", *code++);
	  break;

	case 0xe7:
	  d86_printf("out     %#02x,ax", *code++);
	  break;

	case 0xe8:
	  if (addr32) {
	    d86_printf("call    %08x",
		    SEG32REL(resolva((code-code0)+org + 4 + IMMED32I(code))));
	    code += 4;
	  } else {
	    d86_printf("call    %04x",
		    SEG16REL(resolva((code-code0)+org + 2 + (code[0] | (code[1] << 8)))) & 0xffff);

	    code += 2;
	  }
	  break;

	case 0xe9:
	  if (data32) {
	    d86_printf("jmp     %08x",
		    SEG32REL(resolva((code-code0)+org + 4 + IMMED32I(code))));
	    code += 4;
	  } else {
	    d86_printf("jmp     %04x",
		    SEG16REL((unsigned short) ((code-code0)+org + 2 + (code[0] | (code[1] << 8)))) & 0xffff);
	    code += 2;
	  }
	  break;

	case 0xea:
	  if (data32) {
	    d86_printf("jmp     %04x:%08x", immed16(code+4),
		    immed32i(code));
	    code += 6;
	  } else {
	    d86_printf("jmp     %04x:%04x", IMMED16(code+2),
		    IMMED16(code));
	    resolva( makeaddr( IMMED16(code+2), IMMED16(code)));
	    code += 4;
	  }
	  break;

	case 0xeb:
	  if (data32) d86_printf("jmp     %08x", SEG32REL(resolva((code-code0)+org + (signed char)*code + 1)));
	  else d86_printf("jmp     %04x", SEG16REL(resolva((code-code0)+org + (signed char)*code + 1)));
	  code++;
	  break;

	case 0xec:
	  d86_printf("in      al,dx");
	  break;

	case 0xed:
	  d86_printf("in      ax,dx");
	  break;

	case 0xee:
	  d86_printf("out     dx,al");
	  break;

	case 0xef:
	  d86_printf("out     dx,ax");
	  break;

	case 0xf0:
	  d86_printf("lock    ");
	  prefix = 1; i--;
	  break;

	case 0xf2:
	  d86_printf("repne   ");
	  prefix = 1; i--;
	  continue;
	
	case 0xf3:
	  d86_printf("repe    ");
	  prefix = 1; i--;
	  continue;

	case 0xf4:
	  d86_printf("hlt");
	  break;

	case 0xf5:
	  d86_printf("cmc");
	  break;

	case 0xf6:
	  opcode = *code;
	  d86_printf(f7_code[(*code >> 3) & 7]);
	  d86_printf("    ");
	  if (((*code & 0xc0) != 0xc0) && (*code & 0x38))
	    d86_printf("byte ptr ");
	  code = mod_rm(code, seg, reg8, addr32);
	  if ((opcode & 0x38) == 0)
	    d86_printf(",%#02x", *code++);
	  break;

	case 0xf7:
	  opcode = *code;
	  d86_printf(f7_code[(*code >> 3) & 7]);
	  d86_printf("    ");
	  if (((*code & 0xc0) != 0xc0) && (*code & 0x38))
	      d86_printf("%cword ptr ",(data32? 'd':' '));
	  code = mod_rm(code, seg, wreg, addr32);
	  if ((opcode & 0x38) == 0)
	    {
	      if (data32)
		{
		  d86_printf(",%08x", immed32(code));
		  code += 4;
		}
	      else
		{
		  d86_printf(",%04x", immed16(code));
		  code += 2;
		}
	    }
	  break;

	case 0xf8:
	  d86_printf("clc");
	  break;

	case 0xf9:
	  d86_printf("stc");
	  break;

	case 0xfa:
	  d86_printf("cli");
	  break;

	case 0xfb:
	  d86_printf("sti");
	  break;

	case 0xfc:
	  d86_printf("cld");
	  break;

	case 0xfd:
	  d86_printf("std");
	  break;

	case 0xfe:
	  if (*code & 0x30)
	    {
	      d86_printf("db      %#02x", opcode);
	      break;
	    }
	  d86_printf( ((*code & 0x08) ? "dec     " : "inc     ") );

	  if ((*code  & 0xc0) != 0xc0)
	      d86_printf("byte ptr ");
	
	  code = mod_rm(code, seg, reg8, addr32);
	  break;

	case 0xff:
	  d86_printf(ff_code[(*code >> 3) & 7]);
	  if (((*code & 0xc0) != 0xc0) &&
	      (((*code & 0x30) == 0) || ((*code & 0x30) == 0x30)))
	      d86_printf("%cword ptr ",(data32? 'd':' '));
	  code = mod_rm(code, seg, wreg, addr32);
	  break;

	default:
	  d86_printf("db      %#02x", opcode);
	  break;
	}
      seg = "";
      data32 = def_size;
      addr32 = def_size;
      prefix = 0;
      break;
    }	/* end of while() */
    return ( (int) (code-code0) );
  #undef SEG16REL
  #undef SEG32REL
}   /* end of dis_8086 */

