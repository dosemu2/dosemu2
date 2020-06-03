/*
 * DOSEMU debugger,  1995 Max Parke <mhp@light.lightlink.com>
 *
 * (c) Copyright 1995-1996, Max H. Parke under the terms of the
 *     GNU Public License, incorporated herein by reference
 *
 * This is file mhpdbgc.c
 *
 * changes:
 *
 *   10Jul96 Hans Lermen <lermen@elserv.ffm.fgan.de>
 *           next round of DPMI support:
 *           - new modes for mode command: +d, -d
 *             ( defaults to having DPMI enabled )
 *           - now can single step through DPMI-client
 *           - now can set breakpoints in DPMI-client
 *           - now has DPMI-INTx type breakpoints with matching for AX
 *           - fixed disassembler and addresses to reflect
 *             segmented protected mode (linear mode wasn't useable for DPMI)
 *           - ldt commando now skips NULL-entries.
 *
 *   19May96 Max Parke <mhp@lightlink.com>
 *           - added a little support for DPMI:
 *               - new 'ldt' command
 *               - if '#' as first char of addresses, use LDT to lookup
 *           - changed default 'd' command length to 128 bytes
 *           - changed handling of "previous" d and u commands
 *           - fix a warning on sprintf
 *
 *   16Sep95 Hans Lermen <lermen@elserv.ffm.fgan.de>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <regex.h>

#include "bitops.h"
#include "emu.h"
#include "cpu.h"
#include "timers.h"
#include "dpmi.h"
#include "int.h"
#include "hlt.h"
#include "utilities.h"
#include "dosemu_config.h"
#include "hma.h"
#include "bios_sym.h"
#include "dis8086.h"
#include "dos2linux.h"
#include "kvm.h"

#define MHP_PRIVATE
#include "mhpdbg.h"

#define makeaddr(x,y) ((((unsigned int)x) << 4) + (unsigned int)y)

/* prototypes */
static unsigned int mhp_getadr(char *, dosaddr_t *, unsigned int *,
    unsigned int *, unsigned int *, int);
static void mhp_regs  (int, char *[]);
static void mhp_r0    (int, char *[]);
static void mhp_dump    (int, char *[]);
static void mhp_disasm  (int, char *[]);
static void mhp_go      (int, char *[]);
static void mhp_stop    (int, char *[]);
static void mhp_trace   (int, char *[]);
static void mhp_tracec  (int, char *[]);
static void mhp_regs32  (int, char *[]);
static void mhp_bp      (int, char *[]);
static void mhp_bc      (int, char *[]);
static void mhp_bl      (int, char *[]);
static void mhp_bpint   (int, char *[]);
static void mhp_bcint   (int, char *[]);
static void mhp_bpintd  (int, char *[]);
static void mhp_bcintd  (int, char *[]);
static void mhp_bpload  (int, char *[]);
static void mhp_mode    (int, char *[]);
static void mhp_usermap (int, char *[]);
static void mhp_symbol  (int, char *[]);
static void mhp_kill    (int, char *[]);
static void mhp_memset  (int, char *[]);
static void mhp_print_ldt       (int, char *[]);
static void mhp_debuglog (int, char *[]);
static void mhp_dump_to_file (int, char *[]);
static void mhp_ivec    (int, char *[]);
static void mhp_mcbs    (int, char *[]);
static void mhp_devs    (int, char *[]);
static void mhp_ddrh    (int, char *[]);
static void mhp_dpbs    (int, char *[]);
static void mhp_bplog   (int, char *[]);
static void mhp_bclog   (int, char *[]);
static void print_log_breakpoints(void);

static int bpchk(dosaddr_t addr);

/* static data */
static unsigned int linmode = 0;
static unsigned int codeorg = 0;

static unsigned int dpmimode=1, saved_dpmimode=1;
#define IN_DPMI  (in_dpmi_pm() && dpmimode)

#define MAXSYM 10000
static struct {
  uint16_t seg;
  uint16_t off;
  enum {DYN, ABS} type;
  char name[49];
} user_symbol[MAXSYM];
int user_symbol_num;

static int trapped_bp=-1, trapped_bp_;

int traceloop=0;
char loopbuf[4] = "";

/* constants */
static const struct cmd_db cmdtab[] = {
   {"r0",            mhp_r0},
   {"r" ,            mhp_regs},
   {"m",             mhp_memset},
   {"d",             mhp_dump},
   {"u",             mhp_disasm},
   {"g",             mhp_go},
   {"stop",          mhp_stop},
   {"mode",          mhp_mode},
   {"t",             mhp_trace},
   {"ti",            mhp_trace},
   {"tc",            mhp_tracec},
   {"r32",           mhp_regs32},
   {"bp",            mhp_bp},
   {"bc",            mhp_bc},
   {"bl",            mhp_bl},
   {"bpint",         mhp_bpint},
   {"bcint",         mhp_bcint},
   {"bpintd",        mhp_bpintd},
   {"bcintd",        mhp_bcintd},
   {"bpload",        mhp_bpload},
   {"bplog",         mhp_bplog},
   {"bclog",         mhp_bclog},
   {"usermap",       mhp_usermap},
   {"symbol",        mhp_symbol},
   {"kill",          mhp_kill},
   {"ldt",           mhp_print_ldt},
   {"log",           mhp_debuglog},
   {"dump",          mhp_dump_to_file},
   {"ivec",          mhp_ivec},
   {"mcbs",          mhp_mcbs},
   {"devs",          mhp_devs},
   {"ddrh",          mhp_ddrh},
   {"dpbs",          mhp_dpbs},
   {"",              NULL}
};

/********/
/* CODE */
/********/

#define AXLIST_SIZE  32
static unsigned long mhp_axlist[AXLIST_SIZE];
static int axlist_count=0;


/* simple ASCII only toupper, this to avoid problems with the dotless i
   and here we only need to match the register si with SI etc. */
static unsigned char toupper_ascii(unsigned char c)
{
  if (c >= 'a' && c <= 'z')
    return c - ('a' - 'A');
  return c;
}

int mhp_getaxlist_value(int v, int mask)
{
  int i;
  for (i=0; i < axlist_count; i++) {
    if (!((v ^ mhp_axlist[i]) & mask)) return i;
  }
  return -1;
}

#if WITH_DPMI
static void mhp_delaxlist_value(int v, int mask)
{
  int i,j;

  for (i=0,j=0; i < axlist_count; i++) {
    if (((v ^ mhp_axlist[i]) & mask)) {
      mhp_axlist[j] = mhp_axlist[i];
      j++;
    }
  }
  axlist_count = j;
}

static int mhp_addaxlist_value(int v)
{
  if (mhp_getaxlist_value(v, -1) >= 0) return 1;
  if (axlist_count < (AXLIST_SIZE-1)) {
    mhp_axlist[axlist_count++]=v;
    return 1;
  }
  return 0;
}
#endif

static int getval_ul(char *s, int defaultbase, unsigned long *v)
{
  int base;
  char *endptr, *p;
  unsigned long ul;

  /* To prevent strtoul() recognising leading zero like 0123 as octal we
   * do our own radix processing. Valid literals are:
   *
   * 0xffff - hexadecimal
   * \xffff - hexadecimal
   * \d9999 - decimal
   * \o7777 - octal
   * \b1111 - binary
   */
  base = (defaultbase != 0) ? defaultbase : 10;
  p = s;

  if (strlen(s) >= 2 && s[0] == '\\') {
    p += 2; // assume we match
    if (s[1] == 'b')
      base = 2;
    else if (s[1] == 'o')
      base = 8;
    else if (s[1] == 'd')
      base = 10;
    else if (s[1] == 'x')
      base = 16;
    else
      p = s;
  }

  if (strncmp(s, "0x", 2) == 0) {
    p += 2;
    base = 16;
  }

  ul = strtoul(p, &endptr, base);
  if ((endptr == p) || (*endptr != '\0')) // no chars or trailing rubbish
    return 0;

  *v = ul;
  return 1;
}

static int getval_ui(char *s, int defaultbase, unsigned int *v)
{
  unsigned long ul;

  if (!getval_ul(s, defaultbase, &ul))
    return 0;

  if (ul > 0xffffffff)
    return 0;

  *v = (unsigned int)ul;
  return 1;
}

static char *getsym_from_dos_segofs(unsigned int seg, unsigned int off)
{
  int i;

  for (i = 0; i < user_symbol_num; i++) {
    if ((user_symbol[i].seg == seg) && (user_symbol[i].off == off) &&
        user_symbol[i].name[0])
      return user_symbol[i].name;
  }
  return NULL;
}

static char *getsym_from_dos_linear(unsigned int addr)
{
  int i;

  for (i = 0; i < user_symbol_num; i++) {
    if (addr == makeaddr(user_symbol[i].seg, user_symbol[i].off) &&
        user_symbol[i].name[0])
      return user_symbol[i].name;
  }
  return NULL;
}

static const char *getsym_from_bios(unsigned int seg, unsigned int off)
{
  dosaddr_t addr = SEGOFF2LINEAR(seg, off);
  int i;

  /* note only works correctly if BIOSSEG is the normalised value */
  if ((addr & 0xffff0000) >> 4 != BIOSSEG)
    return NULL;

  for (i = 0; i < bios_symbol_num; i++) {
    if (SEGOFF2LINEAR(BIOSSEG, bios_symbol[i].off) == addr)
      return bios_symbol[i].name;
  }

  return NULL;
}

static unsigned int getaddr_from_dos_sym(char *n1, unsigned int *v1, unsigned int *s1, unsigned int *o1)
{
  int i;

  if (!strlen(n1))
    return 0;

  for (i = 0; i < user_symbol_num; i++) {
    if (strcmp(user_symbol[i].name, n1) == 0) {
      *s1 = user_symbol[i].seg;
      *o1 = user_symbol[i].off;
      *v1 = makeaddr(*s1, *o1);
      return 1;
    }
  }

  return 0;
}

static unsigned int getaddr_from_bios_sym(char *n1, unsigned int *v1, unsigned int *s1, unsigned int *o1)
{
  int i;

  if (!strlen(n1))
    return 0;

  for (i = 0; i < bios_symbol_num; i++) {
    if (!strcmp(bios_symbol[i].name, n1)) {
      *s1 = BIOSSEG;
      *o1 = bios_symbol[i].off;
      *v1 = makeaddr(*s1, *o1);
      return 1;
    }
  }

  return 0;
}

static int check_for_stopped(void)
{
  if (!mhpdbgc.stopped) {
    mhp_printf("need to be in 'stopped' state for this command\n");
    mhp_send();
  }
  return mhpdbgc.stopped;
}

