/* 80x86 disassembler for real mode and 16 bit segments.
 * 32 bit address and data prefixes are partially supported; there
 * may be some missing cases.
 *
 * Copyright 1993 by John F. Carr (jfc@athena.mit.edu)
 *
 * Changes for dosemu-debugger by
 *       Hans Lermen <lermen@elserv.ffm.fgan.de>
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

#define IBUFS 100
#define makeaddr(x,y) ((((unsigned long)x) << 4) + (unsigned long)y)
static unsigned char* linebuf;
static unsigned char * nameptr;
static unsigned int  * refaddr;

static const char *conditions[16] =
{
  "o", "no", "b", "ae", "e", "ne", "be", "a",
  "s", "ns", "p", "np", "le", "g", "le", "g"
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

static const char *freg[8] =
{
  "st(0)", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)"
};

static const char *farith[8] =
{
  "fadd", "fmul", "fcom", "fcomp", "fsub", "fsubr", "fdiv", "fdivr"
};

#if 0 /* Jan 96 JES */
static const char *d9[8] =
{
  "flds", "fxch", "fsts", "fstps", "ftst/fxam", "fldc", "fmisc1", "fmisc2"
};
#endif

static const char *f00[8] =
{
  "sldt", "str", "lldt", "ltr", "verr", "verw", "???", "???"
};

static const char *f01[8] =
{
  "sgdt", "sidt", "lgdt", "lidt", "smsw", "???", "lmsw", "???"
};

static const char *shift[8] =
{
  "rol", "ror", "rcl", "rcr", "shl", "shr", "sal", "sar"
};

static const char *f7_code[8] =
{
  "test", "???", "not", "neg", "mul", "imul", "div", "idiv"
};

static const char *ff_code[8] =
{
  "inc", "dec", "call", "call far", "jmp", "jmp far", "push", "???"
};

