/*
 * Install a module in the kernel.
 *
 * See the file COPYING for your rights (GNU GPL)
 *
 * Originally by Anonymous (as far as I know...)
 * Linux version by Bas Laarhoven <bas@vimec.nl>
 * Modified by Jon Tombs.
 *
 * Support for transient and resident symbols
 * added by Bjorn Ekwall <bj0rn@blox.se> in 1994 (C)
 *
 * Load map option conceived by Derek Atkins <warlord@MIT.EDU>
 *
 * Support for versioned kernels and symbols: Bjorn Ekwall in December 1994
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <a.out.h>
#include <linux/elf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <linux/unistd.h>	/* Ehrm... well, well, well... */
#include <linux/module.h>
#include <fcntl.h>

#define TRANSIENT 2	/* flag that symbol was resolved by another module */
#define DEF_BY_MODULE 1	/* flag that this is a define, _not_ a reference */
#define RESIDENT 0	/* flag that symbol was resolved by resident kernel */

extern struct kernel_sym *load_syms(struct utsname *);
extern int rmmod(int, char **);
extern int ksyms(int, char **);

/*
 * This is here as syscall.h and sys/syscall.h redefine the defines in
 * unistd.h why doesn't unistd #include them?
 */

#ifdef __cplusplus
extern "C" int syscall(int, ...);
#else
extern int syscall(int, ...);
#endif

#define symname(sp) (aout_flag?(sp->u.n.n_un.n_name):((char *)sp->u.e.st_name))
#define symvalue(sp) (aout_flag?(sp->u.n.n_value):(sp->u.e.st_value))
#define symother(sp) (aout_flag?(sp->u.n.n_other):(sp->u.e.st_other))

struct symbol {
	union {
		struct nlist n;
		struct elf32_sym e;
	} u;
	struct symbol *child[2];
};

/* hack: sizeof(struct elfhdr) > sizeof(struct exec) */
extern Elf32_Ehdr header;
extern int aout_flag;
extern int elf_kernel;
extern int verbose;
extern char *load_aout(FILE *);
extern void relocate_aout(FILE *, long);
extern char *load_elf(FILE *);
extern void relocate_elf(FILE *, long);

void *ckalloc(size_t);
void hidesym(const char *);
int defsym(int (*pfi)(const char *, const char *, size_t), const char *, unsigned long, int, int);
struct symbol *findsym(const char *, struct symbol *, int (*pfi)(const char *, const char *, size_t));
unsigned long looksym(const char *);

extern size_t codesize;
extern size_t progsize;
extern size_t bss1size;
extern size_t bss2size;
extern char *textseg;
extern int nsymbols;
struct symbol *symtab;
extern char *stringtab;
extern unsigned long addr;

/* error.c 29/05/95 21.43.42 */
void insmod_setsyslog (const char *name);
void insmod_error (const char *ctl, ...);
void insmod_debug (const char *ctl, ...);