int mhp_usermap_load_gnuld(const char *fname, uint16_t origin)
{
  FILE *fp;
  char bytebuf[IBUFS];
  int num;
  char *p;
  unsigned int load_address, offset, tmp1, tmp2;

  if (!(fp = fopen(fname, "r"))) {
    return 0;
  }

  for (num = 0, load_address = 0; num < MAXSYM; /* */) {
    if (user_symbol[num].name[0]) { // Already set
      num++;
      continue;
    }
    if (!fgets(bytebuf, sizeof bytebuf, fp))
      break;

    // Set the current load address to be applied to the following symbols
/*.data           0x0000000000000000     0x12b8 load address 0x0000000000000790 */
    p = strstr(bytebuf, "load address");
    if (p) {
      if (!sscanf(p + 13, "%x", &load_address)) {
        return 0;
      }
      continue;
    }

/*                0x0000000000000600                MEMOFS = (DOS_PSP * 0x10)*/
    if (index(bytebuf, '='))
      continue;

    if (bytebuf[1] != ' ')
      continue;

/*_IO_FIXED_DATA
                0x0000000000000690        0x0 nlssupt.o */
    if (sscanf(bytebuf, "%x %x %*s", &tmp1, &tmp2) == 2)
      continue;

/* _FIXED_DATA    0x0000000000000000      0xaac kernel.o */
    if (sscanf(bytebuf, "%*s %x %x %*s", &tmp1, &tmp2) == 2)
      continue;

/*                0x000000000000000e                _NetBios */
    if (sscanf(bytebuf, "%x %48s", &offset, user_symbol[num].name) == 2) {
      user_symbol[num].type = DYN;
      user_symbol[num].seg = load_address >> 4;
      user_symbol[num].off = offset;

      user_symbol[num].seg += origin;

      num++;
    }
  }
  fclose(fp);

  if (user_symbol_num < num)
    user_symbol_num = num;

  return 1;
}

static void usermap_load_file_gnuld(const char *fname, uint16_t origin)
{
  int num = user_symbol_num;

  if (mhp_usermap_load_gnuld(fname, origin)) {
    mhp_printf("reading Gnu LD map file '%s'\n", fname);

    if ((user_symbol_num - num) <= 0) {
      mhp_printf("warning: failed to read any symbols from map file\n");
    } else if (user_symbol_num == MAXSYM) {
      mhp_printf("warning: symbol table full, some discarded\n");
    }

    mhp_printf("symbol table now contains %d symbol(s)\n", user_symbol_num);
  } else {
    mhp_printf("error: unable to open or parse map file '%s'\n", fname);
  }
}

static void usermap_load_file_mslink(const char *fname, uint16_t origin)
{
  const char *srchfor = "  Address         Publics by Value";
  FILE *fp;
  char bytebuf[IBUFS];
  unsigned int seg;
  unsigned int off;
  int num;

  if (!(fp = fopen(fname, "r"))) {
    mhp_printf("error: unable to open map file '%s'\n", fname);
    return;
  }

  mhp_printf("reading MSLINK map file '%s'\n", fname);
  while (1) {
    if (!fgets(bytebuf, sizeof(bytebuf), fp)) {
      fclose(fp);
      mhp_printf("error: unable to find significant section in map file\n");
      return;
    }
    if (!strlen(bytebuf))
      continue;
    if (!memcmp(bytebuf, srchfor, strlen(srchfor)))
      break;
  }
  for (num = 0; num < MAXSYM; /* */) {
    if (user_symbol[num].name[0]) { // Already set
      num++;
      continue;
    }
    if (!fgets(bytebuf, sizeof(bytebuf), fp))
      break;
    if (bytebuf[5] != ':')
      continue;

    if (memcmp(&bytebuf[12], "Abs ", 4) == 0)
      user_symbol[num].type = ABS;
    else if (memcmp(&bytebuf[12], "    ", 4) == 0)
      user_symbol[num].type = DYN;
    else
      continue;
    sscanf(&bytebuf[1], "%x:%x", &seg, &off);
    user_symbol[num].seg = seg;
    user_symbol[num].off = off;
    if (user_symbol[num].type == DYN)
      user_symbol[num].seg += origin;
    sscanf(&bytebuf[17], "%48s", user_symbol[num].name);

    num++;
  }
  fclose(fp);

  if (num == 0) {
    mhp_printf("warning: failed to read any symbols from map file\n");
    return;
  } else if (num == MAXSYM) {
    mhp_printf("warning: symbol table full, some discarded\n");
  }

  if (user_symbol_num < num)
    user_symbol_num = num;

  mhp_printf("symbol table now contains %d symbol(s)\n", user_symbol_num);
  return;
}

static void usermap_clear(void)
{
  memset(&user_symbol, 0, sizeof user_symbol);
  user_symbol_num = 0;
}

int mhp_usermap_move_block(uint16_t oldseg, uint16_t newseg,
                           uint16_t startoff, uint32_t blklen)
{
  dosaddr_t start = SEGOFF2LINEAR(oldseg, startoff);
  dosaddr_t end = start + blklen;
  int32_t delta = newseg - oldseg;
  int i;

  // Check for wrap here!
  if ((int32_t)oldseg + delta < 0)
    return 0;

  for (i = 0; i < user_symbol_num; i++) {
    dosaddr_t symaddr = SEGOFF2LINEAR(user_symbol[i].seg, user_symbol[i].off);
    if (user_symbol[i].name[0] && user_symbol[i].type == DYN &&
        symaddr >= start && symaddr <= end)
      user_symbol[i].seg += delta;
  }
  return 1;
}

static void mhp_usermap(int argc, char *argv[])
{
  unsigned int origin;

  if (argc < 2 ||
      (strcmp(argv[1], "list") != 0 &&
       strcmp(argv[1], "load-ms") != 0 &&
       strcmp(argv[1], "load-gnu") != 0 &&
       strcmp(argv[1], "clear") != 0)) {
    mhp_printf("syntax: usermap load-ms <file> [origin]\n");
    mhp_printf("syntax: usermap load-gnu <file> [origin]\n");
    mhp_printf("syntax: usermap clear\n");
    mhp_printf("syntax: usermap list\n");
    return;
  }

  if (strcmp(argv[1], "list") == 0) {
    int i;

    for (i = 0; i < user_symbol_num; i++) {
      if (user_symbol[i].name[0])
        mhp_printf("  %04x:%04x %s %s\n",
                   user_symbol[i].seg, user_symbol[i].off,
                   user_symbol[i].type == ABS ? "ABS" : "   ",
                   user_symbol[i].name);
    }
    return;
  }

  if (strcmp(argv[1], "clear") == 0) {
    usermap_clear();
    return;
  }

  // load
  if (argc < 3) {
    mhp_printf("error: load requires a map file argument\n");
    return;
  }

  if (argc >= 4) {
    if (!getval_ui(argv[3], 16, &origin)) {
      mhp_printf("error: origin parse error '%s'\n", argv[3]);
      return;
    }
  } else {
    origin = 0;
  }

  if (strcmp(argv[1], "load-ms") == 0)
    usermap_load_file_mslink(argv[2], origin);
  else
    usermap_load_file_gnuld(argv[2], origin);
}

static void mhp_symbol(int argc, char *argv[])
{
  dosaddr_t symaddr, target;
  uint32_t d, lastd;
  int i, lasti;

  if (argc > 1) {
    unsigned int seg, off, limit;

    if (!mhp_getadr(argv[1], &target, &seg, &off, &limit, IN_DPMI)) {
      mhp_printf("Invalid address\n");
      return;
    }
  } else {
    target = SEGOFF2LINEAR(_CS, _IP);
  }

  for (i = 0, lastd = UINT32_MAX; i < user_symbol_num; i++) {
    if (!user_symbol[i].name[0])
      continue;

    symaddr = SEGOFF2LINEAR(user_symbol[i].seg, user_symbol[i].off);
    if (symaddr > target)
      continue;

    d = target - symaddr;
    if (d < lastd) {
      lastd = d;
      lasti = i;
    }
  }

  if (lastd == UINT32_MAX)
    mhp_printf("No symbols found\n");
  else
    mhp_printf("  %s @ %04x:%04x with distance %" PRIu32 "\n",
               user_symbol[lasti].name,
               user_symbol[lasti].seg, user_symbol[lasti].off, lastd);
}

enum {
  V_NONE=0, V_BYTE=1, V_WORD=2, V_DWORD=4, V_STRING=8
};

static int decode_symreg(char *regn, regnum_t *sym, int *typ)
{
  const char *reg_syms[] = {
    "SS", "CS", "DS", "ES", "FS", "GS",
    "AX", "BX", "CX", "DX", "SI", "DI", "BP", "SP", "IP", "FL",
    "EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP", "ESP", "EIP",
    NULL
  }, *p;

  int n;

  if (!isalpha(*regn))
    return 0;

  for (n = 0, p = reg_syms[0]; p ; p = reg_syms[++n]) {
    if (strcasecmp(regn, p) == 0) {
      if (typ)
        *typ = (n < _EAXr) ? V_WORD : V_DWORD;
      *sym = n;
      return 1;
    }
  }

  return 0;
}

static unsigned long mhp_getreg(regnum_t symreg)
{
  if (IN_DPMI)
    return dpmi_mhp_getreg(symreg);

  switch (symreg) {
    case _SSr: return SREG(ss);
    case _CSr: return SREG(cs);
    case _DSr: return SREG(ds);
    case _ESr: return SREG(es);
    case _FSr: return SREG(fs);
    case _GSr: return SREG(gs);
    case _AXr: return LWORD(eax);
    case _BXr: return LWORD(ebx);
    case _CXr: return LWORD(ecx);
    case _DXr: return LWORD(edx);
    case _SIr: return LWORD(esi);
    case _DIr: return LWORD(edi);
    case _BPr: return LWORD(ebp);
    case _SPr: return LWORD(esp);
    case _IPr: return LWORD(eip);
    case _FLr: return get_FLAGS(REG(eflags));
    case _EAXr: return REG(eax);
    case _EBXr: return REG(ebx);
    case _ECXr: return REG(ecx);
    case _EDXr: return REG(edx);
    case _ESIr: return REG(esi);
    case _EDIr: return REG(edi);
    case _EBPr: return REG(ebp);
    case _ESPr: return REG(esp);
    case _EIPr: return REG(eip);
  }

  assert(0);
  return -1; // keep compiler happy, but control never reaches here
}


static void mhp_setreg(regnum_t symreg, unsigned long val)
{
  if (IN_DPMI) {
    dpmi_mhp_setreg(symreg, val);
    return;
  }

  switch (symreg) {
    case _SSr: SREG(ss) = val; break;
    case _CSr: SREG(cs) = val; break;
    case _DSr: SREG(ds) = val; break;
    case _ESr: SREG(es) = val; break;
    case _FSr: SREG(fs) = val; break;
    case _GSr: SREG(gs) = val; break;
    case _AXr: LWORD(eax) = val; break;
    case _BXr: LWORD(ebx) = val; break;
    case _CXr: LWORD(ecx) = val; break;
    case _DXr: LWORD(edx) = val; break;
    case _SIr: LWORD(esi) = val; break;
    case _DIr: LWORD(edi) = val; break;
    case _BPr: LWORD(ebp) = val; break;
    case _SPr: LWORD(esp) = val; break;
    case _IPr: LWORD(eip) = val; break;
    case _FLr: set_FLAGS(val); break;
    case _EAXr: REG(eax) = val; break;
    case _EBXr: REG(ebx) = val; break;
    case _ECXr: REG(ecx) = val; break;
    case _EDXr: REG(edx) = val; break;
    case _ESIr: REG(esi) = val; break;
    case _EDIr: REG(edi) = val; break;
    case _EBPr: REG(ebp) = val; break;
    case _ESPr: REG(esp) = val; break;
    case _EIPr: REG(eip) = val; break;

    default:
      assert(0);
  }
}

