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

#define LDT_ENTRIES 8192
#define LDT_ENTRY_SIZE 8
#define MAX_SELECTORS 8192

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <asm/bitops.h>
#include "config.h"
#include "emu.h"
#include "cpu.h"

#define MHP_PRIVATE
#include "mhpdbg.h"

#define makeaddr(x,y) ((((unsigned long)x) << 4) + (unsigned long)y)

#if 0 /* modify_ldt already defined in dpmi.c */
_syscall3(int, modify_ldt, int, func, void *, ptr, unsigned long, bytecount)
#endif

/* externs */
extern struct mhpdbgc mhpdbgc;
extern int a20;

#if 0
/* NOTE: the below is already defined with #include "emu.h"
 *       Must NOT redefine it, else USE_VM86PLUS won't work !!!
 */
extern struct vm86_struct vm86s;
#endif

extern int  dis_8086(unsigned int, const unsigned char *,
                     unsigned char *, int, unsigned int *, unsigned int *);

/* prototypes */
static void* mhp_getadr(unsigned char *, unsigned int *, unsigned int *, unsigned int *);
static int mhp_parse(char*, char *[]);
static void mhp_regs  (int, char *[]);
static void mhp_dis   (int, char *[]);
static void mhp_disasm  (int, char *[]);
static void mhp_go      (int, char *[]);
static void mhp_stop    (int, char *[]);
static void mhp_trace   (int, char *[]);
static void mhp_trace_force (int, char *[]);
static void mhp_regs32  (int, char *[]);
static void mhp_bp      (int, char *[]);
static void mhp_bc      (int, char *[]);
static void mhp_bl      (int, char *[]);
static void mhp_bpint   (int, char *[]);
static void mhp_bcint   (int, char *[]);
static void mhp_bpload  (int, char *[]);
static void mhp_rmapfile(int, char *[]);
static void mhp_mode    (int, char *[]);
static void mhp_rusermap(int, char *[]);
static void mhp_kill    (int, char *[]);
static void mhp_help    (int, char *[]);
static void mhp_enter   (int, char *[]);
static void mhp_print_ldt       (int, char *[]);
static unsigned int lookup(unsigned char *, unsigned int *, unsigned int *);
static inline int get_ldt(void *);

/* static data */
static unsigned int linmode = 0;
static unsigned int codeorg = 0;

static char lastd[32];
static char lastu[32];
static char lastldt[32];

static struct symbol_entry symbol_table[MAXSYM];
static unsigned int last_symbol = 0;
static unsigned int addrmax;

static struct symbl2_entry symbl2_table[MAXSYM];
static unsigned int last_symbol2 = 0;
static unsigned int symbl2_org = 0;
/* static unsigned int symbl2_end = 0; */

/* constants */
static const struct cmd_db cmdtab[] = {
   "r",             mhp_regs,
   "e",             mhp_enter,
   "d",             mhp_dis,
   "u",             mhp_disasm,
   "g",             mhp_go,
   "stop",          mhp_stop,
   "mode",          mhp_mode,
   "t",             mhp_trace,
   "tf",            mhp_trace_force,
   "r32",           mhp_regs32,
   "bp",            mhp_bp,
   "bc",            mhp_bc,
   "bl",            mhp_bl,
   "bpint",         mhp_bpint,
   "bcint",         mhp_bcint,
   "bpload",        mhp_bpload,
   "rmapfile",      mhp_rmapfile,
   "rusermap",      mhp_rusermap,
   "kill",          mhp_kill,
   "?",             mhp_help,
   "ldt",           mhp_print_ldt,
   "",              NULL
};

static const char help_page[]=
  "q                 Quit the debug session\n"
  "kill              Kill the dosemu process\n"
  "r                 list regs\n"
  "e ADDR HEXSTR     modify memory (0-1Mb)\n"
  "d ADDR SIZE       dump memory (no limit)\n"
  "u ADDR SIZE       unassemble memory (no limit)\n"
  "g                 go (if stopped)\n"
  "stop              stop (if running)\n"
  "mode 0|1          set mode (0=SEG16, 1=LIN32) for u and d commands\n"
  "t                 single step (not fully debugged!!!)\n"
  "tf                single step, force over IRET\n"
  "r32               dump regs in 32 bit format\n"
  "bp addr           set int3 style breakpoint\n"
  "bpint xx          set breakpoint on INT xx\n"
  "bcint xx          clr breakpoint on INT xx\n"
  "bpload            stop at start of next loaded a DOS program\n"
  "bl                list active breakpoints\n"
  "rusermap org fn   read microsoft linker format .MAP file 'fn'\n"
  "                  code origin = 'org'.\n"
  "ldt sel lines     dump ldt starting at selector 'sel' for 'lines'\n"
  "<ENTER>           repeats previous command\n";

