/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * file: tools86.c
 *
 * Toolbox to make as86/ld86 usable for DOSEMU
 *
 * (C) 1994 under GPL:  Hans Lermen <lermen@elserv.ffm.fgan.de>
 * (put under DOSEMU policy 1998, --Hans)
 *  
 * Tools86 has two operation modes:
 * 
 * 1. Additional Preprocessor for as86
 *
 *    Because some of the features in as86 are buggy or not
 *    not implemented, we have to do work around.
 *    I did not suceed in using the MACRO feature of the as86, but
 *    with GCC -E we can use the GNU-CPP, and after pipeing the result
 *    through tools86 we have the the following "extensions" to as86:
 *
 *    - We can have multiple asm instructions per line (needed if using
 *      CPP macros). 
 *      The delimiter between the instructions is "!!!" (three bangs).
 *      This will be translated by tools86 to "\n", which can't not be
 *      passed through CPP macros. Example:
 *
 *          #define SAVE_FLAGS(s) pushfd !!! pop dword ptr s
 *
 *      and SAVE_FLAGS(saved_flags) will produce
 *
 *          pushfd
 *          pop dword ptr saved_flags
 *
 *    - We can define repeatition of code of data as follows:
 *      .REPT  count
 *        ...any code or data...
 *      .ENDR
 *      
 *      "count" must be a simple number or a expression of form 
 *      "(count -const)" or "(count +const)", sorry, but I was too lazy to
 *      to implement more.
 *
 *      .REPT and .ENDR may appear on the same line. So we can realize
 *      a FILL_LONG macro as follows:
 *
 *          #define FILL_LONG(x,value) .long value .REPT (x-1) ,value .ENDR      
 *
 *      And  FILL_LONG(5,-2) will produce the following output:
 *
 *          .long  -2  , -2  , -2  , -2  , -2
 *
 *      But NOTE: as86 does not work proper on more then 63 values per line !
 *                ====      ==============               =========
 *
 *    You may call as86 for source file test.S as follows:
 *
 *      gcc -E test.S | tools86 -E >test.s
 *      as86 -0 -w -j -g -o test.o
 *
 *    NOTE: 
 *    The -g switch is necessary to avoid local "a" labels to be
 *    converted later (see below).
 *    The -j switch is neccessary for jumping larger then +/-128.
 *    However, you may compile without the -j switch and use the
 *    macros from macros86.h for 16-bit (near) displacement.
 *    e.g.  jmp label    <==>  JMPL (label)
 *          jne label    <==>  JNEL (label)
 *    This produces shorter and faster code.
 *
 * 2. Making the asm86/ld86 output linkable to GNU-ld
 *
 *    This is done by modifying the header of ld86 outfile to built a GNU-a.out
 *    and by converting the symbol table ("a" flags become "T" flags):
 *
 *      ld86 -0 -r -o test test.o
 *      tools86 test
 *      mv test test.o
 *
 *    The resulting test.o is now linkable with ld and can be put into
 *    a library.
 *    The only restriction is, that you can only have a .text segment, 
 *    ( no .data or .bss). But you *can* have public
 *    global symbols and reference them from other GCC produced code.
 *
 * NOTE:
 *    Do not use the asm86 -a switch, else you get error on instructions like
 *       pop word ptr [bx]
 *    (don't know why)
 *    Be careful if using .org (do not overlap areas), because ld86 will
 *    then write infinitely to your disk ( be prepared for <Ctrl>C ! ).
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h> 
#include <errno.h>
#include <ctype.h>

#ifdef __linux__
struct gnu_header {
  unsigned long a_info;         /* Use macros N_MAGIC, etc for access */
  unsigned long a_text;         /* length of text, in bytes */
  unsigned long a_data;         /* length of data, in bytes */
  unsigned long a_bss;          /* length of uninitialized data area for file, in bytes */
  unsigned long a_syms;         /* length of symbol table data in file, in bytes */
  unsigned long a_entry;        /* start address */
  unsigned long a_trsize;       /* length of relocation info for text, in bytes */
  unsigned long a_drsize;       /* length of relocation info for data, in bytes */
};

struct bsd_header {            /* a.out header */
  unsigned char a_magic[2];     /* magic number */
  unsigned char a_flags;        /* flags, see below */
  unsigned char a_cpu;          /* cpu id */
  unsigned char a_hdrlen;       /* length of header */
  unsigned char a_unused;       /* reserved for future use */
  unsigned short a_version;     /* version stamp (not used at present) */
  long          a_text;         /* size of text segement in bytes */
  long          a_data;         /* size of data segment in bytes */
  long          a_bss;          /* size of bss segment in bytes */
  long          a_entry;        /* entry point */
  long          a_total;        /* total memory allocated */
  long          a_syms;         /* size of symbol table */
                                /* SHORT FORM ENDS HERE */
#if 0 /* we support only short form */
  long          a_trsize;       /* text relocation size */
  long          a_drsize;       /* data relocation size */
  long          a_tbase;        /* text relocation base */
  long          a_dbase;        /* data relocation base */
#endif
};
#endif

#ifdef __linux__
static int header_ld86out_to_gnuasout(struct bsd_header *bsd, struct gnu_header *gnu)
{
  if (  ((uint32_t *)bsd)[0] != 0x10000301 ) return -1;
  if (  ((uint32_t *)bsd)[1] != 0x20 ) return -1;
  gnu->a_info   = 0x107;
  gnu->a_text   = bsd->a_text;
  gnu->a_data   = bsd->a_data;
  gnu->a_bss    = bsd->a_bss;
  gnu->a_syms   = bsd->a_syms;
  gnu->a_entry  = bsd->a_entry;
  gnu->a_trsize = 0;
  gnu->a_drsize = 0;
  return 0;
}

struct sym_entry {
  unsigned long addr,flags,stringoffs;
};
#endif

static int change_aout(char *objfile, int update_symtable)
{
  FILE * f;
#ifdef __linux__
  struct bsd_header bsd; 
  struct gnu_header gnu;
#endif
  int ret;
  if ( (f = fopen(objfile,"r+")) == 0) {
    perror("problems on opening obj file");
    return errno;
  }
#ifdef __linux__
  if (fread(&bsd,sizeof(gnu),1,f) != 1 ) {
    fclose(f);
    return -1;
  }
  if (header_ld86out_to_gnuasout(&bsd, &gnu)) {
    fclose(f);
    return -1;
  }
  fseek(f,0,SEEK_SET);
  ret = fwrite(&gnu,sizeof(gnu),1,f);
  if(ret != 1 ) {
    fclose(f);
    return -1;
  }
#endif
  if (update_symtable && gnu.a_syms) {
    struct sym_entry *b = malloc(gnu.a_syms);
    int nsyms=gnu.a_syms / sizeof(struct sym_entry);
    int i;
    fseek(f,sizeof(gnu)+gnu.a_text,SEEK_SET);
    if (fread(b,gnu.a_syms,1,f) != 1 ) {
      fclose(f);
      return -1;
    }
    for (i=0; i<nsyms; i++) {
      if (b[i].flags == 2) b[i].flags = 5; /* convert a to T */
    }
#ifdef __linux__
    fseek(f,sizeof(gnu)+gnu.a_text,SEEK_SET);
#endif
    ret = fwrite(b,gnu.a_syms,1,f);
  }
  fclose(f);
  if(ret != 1 ) return -1;
  return 0;
}


static int preprocess(FILE *f, FILE *fo)
{
  #define SIZE_BBUF 8
  #define MASK_BBUF (SIZE_BBUF-1)
  #define INC_BBUF(i,step) (i = (i+(step)) & MASK_BBUF)
  #define COPY_BUF_SIZE 0x4000
  
  char b[MASK_BBUF+1];
  char cb[COPY_BUF_SIZE];
  int buf_i=0, buf_n=0, copy=0, cbuf_i=0, copy_count=0;
  int c;
  

  auto inline void _skipwhite(void);
  inline void _skipwhite(void) {
    while ((c=fgetc(f)) != EOF) {
      if (!isspace(c)) break;
    }
    ungetc(c,f);
  }
  
  auto inline int was(char *s);
  inline int was(char *s) {
    int i=buf_i, n=buf_n;
    while (*s) {
      INC_BBUF(i,-1);
      if ((--n) < 0) return 0;
      if (*s != b[i]) return 0;
      s++;
    }
    return 1;
  }

  auto inline void _fputc(char c);
  inline void _fputc(char c) {
    fputc(c,fo);
    if (copy) {
      cb[cbuf_i++] = c;
      if (cbuf_i >= COPY_BUF_SIZE) {
        fprintf(stderr, "copy buffer overflow\n");
        exit(1);
      }
    }
  }
  
  auto inline void _put(char c);
  inline void _put(char c) {
    if (buf_n >= SIZE_BBUF) _fputc(b[buf_i]);
    else buf_n++;
    b[buf_i]=c;
    INC_BBUF(buf_i,1);
  }
  
  auto inline void _flush(void);
  inline void _flush(void) {
    INC_BBUF(buf_i,-buf_n);
    while (buf_n-- > 0) {
      _fputc(b[buf_i]);
      INC_BBUF(buf_i,1);
    }
    buf_i = 0;
    buf_n = 0;
  }
  
  auto inline void _unget(int count);
  inline void _unget(int count) {
    if (count > buf_n) count = buf_n;
    INC_BBUF(buf_i,-count);
    buf_n -= count;
  }
  
  auto inline int _nextnum(void);
  inline int _nextnum() {
    char auxb[40];
    int i=0, ii=0;
    _skipwhite();
    while ((c=fgetc(f)) !=EOF) {
      if ( c=='(' ) {
        ii +=_nextnum();
      }
      else {
        if (isspace(c)) {
          _skipwhite(); 
          if (c==')') fgetc(f);
          break;
        }
        if (c==')') break;
        if (i && (c=='+' || c=='-')) {
          ungetc(c,f);
          break;
        }
        if (ispunct(c) && (!(c=='+' || c=='-'))) {
          ungetc(c,f);
          return ii;
        }
        auxb[i++]=c;
      }
    }
    if (c=='+' || c=='-') ii +=_nextnum();
    errno = 0;
    if (i) {
      auxb[i]=0;
      i=strtol(auxb,0,0);
    }
    if (errno) i=0;
    return i+ii;
  }

  while ((c=fgetc(f)) != EOF) {
    switch (c) {
      case '!': {
        if (was("!!")) {
          _unget(2);
          _put('\n');
        }
        else _put(c);
        break;
      }
      case 'T': {
        if ((!copy) && was("PER.")) {
          _unget(4);
          copy_count = _nextnum()-1;
          _flush();
          cbuf_i=0;
          copy=1;
        }
        else _put(c);
        break;
      }
      case 'R': {
        if (copy && was("DNE.")) {
          int i,j;
          _unget(4);
          _flush();
          copy=0;
          if (copy_count <=0) {
            fprintf(stderr, "tools86: illegal repeat count after .REPT\n");
            exit(1);
          }
          for (j=0; j<copy_count; j++) {
            for (i=0; i<cbuf_i; i++) _put(cb[i]);
          }
        }
        else _put(c);
        break;
      }
      default: {
        _put(c);
      }
    }
  }
  _flush();
  return 0;
}


static int usage(void)
{
  fprintf(stderr,
    "USAGE:\n\n"
    "tools86 objfile\n"
    "  if called like above, it converts the header of a ld86 output\n"
    "  to GNU-a.out format, so you can use it later with ld as a normal\n"
    "  linkable *.o object file. But you must only have *one* segment\n"
    "  (e.g. .text or .data, but not both).\n\n"
    "tools86 -E\n"
    "  if called like above, it preprocesses stdin to stdout to support some\n"
    "  features, the as86 normally does not support\n"
  );
  exit(1);
}

int main (int argc, char** argv)
{
  if (argc <= 1) usage();

  if (!strcmp(argv[1],"-E")) {
    return preprocess(stdin,stdout);
  }
  
  if (change_aout(argv[1],1)) {
    fprintf(stderr, "conv_aout: error on converting\n");
    return 1;
  }  
  return 0;
}