static void mhp_go(int argc, char * argv[])
{
   unfreeze_dosemu();
   if (!mhpdbgc.stopped) {
      mhp_printf("already in running state\n");
   } else {
      dosaddr_t csip = mhp_getcsip_value();
      mhpdbgc.stopped = 0;
      dpmimode = 1;
      if (bpchk(csip)) {
        dpmi_mhp_setTF(1);
        set_TF();
        mhpdbgc.trapcmd = TRACE_OVER;
        trapped_bp = -1;
        return;
      }
      dpmi_mhp_setTF(0);
      clear_TF();
      if (mhpdbgc.saved_if)
         set_IF();
      mhp_bpset();
   }
}

static void mhp_r0(int argc, char *argv[])
{
   if (trapped_bp == -2) trapped_bp=trapped_bp_;
   else trapped_bp=-1;
   mhp_bpclr();
   mhp_regs(argc,argv);
}

static void mhp_stop(int argc, char * argv[])
{
   if (mhpdbgc.stopped) {
      mhp_printf("already in stopped state\n");
   } else {
      mhpdbgc.saved_if = isset_IF();
      clear_IF();
      mhpdbgc.stopped = 1;
      mhp_cmd("r0");
   }
}

static void mhp_trace(int argc, char *argv[])
{
  dosaddr_t current_instruction = mhp_getcsip_value();
  unsigned char *csp = LINEAR2UNIX(current_instruction);

  if (!check_for_stopped())
    return;

  mhpdbgc.stopped = 0;
  dpmimode = 1;
  if (dpmi_active())
    dpmi_mhp_setTF(1);
  set_TF();

  mhpdbgc.trapcmd = (strcmp(argv[0], "ti") == 0) ? TRACE_INTO : TRACE_OVER;

  if (mhpdbgc.trapcmd == TRACE_OVER) {               // plain 't'
    if (csp[0] == 0xcd) {  // INT x
      if (!mhp_setbp(current_instruction + 2, 1)) {
        mhp_printf("Breakpoint at traceover location already exists\n");
      }
      mhpdbgc.stopped = 0;
      dpmimode = 1;
      dpmi_mhp_setTF(0);
      clear_TF();
      if (mhpdbgc.saved_if)
        set_IF();
      mhp_bpset();
    } else {
      mhpdbgc.trapcmd = TRACE_INTO;                  // downgrade to single step
    }
  }

  if (mhpdbgc.trapcmd == TRACE_INTO) {               // 'ti'
    switch (csp[0]) {
      case 0xcc:  // INT3
        mhp_modify_eip(+1);
        do_int(3);
        set_TF();
        mhpdbgc.stopped = 1;
        mhp_cmd("r0");
        mhpdbgc.int_handled = 1;
        break;
      case 0xcd:  // INT x
        mhp_modify_eip(+2);
        do_int(csp[1]);
        set_TF();
        mhpdbgc.stopped = 1;
        mhp_cmd("r0");
        mhpdbgc.int_handled = 1;
        break;
      case 0xcf:  // IRET
        mhp_modify_eip(+1);
        fake_iret();
        set_TF();
        mhpdbgc.stopped = 1;
        mhp_cmd("r0");
        break;
    }
  }
}

static void mhp_tracec(int argc, char *argv[])
{
  if (!check_for_stopped())
    return;

  mhp_trace(argc, argv);
  traceloop = 1;
  loopbuf[0] = 't';
  loopbuf[1] = '\0';
}

static void mhp_dump(int argc, char * argv[])
{
   static char lastd[32];

   unsigned int nbytes;
   dosaddr_t seekval;
   int i,i2;
   unsigned int buf = 0;
   unsigned int seg;
   unsigned int off;
   unsigned int limit;
   int data32=0;
   int unixaddr;
   unsigned char c;

   if (argc > 1) {
      if (!mhp_getadr(argv[1], &seekval, &seg, &off, &limit, IN_DPMI)) {
         mhp_printf("Invalid ADDR\n");
         return;
      }
      snprintf(lastd, sizeof(lastd), "%s", argv[1]);
   } else {
      if (!strlen(lastd)) {
         mhp_printf("No previous \'d\' command\n");
         return;
      }
      if (!mhp_getadr(lastd, &seekval, &seg, &off, &limit, IN_DPMI)) {
         mhp_printf("Invalid ADDR\n");
         return;
      }
   }
   buf = seekval;

   if (argc > 2) {
     if (!getval_ui(argv[2], 0, &nbytes) || nbytes == 0 || nbytes > 256) {
       mhp_printf("Invalid size '%s'\n", argv[2]);
       return;
     }
   } else {
     nbytes = 128;
   }

#if 0
   mhp_printf( "seekval %08lX nbytes:%d\n", seekval, nbytes);
#else
   mhp_printf( "\n");
#endif
   if (IN_DPMI && seg)
     data32 = dpmi_segment_is32(seg);
   unixaddr = linmode == 2 && seg == 0 && limit == 0xFFFFFFFF;
   for (i=0; i<nbytes; i++) {
      if ((i&0x0f) == 0x00) {
         if (seg != 0 || limit != 0xFFFFFFFF) {
            if (data32) mhp_printf("%s%04x:%08x ", IN_DPMI?"#":"" ,seg, off+i);
            else mhp_printf("%s%04x:%04x ", IN_DPMI?"#":"" ,seg, off+i);
         }
	 else if (unixaddr)
            mhp_printf( "%#08x ", seekval+i);
	 else
            mhp_printf( "%08X ", seekval+i);
      }
      if (unixaddr)
	c = UNIX_READ_BYTE((uintptr_t)buf+i);
      else
	c = READ_BYTE(buf+i);
      mhp_printf( "%02X ", c);
      if ((i&0x0f) == 0x0f) {
         mhp_printf( " ");
         for (i2=i-15;i2<=i;i2++){
	    if (unixaddr)
	       c = UNIX_READ_BYTE((uintptr_t)buf+i2);
	    else
	       c = READ_BYTE(buf+i2);
	    c &= 0x7F;
	    if (c >= 0x20) {
	       mhp_printf( "%c", c);
            } else {
               mhp_printf( "%c", '.');
            }
         }
         mhp_printf( "\n");
      }
   }

   if (seg != 0 || limit != 0xFFFFFFFF) {
      if ((lastd[0] == '#') || (IN_DPMI)) {
         snprintf(lastd, sizeof(lastd), "#%x:%x", seg, off + i);
      } else {
         snprintf(lastd, sizeof(lastd), "%x:%x", seg, off + i);
      }
   } else if (unix) {
      snprintf(lastd, sizeof(lastd), "%#x", seekval + i);
   } else {
      snprintf(lastd, sizeof(lastd), "%x", seekval + i);
   }
}

static void mhp_dump_to_file(int argc, char * argv[])
{
   unsigned int nbytes;
   dosaddr_t seekval;
   const unsigned char *buf = 0;
   unsigned int seg;
   unsigned int off;
   unsigned int limit=0;
   int fd;

   if (argc <= 3) {
      mhp_printf("USAGE: dump <addr> <size> <filename>\n");
      return;
   }

   if (!mhp_getadr(argv[1], &seekval, &seg, &off, &limit, IN_DPMI)){
      mhp_printf("Invalid ADDR\n");
      return;
   }
   if (linmode == 2 && seg == 0 && limit == 0xFFFFFFFF)
     buf = (const unsigned char *)(uintptr_t)seekval;
   else
     buf = LINEAR2UNIX(seekval);

   if (!getval_ui(argv[2], 0, &nbytes) || nbytes == 0) {
     mhp_printf("Invalid size '%s'\n", argv[2]);
     return;
   }

   fd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 00775);
   if (fd < 0) {
      mhp_printf("cannot open/create file %s\n%s\n", argv[3], strerror(errno));
      return;
   }
   if (write(fd, buf, nbytes) != nbytes) {
      mhp_printf("write error: %s\n", strerror(errno));
   }
   close(fd);
}

static int is_valid_program_name(const char *s)
{
  const char *p;

  for (p = s; *p; p++) {
    if (iscntrlDOS(*p))
      return 0;
  }
  return (s[0] != 0); // at least one character long
}

static const char *get_name_from_mcb(struct MCB *mcb, int *is_lnk)
{
  const char *dos = "DOS", *fre = "FREE", *lnk = "LINK";
  static char name[9];

  if (is_lnk)
    *is_lnk = 0;
  if (mcb->owner_psp == 0)
    return fre;
  if (mcb->owner_psp == 8) {
    if (strcmp(mcb->name, "SC") == 0) {
      if (is_lnk)
        *is_lnk = 1;
      return lnk;
    }
    return dos;
  }
  snprintf(name, sizeof name, "%s", mcb->name);
  if (!is_valid_program_name(name))
    snprintf(name, sizeof name, "%05d", mcb->owner_psp);

  return name;
}

static const char *get_mcb_name_walk_chain(uint16_t seg, uint16_t off)
{
  char *start, *end, *target = MK_FP32(seg, off);
  struct MCB *mcb;

  if (!lol)
    return NULL;

  for (mcb = MK_FP32(READ_WORD(lol - 2), 0); mcb->id == 'M'; /* */) {
    start = ((char *)mcb) + 16;
    end = start + mcb->size * 16;
    if (target >= start && target < end)
      return get_name_from_mcb(mcb, NULL);

    mcb = (struct MCB *)end;
  }
  if (mcb->id != 'Z') {
    g_printf("MCB chain corrupt - missing final entry\n");
  }
  return NULL;
}

static const char *get_mcb_name_segment_psp(uint16_t seg, uint16_t off)
{
  struct PSP *psp = MK_FP32(seg, 0);
  char *start, *end, *target = MK_FP32(seg, off);
  struct MCB *mcb;

  if (psp->opint20 != 0x20cd) // INT 20
    return NULL;

  mcb = MK_FP32(seg - 1, 0);
  if (mcb->id != 'M')
    return NULL;

  start = ((char *)mcb) + 16;
  end = start + mcb->size * 16;
  if (target < start || target >= end)
    return NULL;

  return get_name_from_mcb(mcb, NULL);
}