/********/
/* CODE */
/********/

static inline int get_ldt(void *buffer)
{
  return modify_ldt(0, buffer, LDT_ENTRIES * LDT_ENTRY_SIZE);
}

static unsigned char * getname(unsigned int addr)
{
   int i;
   if ((addr < 0x100000) || (addr > addrmax))
      return(NULL);
   for (i=0; i < last_symbol; i++) {
      if (symbol_table[i].addr == addr)
         return(symbol_table[i].name);
   }
   return(NULL);
}

static unsigned char * getname2(unsigned int seg, unsigned int off)
{
   int i;
   for (i=0; i < last_symbol2; i++) {
      if ((symbl2_table[i].seg == seg) &&
          (symbl2_table[i].off == off))
         return(symbl2_table[i].name);
   }
   return(NULL);
}

static unsigned char * getname2u(unsigned int addr)
{
   int i;
   for (i=0; i < last_symbol2; i++) {
      if (addr == makeaddr(symbl2_table[i].seg, symbl2_table[i].off))
         return(symbl2_table[i].name);
   }
   return(NULL);
}

static unsigned int    getaddr(unsigned char * n1)
{
   int i;
   if (strlen(n1) == 0)
      return 0;
   for (i=0; i < last_symbol; i++) {
      if (strcmp(symbol_table[i].name, n1) == 0)
         return(symbol_table[i].addr);
   }
   return 0;
}

static unsigned int lookup(unsigned char * n1, unsigned int *s1, unsigned int * o1)
{
   int i;
   if (!strlen(n1))
      return 0;
   for (i=0; i < last_symbol2; i++) {
      if (!strcmp(symbl2_table[i].name, n1)) {
         *s1=symbl2_table[i].seg;
         *o1=symbl2_table[i].off;
         return makeaddr(*s1,*o1);
      }
   }
   return 0;
}

static void mhp_rusermap(int argc, char *argv[])
{
  FILE * ifp;
  unsigned char bytebuf[IBUFS];
  unsigned long org;
  unsigned int  seg;
  unsigned int  off;

  char * srchfor = "  Address         Publics by Value";

  if (argc < 3) {
     mhp_printf("syntax: rusermap <org> <file>\n");
     return;
  }

  sscanf(argv[1], "%lx", &org);
  symbl2_org = org;

  ifp = fopen(argv[2], "r");
  if (!ifp) {
     mhp_printf("unable to open map file %s\n", argv[2]);
     return;
  }
  mhp_printf("reading map file %s\n", argv[2]);
  last_symbol2 = 0;
  for(;;) {
     if(!fgets(bytebuf, 100, ifp)) {
        mhp_printf("error: could not find following in %s:\n%s\n",
                   argv[2], srchfor);
        return;
     }
     if (!strlen(bytebuf))
        continue;
     if (!memcmp(bytebuf, srchfor, strlen(srchfor)))
        break;
  }
  for(;;) {
     if(!fgets(bytebuf, 100, ifp))
        break;
     if(bytebuf[5] != ':')
        continue;
     if(bytebuf[12] != ' ')
        continue;
     bytebuf[5] = ' ';
     sscanf(&bytebuf[1], "%x %x", &seg, &off);
     symbl2_table[last_symbol2].seg  = seg + (symbl2_org >> 4);
     symbl2_table[last_symbol2].off  = off;
     symbl2_table[last_symbol2].type = ' ';
     sscanf(&bytebuf[17], "%s", (char *)&symbl2_table[last_symbol2].name);
     last_symbol2++;
  }
  fclose(ifp);
/*symbl2_end = symbl2_table[last_symbol2-1].addr;*/
  mhp_printf("%d symbol(s) processed\n", last_symbol2);
  mhp_printf("highest address %04x:%04x(%s)\n",
             symbl2_table[last_symbol2-1].seg,
             symbl2_table[last_symbol2-1].off,
             symbl2_table[last_symbol2-1].name);

}