static const char *arith[8] =
{
  "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"
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

static const char *fop[8] =
{
  "0", "1", "2", "3", "4", "5", "6", "7"
};

static const char *loop_codes[] =
{
  "loopne", "loope", "loop", "jcxz"
};

/* bit set: instruction takes explicit float register operand */
static unsigned char freg_op[8] =
{
  0xff, 0x03, 0xd3, 0x00, 0xf0, 0x35, 0xf0, 0x00
};

#if defined(i386) || defined(vax)
/* Little-endian machines which allow unaligned addresses.  */
#define IMMED16(code) *(unsigned short *)(code)
#define IMMED32(code) *(unsigned int *)(code)
#define IMMED32I(code) *(int *)(code)
#else
/* Big-endian or strict alignment.  */
#define IMMED16(code) code[0] | (code[1] << 8)
#define IMMED32(code) code[0] | (code[1] << 8) | (code[2] << 16) | (code[3] << 24)
#define IMMED32I(code) code[0] | (code[1] << 8) | (code[2] << 16) | (code[3] << 24)
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

static INLINE const unsigned char *mod_rm(FILE *out, const unsigned char *code,
					  const char *seg, const char **regs,
					  int addr32)
{
  unsigned char rm = *code++;

  if (addr32)
    {
      char index[5];
      const char *scale="";
      const char *base;

      if ((rm >> 6) == 3)
	{
	  d86_printf(regs[rm & 7]);
	  return code;
	}
      if ((rm & 7) == 4)
	{
	  unsigned char sib = *code++;

	  base = reg32[sib & 7];

	  if (((sib >> 3) & 7) == 4)
	    {
	      index[0] = 0;
	      scale = "";
	    }
	  else
	    {
	      index[0] = '+';
	      index[1] = reg32[(sib >> 3) & 7][0];
	      index[2] = reg32[(sib >> 3) & 7][1];
	      index[3] = reg32[(sib >> 3) & 7][2];
	      index[4] = 0;

	      switch(sib >> 6)
		{
		case 0:
		  scale = "";
		  break;
		case 1:
		  scale = "*2";
		  break;
		case 2:
		  scale = "*4";
		  break;
		case 3:
		  scale = "*8";
		  break;
		}
	    }
	}
      else
	{
	  base = reg32[rm & 7];
	  index[0] = 0;
	  scale = "";
	}

      switch(rm >> 6)
	{
	case 2:
	  d86_printf("%s[%s%s%s%+d]", seg, reg32[rm & 7],
		  index, scale, immed32(code));
	  return code + 4;

	case 1:
	  d86_printf("%s[%s%s%s%+d]", seg, reg32[rm & 7], index, scale,
		  (signed char)code[0]);
	  return code + 1;

	case 0:
	  if ((rm & 7) == 5)
	    {
	      d86_printf("%s[%08x%s%s]", seg, immed32(code),
		      index, scale);
	      return code + 4;
	    }
	  d86_printf("%s[%s%s%s]", seg, reg32[rm & 7], index, scale);
	  return code;
	}
    }
  else
    {
      switch(rm >> 6)
	{
	case 3:
	  d86_printf(regs[rm & 7]);
	  return code;

	case 2:
	  d86_printf("%s[%s%+d]", seg, memref[rm & 7], (short)immed16(code));
	  return code + 2;

	case 1:
	  d86_printf("%s[%s%+d]", seg, memref[rm & 7], (signed char)code[0]);
	  return code + 1;

	case 0:
	  if ((rm & 7) == 6)
	    {
	      d86_printf("%s[%04x]", seg, code[0] + (code[1] << 8));
	      return code + 2;
	    }
	  d86_printf("%s[%s]", seg, memref[rm & 7]);
	  return code;
	}
    }
/* What should return be here? JES */
    d86_printf("No Answer in mod_rm");
    return 0;

}

static const unsigned char *mod_reg_rm(FILE *out, const unsigned char *code,
				   const char *seg, const char **sregs,
				   const char **regs, int addr32)
{
  d86_printf(sregs[(*code >> 3) & 7]);
  d86_printf(",");
  return mod_rm(out, code, seg, regs, addr32);
}

static const unsigned char *mod_reg_rm_r(FILE *out, const unsigned char *code,
				     const char *seg, const char **sregs,
				     const char **regs, int addr32)
{
  unsigned char rm = *code;

  code = mod_rm(out, code, seg, regs, addr32);
  d86_printf(",");
  d86_printf(sregs[(rm >> 3) & 7]);
  return code;
}

int  dis_8086(unsigned int org,
	      register const unsigned char *code,
	      unsigned char *outbuf,
	      int def_size,
	      unsigned int * refseg,
	      unsigned int * refoff)
{
  const unsigned char *code0 = code;
  FILE *out = 0;
  const char *seg = "";
  int data32 = def_size;
  int addr32 = def_size;
  int prefix = 0;
  int i;

  linebuf = outbuf;
  memset(linebuf, 0x00, IBUFS);

  #define SEG16REL(x) ((x)-(*refseg<<4))
  refaddr = refoff;
  *refaddr = 0;

  for(i=0;i<16;i++)
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
	  d86_printf(" ");
	  code = mod_reg_rm_r(out, code, seg, reg8, reg8, addr32);
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
	  d86_printf(" ");
	  code = mod_reg_rm_r(out, code, seg, wreg, wreg, addr32);
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
	  d86_printf(" ");
	  code = mod_reg_rm(out, code, seg, reg8, reg8, addr32);
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
	  d86_printf(" ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0x04:
	case 0x0c:
	case 0x14:
	case 0x1c:
	case 0x24:
	case 0x2c:
	case 0x34:
	case 0x3c:
	  d86_printf("%s al,%d", arith[opcode >> 3],
		  (signed char)*code++);
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
	      d86_printf("%s eax,0x%x", arith[opcode >> 3], immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf("%s ax,0x%hx", arith[opcode >> 3], immed16(code));
	      code += 2;
	    }
	  break;

	case 0x06:
	  d86_printf("push es");
	  break;

	case 0x07:
	  d86_printf("pop es");
	  break;

	case 0x0e:
	  d86_printf("push cs");
	  break;

	case 0x0f:
	  opcode = *code++;
	  switch (opcode)
	    {
	    case 0x00:
	      d86_printf(f00[(*code >> 3) & 7]);
	      d86_printf(" ");
	      code = mod_rm(out, code, seg, wreg, addr32);
	      break;
	    case 0x01:
	      d86_printf(f01[(*code >> 3) & 7]);
	      d86_printf(" ");
	      code = mod_rm(out, code, seg, wreg, addr32);
	      break;
	    case 0x02:
	      d86_printf("lar ");
	      code = mod_rm(out, code, seg, wreg, addr32);
	      break;
	    case 0x03:
	      d86_printf("lsl ");
	      code = mod_rm(out, code, seg, wreg, addr32);
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
	    case 0x10:
	      d86_printf("invlpg ");
	      code = mod_rm(out, code, seg, wreg, addr32);
	      break;
	    case 0x20:
	      d86_printf("mov ");
	      code = mod_reg_rm_r(out, code, seg, cr, reg32, addr32);
	      break;
	    case 0x21:
	      d86_printf("mov ");
	      code = mod_reg_rm_r(out, code, seg, dr, reg32, addr32);
	      break;
	    case 0x22:
	      d86_printf("mov ");
	      code = mod_reg_rm(out, code, seg, cr, reg32, addr32);
	      break;
	    case 0x23:
	      d86_printf("mov ");
	      code = mod_reg_rm(out, code, seg, dr, reg32, addr32);
	      break;
	    case 0x24:
	      d86_printf("mov ");
	      code = mod_reg_rm_r(out, code, seg, tr, reg32, addr32);
	      break;
	    case 0x26:
	      d86_printf("mov ");
	      code = mod_reg_rm(out, code, seg, tr, reg32, addr32);
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
		d86_printf("j%s %08x", conditions[opcode & 15],
			resolva((code-code0)+org + 4 + IMMED32I(code)));
		code += 4;
	      } else {
		d86_printf("j%s %04hx", conditions[opcode & 15],
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
	      d86_printf("set%s ", conditions[opcode & 15]);
	      code = mod_rm(out, code, seg, reg8, addr32);
	      break;

	    case 0xa0:
	      d86_printf("push fs");
	      break;

	    case 0xa1:
	      d86_printf("pop fs");
	      break;

	    case 0xa3:
	      d86_printf("bt ");
	      code = mod_reg_rm_r(out, code, seg, wreg, wreg, addr32);
	      break;

	    case 0xa4:
	      d86_printf("shld ");
	      mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	      d86_printf(",%d", *code++);
	      break;

	    case 0xa5:
	      d86_printf("shrd ");
	      mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	      d86_printf(",%d", *code++);
	      break;

	    case 0xa8:
	      d86_printf("push gs");
	      break;

	    case 0xa9:
	      d86_printf("pop gs");
	      break;

	    case 0xab:
	      d86_printf("bts ");
	      code = mod_reg_rm_r(out, code, seg, wreg, wreg, addr32);
	      break;

	    case 0xac:
	      d86_printf("shld ");
	      mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	      d86_printf(",cl");
	      break;

	    case 0xad:
	      d86_printf("shrd ");
	      mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	      d86_printf(",cl");
	      break;

	    case 0xb2:
	      d86_printf("lss ");
	      code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	      break;

	    case 0xb3:
	      d86_printf("btr ");
	      code = mod_reg_rm_r(out, code, seg, wreg, wreg, addr32);
	      break;

	    case 0xb4:
	      d86_printf("lfs ");
	      code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	      break;

	    case 0xb5:
	      d86_printf("lgs ");
	      code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	      break;

	    case 0xba:
	      d86_printf("bt? ");
	      code = mod_rm(out, code, seg, wreg, addr32);
	      d86_printf(",%d", *code++);
	      break;

	    case 0xbb:
	      d86_printf("btc ");
	      code = mod_reg_rm_r(out, code, seg, wreg, wreg, addr32);
	      break;

	    case 0xbc:
	      d86_printf("bsf ");
	      code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	      break;

	    case 0xbd:
	      d86_printf("bsr ");
	      code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	      break;

	    case 0xc8:
	    case 0xc9:
	    case 0xca:
	    case 0xcb:
	    case 0xcc:
	    case 0xcd:
	    case 0xce:
	    case 0xcf:
	      d86_printf("bswap %s", reg32[opcode & 7]);
	      break;

	    default:
	      d86_printf("ESC 0f %02x", opcode);
	      break;
	    }
	  break;

	case 0x16:
	  d86_printf("push ss");
	  break;

	case 0x17:
	  d86_printf("pop ss");
	  break;

	case 0x1e:
	  d86_printf("push ds");
	  break;

	case 0x1f:
	  d86_printf("pop ds");
	  break;

	case 0x26:
	  seg = "es:";
	  prefix = 1;
	  continue;

	case 0x27:
	  d86_printf("daa");
	  break;

	case 0x2e:
	  seg = "cs:";
	  prefix = 1;
	  continue;

	case 0x2f:
	  d86_printf("das");
	  break;

	case 0x36:
	  seg = "ss:";
	  prefix = 1;
	  continue;

	case 0x37:
	  d86_printf("aaa");
	  break;

	case 0x3e:
	  seg = "ds:";
	  prefix = 1;
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
	  d86_printf("inc %s", wreg[opcode & 7]);
	  break;

	case 0x48:
	case 0x49:
	case 0x4a:
	case 0x4b:
	case 0x4c:
	case 0x4d:
	case 0x4e:
	case 0x4f:
	  d86_printf("dec %s", wreg[opcode & 7]);
	  break;

	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57:
	  d86_printf("push %s", wreg[opcode & 7]);
	  break;

	case 0x58:
	case 0x59:
	case 0x5a:
	case 0x5b:
	case 0x5c:
	case 0x5d:
	case 0x5e:
	case 0x5f:
	  d86_printf("pop %s", wreg[opcode & 7]);
	  break;

	case 0x60:
	  d86_printf("pusha");
	  break;

	case 0x61:
	  d86_printf("popa");
	  break;

	case 0x62:
	  d86_printf("bound ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0x63:
	  d86_printf("arpl ");
	  code = mod_reg_rm_r(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0x64:
	  seg = "fs:";
	  prefix = 1;
	  continue;

	case 0x65:
	  seg = "gs:";
	  prefix = 1;
	  continue;

	case 0x66:
	  data32 = !data32;
	  prefix = 1;
	  continue;

	case 0x67:
	  addr32 = !addr32;
	  prefix = 1;
	  continue;

	case 0x68:
	  if (data32)
	    {
	      d86_printf("push %d", immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf("push %d", immed16(code));
	      code += 2;
	    }
	  break;

	case 0x69:
	  d86_printf("imul ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  if (data32)
	    {
	      d86_printf(",%d", immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf(",%hd", immed16(code));
	      code += 2;
	    }
	  break;

	case 0x6a:
	  d86_printf("push %d", (signed char)*code++);
	  break;

	case 0x6b:
	  d86_printf("imul ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  d86_printf(",%d", (signed char)*code++);
	  break;

	case 0x6c:
	  d86_printf("insb es:[%s]", addr32 ? "edi" : "di");
	  break;

	case 0x6d:
	  d86_printf("ins%c es:[%s]", data32 ? 'd' : 'w',
		  addr32 ? "edi" : "di");
	  break;

	case 0x6e:
	  d86_printf("outsb dx,%s[%s]", seg, addr32 ? "esi" : "si");
	  break;

	case 0x6f:
	  d86_printf("outs%c dx,%s[%s]", data32 ? 'd' : 'w',
		  seg, addr32 ? "esi" : "si");
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
	  if (addr32) d86_printf("j%s %08x", conditions[opcode & 15],
		  resolva((code-code0)+org + (signed char)*code + 1));
	  else d86_printf("j%s %04x", conditions[opcode & 15],
	          SEG16REL(resolva((code-code0)+org + (signed char)*code + 1)));
	  code++;
	  break;

	case 0x80:
	case 0x82:
	  d86_printf(arith[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, reg8, addr32);
	  d86_printf(",%d", (signed char)*code++);
	  break;

	case 0x81:
	  d86_printf(arith[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, wreg, addr32);
	  if (data32)
	    {
	      d86_printf(",0x%x", immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf(",0x%hx", immed16(code));
	      code += 2;
	    }
	  break;

	case 0x83:
	  d86_printf(arith[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, wreg, addr32);
	  d86_printf(",%d", (signed char)*code++);
	  break;

	case 0x84:
	  d86_printf("test ");
	  code = mod_reg_rm(out, code, seg, reg8, reg8, addr32);
	  break;

	case 0x85:
	  d86_printf("test ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0x86:
	  d86_printf("xchg ");
	  code = mod_reg_rm(out, code, seg, reg8, reg8, addr32);
	  break;

	case 0x87:
	  d86_printf("xchg ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0x88:
	  d86_printf("mov ");
	  code = mod_reg_rm_r(out, code, seg, reg8, reg8, addr32);
	  break;

	case 0x89:
	  d86_printf("mov ");
	  code = mod_reg_rm_r(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0x8a:
	  d86_printf("mov ");
	  code = mod_reg_rm(out, code, seg, reg8, reg8, addr32);
	  break;

	case 0x8b:
	  d86_printf("mov ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0x8c:
	  d86_printf("mov ");
	  code = mod_reg_rm_r(out, code, seg, sreg, reg16, addr32);
	  break;

	case 0x8d:
	  d86_printf("lea ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0x8e:
	  d86_printf("mov ");
	  code = mod_reg_rm(out, code, seg, sreg, reg16, addr32);
	  break;

	case 0x8f:
	  d86_printf("pop ");
	  code = mod_rm(out, code, seg, wreg, addr32);
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
	  d86_printf("xchg ax,%s", wreg[opcode & 7]);
	  break;

	case 0x98:
	  d86_printf("cbw");
	  break;

	case 0x99:
	  d86_printf("cwd");
	  break;

	case 0x9a:
	  d86_printf("call %04hx:%04hx", immed16(code+2),
		  immed16(code));
	  resolva( makeaddr( immed16(code+2), immed16(code)));
	  code += 4;
	  break;

	case 0x9b:
	  d86_printf("wait");
	  break;

	case 0x9c:
	  d86_printf("pushf");
	  break;

	case 0x9d:
	  d86_printf("popf");
	  break;

	case 0x9e:
	  d86_printf("sahf");
	  break;

	case 0x9f:
	  d86_printf("lahf");
	  break;

	case 0xa0:
	  if (addr32) {
	    d86_printf("mov al,%s[%08x]", seg, immed32(code));
	    code += 4;
	  } else {
	    d86_printf("mov al,%s[%04hx]", seg, immed16(code));
	    code += 2;
	  }
	  break;

	case 0xa1:
	  if (addr32) {
	    d86_printf("mov ax,%s[%08x]", seg, immed32(code));
	    code += 4;
	  } else {
	    d86_printf("mov ax,%s[%04hx]", seg, immed16(code));
	    code += 2;
	  }
	  break;

	case 0xa2:
	  if (addr32) {
	    d86_printf("mov %s[%08x],al", seg, immed32(code));
	    code += 4;
	  } else {
	    d86_printf("mov %s[%04hx],al", seg, immed16(code));
	    code += 2;
	  }
	  break;

	case 0xa3:
	  if (addr32) {
	    d86_printf("mov %s[%08x],ax", seg, immed32(code));
	    code += 4;
	  } else {
	    d86_printf("mov %s[%04hx],ax", seg, immed16(code));
	    code += 2;
	  }
	  break;

	case 0xa4:
	  d86_printf("movsb es:[%s],%s[%s]", addr32 ? "edi" : "di",
		  seg, addr32 ? "esi" : "si");
	  break;

	case 0xa5:
	  d86_printf("movs%c es:[%s],%s[%s]", data32 ? 'd' : 'w',
		  addr32 ? "edi" : "di", seg, addr32 ? "esi" : "si");
	  break;

	case 0xa6:
	  d86_printf("cmpsb");
	  break;

	case 0xa7:
	  if (data32)
	    d86_printf("cmpsd");
	  else
	    d86_printf("cmpsw");
	  break;

	case 0xa8:
	  d86_printf("test al,%d", *code++);
	  break;

	case 0xa9:
	  if (data32)
	    {
	      d86_printf("test eax,0x%x", immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf("test ax,0x%hx", immed16(code));
	      code += 2;
	    }
	  break;

	case 0xaa:
	  d86_printf("stosb es:[%s]", addr32 ? "edi" : "di");
	  break;

	case 0xab:
	  d86_printf("stos%c es:[%s]", data32 ? 'd' : 'w',
		  addr32 ? "edi" : "di");
	  break;

	case 0xac:
	  d86_printf("lodsb %s[si]", seg);
	  break;

	case 0xad:
	  d86_printf("lods%c %s[%s]", data32 ? 'd' : 'c', seg,
		  addr32 ? "esi" : "si");
	  break;

	case 0xae:
	  d86_printf("scasb");
	  break;

	case 0xaf:
	  if (data32)
	    d86_printf("scasd");
	  else
	    d86_printf("scasw");
	  break;

	case 0xb0:
	case 0xb1:
	case 0xb2:
	case 0xb3:
	case 0xb4:
	case 0xb5:
	case 0xb6:
	case 0xb7:
	  d86_printf("mov %s,0x%x", reg8[opcode & 7], *code++);
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
	      d86_printf("mov %s,0x%x", wreg[opcode & 7], immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf("mov %s,0x%hx", wreg[opcode & 7], immed16(code));
	      code += 2;
	    }
	  break;

	case 0xc0:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, reg8, addr32);
	  d86_printf(",%d", 31 & *code++);
	  break;

	case 0xc1:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, wreg, addr32);
	  d86_printf(",%d", 31 & *code++);
	  break;

	case 0xc2:
	  d86_printf("ret %d", *code++);
	  break;

	case 0xc3:
	  d86_printf("ret");
	  break;

	case 0xc4:
	  d86_printf("les ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0xc5:
	  d86_printf("lds ");
	  code = mod_reg_rm(out, code, seg, wreg, wreg, addr32);
	  break;

	case 0xc6:
	  d86_printf("mov ");
	  code = mod_rm(out, code, seg, reg8, addr32);
	  d86_printf(",%d", (signed char)*code++);
	  break;

	case 0xc7:
	  d86_printf("mov ");
	  code = mod_rm(out, code, seg, wreg, addr32);
	  if (data32)
	    {
	      d86_printf(",0x%x", immed32(code));
	      code += 4;
	    }
	  else
	    {
	      d86_printf(",%d", immed16(code));
	      code += 2;
	    }
	  break;

	case 0xc8:
	  d86_printf("enter");
	  break;

	case 0xc9:
	  d86_printf("leave");
	  break;

	case 0xca:
	  d86_printf("retf %d", immed16(code));
	  code += 2;
	  break;

	case 0xcb:
	  d86_printf("retf");
	  break;

	case 0xcc:
	  d86_printf("int 3");
	  break;

	case 0xcd:
	  d86_printf("int %02x", *code++);
	  break;

	case 0xce:
	  d86_printf("into");
	  break;

	case 0xcf:
	  d86_printf("iret");
	  break;

	case 0xd0:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, reg8, addr32);
	  d86_printf(",1");
	  break;

	case 0xd1:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, wreg, addr32);
	  d86_printf(",1");
	  break;

	case 0xd2:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, reg8, addr32);
	  d86_printf(",cl");
	  break;

	case 0xd3:
	  d86_printf(shift[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, wreg, addr32);
	  d86_printf(",cl");
	  break;

	case 0xd4:
	  d86_printf("aam %d", *code++);
	  break;

	case 0xd5:
	  d86_printf("aad %d", *code++);
	  break;

	case 0xd7:
	  d86_printf("xlatb");
	  break;

	case 0xd8:
	  d86_printf(farith[(*code >> 3) & 7]);
	  d86_printf("s ");
	  code = mod_rm(out, code, seg, freg, addr32);
	  break;
	case 0xda:
	  d86_printf(farith[(*code >> 3) & 7]);
	  d86_printf("w ");
	  code = mod_rm(out, code, seg, freg, addr32);
	  break;
	case 0xdc:
	  d86_printf(farith[(*code >> 3) & 7]);
	  d86_printf("l ");
	  code = mod_rm(out, code, seg, freg, addr32);
	  break;
	case 0xde:
	  d86_printf(farith[(*code >> 3) & 7]);
	  d86_printf("i ");
	  code = mod_rm(out, code, seg, freg, addr32);
	  break;
	case 0xd9:
	case 0xdb:
	case 0xdd:
	case 0xdf:
	  d86_printf("esc %d,%d ", opcode & 7, ((*code >> 3) & 7));
	  if (freg_op[opcode & 7] & 1 << ((*code >> 3) & 7))
	    code = mod_rm(out, code, seg, freg, addr32);
	  else
	    code = mod_rm(out, code, seg, fop, addr32);
	  break;

	case 0xe0:
	case 0xe1:
	case 0xe2:
	case 0xe3:
	  if (addr32) d86_printf("%s %08x", loop_codes[opcode & 3], (code-code0)+org + (signed char)*code + 1);
	  else d86_printf("%s %04x", loop_codes[opcode & 3], SEG16REL((code-code0)+org + (signed char)*code + 1));
	  code++;
	  break;

	case 0xe4:
	  d86_printf("in al,0x%hx", *code++);
	  break;

	case 0xe5:
	  d86_printf("in ax,0x%hx", *code++);
	  break;

	case 0xe6:
	  d86_printf("out 0x%hx,al", *code++);
	  break;

	case 0xe7:
	  d86_printf("out 0x%hx,ax", *code++);
	  break;

	case 0xe8:
	  if (addr32) {
	    d86_printf("call %08x",
		    resolva((code-code0)+org + 4 + IMMED32I(code)));
	    code += 4;
	  } else {
	    d86_printf("call %04hx",
		    SEG16REL(resolva((code-code0)+org + 2 + code[0] + (code[1] << 8))));
	    code += 2;
	  }
	  break;

	case 0xe9:
	  if (addr32) {
	    d86_printf("jmp %08x",
		    resolva((code-code0)+org + 4 + IMMED32I(code)));
	    code += 4;
	  } else {
	    d86_printf("jmp %04hx",
		    SEG16REL((unsigned short)((code-code0)+org + 2 + code[0] + (code[1] << 8))));
	    code += 2;
	  }
	  break;

	case 0xea:
	  if (addr32) {
	    d86_printf("jmp %04hx:%08x", immed16(code+4),
		    immed32i(code));
	    code += 6;
	  } else {
	    d86_printf("jmp %04hx:%04hx", immed16(code+2),
		    immed16(code));
	    resolva( makeaddr( immed16(code+2), immed16(code)));
	    code += 4;
	  }
	  break;

	case 0xeb:
	  if (addr32) d86_printf("jmp %08x", resolva((code-code0)+org + (signed char)*code + 1));
	  else d86_printf("jmp %04x", SEG16REL(resolva((code-code0)+org + (signed char)*code + 1)));
	  code++;
	  break;

	case 0xec:
	  d86_printf("in al,dx");
	  break;

	case 0xed:
	  d86_printf("in ax,dx");
	  break;

	case 0xee:
	  d86_printf("out dx,al");
	  break;

	case 0xef:
	  d86_printf("out dx,ax");
	  break;

	case 0xf0:
	  d86_printf("lock ");
	  prefix = 1;
	  break;

	case 0xf2:
	  d86_printf("rep ");
	  prefix = 1;
	  continue;

	case 0xf3:
	  d86_printf("repe ");
	  prefix = 1;
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
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, reg8, addr32);
	  if (((opcode >> 3) & 7) == 0)
	    d86_printf(",%d", *code++);
	  break;

	case 0xf7:
	  opcode = *code;
	  d86_printf(f7_code[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, wreg, addr32);
	  if (((opcode >> 3) & 7) == 0)
	    {
	      if (data32)
		{
		  d86_printf(",%d", immed32(code));
		  code += 4;
		}
	      else
		{
		  d86_printf(",%d", immed16(code));
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
	  d86_printf(ff_code[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, reg8, addr32);
	  break;

	case 0xff:
	  d86_printf(ff_code[(*code >> 3) & 7]);
	  d86_printf(" ");
	  code = mod_rm(out, code, seg, wreg, addr32);
	  break;

	default:
	  d86_printf("%x", opcode);
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
}   /* end of dis_8086 */