static void mhp_ivec(int argc, char *argv[])
{
  unsigned int i, j, dmin, dmax;
  uint16_t sseg, soff;
  struct {
    unsigned char jmp_to_code[2];
    uint16_t ooff, oseg;
    uint16_t sig; // 0x424b ("KB")
    unsigned char flag;
    unsigned char jmp_to_hrst[2];
    unsigned char pad[7];
  } __attribute__((packed)) *c;
  const char *s;

  if (argc < 2) {
    dmin = 0;
    dmax = 0xff;
  } else if (getval_ui(argv[1], 16, &dmin) && dmin <= 0xff) {
    dmax = dmin;
  } else {
    mhp_printf("Invalid interrupt number\n");
    return;
  }

  mhp_printf("Interrupt vector table:\n");

  for (i = dmin; i <= dmax; i++) {
    sseg = ISEG(i);
    soff = IOFF(i);
    if ((sseg || soff) || (dmin == dmax)) {

      // Show the head of any interrupt chain
      mhp_printf("  %02x  %04X:%04X", i, sseg, soff);

      // Print the name of the owning program if we can
      if ((s = get_mcb_name_segment_psp(sseg, soff)) ||
          (s = get_mcb_name_walk_chain(sseg, soff))) {
        mhp_printf("[%s]", s);
      }

      // Display any symbol we have for that address
      if ((s = getsym_from_bios(sseg, soff)) ||
          (s = getsym_from_dos_segofs(sseg, soff)))
        mhp_printf("(%s)\n", s);
      else
        mhp_printf("\n");

      // See if this handler follows the IBM shared interrupt specification
      for (j = 0; (sseg || soff); j++) {
        if (j > 255) {
          mhp_printf("Hops exceeded, possible circular reference\n");
          break;
        }
        c = MK_FP32(sseg, soff);
        if (c->sig == 0x424b) {
          sseg = c->oseg;
          soff = c->ooff;
          mhp_printf("   => %04X:%04X", sseg, soff);
          if ((s = get_mcb_name_segment_psp(sseg, soff)) ||
              (s = get_mcb_name_walk_chain(sseg, soff))) {
            mhp_printf("[%s]", s);
          }
          if ((s = getsym_from_bios(sseg, soff)) ||
              (s = getsym_from_dos_segofs(sseg, soff)))
            mhp_printf("(%s)\n", s);
          else
            mhp_printf("\n");
        } else {
          sseg = soff = 0;
        }
      }
    }
  }
}

static void print_mcb(struct MCB *mcb, uint16_t seg)
{
  int lnk;
  const char *name = get_name_from_mcb(mcb, &lnk);

  if (mcb->id == 'M') {
    if (lnk)
      mhp_printf("%04x:0000 ------ [%s]\n", seg, name);
    else
      mhp_printf("%04x:0000 0x%04x [%s]\n", seg, mcb->size, name);
  } else if (mcb->id == 'Z') {
    mhp_printf("%04x:0000 0x%04x [%s] (END)\n", seg, mcb->size, name);
  }
}

static void print_dscb(struct DSCB *dscb)
{
  const char *stnam;
  char name[9];
  char buf[80];

  switch (dscb->stype) {
    case 'D':
      snprintf(name, sizeof name, "%s", dscb->fname);
      snprintf(buf, sizeof buf, "Driver (%s)", name);
      stnam = buf;
      break;
    case 'E':
      stnam = "Driver Extension";
      break;
    case 'I':
      snprintf(name, sizeof name, "%s", dscb->fname);
      snprintf(buf, sizeof buf, "Installable Filesystem (%s)", name);
      stnam = buf;
      break;
    case 'F':
      stnam = "Files";
      break;
    case 'X':
      stnam = "FCBs Extension";
      break;
    case 'C':
      stnam = "EMS Buffers";
      break;
    case 'B':
      stnam = "Buffers";
      break;
    case 'L':
      stnam = "CDS Array";
      break;
    case 'S':
      stnam = "Stacks";
      break;
    default:
      stnam = "Unknown Type";
      break;
  }
  mhp_printf("     %04x:0000 0x%04x [%c] %s\n",
      dscb->start - 1, // dscb->start points to its data
      dscb->size, dscb->stype, stnam);
}

static void mhp_mcbs(int argc, char *argv[])
{
  struct MCB *mcb;
  uint16_t seg;
  int uma, hdr;
  struct DSCB *dscb;
  uint16_t dsseg;

  if (!lol) {
    mhp_printf("DOS's LOL not set\n");
    return;
  }

  for (seg = READ_WORD(lol - 2), mcb = MK_FP32(seg, 0), uma = 0, hdr = 1;
       mcb->id == 'M' || mcb->id == 'Z';
       seg += (1 + mcb->size), mcb = MK_FP32(seg, 0)) {
    if (mcb->id == 'M') {
      if (hdr) {
        mhp_printf("\nADDR(%s) PARAS  OWNER\n", uma == 0 ? "LOW" : "UMA");
        hdr = 0;
      }
      print_mcb(mcb, seg);

      /* is this a DOS data segment */
      if (mcb->owner_psp == 8 && mcb->name[0] == 'S' && mcb->name[1] == 'D') {
        mhp_printf("  => ADDR      PARAS TYPE USAGE\n");
        for (dsseg = seg + 1; dsseg < seg + mcb->size; dsseg = dscb->start + dscb->size) {
          dscb = MK_FP32(dsseg, 0);
          print_dscb(dscb);
        }
      }
    } else /* mcb->id == 'Z' */ {
      print_mcb(mcb, seg);
      if (uma)
        break;
      uma = 1;
      hdr = 1;
    }
  }
}

static void mhp_devs(int argc, char *argv[])
{
  struct DDH *dev;
  FAR_PTR p;
  int cnt;

  const char *char_attr[] = {
    "STDIN", "STDOUT", "NULDEV", "CLOCK", "CONSOLE", "UNDEF5",
    "UNDEF6", "UNDEF7", "UNDEF8", "UNDEF9", "UNDEF10", "UNDEF11", "UNDEF12",
    "Output until busy", "IOCTL"
  };

  const char *bloc_attr[] = {
    "Generic IOCTL", "UNDEF1", "UNDEF2", "UNDEF3", "UNDEF4", "UNDEF5",
    "Get/Set logical device calls", "UNDEF7", "UNDEF8", "UNDEF9", "UNDEF10",
    "Removable media calls", "UNDEF12", "Non IBM", "IOCTL"
  };

  if (!lol) {
    mhp_printf("DOS's LOL not set\n");
    return;
  }

  mhp_printf("DOS Devices\n\n");

  for (p = lol_nuldev(lol), cnt = 0; FP_OFF16(p) != 0xffff && cnt < 256; p = dev->next, cnt++) {
    int i;

    dev = FAR2PTR(p);

    mhp_printf("%04x:%04x", FP_SEG16(p), FP_OFF16(p));

    if (dev->attr & (1 << 15)) {
      char name[9], *q;

      memcpy(name, dev->name, 8);
      name[8] = '\0';
      q = strchr(name, ' ');
      if (q)
        *q = '\0';
      mhp_printf(" Char '%-8s'\n", name);
      mhp_printf("  Attributes: 0x%04x", dev->attr);
      mhp_printf(" (Char");
      for (i = 14; i >= 0; i--)
        if (dev->attr & (1 << i))
          mhp_printf(", %s", char_attr[i]);
    } else {
      mhp_printf(" Block (%d Units)\n", dev->name[0]);
      mhp_printf("  Attributes: 0x%04x", dev->attr);
      mhp_printf(" (Block");
      for (i = 14; i >= 0; i--)
        if (dev->attr & (1 << i))
          mhp_printf(", %s", bloc_attr[i]);
    }
    mhp_printf(")\n");

    mhp_printf("  Routines: Strategy(%04x:%04x), Interrupt(%04x:%04x)\n",
        FP_SEG16(p), FP_OFF16(dev->strat), FP_SEG16(p), FP_OFF16(dev->intr));

    mhp_printf("\n");
  }
}

static const char *cmd2name(uint8_t cmd)
{
  static char s[32];
  const char *n[] = {
    "Init", "Media Check", "Get BPB",
    "Ioctl Input", "Input", "Nondestructive Input", "Input Status",
    "Input Flush", "Output", "Output Verify", "Output Status",
    "Output Flush", "Ioctl Output", "Open", "Close", "Removable",
    "Output Busy", "Command 17", "Command 18", "Generic Ioctl",
    "Command 20", "Command 21", "Command 22", "Get Device", "Set Device",
  };

  if (cmd > 24) {
    snprintf(s, sizeof s, "Unknown command (%d)\n", cmd);
    return s;
  }

  return n[cmd];
}

static void mhp_ddrh(int argc, char *argv[])
{
  struct DDRH *req;

  if (argc > 1) {
    dosaddr_t val;
    unsigned int seg, off, limit;

    if (!mhp_getadr(argv[1], &val, &seg, &off, &limit, IN_DPMI)) {
      mhp_printf("Invalid address\n");
      return;
    }
    req = MK_FP32(seg, off);
  } else {
    mhp_printf("No address given\n");
    return;
  }

  mhp_printf("Request\n"
             "  length %d\n"
	     "  unit   %d\n"
	     "  command '%s'\n",
	     req->length, req->unit, cmd2name(req->command));

  switch (req->command) {

    case 0:	// Init
      mhp_printf("    nunits %d\n", req->init.nunits);
      mhp_printf("    break %04x:%04x\n", FP_SEG16(req->init.brk),
                                          FP_OFF16(req->init.brk));
      mhp_printf("    At Entry\n");
      mhp_printf("      cmdline %04x:%04x\n", FP_SEG16(req->init.cmdline),
                                              FP_OFF16(req->init.cmdline));
      mhp_printf("        => '%s'\n", (char *)FAR2PTR(req->init.cmdline));
      mhp_printf("    At Exit\n");
      mhp_printf("      address of the driver's NEAR ptr to BPB %04x:%04x\n",
                                          FP_SEG16(req->init.bpb),
                                          FP_OFF16(req->init.bpb));
      mhp_printf("    first_drive %d\n", req->init.first_drv);
      break;

    case 1:	// Media Check
      mhp_printf("    media id 0x%02x\n", req->media_check.id);
      mhp_printf("    status %d\n", req->media_check.status);
      break;

    case 2:	// Get BPB
      mhp_printf("    media id 0x%02x\n", req->get_bpb.id);
      mhp_printf("    buffer %04x:%04x\n", FP_SEG16(req->get_bpb.buf),
                                           FP_OFF16(req->get_bpb.buf));
      mhp_printf("    BPB %04x:%04x\n", FP_SEG16(req->get_bpb.bpb),
                                        FP_OFF16(req->get_bpb.bpb));
      break;

    case 3:	// Ioctl Read
    case 4:	// Read
    case 8:	// Write
    case 9:	// Write Verify
    case 12:	// Ioctl Write
      mhp_printf("    media id 0x%02x\n", req->io.id);
      mhp_printf("    buffer %04x:%04x\n", FP_SEG16(req->io.buf),
                                           FP_OFF16(req->io.buf));
      mhp_printf("    count %d\n", req->io.count);
      mhp_printf("    start %d\n", req->io.start);
      if (req->command != 3 && req->command != 12)
        mhp_printf("    volume id %04x:%04x\n", FP_SEG16(req->io.volumeid),
                                                FP_OFF16(req->io.volumeid));
      break;

    case 5:	// Nondestructive Input
      mhp_printf("    return value 0x%02x\n", req->nd_input.retval);
      break;

    case 6:	// Input Status
    case 7:	// Flush Input:
    case 10:	// Output Status
    case 11:	// Flush Output
    case 13:	// Open
    case 14:	// Close
    case 15:	// Removable
      /* No unique fields to print */
      break;

    default:
      mhp_printf("    Don't know how to parse this command structure\n");
      break;
  }

  mhp_printf("  status 0x%04x\n", req->status);
}