static void mhp_rmapfile(int argc, char *argv[])
{
  FILE * ifp;
  unsigned char bytebuf[IBUFS];
  unsigned long a1;

  ifp = fopen("/usr/src/dosemu/dosemu.map", "r");
  if (!ifp) {
     mhp_printf("unable to open map file\n");
     return;
  }
  mhp_printf("Reading map file\n");
  last_symbol = 0;
  for(;;) {
     if(!fgets(bytebuf, 100, ifp))
        break;
     if (!strlen(bytebuf))
        continue;
     sscanf(&bytebuf[0], "%lx", &a1);
#ifdef __ELF__
     if (a1 < 0x08000000) continue;
#else
     if (a1 < 0x20000000)
        continue;
     if (a1 > 0x28000000)
        break;
#endif
     symbol_table[last_symbol].addr = a1;
     symbol_table[last_symbol].type = bytebuf[9];
     sscanf(&bytebuf[11], "%s", (char *)&symbol_table[last_symbol].name);
     last_symbol++;
  }
  fclose(ifp);
  addrmax = symbol_table[last_symbol-1].addr;
  mhp_printf("%d symbol(s) processed\n", last_symbol);
  mhp_printf("highest address %08x(%s)\n", addrmax, getname(addrmax));
}

enum {
   _SSr, _CSr, _DSr, _ESr, _FSr, _GSr,
   _AXr, _BXr, _CXr, _DXr, _SIr, _DIr, _BPr, _SPr, _IPr, _FLr,
  _EAXr,_EBXr,_ECXr,_EDXr,_ESIr,_EDIr,_EBPr,_ESPr,_EIPr
};

static int decode_symreg(char *regn)
{
  static char reg_syms[]="SS  CS  DS  ES  FS  GS EAX EBX ECX EDX ESI EDI EBP ESP EIP  FL";
  char rn[5], *s;
  int n;
  strncpy(rn,regn,4);
  if (isalpha(rn[2])) rn[3]=0;
  else rn[2]=0;
  if (!(s=strstr(reg_syms,rn))) return -1;
  n=s-reg_syms;
  return (n >> 2) + ((n & 1) ? (16-5) : 0);
}