static void mhp_dpbs(int argc, char *argv[])
{
  struct DPB *dpbp;
  far_t p;
  int cnt;

  if (argc > 1) {
    dosaddr_t val;
    unsigned int seg, off, limit;

    if (!mhp_getadr(argv[1], &val, &seg, &off, &limit, IN_DPMI)) {
      mhp_printf("Invalid DPB address\n");
      return;
    }
    p = MK_FARt(seg, off);
  } else {
    if (!lol) {
      mhp_printf("DOS's LOL not set and no DPB address given\n");
      return;
    }
    p = lol_dpbfarptr(lol);
  }

#define DV v4
  mhp_printf("DPBs (compiled for DOS v4+ format)\n\n");

  for (cnt = 0; p.offset != 0xffff && cnt < 26; p = dpbp->DV.next_DPB, cnt++) {

    dpbp = FARt_PTR(p);
    if (!dpbp) {
      mhp_printf("Null DPB pointer\n");
      return;
    }

    mhp_printf("%04X:%04X (%c:)\n", p.segment, p.offset, 'A' + dpbp->drv_num);
    mhp_printf("  driver unit: %d\n", dpbp->unit_num);
    mhp_printf("  bytes_per_sect = 0x%x\n", dpbp->bytes_per_sect);
    mhp_printf("  last_sec_in_clust = 0x%x\n", dpbp->last_sec_in_clust);
    mhp_printf("  sec_shift = 0x%x\n", dpbp->sec_shift);
    mhp_printf("  reserv_secs = 0x%x\n", dpbp->reserv_secs);
    mhp_printf("  num_fats = 0x%x\n", dpbp->num_fats);
    mhp_printf("  root_ents = 0x%x\n", dpbp->root_ents);
    mhp_printf("  data_start = 0x%x\n", dpbp->data_start);
    mhp_printf("  max_clu = 0x%x\n", dpbp->max_clu);

    mhp_printf("  sects_per_fat = 0x%x\n", dpbp->DV.sects_per_fat);
    mhp_printf("  first_dir_off = 0x%x\n", dpbp->DV.first_dir_off);
    mhp_printf("  device driver = %04X:%04X\n", dpbp->DV.ddh_ptr.segment, dpbp->DV.ddh_ptr.offset);
    mhp_printf("  media_id = 0x%x\n", dpbp->DV.media_id);
    mhp_printf("  accessed = 0x%x\n", dpbp->DV.accessed);
    mhp_printf("  next_DPB = %04X:%04X\n", dpbp->DV.next_DPB.segment, dpbp->DV.next_DPB.offset);
    mhp_printf("  first_free_clu = 0x%x\n", dpbp->DV.first_free_clu);
    mhp_printf("  fre_clusts = 0x%x\n", dpbp->DV.fre_clusts);

    mhp_printf("\n");
  }
}

static void mhp_mode(int argc, char * argv[])
{
   if (argc >=2) {
     if (argv[1][0] == '0') linmode = 0;
     if (argv[1][0] == '1') linmode = 1;
     if (argv[1][0] == '2') linmode = 2;
     if (!strcmp(argv[1],"+d")) dpmimode=saved_dpmimode=1;
     if (!strcmp(argv[1],"-d")) dpmimode=saved_dpmimode=0;
   }
   mhp_printf ("current mode: %s, dpmi %s%s\n",
     linmode==2?"unix32":linmode?"lin32":"seg16", dpmimode?"enabled":"disabled",
     dpmimode!=saved_dpmimode ? (saved_dpmimode?"[default enabled]":"[default disabled]"):"");
   return;
}

static void mhp_disasm(int argc, char * argv[])
{
   static char lastu[32];

   int rc;
   unsigned int nbytes;
   unsigned int org;
   dosaddr_t seekval;
   int def_size;
   unsigned int bytesdone;
   int i;
   unsigned int buf = 0;
   char bytebuf[IBUFS];
   char frmtbuf[IBUFS];
   unsigned int seg;
   unsigned int off;
   unsigned int refseg;
   unsigned int ref;
   unsigned int limit;
   int segmented = (linmode == 0);
   const char *s;

   if (argc > 1) {
      if (!mhp_getadr(argv[1], &seekval, &seg, &off, &limit, IN_DPMI)) {
         mhp_printf("Invalid ADDR\n");
         return;
      }
      snprintf(lastu, sizeof(lastu), "%s", argv[1]);
   } else {
      if (!strlen(lastu)) {
         mhp_printf("No previous \'u\' command\n");
         return;
      }
      if (!mhp_getadr(lastu, &seekval, &seg, &off, &limit, IN_DPMI)) {
         mhp_printf("Invalid ADDR\n");
         return;
      }
   }

   if (argc > 2) {
     if (!getval_ui(argv[2], 0, &nbytes) || nbytes == 0 || nbytes > 256) {
       mhp_printf("Invalid size '%s'\n", argv[2]);
       return;
     }
   } else {
     nbytes = 32;
   }

#if 0
   mhp_printf( "seekval %08X nbytes:%d\n", seekval, nbytes);
#else
   mhp_printf( "\n");
#endif

   if (IN_DPMI) {
     def_size = (dpmi_segment_is32(seg)? 3:0);
     segmented =1;
   }
   else {
     if (seekval < (a20 ? 0x10fff0 : 0x100000)) def_size = (linmode? 3:0);
     else if (lastu[0] == '#')
        def_size = 0;
     else def_size = 3;
   }
   if (linmode == 2) {
     if (seg != 0 || limit != 0xFFFFFFFF)
       seekval += (uintptr_t)mem_base;
     def_size |= 4;
   }
   rc=0;
   buf = seekval;
   org = codeorg ? codeorg : seekval;

   for (bytesdone = 0; bytesdone < nbytes; bytesdone += rc) {
       if (!(def_size & 4) && segmented) {
          if ((s = getsym_from_bios(seg, off + bytesdone)) ||
              (s = getsym_from_dos_segofs(seg, off + bytesdone)))
             mhp_printf ("%s:\n", s);
       }
       if (IN_DPMI && !dpmi_is_valid_range(GetSegmentBase(seg) + off +
               bytesdone, 10))
          break;
       refseg = seg;
       rc = dis_8086(buf+bytesdone, frmtbuf, def_size, &ref,
                  (IN_DPMI ? GetSegmentBase(refseg) : refseg * 16));
       for (i=0;i<rc;i++) {
	   if(def_size&4)
	     sprintf(&bytebuf[i*2], "%02X", UNIX_READ_BYTE((uintptr_t)buf+bytesdone+i) );
	   else
	     sprintf(&bytebuf[i*2], "%02X", READ_BYTE(buf+bytesdone+i) );
           bytebuf[(i*2)+2] = 0x00;
       }
       if (segmented) {
          const char *x = (IN_DPMI ? "#" : "");
          if (def_size) {
            mhp_printf( "%s%04x:%08x %-16s %s", x, seg, off+bytesdone, bytebuf, frmtbuf);
          }
          else mhp_printf( "%s%04x:%04x %-16s %s", x, seg, off+bytesdone, bytebuf, frmtbuf);
          /*
           * FIXME - the following is clearly wrong as it won't print a
           * reference to a symbol at the start of the same segment, but
           * there's not currently any way to determine if there's an
           * immediate memory reference in dis_8086(). The alternative is
           * spurious printing of immediate memory references if a symbol
           * at seg:0000 has been defined.
           */
          if ((ref != (refseg << 4)) && ((s = getsym_from_dos_linear(ref))))
             mhp_printf ("(%s)", s);
       } else {
	  if (def_size&4)
	     mhp_printf( "%#08x: %-16s %s", org+bytesdone, bytebuf, frmtbuf);
	  else
	     mhp_printf( "%08x: %-16s %s", org+bytesdone, bytebuf, frmtbuf);
       }
       mhp_printf( "\n");
   }

   if (segmented) {
      if ((lastu[0] == '#') || (IN_DPMI)) {
         snprintf(lastu, sizeof(lastu), "#%x:%x", seg, off + bytesdone);
      } else {
         snprintf(lastu, sizeof(lastu), "%x:%x", seg, off + bytesdone);
      }
   } else {
      snprintf(lastu, sizeof(lastu), "%x", seekval + bytesdone);
   }
}

static int get_value(char *s, unsigned long *v)
{
   int len = strlen(s);
   int t;
   char *tt;
   const char *wl = " WL";
   regnum_t symreg;

   if (!len)
     return V_NONE;

   /* Double quoted string value */
   if (len >2) {
     if (s[0] == '"' && s[len-1] == '"') {
       s[len-1] = 0;
       return V_STRING;
     }
   }

   /* Type suffix */
   if ((tt = strchr(wl, toupper_ascii(s[len-1]))) !=0) {
     len--;
     s[len] = 0;
     t = (int)(tt - wl) << 1;
   } else {
     t = V_NONE;
   }

   /* Character constant i.e. 'A' or strangely 'ABCD' */
   if (len >2) {
     if (s[len-1] == '\'' && s[0] == '\'') {
       *v = 0;
       len -=2;
       if (len > sizeof(*v))
         return V_NONE;  // don't silently truncate value
       strncpy((char *)v, s+1, len);
       if (t != V_NONE) return t;
       if (len <  2) return V_BYTE;
       if (len <  4) return V_WORD;
       return V_DWORD;
     }
   }

   /* Register */
   if (decode_symreg(s, &symreg, &t)) {
     *v = mhp_getreg(symreg);
     return t;
   }

   /* Plain number */
   if (getval_ul(s, 0, v))
     return (t == V_NONE) ? V_BYTE : t;

   return V_NONE;
}

static void mhp_memset(int argc, char * argv[])
{
   int size;
   static dosaddr_t zapaddr = -1;
   unsigned int seg, off;
   unsigned long val;
   unsigned int limit;
   char *arg;

   if (argc < 3) {
     mhp_printf("USAGE: m ADDR <value>\n");
     return;
   }

   if (!strcmp(argv[1],"-")) {
     if (zapaddr == -1) {
        mhp_printf("Address invalid, no previous 'm' command with address\n");
        return;
     }
   } else {
     if (!mhp_getadr(argv[1], &zapaddr, &seg, &off, &limit, IN_DPMI)) {
        mhp_printf("Address invalid\n");
        return;
     }
   }

   if ((a20 ?0x10fff0 : 0x100000) < zapaddr) {
      mhp_printf("Address invalid\n");
      return;
   }

   argv += 2;
   while ((arg = *argv) != 0) {
      size = get_value(arg, &val);
      switch (size) {
        case V_NONE:
          mhp_printf("Value invalid\n");
          return;

        case V_BYTE:
        case V_WORD:
        case V_DWORD:
          if ((size == V_BYTE && val > 0xff) ||
              (size == V_WORD && val > 0xffff) ||
              (size == V_DWORD && val > 0xffffffff)) {
            mhp_printf("Value too large for data type\n");
            return;
          }
          MEMCPY_2DOS(zapaddr, &val, size);
          mhp_printf("Modified %d byte(s) at 0x%08x with value %#lx\n", size, zapaddr, val);
          zapaddr += size;
          break;

        case V_STRING:
          size = strlen(arg+1);
          MEMCPY_2DOS(zapaddr, arg+1, size);
          mhp_printf("Modified %d byte(s) at 0x%08x with value \"%s\"\n", size, zapaddr, arg+1);
          zapaddr += size;
          break;
      }
      argv++;
   }
}

static unsigned int mhp_getadr(char *a1, dosaddr_t *v1, unsigned int *s1,
      unsigned int *o1, unsigned int *lim, int use_ldt)
{
   char * srchp;
   unsigned int seg1;
   unsigned int off1;
   unsigned long ul1;
   int selector = 0;
   unsigned int base_addr, limit;
   regnum_t symreg;

   *lim = 0xFFFFFFFF;

   if (use_ldt) selector = 1;
   if (*a1 == '#') {
      selector = 1;
      *lim = 0;
      a1 ++;
   }
   if(*a1 == '*') {
      if (use_ldt) {
        dpmi_mhp_getcseip(&seg1,&off1);
        *lim = 0;
        selector=2;
      }
      else {
        *v1 = makeaddr(SREG(cs), LWORD(eip));
        *s1 = SREG(cs);
        *o1 = LWORD(eip);
        *lim = 0xFFFF;
        return 1;
      }
   }
   if (selector != 2) {
     if(*a1 == '$') {
        if (use_ldt) {
          dpmi_mhp_getssesp(&seg1,&off1);
          *lim = 0;
          selector=2;
        }
        else {
          *v1 = makeaddr(SREG(ss), LWORD(esp));
          *s1 = SREG(ss);
          *o1 = LWORD(esp);
          *lim = 0xFFFF;
          return 1;
        }
     }
     if (selector != 2) {
       if (getaddr_from_bios_sym(a1, v1, s1, o1)) {
          return 1;
       }
       if (getaddr_from_dos_sym(a1, v1, s1, o1)) {
          return 1;
       }
       if (!(srchp = strchr(a1, ':'))) {
          if (!getval_ul(a1, 16, &ul1))
            return 0;

          *v1 = ul1;
          *s1 = (ul1 >> 4);
          *o1 = (ul1 & 0b00001111);
          return 1;
       }

       // we have a colon in the address so split it
       *srchp = '\0';

       if (decode_symreg(a1, &symreg, NULL))
         seg1 = mhp_getreg(symreg);
       else if (!getval_ui(a1, 16, &seg1)) {
         mhp_printf("segment parse error '%s'\n", a1);
         return 0;
       }

       if (decode_symreg(srchp+1, &symreg, NULL))
         off1 = mhp_getreg(symreg);
       else if (!getval_ui(srchp+1, 16, &off1)) {
         mhp_printf("offset parse error '%s'\n", srchp+1);
         return 0;
       }
     }
   }
   *s1 = seg1;
   *o1 = off1;

   if (!selector) {
      *v1 = makeaddr(seg1, off1);
      *lim = 0xFFFF;
      return 1;
   }

   if (!(seg1 & 0x4)) {
     mhp_printf("GDT selectors not supported yet\n");
     return 0;
   }

   if (!DPMIValidSelector(seg1)) {
     mhp_printf("selector %x appears to be invalid\n", seg1);
     return 0;
   }

   base_addr = GetSegmentBase(seg1);
   limit = GetSegmentLimit(seg1);

   if (off1 >= limit) {
     mhp_printf("offset %x exceeds segment limit %x\n", off1, limit);
     return 0;
   }

   if (!dpmi_is_valid_range(base_addr + off1, 10)) {
     mhp_printf("CS:IP points to invalid memory\n");
     return 0;
   }

   *v1 = base_addr + off1;
   *lim = limit - off1;
   return 1;
}

int mhp_setbp(dosaddr_t seekval, int one_shot)
{
  int i;

  for (i = 0; i < MAXBP; i++) {
    if (mhpdbgc.brktab[i].brkaddr == seekval &&
        mhpdbgc.brktab[i].is_valid) {
      if (!one_shot)
        mhp_printf("Duplicate breakpoint, nothing done\n");
      return 0;
    }
  }
  for (i = 0; i < MAXBP; i++) {
    if (!mhpdbgc.brktab[i].is_valid) {
      if (i == trapped_bp)
        trapped_bp = -1;
      mhpdbgc.brktab[i].brkaddr = seekval;
      mhpdbgc.brktab[i].is_valid = 1;
      mhpdbgc.brktab[i].is_dpmi = IN_DPMI;
      mhpdbgc.brktab[i].is_one_shot = one_shot;
      return 1;
    }
  }
  mhp_printf("Breakpoint table full, nothing done\n");
  return 0;
}

int mhp_clearbp(dosaddr_t seekval)
{
  int i;

  for (i = 0; i < MAXBP; i++) {
    if (mhpdbgc.brktab[i].brkaddr == seekval &&
        mhpdbgc.brktab[i].is_valid) {
      mhp_bpclr();
      if (i == trapped_bp)
        trapped_bp = -1;
      mhpdbgc.brktab[i].brkaddr = 0;
      mhpdbgc.brktab[i].is_valid = 0;
      return 1;
    }
  }
  return 0;
}

static void mhp_bp(int argc, char * argv[])
{
   dosaddr_t seekval;
   unsigned int seg;
   unsigned int off;
   unsigned int limit;

   if (!check_for_stopped())
      return;

   if (argc < 2) {
      mhp_printf("location argument required\n");
      return;
   }

   if (!mhp_getadr(argv[1], &seekval, &seg, &off, &limit, IN_DPMI)) {
      mhp_printf("Invalid ADDR\n");
      return;
   }

   mhp_setbp(seekval, 0);
}

static void mhp_bl(int argc, char * argv[])
{
   int i1;

   mhp_printf( "Breakpoints:\n");
   for (i1=0; i1 < MAXBP; i1++) {
      if (mhpdbgc.brktab[i1].is_valid) {
         mhp_printf( "%d: %08x\n", i1, mhpdbgc.brktab[i1].brkaddr);
      }
   }
   mhp_printf( "Interrupts: ");
   for (i1=0; i1 < 256; i1++) {
      if (test_bit(i1, mhpdbg.intxxtab)) {
         mhp_printf( "%02x ", i1);
      }
   }
#if WITH_DPMI
   mhp_printf( "\nDPMI Interrupts:");
   for (i1=0; i1 < 256; i1++) {
     if (dpmi_mhp_intxxtab[i1]) {
       mhp_printf( " %02x", i1);
       if (dpmi_mhp_intxxtab[i1] & 0x80) {
         int i,j=0;
         mhp_printf("[");
         for (i=0; i < axlist_count; i++) {
           if ((mhp_axlist[i] >>16) == i1) {
             if (j)  mhp_printf(",");
             mhp_printf("%lx",mhp_axlist[i] & 0xffff);
             j++;
           }
         }
         mhp_printf("]");
       }
     }
   }
#endif
   mhp_printf( "\n");
   if (mhpdbgc.bpload)
     mhp_printf("bpload active\n");
   print_log_breakpoints();
   return;
}

static void mhp_bc(int argc, char * argv[])
{
   unsigned int num;

   if (!check_for_stopped())
     return;

   if (argc < 2 || !getval_ui(argv[1], 0, &num) || num >= MAXBP) {
     mhp_printf("Invalid breakpoint number\n");
     return;
   }

   if (!mhpdbgc.brktab[num].is_valid) {
     mhp_printf( "No breakpoint %d, nothing done\n", num);
     return;
   }

   mhpdbgc.brktab[num].brkaddr = 0;
   mhpdbgc.brktab[num].is_valid = 0;
   return;
}

static void mhp_bpint(int argc, char * argv[])
{
   unsigned int num;

   if (!check_for_stopped())
     return;

   if (argc < 2 || !getval_ui(argv[1], 16, &num) || num > 0xff) {
     mhp_printf("Invalid interrupt number\n");
     return;
   }

   if (test_bit(num, mhpdbg.intxxtab)) {
     mhp_printf( "Duplicate BPINT %02x, nothing done\n", num);
     return;
   }

   set_bit(num, mhpdbg.intxxtab);
   if (!test_bit(num, &vm86s.int_revectored)) {
     set_bit(num, mhpdbgc.intxxalt);
     set_revectored(num, &vm86s.int_revectored);
   }
   if (num == 0x21)
     mhpdbgc.int21_count++;

   return;
}

static void mhp_bcint(int argc, char * argv[])
{
   unsigned int num;

   if (!check_for_stopped())
     return;

   if (argc < 2 || !getval_ui(argv[1], 16, &num) || num > 0xff) {
     mhp_printf("Invalid interrupt number\n");
     return;
   }

   if (!test_bit(num, mhpdbg.intxxtab)) {
     mhp_printf( "No BPINT %02x set, nothing done\n", num);
     return;
   }

   clear_bit(num, mhpdbg.intxxtab);
   if (test_bit(num, mhpdbgc.intxxalt)) {
     clear_bit(num, mhpdbgc.intxxalt);
     reset_revectored(num, &vm86s.int_revectored);
   }
   if (num == 0x21)
     mhpdbgc.int21_count--;

   return;
}