static unsigned long mhp_getreg(unsigned char * regn)
{
  switch (decode_symreg(regn)) {
    case _SSr: return LWORD(ss);
    case _CSr: return LWORD(cs);
    case _DSr: return LWORD(ds);
    case _ESr: return LWORD(es);
    case _FSr: return LWORD(fs);
    case _GSr: return LWORD(gs);
    case _AXr: return LWORD(eax);
    case _BXr: return LWORD(ebx);
    case _CXr: return LWORD(ecx);
    case _DXr: return LWORD(edx);
    case _SIr: return LWORD(esi);
    case _DIr: return LWORD(edi);
    case _BPr: return LWORD(ebp);
    case _SPr: return LWORD(esp);
    case _IPr: return LWORD(eip);
    case _FLr: return LWORD(eflags);
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
  return -1;
}


static void mhp_setreg(unsigned char * regn, unsigned long val)
{
  switch (decode_symreg(regn)) {
    case _SSr: LWORD(ss) = val; break;
    case _CSr: LWORD(cs) = val; break;
    case _DSr: LWORD(ds) = val; break;
    case _ESr: LWORD(es) = val; break;
    case _FSr: LWORD(fs) = val; break;
    case _GSr: LWORD(gs) = val; break;
    case _AXr: LWORD(eax) = val; break;
    case _BXr: LWORD(ebx) = val; break;
    case _CXr: LWORD(ecx) = val; break;
    case _DXr: LWORD(edx) = val; break;
    case _SIr: LWORD(esi) = val; break;
    case _DIr: LWORD(edi) = val; break;
    case _BPr: LWORD(ebp) = val; break;
    case _SPr: LWORD(esp) = val; break;
    case _IPr: LWORD(eip) = val; break;
    case _FLr: LWORD(eflags) = val; break;
    case _EAXr: REG(eax) = val; break;
    case _EBXr: REG(ebx) = val; break;
    case _ECXr: REG(ecx) = val; break;
    case _EDXr: REG(edx) = val; break;
    case _ESIr: REG(esi) = val; break;
    case _EDIr: REG(edi) = val; break;
    case _EBPr: REG(ebp) = val; break;
    case _ESPr: REG(esp) = val; break;
    case _EIPr: REG(eip) = val; break;
  }
}


static void mhp_go(int argc, char * argv[])
{
   if (!mhpdbgc.stopped) {
      mhp_printf("already in running state\n");
   } else {
      WRITE_FLAGS(READ_FLAGS() & ~TF);
      vm86s.vm86plus.mhpdbg_TFpendig=0;
      mhpdbgc.stopped = 0;
   }
}

static void mhp_stop(int argc, char * argv[])
{
   if (mhpdbgc.stopped) {
      mhp_printf("already in stopped state\n");
   } else {
      mhpdbgc.stopped = 1;
      mhp_cmd("r");
   }
}

static void mhp_trace(int argc, char * argv[])
{
   if (!mhpdbgc.stopped) {
      mhp_printf("must be in stopped state\n");
   } else {
      WRITE_FLAGS(READ_FLAGS() | TF);
#if 0
      vm86s.vm86plus.mhpdbg_TFpendig=1; */
#endif
      mhpdbgc.trapcmd = 1;
      mhpdbgc.stopped = 0;
   }
}

static void mhp_trace_force(int argc, char * argv[])
{
   if (!mhpdbgc.stopped) {
      mhp_printf("must be in stopped state\n");
   } else {
      WRITE_FLAGS(READ_FLAGS() | TF);
      vm86s.vm86plus.mhpdbg_TFpendig=1;
      mhpdbgc.trapcmd = 1;
      mhpdbgc.stopped = 0;
   }
}

static void mhp_dis(int argc, char * argv[])
{
   unsigned int nbytes;
   unsigned long seekval;
   int i,i2;
   unsigned char * buf = 0;
   unsigned int seg;
   unsigned int off;
   unsigned int limit;

   if (argc > 1) {
      seekval = (unsigned long)mhp_getadr(argv[1], &seg, &off, &limit);
      strcpy(lastd, argv[1]);
   } else {
      if (!strlen(lastd)) {
         mhp_printf("No previous \'d\' command\n");
         return;
      }
      seekval = (unsigned long)mhp_getadr(lastd, &seg, &off, &limit);
   }
   buf = (unsigned char *) seekval;

   if (argc > 2)
      sscanf(argv[2], "%x", &nbytes);
   else
      nbytes = 128;

   if ((nbytes == 0) || (nbytes > 256))
      nbytes = 128;

#if 0
   mhp_printf( "seekval %08lX nbytes:%d\n", seekval, nbytes);
#else
   mhp_printf( "\n");
#endif
   for (i=0; i<nbytes; i++) {
      if ((i&0x0f) == 0x00) {
         if (seg|off)
            mhp_printf("%04x:%04x ", seg, off+i);
         else
            mhp_printf( "%08lX ", (unsigned long)seekval+i);
      }
      mhp_printf( "%02X ", buf[i]);
      if ((i&0x0f) == 0x0f) {
         mhp_printf( " ");
         for (i2=i-15;i2<=i;i2++){
            if ((buf[i2]&0x7F) >= 0x20) {
               mhp_printf( "%c", buf[i2]&0x7f);
            } else {
               mhp_printf( "%c", '.');
            }
         }
         mhp_printf( "\n");
      }
   }

   if (seg|off) {
      if (lastd[0] == '#') {
         sprintf(lastd, "#%x:%x", seg, off+i);
      } else {
         sprintf(lastd, "%x:%x", seg, off+i);
      }
   } else {
      sprintf(lastd, "%lx", seekval + i);
   }
}

static void mhp_mode(int argc, char * argv[])
{
   if (argc < 2) {
      mhp_printf ("current mode: %s\n", linmode?"lin32":"seg16");
      return;
   }
   if (argv[1][0] == '0')
      linmode = 0;
   if (argv[1][0] == '1')
      linmode = 1;
   mhp_printf ("current mode: %s\n", linmode?"lin32":"seg16");
   return;
}

static void mhp_disasm(int argc, char * argv[])
{
   extern int a20; /* a20 is defined in dosemu/xms.c */
   int rc;
   unsigned int nbytes;
   unsigned long org;
   unsigned long seekval;
   int def_size;
   unsigned int bytesdone;
   int i;
   unsigned char * buf = 0;
   unsigned char bytebuf[IBUFS];
   unsigned char frmtbuf[IBUFS];
   unsigned int seg;
   unsigned int off;
   unsigned int refseg;
   unsigned int ref;
   unsigned int limit;

   if (argc > 1) {
      seekval = (unsigned long)mhp_getadr(argv[1], &seg, &off, &limit);
      strcpy(lastu, argv[1]);
   } else {
      if (!strlen(lastu)) {
         mhp_printf("No previous \'u\' command\n");
         return;
      }
      seekval = (unsigned long)mhp_getadr(lastu, &seg, &off, &limit);
   }

   if (argc > 2) {
      if ((argv[2][0] == 'l') || (argv[2][0] == 'L'))
         sscanf(&argv[2][1], "%x", &nbytes);
      else
         sscanf(argv[2], "%x", &nbytes);
   } else {
      nbytes = 32;
   }

   if ((nbytes == 0) || (nbytes > 256))
      nbytes = 32;

#if 0
   mhp_printf( "seekval %08lX nbytes:%d\n", seekval, nbytes);
#else
   mhp_printf( "\n");
#endif
   if (seekval < (a20 ? 0x10fff0 : 0x100000)) def_size = linmode;
   else if (lastu[0] == '#')
      def_size = 0;
   else def_size = 1;
   rc=0;
   buf = (unsigned char *) seekval;
   org = codeorg ? codeorg : seekval;

   for (bytesdone = 0; bytesdone < nbytes; bytesdone += rc) {
       if (linmode) {
          if (getname(org+bytesdone))
             mhp_printf ("%s:\n", getname(org+bytesdone));
       } else {
          if (getname2(seg,off+bytesdone))
             mhp_printf ("%s:\n", getname2(seg,off));
       }
       refseg = seg;
       rc = dis_8086(org+bytesdone, buf+bytesdone, frmtbuf, def_size, &refseg, &ref);
       for (i=0;i<rc;i++) {
           sprintf(&bytebuf[i*2], "%02X", *(buf+bytesdone+i) );
           bytebuf[(i*2)+2] = 0x00;
       }
       if (!linmode) {
          mhp_printf( "%04x:%04x %-16s %s", seg, off+bytesdone, bytebuf, frmtbuf);
          if ((ref) && (getname2u(ref)))
             mhp_printf ("(%s)", getname2u(ref));
       } else {
          mhp_printf( "%08x: %-16s %s", org+bytesdone, bytebuf, frmtbuf);
          if ((ref) && (getname(ref)))
             mhp_printf ("(%s)", getname(ref));
       }
       mhp_printf( "\n");
   }

   if (!linmode) {
      if (lastu[0] == '#') {
         sprintf(lastu, "#%x:%x", seg, off+bytesdone);
      } else {
         sprintf(lastu, "%x:%x", seg, off+bytesdone);
      }
   } else {
      sprintf(lastu, "%lx", seekval + bytesdone);
   }
}
static void mhp_enter(int argc, char * argv[])
{
   int i1;
   unsigned char * zapaddr;
   unsigned int seg;
   unsigned int off;
   unsigned int limit;
   unsigned char * inputptr;
   unsigned char   c;
   unsigned char   workchrs[4];

   if (argc < 3)
      return;

   zapaddr = (unsigned char*)mhp_getadr(argv[1], &seg, &off, &limit);
   if ((a20 ?0x10fff0 : 0x100000) < (unsigned long)zapaddr) {
      mhp_printf("Address invalid\n");
      return;
   }
   inputptr = argv[2];
   for (i1 = 0; ; i1++) {
      workchrs[0] = *inputptr++;
      if (!workchrs[0])
         break;
      workchrs[1] = *inputptr++;
      workchrs[2] = 0x00;
      sscanf(workchrs, "%02x", (unsigned int*)&c);
      zapaddr[i1] = c;
      if (!workchrs[1])
         break;
   }
}
static void* mhp_getadr(unsigned char * a1, unsigned int * s1, unsigned int *o1, unsigned int *lim)
{
   unsigned char * srchp;
   unsigned int seg1;
   unsigned int off1;
   unsigned long ul1;
   int selector = 0;
   unsigned long * lp;
   unsigned long base_addr, limit;
   char buffer[0x10000];

   *lim = 0xFFFFFFFF;

   if (*a1 == '#') {
      selector = 1;
      *lim = 0;
      a1 ++;
   }
   if(*a1 == '*') {
      *s1 = LWORD(cs);
      *o1 = LWORD(eip);
      return (void*) makeaddr(LWORD(cs), LWORD(eip));
   }
   if(*a1 == '$') {
      *s1 = LWORD(ss);
      *o1 = LWORD(esp);
      return (void*) makeaddr(LWORD(ss), LWORD(esp));
   }
   if ((ul1 = lookup(a1, s1, o1))) {
      return (void*)ul1;
   }
   if ((ul1 = getaddr(a1))) {
      *s1 = 0;
      *o1 = 0;
      return (void*)ul1;
   }
   if (!(srchp = (unsigned char *)strchr(a1, ':'))) {
      sscanf(a1, "%lx", &ul1);
      *s1 = 0;
      *o1 = 0;
      return (void*)ul1;
   }
   if ( (seg1 = mhp_getreg(a1)) == -1) {
           *srchp = ' ';
           sscanf(a1, "%x", &seg1);
           *srchp = ':';
   }
   if ( (off1 = mhp_getreg(srchp+1)) == -1) {
           sscanf(srchp+1, "%x", &off1);
   }
   *s1 = seg1;
   *o1 = off1;

   if (!selector)
      return (void*)makeaddr(seg1,off1);

   if (!(seg1 & 0x4)) {
     mhp_printf("GDT selectors not supported yet\n");
     return (void *)0;
   }

   if (get_ldt(buffer) < 0) {
     mhp_printf("error getting ldt\n");
     return (void *)0;
   }

   lp = (unsigned long *) buffer;
   lp += (seg1 & 0xFFF8) >> 2;

   base_addr = (*lp >> 16) & 0x0000FFFF;
   limit = *lp & 0x0000FFFF;
   lp++;
   /* Second 32 bits of descriptor */
   base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
   limit |= (*lp & 0x000F0000);

   if ((limit == 0) && (base_addr == 0)) {
     mhp_printf("selector %x appears to be invalid\n", seg1);
     return (void *)0;
   }

   if (off1 >= limit) {
     mhp_printf("offset %x exceeds segment limit %x\n", off1, limit);
     return (void *)0;
   }

   *lim = limit - off1;
   return (void *) base_addr + off1;
}


int mhp_setbp(unsigned long seekval)
{
   int i1;
   for (i1=0; i1 < MAXBP; i1++) {
      if (mhpdbgc.brktab[i1].brkaddr == (unsigned char *)seekval) {
         mhp_printf( "Duplicate breakpoint, nothing done\n");
         return 0;
      }
   }
   for (i1=0; i1 < MAXBP; i1++) {
      if (!mhpdbgc.brktab[i1].brkaddr) {
         mhpdbgc.brktab[i1].brkaddr = (unsigned char *)seekval;
         return 1;
      }
   }
   mhp_printf( "Breakpoint table full, nothing done\n");
   return 0;
}

int mhp_clearbp(unsigned long seekval)
{
   int i1;
   for (i1=0; i1 < MAXBP; i1++) {
      if (mhpdbgc.brktab[i1].brkaddr == (unsigned char *)seekval) {
         mhpdbgc.brktab[i1].brkaddr = 0;
         return 1;
      }
   }
   return 0;
}

static void mhp_bp(int argc, char * argv[])
{
   unsigned long seekval;
   unsigned int seg;
   unsigned int off;
   unsigned int limit;

   if (argc < 2) {
      mhp_printf("location argument required\n");
      return;
   }
   seekval = (unsigned long)mhp_getadr(argv[1], &seg, &off, &limit);
   mhp_setbp(seekval);
}

static void mhp_bl(int argc, char * argv[])
{
   int i1;

   mhp_printf( "Breakpoints:\n");
   for (i1=0; i1 < MAXBP; i1++) {
      if (mhpdbgc.brktab[i1].brkaddr) {
         mhp_printf( "%d: %08lx\n", i1, mhpdbgc.brktab[i1].brkaddr);
      }
   }
   mhp_printf( "Interrupts: ");
   for (i1=0; i1 < 256; i1++) {
      if (test_bit(i1, vm86s.vm86plus.mhpdbg_intxxtab)) {
         mhp_printf( "%02x ", i1);
      }
   }
   mhp_printf( "\n");
   if (mhpdbgc.bpload) mhp_printf("bpload active\n");
   return;
}

static void mhp_bc(int argc, char * argv[])
{
   int i1;

   sscanf(argv[1], "%d", &i1);
   if (!mhpdbgc.brktab[i1].brkaddr) {
         mhp_printf( "No breakpoint %d, nothing done\n", i1);
         return;
   }
   mhpdbgc.brktab[i1].brkaddr = 0;
   return;
}

static void mhp_bpint(int argc, char * argv[])
{
   int i1;

   sscanf(argv[1], "%x", &i1);
   if (test_bit(i1, vm86s.vm86plus.mhpdbg_intxxtab)) {
         mhp_printf( "Duplicate BPINT %02x, nothing done\n", i1);
         return;
   }
   set_bit(i1, vm86s.vm86plus.mhpdbg_intxxtab);
   if (i1 == 0x21) mhpdbgc.int21_count++;
   return;
}

static void mhp_bcint(int argc, char * argv[])
{
   int i1;

   sscanf(argv[1], "%x", &i1);
   if (!test_bit(i1, vm86s.vm86plus.mhpdbg_intxxtab)) {
         mhp_printf( "No BPINT %02x, nothing done\n", i1);
         return;
   }
   clear_bit(i1, vm86s.vm86plus.mhpdbg_intxxtab);
   if (i1 == 0x21) mhpdbgc.int21_count--;
   return;
}

static void mhp_bpload(int argc, char * argv[])
{
   if (mhpdbgc.bpload) {
     mhp_printf("load breakpoint already pending\n");
     return;
   }
   mhpdbgc.bpload=1;
   {
     volatile register int i=0x21; /* beware, set_bit-macro has wrong constraints */
     set_bit(i, vm86s.vm86plus.mhpdbg_intxxtab);
   }
   mhpdbgc.int21_count++;
   return;
}

static int mhp_parse(char *s, char* argvx[])
{
   int mode = 0;
   int argcx = 0;

   for ( ; *s; s++) {
      if (!mode) {
         if (*s > ' ') {
            mode = 1;
            argvx[argcx++] = s;
            if (argcx >= MAXARG)
               break;
         }
      } else {
         if (*s <= ' ') {
            mode = 0;
            *s = 0x00;
         }
      }
   }
   return(argcx);
}

static void mhp_regs(int argc, char * argv[])
{
  unsigned long newval;
  int i;
  if (argc == 3) {
     if (!mhpdbgc.stopped) {
        mhp_printf("Must be in stopped state\n");
        return;
     }
     sscanf(argv[2], "%lx", &newval);
     i= strlen(argv[1]);
     do  argv[1][i] = toupper(argv[1][i]); while (i--);
     mhp_setreg(argv[1], newval);
     if (newval == mhp_getreg(argv[1]))
        mhp_printf("reg %s changed to %04x\n", argv[1], mhp_getreg(argv[1]) );
     return;
  }
  if (DBG_TYPE(mhpdbgc.currcode) == DBG_INTx)
     mhp_printf( "INT 0x%02X, ", DBG_ARG(mhpdbgc.currcode));
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

  mhp_printf( "system state: %s\n", mhpdbgc.stopped ? "stopped" : "running");

  mhp_printf("AX=%04x  BX=%04x  CX=%04x  DX=%04x",
              LWORD(eax), LWORD(ebx), LWORD(ecx), LWORD(edx));

  mhp_printf("  SI=%04x  DI=%04x  SP=%04x  BP=%04x",
              LWORD(esi), LWORD(edi), LWORD(esp), LWORD(ebp));

  mhp_printf("\nDS=%04x  ES=%04x  FS=%04x  GS=%04x  FL=%04x",
              LWORD(ds), LWORD(es), LWORD(fs), LWORD(gs), LWORD(eflags));

  mhp_printf("\nCS:IP=%04x:%04x       SS:SP=%04x:%04x\n",
              LWORD(cs), LWORD(eip), LWORD(ss), LWORD(esp));

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

  mhp_printf("\nEAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx VFLAGS(h): %08lx",
              REG(eax), REG(ebx), REG(ecx), REG(edx), (unsigned long)vflags);

  mhp_printf("\nESI: %08lx EDI: %08lx EBP: %08lx",
              REG(esi), REG(edi), REG(ebp));

  mhp_printf(" DS: %04x ES: %04x FS: %04x GS: %04x\n",
              LWORD(ds), LWORD(es), LWORD(fs), LWORD(gs));

  mhp_printf(" CS: %04x IP: %04x SS: %04x SP: %04x\n",
              LWORD(cs), LWORD(eip), LWORD(ss), LWORD(esp));
}

static void mhp_kill(int argc, char * argv[])
{
  extern void mhp_send(void), mhp_close(void), mhp_putc(char c1);
  mhp_cmd("r");
  mhp_printf("\ndosemu killed via debug terminal\n");
  mhp_send();
#if 0
  mhp_putc(1); /* tell debugger terminal to also quit */
  mhp_send();
#endif
  mhp_close();
  leavedos(1);
}

static void mhp_help(int argc, char * argv[])
{
  mhp_printf("%s\n",help_page);
}

void mhp_bpset(void)
{
   int i1;

   for (i1=0; i1 < MAXBP; i1++) {
      if (mhpdbgc.brktab[i1].brkaddr) {
         mhpdbgc.brktab[i1].opcode = *mhpdbgc.brktab[i1].brkaddr;
         *mhpdbgc.brktab[i1].brkaddr = 0xCC;
      }
   }
   return;
}

void mhp_bpclr(void)
{
   int i1;

   for (i1=0; i1 < MAXBP; i1++) {
      if (mhpdbgc.brktab[i1].brkaddr) {
         if( (*mhpdbgc.brktab[i1].brkaddr) != 0xCC) {
            mhpdbgc.brktab[i1].brkaddr = 0;
            continue;
         }
         *mhpdbgc.brktab[i1].brkaddr = mhpdbgc.brktab[i1].opcode;
      }
   }
   return;
}

int mhp_bpchk(unsigned char * a1)
{
   int i1;

   for (i1=0; i1 < MAXBP; i1++) {
      if (mhpdbgc.brktab[i1].brkaddr == a1)
         return 1;
   }
   return 0;
}

void mhp_cmd(const char * cmd)
{
   int argc1;
   char * argv1[MAXARG];
   char tmpcmd[250];
   void (*cmdproc)(int, char *[]);
   const struct cmd_db * cmdp;

   strcpy(tmpcmd, cmd);
   argc1 = mhp_parse(tmpcmd, argv1);
   if (argc1 < 1)
      return;
   for (cmdp = cmdtab, cmdproc = NULL; cmdp->cmdproc; cmdp++) {
      if (!memcmp(cmdp->cmdname, argv1[0], strlen(argv1[0])+1)) {
         cmdproc = cmdp->cmdproc;
         break;
      }
   }
   if (!cmdproc) {
      mhp_printf("Command %s not found\n", argv1[0]);
      return;
   }
   (*cmdproc)(argc1, argv1);
}

static void mhp_print_ldt(int argc, char * argv[])
{
  char buffer[0x10000];
  unsigned long *lp;
  unsigned long base_addr, limit;
  int type, type2, i;
  unsigned int seg;
  int lines;

  if (get_ldt(buffer) < 0) {
    mhp_printf("error getting ldt\n");
    return;
  }

  if (argc > 1) {
     sscanf (argv[1], "%x", &seg);
     strcpy(lastldt, argv[1]);
  } else {
     if (!strlen(lastldt)) {
        mhp_printf("No previous \'ldt\' command\n");
        return;
     }
     sscanf (lastldt, "%x", &seg);
  }

  if (argc > 2)
     sscanf (argv[2], "%d", &lines);
  else
     lines = 16;

  lp = (unsigned long *) buffer;
  lp += (seg & 0xFFF8) >> 2;

  for (i=0; i< lines; i++,lp++) {
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
           mhp_printf ("%04x: %08lx %08lx %s%d %d %c\n",
              (seg + (i << 3)) | 0x07,
              base_addr,
              limit,
              (type & 0x08) ? "Code" : "Data",
              (type2 & 0x04) ? 32 : 16,
              (type >> 5) & 0x03,
              (type >> 7) ? 'P' : ' ');
        else
           mhp_printf ("%04x: %08lx %08lx System(%02x)\n", seg + (i << 3), base_addr, limit, type);
  }
  sprintf (lastldt, "%x", seg + (i << 3));
}