static void mhp_bpintd(int argc, char * argv[])
{
#if WITH_DPMI
   unsigned int i1, v1=0;

   if (!check_for_stopped())
     return;

   if (argc < 2 || !getval_ui(argv[1], 16, &i1) || i1 > 0xff) {
     mhp_printf("Invalid interrupt number\n");
     return;
   }

   if (argc > 2) {
     if (!getval_ui(argv[2], 16, &v1) || v1 > 0xffff) {
       mhp_printf("Invalid ax value '%s'\n", argv[2]);
       return;
     }
     v1 |= (i1 << 16);
   }

   if ((!v1 && dpmi_mhp_intxxtab[i1]) || (mhp_getaxlist_value(v1,-1) >=0)) {
     if (v1)  mhp_printf( "Duplicate BPINTD %02x %04x, nothing done\n", i1, v1 & 0xffff);
     else mhp_printf( "Duplicate BPINTD %02x, nothing done\n", i1);
     return;
   }
   if (!dpmi_mhp_intxxtab[i1]) dpmi_mhp_intxxtab[i1]=7;
   if (v1) {
     if (mhp_addaxlist_value(v1)) dpmi_mhp_intxxtab[i1] |= 0x80;
   }
   if (config.cpu_vm_dpmi == CPUVM_KVM)
	 kvm_set_idt_default(i1);
#endif
}

static void mhp_bcintd(int argc, char * argv[])
{
#if WITH_DPMI
   unsigned int i1, v1=0;

   if (!check_for_stopped())
     return;

   if (argc < 2 || !getval_ui(argv[1], 16, &i1) || i1 > 0xff) {
     mhp_printf("Invalid interrupt number\n");
     return;
   }

   if (argc > 2) {
     if (!getval_ui(argv[2], 16, &v1) || v1 > 0xffff) {
       mhp_printf("Invalid ax value '%s'\n", argv[2]);
       return;
     }
     v1 |= (i1 << 16);
   }

   if ((!dpmi_mhp_intxxtab[i1]) || (v1 && (mhp_getaxlist_value(v1,-1) <0))) {
     if (v1)  mhp_printf( "No BPINTD %02x %04x, nothing done\n", i1, v1 & 0xffff);
     else mhp_printf( "No BPINTD %02x, nothing done\n", i1);
     return;
   }
   if (v1) {
     mhp_delaxlist_value(v1,-1);
     if (mhp_getaxlist_value(i1<<16,0xff0000) <0) dpmi_mhp_intxxtab[i1]=0;
   }
   else {
     mhp_delaxlist_value(i1<<16,0xff0000);
     dpmi_mhp_intxxtab[i1]=0;
   }
#endif
}

static void mhp_bpload(int argc, char * argv[])
{
   if (!check_for_stopped())
     return;

   if (mhpdbgc.bpload) {
     mhp_printf("load breakpoint already pending\n");
     return;
   }
   mhpdbgc.bpload=1;
   {
     volatile register int i=0x21; /* beware, set_bit-macro has wrong constraints */
     set_bit(i, mhpdbg.intxxtab);
     if (!test_bit(i, &vm86s.int_revectored)) {
          set_bit(i, mhpdbgc.intxxalt);
          set_revectored(i, &vm86s.int_revectored);
     }
   }
   mhpdbgc.int21_count++;
   return;
}

static char *flags_to_string(uint32_t flags)
{
  static char ret[17];
  const char *names = ".N..ODITSZ.A.P.C";
  int i;

  for (i = 0; i <= 15; i++)
    ret[15 - i] = (flags & (1 << i)) ? names[15 - i] : '_';
  ret[16] = '\0';
  return ret;
}

static void mhp_regs(int argc, char * argv[])
{
  unsigned long newval;
  int typ, size;
  regnum_t symreg;

  if (argc == 3) {  /* set register */
    if (!check_for_stopped())
      return;

    if (!decode_symreg(argv[1], &symreg, &typ)) {
      mhp_printf("invalid register name '%s'\n", argv[1]);
      return;
    }

    size = get_value(argv[2], &newval);
    if (size == V_NONE) {
      mhp_printf("cannot parse value '%s'\n", argv[2]);
      return;
    }
    if (size == V_STRING) {
      mhp_printf("string not valid here, use character literal instead\n");
      return;
    }

    if ((typ == V_WORD && newval > 0xffff) ||
         (typ == V_DWORD && newval > 0xffffffff)) {
      mhp_printf("value '0x%04lx' too large for register '%s'\n", newval, argv[1]);
      return;
    }

    mhp_setreg(symreg, newval);
    if (newval == mhp_getreg(symreg))
      mhp_printf("reg '%s' changed to '0x%04lx'\n", argv[1], newval);
    else
      mhp_printf("failed to set register '%s'\n", argv[1]);

    return;
  }

  if (argc != 1) {
    mhp_printf("additional arguments, not properly formatted set command\n");
    return;
  }

  if (DBG_TYPE(mhpdbgc.currcode) == DBG_INTx)
     mhp_printf( "INT 0x%02X, ", DBG_ARG(mhpdbgc.currcode));
  if (DBG_TYPE(mhpdbgc.currcode) == DBG_INTxDPMI)
     mhp_printf( "INT 0x%02X in DPMI, ", DBG_ARG(mhpdbgc.currcode) & 0xff);
  if (DBG_TYPE(mhpdbgc.currcode) == DBG_TRAP) {
     if (DBG_ARG(mhpdbgc.currcode)==1) mhp_printf( "Trap %d, ", DBG_ARG(mhpdbgc.currcode));
     else {
       if (mhpdbgc.bpload_bp==3) {
         mhpdbgc.bpload_bp=0;
         mhp_printf("At entry of program %s\n", mhpdbgc.bpload_cmd);
         if (mhpdbgc.bpload_cmdline[0]) {
           mhpdbgc.bpload_cmdline[mhpdbgc.bpload_cmdline[0]+1]=0;
           mhp_printf("command line: %s\n", mhpdbgc.bpload_cmdline+1);
         }
         else mhp_printf(" (empty command line)\n");
       }
       else mhp_printf( "Trap %d, ", DBG_ARG(mhpdbgc.currcode));
     }
  }
  if (DBG_TYPE(mhpdbgc.currcode) == DBG_GPF)
     mhp_printf( "General Protection Fault, ");

 if (!traceloop)
  mhp_printf( "system state: %s%s%s%s\n",
#ifdef X86_EMULATOR
       config.cpuemu > 1 ? "emulated," : "",
#else
       "",
#endif
       mhpdbgc.stopped ? "stopped" : "running",
       IN_DPMI ? " in DPMI" : (dpmi_active()?" in real mode while in DPMI":""),
       IN_DPMI ?(dpmi_mhp_getcsdefault()?"-32bit":"-16bit") : "");

  if (!dpmi_mhp_regs()) {
    mhp_printf("AX=%04x  BX=%04x  CX=%04x  DX=%04x",
                LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx));
    mhp_printf("  SI=%04x  DI=%04x  SP=%04x  BP=%04x",
                LWORD(esi), LWORD(edi), LWORD(esp), LWORD(ebp));
    mhp_printf("\nDS=%04x  ES=%04x  FS=%04x  GS=%04x  FL=%08x(%s)",
                SREG(ds), SREG(es), SREG(fs), SREG(gs), REG(eflags), flags_to_string(REG(eflags)));
    mhp_printf("\nCS:IP=%04x:%04x       SS:SP=%04x:%04x\n",
                SREG(cs), LWORD(eip), SREG(ss), LWORD(esp));
  }
  mhp_cmd("u * 1");
}

static void mhp_regs32(int argc, char * argv[])
{
  if (DBG_TYPE(mhpdbgc.currcode) == DBG_INTx)
     mhp_printf( "\nInterrupt 0x%02X", DBG_ARG(mhpdbgc.currcode));
  if (DBG_TYPE(mhpdbgc.currcode) == DBG_TRAP)
     mhp_printf( "\nTrap 0x%02X", DBG_ARG(mhpdbgc.currcode));
  if (DBG_TYPE(mhpdbgc.currcode) == DBG_GPF)
     mhp_printf( "\nGeneral Protection Fault");

  mhp_printf("\nEAX: %08x EBX: %08x ECX: %08x EDX: %08x VFLAGS(h): %08lx",
              REG(eax), REG(ebx), REG(ecx), REG(edx), (unsigned long)vflags);

  mhp_printf("\nESI: %08x EDI: %08x EBP: %08x",
              REG(esi), REG(edi), REG(ebp));

  mhp_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n",
              SREG(ds), SREG(es), SREG(fs), SREG(gs));

  mhp_printf(" CS: %04x IP: %04x SS: %04x SP: %04x\n",
              SREG(cs), LWORD(eip), SREG(ss), LWORD(esp));
}

static void mhp_kill(int argc, char * argv[])
{
  mhp_cmd("r0");
  mhp_printf("\ndosemu killed via debug terminal\n");
  mhp_send();
#if 0
  mhp_putc(1); /* tell debugger terminal to also quit */
  mhp_send();
#endif
  mhp_close();
  if (dosdebug_flags & DBGF_IN_LEAVEDOS) dosdebug_flags &= ~DBGF_IN_LEAVEDOS;
  else leavedos(1);
}

void mhp_bpset(void)
{
  int i;
  dpmimode = saved_dpmimode;

  mhpdbgc.bpcleared = 0;
  for (i = 0; i < MAXBP; i++) {
    if (mhpdbgc.brktab[i].is_valid) {
      if (mhpdbgc.brktab[i].is_dpmi && !dpmi_active()) {
        mhpdbgc.brktab[i].brkaddr = 0;
        mhpdbgc.brktab[i].is_valid = 0;
        mhp_printf("Warning: cleared breakpoint %d because not in DPMI\n", i);
        continue;
      }
      mhpdbgc.brktab[i].opcode = READ_BYTE(mhpdbgc.brktab[i].brkaddr);
      if (i != trapped_bp)
        WRITE_BYTE(mhpdbgc.brktab[i].brkaddr, 0xCC);
    }
  }
  return;
}

void mhp_bpclr(void)
{
  int i;
  uint8_t opcode;

  if (mhpdbgc.bpcleared)
    return;
  mhpdbgc.bpcleared = 1;
  for (i = 0; i < MAXBP; i++) {
    if (mhpdbgc.brktab[i].is_valid) {
      if (mhpdbgc.brktab[i].is_dpmi && !dpmi_active()) {
        mhpdbgc.brktab[i].brkaddr = 0;
        mhpdbgc.brktab[i].is_valid = 0;
        mhp_printf("Warning: cleared breakpoint %d because not in DPMI\n", i);
        continue;
      }

      opcode = READ_BYTE(mhpdbgc.brktab[i].brkaddr);
      if (opcode != 0xCC) {
        if (!(dosdebug_flags & DBGF_ALLOW_BREAKPOINT_OVERWRITE)) {
          if (i != trapped_bp) {
            mhpdbgc.brktab[i].brkaddr = 0;
            mhpdbgc.brktab[i].is_valid = 0;
            mhp_printf("Warning: cleared breakpoint %d because INT3 overwritten\n", i);
          }
          continue;
        } else {
          mhpdbgc.brktab[i].opcode = opcode;
          if (i != trapped_bp) {
            WRITE_BYTE(mhpdbgc.brktab[i].brkaddr, 0xCC);
            mhp_printf("Warning: code at breakpoint %d has been overwritten (0x%02x)\n", i, opcode);
          }
        }
      }

      WRITE_BYTE(mhpdbgc.brktab[i].brkaddr, mhpdbgc.brktab[i].opcode);
    }
  }
  saved_dpmimode = dpmimode;
  return;
}

static int bpchk(dosaddr_t addr)
{
  int i;

  for (i = 0; i < MAXBP; i++) {
    if (mhpdbgc.brktab[i].is_valid && mhpdbgc.brktab[i].brkaddr == addr) {
      dpmimode = mhpdbgc.brktab[i].is_dpmi;
      trapped_bp_ = i;
      trapped_bp = -2;
      if (mhpdbgc.brktab[i].is_one_shot) {
        uint8_t opcode = READ_BYTE(mhpdbgc.brktab[i].brkaddr);
        if (opcode == 0xCC)
          WRITE_BYTE(mhpdbgc.brktab[i].brkaddr, mhpdbgc.brktab[i].opcode);
        mhpdbgc.brktab[i].brkaddr = 0;
        mhpdbgc.brktab[i].is_valid = 0;
        mhpdbgc.brktab[i].is_dpmi = 0;
        mhpdbgc.brktab[i].is_one_shot = 0;
        mhpdbgc.brktab[i].opcode = 0;
      }
      return 1;
    }
  }
  return 0;
}

int mhp_bpchk(dosaddr_t addr)
{
    if (mhpdbgc.bpcleared)
        return 0;
    return bpchk(addr);
}

dosaddr_t mhp_getcsip_value()
{
  dosaddr_t val;
  unsigned int seg, off, limit;

  if (in_dpmi_pm()) {
    char str[] = "cs:eip";
    mhp_getadr(str, &val, &seg, &off, &limit, 1); // Can't fail!
    return val;
  } else
    return (SREG(cs) << 4) + LWORD(eip);
}

void mhp_modify_eip(int delta)
{
  if (in_dpmi_pm()) dpmi_mhp_modify_eip(delta);
  else LWORD(eip) +=delta;
}

void mhp_cmd(const char * cmd)
{
   call_cmd(cmd, MAXARG, cmdtab, mhp_printf);
}

static void mhp_print_ldt(int argc, char * argv[])
{
  static char lastldt[32];

  static char buffer[0x10000];
  unsigned int *lp, *lp_;
  unsigned int base_addr, limit;
  int type, type2, i;
  unsigned int seg;
  int page, lines, cache_mismatch;
  enum{Abit=0x100};

  if (argc > 1) {
    if (!getval_ui(argv[1], 16, &seg))  {
      mhp_printf("invalid argument '%s'\n", argv[1]);
      return;
    }
    page = 0;
  } else {
    if (!getval_ui(lastldt, 16, &seg))
      seg = 0;
    page = 1;
  }
  lines = page ? 16 : 1;

  if (get_ldt(buffer) < 0) {
    mhp_printf("error getting ldt\n");
    return;
  }
  lp = (unsigned int *) buffer;
  lp += (seg & 0xFFF8) >> 2;
  lp_ = (unsigned int *)dpmi_get_ldt_buffer();
  lp_ += (seg & 0xFFF8) >> 2;

  for (i=(seg & 0xFFF8); i< 0x10000; i+=8,lp++, lp_+=2) {
    cache_mismatch = (lp[0] != lp_[0]) || ((lp[1]|Abit) != (lp_[1]|Abit));
    if ((lp[0] && lp[1]) ||  cache_mismatch) {
        if (--lines <0) break;
        base_addr = (*lp >> 16) & 0x0000FFFF;
        limit = *lp & 0x0000FFFF;
        lp++;
        /* Second 32 bits of descriptor */
        base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
        limit |= (*lp & 0x000F0000);
        type = (*lp >> 8)  & 0x000000FF;
        type2= (*lp >> 20) & 0x0000000F;
        if (type2 & 0x08)
           limit *= 4096;
        if (type & 0x10)
           mhp_printf ("%04x: %08x %08x %s%d %d %c%c%c%c%c %p%s\n",
              i | 0x07,
              base_addr,
              limit,
              (type & 0x08) ? "Code" : "Data",
              (type2 & 0x04) ? 32 : 16,
              (type >> 5) & 0x03,
              (type >> 7) ? 'P' : ' ',
              (type & 4)?((type & 8)?'C':'E'):' ',
              (type & 2)?((type & 8)?'R':'W'):' ',
              (type & 1)?'A':' ',
              (lp_[1] &Abit)?'a':' ',
	      MEM_BASE32(base_addr),
              cache_mismatch ?" (cache mismatch)":"");
        else
           mhp_printf ("%04x: %08x %08x System(%02x)%s\n",
              i, base_addr, limit, type,
              cache_mismatch ?" (cache mismatch)":"");
    }
    else lp++;
  }

  if (page)
    snprintf(lastldt, sizeof(lastldt), "0x%x", i);
}

static void mhp_debuglog(int argc, char *argv[])
{
  char buf[1024];

  if (argc > 1) {
    if (!strcmp(argv[1], "on")) {
      dosdebug_flags |= DBGF_INTERCEPT_LOG;
      mhp_printf("%s\n", "log intercept enabled");
      return;
    }

    if (!strcmp(argv[1], "off")) {
      dosdebug_flags &= ~DBGF_INTERCEPT_LOG;
      mhp_printf("%s\n", "log intercept disabled");
      return;
    }

    if (!strcmp(argv[1], "info")) {
      if (GetDebugInfoHelper(buf, sizeof(buf)))
        mhp_printf("%s", buf);
      return;
    }

    if (SetDebugFlagsHelper(argv[1]) == 0)
      mhp_printf("%s\n", "flags updated");
    else
      mhp_printf("%s\n", "syntax error");
  }

  if (argc <= 1) {
    GetDebugFlagsHelper(buf, 0);
    mhp_printf("current Debug-log flags:\n%s\n", buf);
  }
}

#define MAX_REGEX		8
static regex_t *rxbuf[MAX_REGEX] = {0};
static char *rxpatterns[MAX_REGEX] = {0};
static int num_regex = 0;

static int get_free_regex_buf(void)
{
  int i;
  for (i=0; i <num_regex; i++) {
    if (!rxbuf[i]) return i;
  }
  if (num_regex >= MAX_REGEX) return -1;
  i = num_regex;
  num_regex++;
  rxbuf[i] = 0;
  rxpatterns[i] =0;
  return i;
}

static void free_regex(int number)
{
  if ((unsigned)number >= MAX_REGEX) return;
  if (!rxbuf[number]) return;
  regfree(rxbuf[number]);
  if (rxpatterns[number]) free(rxpatterns[number]);
  free(rxbuf[number]);
  rxbuf[number] = 0;
  rxpatterns[number] = 0;
}

static char *trimm_string_arg(char *arg)
{
  int len;

  if (!arg || !arg[0]) return 0;
  len = strlen(arg);
  if (     (arg[0] == '"' && arg[len-1] == '"')
        || (arg[0] == '\'' && arg[len-1] == '\'')) {
    arg[len-1] = 0;
    return arg+1;
  }
  return arg;
}

static int enumerate_log_breakpoints(void)
{
  int rx, num = 0;

  for (rx = 0; rx < num_regex; rx++) {
    if (rxbuf[rx])
      num++;
  }

  return num;
}

static void print_log_breakpoints(void)
{
   int rx, num = 0;

   mhp_printf("log intercept %s\n",
              (dosdebug_flags & DBGF_INTERCEPT_LOG) ? "on" : "off");

   for (rx=0; rx <num_regex; rx++) {
     if (rxbuf[rx]) {
       mhp_printf("log breakpoint %d: %s\n", rx, rxpatterns[rx]);
       mhp_send();
       num++;
     }
   }
   if (!num) {
     mhp_printf("no log breakpoint active\n");
   }
}


static void mhp_bplog(int argc, char * argv[])
{
   int rx, errcode;
   char buf[1024];
   char *s;

   if (argc >1) {
     if (!check_for_stopped()) return;
     argv++;
     buf[0] = 0;
     while (*argv) {
       s = trimm_string_arg(*argv);
       if (s) strcat(buf, s);
       argv++;
     }
     if (!buf[0]) {
       mhp_printf ("no valid regular expression defined\n");
       return;
     }
     rx = get_free_regex_buf();
     if (rx <0) {
       mhp_printf ("too many log breakpoints, use bclog to free one\n");
       return;
     }
     rxpatterns[rx] = strdup(buf);
     rxbuf[rx] = malloc(sizeof(regex_t));
     if (!rxbuf[rx]) {
       mhp_printf ("out of memory\n");
       return;
     }
     errcode = regcomp(rxbuf[rx], rxpatterns[rx], REG_NOSUB);
     if (errcode) {
        regerror(errcode, rxbuf[rx], buf, sizeof(buf));
        mhp_printf ("%s\n", buf);
        free_regex(rx);
        return;
     }
     /* ...puh, all that work just for generating a pattern :-)
      * now we enable the 'break point' by intercepting the log
      */
     dosdebug_flags |= DBGF_INTERCEPT_LOG | DBGF_LOG_TO_BREAK;
   }
   /* at least print all active breakpoints */
   print_log_breakpoints();
}

static void mhp_bclog(int argc, char *argv[])
{
  int rx;

  if (argc > 1) {
    if (!check_for_stopped())
      return;

    rx = atoi(argv[1]);
    if (((unsigned)rx >= MAX_REGEX) || !rxbuf[rx]) {
      mhp_printf("log break point %i does not exist\n", rx);
      return;
    }
    free_regex(rx);

    if (!enumerate_log_breakpoints()) {
      dosdebug_flags &= ~DBGF_LOG_TO_BREAK;
      dosdebug_flags &= ~DBGF_INTERCEPT_LOG;
    }
  }

  print_log_breakpoints();
}

static int mhp_check_regex(char *line)
{
   int rx, hit;

   for (rx=0; rx <num_regex; rx++) {
     if (rxbuf[rx]) {
       hit = regexec(rxbuf[rx], line, 0, 0, 0) == 0;
       if (hit) {
         mhp_printf("log break point %d hit: >%s<\n",rx,line);
         mhp_send();
       }
       if (hit) return 1;
     }
   }
   return 0;
}

static char lbuf[1024];
static int lbufi = 0;

void mhp_regex(const char *fmt, va_list args)
{
  int i, hit;
  char *s;

  if (!(dosdebug_flags & DBGF_LOG_TO_BREAK))
    return;

  lbufi += vsprintf(lbuf+lbufi, fmt, args);
  hit = 0;
  i = 0;
  do {
    s = strchr(lbuf+i, '\n');
    if (s) {
      *s = 0;
      hit = mhp_check_regex(lbuf+i);
      i = (s - lbuf)+1;
      if (hit) break;
    }
  } while (s);
  if (i) {
    memcpy(lbuf, lbuf+i, lbufi-i+1);
    lbufi -= i;
    if (hit) {
      mhpdbgc.want_to_stop = 1;
    }
  }
}

