/*
 * Install a module in the kernel.
 *
 * Originally by Anonymous (as far as I know...)
 * Linux version by Bas Laarhoven <bas@vimec.nl>
 * Modified by Jon Tombs.
 *
 * Support for transient and resident symbols
 * added by Bjorn Ekwall <bj0rn@blox.se> in 1994 (C)
 * See the file COPYING for your rights (GNU GPL)
 *
 * Load map option conceived by Derek Atkins <warlord@MIT.EDU>
 *
 * Support for (z)System.map resolving of unresolved symbols (HACKER_TOOL)
 * added by Hans Lermen <lermen@elserv.ffm.fgan.de>
 * NOTE: This feature was added to insmod by the DOSEMU-team
 *       and released as part of the dosemu-distribution 
 *       much *earlier* then the release of modules-1.1.67.
 *       Unfortunately the meanings of the -m switch differs.
 *       Because of this I decided to take over the meaning from the official
 *       moduls release, but replaced the very dumb SHAME_ON_YOU-code
 *       by the more sophisticated and *really* secure HACKER_TOOL-code.
 *       So, for old users of dosemu's insmod:  -m, -M  becomes -z, -Z ! Ok?
 *       Starting with Linux 1.1.76 the zSystem.map exists no longer,
 *       and is replaced by System.map (same file structure), but don't worry,
 *       if zSystem.map is not found, System.map is taken by default.
 */

#if 1
  #define HACKER_TOOL
#endif

#ifdef HACKER_TOOL
  #if 0
    #include "kversion.h"
  #else
    #define KERNEL_VERSION 1001080 /* last verified kernel version */
  #endif
#else
  /*#define SHAME_ON_YOU*/
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <a.out.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <fcntl.h>

#define TRANSIENT 2	/* flag that symbol was resolved by another module */
#define DEF_BY_MODULE 1	/* flag that this is a define, _not_ a reference */
#define RESIDENT 0	/* flag that symbol was resolved by resident kernel */

extern struct kernel_sym *load_syms(struct utsname *);

/*
 * This is here as syscall.h and sys/syscall.h redefine the defines in
 * unistd.h why doesn't unistd #include them?
 */

extern int syscall(int, ...);

/*
 * side-effect of this: these defs are needed... :-(
 */
extern int close(int);
extern int read(int, char *, size_t);
extern int write(int, const char *, size_t);

struct symbol {
	struct nlist n;
	struct symbol *child[2];
};

struct exec header;
char *textseg;
char *dataseg;
struct symbol *symroot;
int nsymbols;
struct symbol *symtab;
char *stringtab;
unsigned long addr;
int export_flag = 1;
int force_load = 0;
int loadmap = 0;

void relocate(char *, int, int, FILE *);
void hidesym(const char *);
int defsym(const char *, unsigned long, int, int);
struct symbol *findsym(const char *, struct symbol *);
unsigned long looksym(const char *);

struct kernel_sym nullsym;

void *
ckalloc(size_t nbytes)
{
	void *p;

	if ((p = malloc(nbytes)) == NULL) {
		fputs("insmod:  malloc failed\n", stderr);
		exit(2);
	}
	return p;
}


static int create_module(const char *name, unsigned long size)
{
	return syscall( __NR_create_module, name, size);
}

static int init_module(const char *name, void *code, unsigned codesize,
		struct mod_routines *routines,
		struct symbol_table *syms) {
	return syscall( __NR_init_module, name, code, codesize, routines,
		syms);
}

static int delete_module(const char *name)
{
	return syscall( __NR_delete_module, name);
}

static int get_kernel_syms(struct kernel_sym *buffer)
{
	return syscall( __NR_get_kernel_syms, buffer);
}

void check_version(int force_load)
{
	struct utsname uts_info;
	unsigned long kernel_version;
	int i;

	/* Check if module and kernel version match */
	if ((i = uname( &uts_info)) != 0) {
		fprintf( stderr, "uname call failed with code %d\n", i);
		exit( 2);
	}
	kernel_version = looksym( "_kernel_version");
	if (strcmp( (char*) textseg + kernel_version, uts_info.release)) {
		fprintf( stderr,
			"Error: module's `kernel_version' doesn't match the current kernel.\n%s\n%s\n",
			"       Check the module is correct for the current kernel, then",
			"       recompile the module with the correct `kernel_version'.");
		if (force_load)
			fprintf(stderr, "       Loading it anyway...\n");
		else
			exit( 2);
	}

}

#ifdef HACKER_TOOL   /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

struct zSystem_entry {
  struct zSystem_entry *next;
  int value;;
  char *name;
};

int use_zSystem=0, use_zSystem_local=0;

char *zsystem_map_name="/usr/src/linux/zSystem.map";

#if KERNEL_VERSION >= 1001076
char *system_map_name="/usr/src/linux/System.map";
static void check_system_map_name()
{
  struct stat buf;
  if (stat(zsystem_map_name, &buf)) zsystem_map_name=system_map_name; 
}
#endif

static struct zSystem_entry *build_zSystem_syms(char *name)
{
  FILE *f;
  struct zSystem_entry *table=0;
  char c;
  int i;
  char n[256];
  static char magic[]="00100000 t startup_32";
  
  f=fopen(name, "r");
  if (!f) {
    perror("reading zSystem.map:");
    return 0;
  }
  i=fread(n,sizeof(magic),1,f);
  n[sizeof(magic)-1]=0;
  if ((i != 1) || (strcmp(magic,n)) ) {
    fprintf(stderr, "wrong file format of %s\n",name);
    return 0;
  } 
  while (!feof(f)) {
    fscanf(f, "%08x %c %s\n", &i, &c, n);
    switch (c) {
      case 't': case 'd': case 'b': 
        if (!use_zSystem_local) break;
        /* else fall through */
      case 'T': case 'D': case 'B': {
        struct zSystem_entry *p=malloc(sizeof(struct zSystem_entry));
        p->name=strdup(n);
        p->value=i;
        p->next=table;
        table=p;
      }
    }
  }
  fclose(f);
  return table;
}


static struct kernel_sym *find_resident_sym(struct kernel_sym *ksym, int nksyms, char *name)
{
  for (; nksyms >0 ; --nksyms, ksym++) {
    if (!strcmp(ksym->name,name)) return ksym;
  }
  return 0;
}

static int check_for_right_zSystem_map(struct zSystem_entry *zsym, struct kernel_sym *ksymtab, int nksyms)
{
  struct kernel_sym *ksym, *p;
  int i,matches;
  
  if (!zsym) return 0;
  
  for (ksym = ksymtab, i = 0 ; i < nksyms ; ++i, ksym++) {
    if (ksym->name[0] == '#') {
      if (!ksym->name[1]) break;
    }
  }
  ksym++; 
  nksyms -= i+1;
  matches=0;
  
  while (zsym) {
    p=find_resident_sym(ksym,nksyms,zsym->name);
    if (p) {
      if (p->value == zsym->value) matches++;
    }
    zsym=zsym->next;
  }
  return (nksyms == matches);
}


#endif  /* HACKER_TOOL <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */


int
main(int argc, char **argv)
{
	FILE *fp;
	struct kernel_sym *curr_module = &nullsym;
	struct kernel_sym *ksymtab = NULL;
	struct kernel_sym *ksym = NULL;
	struct mod_routines routines;
	struct symbol *sp;
	unsigned long init_func, cleanup_func;
	long filesize;
	int fatal_error;
	int i;
	int len;
	int nksyms;
	int pos;
	int progsize;

	struct symbol_table *newtab;
	int module_refs = 0; /* number of references modules */
	int n_symboldefs = 0; /* number of defined symbols in the module */
	int string_table_size = 0; /* size of the new symbol table string table */
	struct internal_symbol *symp;
	struct module_ref *refp;
	char *stringp;

	char *filename;
	char *modname = NULL;
	char *p;
#ifndef HACKER_TOOL
#ifdef SHAME_ON_YOU
	char *zfile = NULL;
#endif
#endif


	while (argc > 1 && (argv[1][0] == '-')) {
		p = &(argv[1][1]);
		while (*p) {
			switch (*p) {
			case 'o':
				modname = argv[2];
				--argc;
				++argv;
				break;

			case 'f': /* force loading */
				force_load = 1;
				break;

			case 'x': /* do _not_ export externs */
				export_flag = 0;
				break;

			case 'm': /* generate loadmap */
				loadmap = 1;
				break;
#ifdef HACKER_TOOL
			case 'Z': /* resolve over zSystem map */
				++argv; --argc;
				if (argv[1]) zsystem_map_name = argv[1];
				/* fall through */
			case 'z': /* resolve over zSystem map */
				use_zSystem = 1;
				break;
			case 'l': /* resolve over zSystem map, take also local symbols */
				use_zSystem_local = 1;
				break;
#else
#ifdef SHAME_ON_YOU
			case 'z': /* read from a "zSystems.map" file */
				zfile = argv[2];
				--argc;
				++argv;
				break;
#endif
#endif
			}
			++p;
		}
		--argc;
		++argv;
	}

	if (argc < 2) {
#ifdef HACKER_TOOL
		fputs("Usage:\n"
		      "insmod [-f] [-x] [-o name] [-m] [-l] [-z | -Z mapfile] module [[sym=value]...]\n"
		      "\n"
		      "  module     Filename of a loadable kernel module (*.o)\n"
		      "  -o name    Set internal modulname to name\n"
		      "  -x         do *not* export externs\n"
		      "  -m         generate loadmap (so crashes can be traced down)\n"
		      "  -f         Force loading under wrong kernel version\n"
		      "             (together with -z also disables mapfile check)\n"
		      "  -z         At last use /usr/src/linux/zSystem.map to resolve\n"
		      "             (on Linux > 1.1.76:  System.map)\n" 
		      "  -Z mapfile As -z, but use mapfile\n"
		      "  -l         Together with -z, -Z, also take local symbols from map\n"
		      "             (But note, local symbols can be multiple defined, last one is taken)\n"
		      , stderr);
#else
		fputs("Usage: insmod [-f] [-x] [-o name] [-m] module [[sym=value]...]\n", stderr);
#endif
		exit(2);
	}

	filename = argv[1];
	argv += 2;
	--argc;

	/* construct the module name */
	if (modname == NULL) {
		if ((p = strrchr(filename, '/')) != NULL)
			p++;
		else
			p = filename;
		len = strlen(p);
		if (len > 2 && strcmp(p + len - 2, ".o") == 0)
			len -= 2;
		else if (len > 4 && strcmp(p + len - 4, ".mod") == 0)
			len -= 4;

		modname = (char*) ckalloc(len + 1);
		memcpy(modname, p, len);
		modname[len] = '\0';
	}

	/* open file and read header */
	if ((fp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Cannot open %s\n", filename);
		exit(2);
	}

	fread(&header, sizeof header, 1, fp);
	if (feof(fp) || ferror(fp)) {
		fprintf(stderr, "Could not read header of %s\n", filename);
		exit(2);
	}

	if (N_MAGIC(header) != OMAGIC) {
		fprintf(stderr, "%s: not an object file\n", filename);
		exit(2);
	}

	/* read in text and data segments */
	textseg = (char*) ckalloc(header.a_text + header.a_data);
	fread(textseg, header.a_text + header.a_data, 1, fp);
	if (feof(fp) || ferror(fp)) {
		fprintf(stderr, "Error reading %s\n", filename);
		exit(2);
	}
	dataseg = textseg + header.a_text;

	/* read in the symbol table */
	fseek(fp, 0L, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, N_SYMOFF(header), SEEK_SET);
	nsymbols = header.a_syms / sizeof (struct nlist);
	symtab = (struct symbol*) ckalloc(nsymbols * sizeof (*symtab));

	for (i = nsymbols, sp = symtab ; --i >= 0 ; sp++) {
		fread(&sp->n, sizeof sp->n, 1, fp);
		sp->child[0] = sp->child[1] = NULL;
	}

	if (feof(fp) || ferror(fp)) {
		fprintf(stderr, "Error reading %s\n", filename);
		exit(2);
	}

	len = filesize - N_STROFF(header);
	stringtab = (char*) ckalloc(len);
	fread(stringtab, len, 1, fp);

	if (feof(fp) || ferror(fp)) {
		fprintf(stderr, "Error reading %s\n", filename);
		exit(2);
	}

	symroot = NULL;
	for (i = nsymbols, sp = symtab ; --i >= 0 ; sp++) {
		pos = sp->n.n_un.n_strx;
		if (pos < 0 || pos >= len) {
			fprintf(stderr, "Bad nlist entry\n");
			exit(2);
		}
		/* look up name and add sp to binary tree */
		findsym(stringtab + sp->n.n_un.n_strx, sp);
		if ((sp->n.n_type & N_EXT) && (sp->n.n_type != N_EXT))
			sp->n.n_other = DEF_BY_MODULE; /* abuse: mark extdef */
		else
			sp->n.n_other = 0; /* abuse: mark extref */
	}

	/* get initialization and cleanup routines */
	init_func = looksym("_init_module");
	cleanup_func = looksym("_cleanup_module");

	/* get the size of the current kernel symbol table */
	nksyms = get_kernel_syms(NULL);

	if (nksyms < 0) {
		fprintf(stderr, "get_kernel_sys failed: Cannot find Kernel symbols!\n");
		exit(2);
	}

	if (nksyms) {
		ksymtab = (struct kernel_sym *) ckalloc(nksyms * sizeof *ksymtab);
		/* NOTE!!! The order of the symbols is important */
		if (get_kernel_syms(ksymtab) != nksyms) {
			fprintf(stderr, "Kernel symbol problem\n");
			exit(2);
		}
	}

	/* check version info */
	check_version(force_load);

	/* bind undefined symbols */
	defsym("_mod_use_count_", 0 - sizeof (int), N_BSS | N_EXT, TRANSIENT);
	progsize = header.a_text + header.a_data + header.a_bss;

	/* First: resolve symbols using kernel transient symbols */
	/* Then: use resident kernel symbols to resolve the last ones... */
	for (ksym = ksymtab, i = 0 ; i < nksyms ; ++i, ksym++) {
		/* Magic in this version of the new get_kernel_syms:
		 * Every module is sent along as a symbol,
		 * where the module name is represented as "#module", and
		 * the adress of the module struct is stuffed into the value.
		 * The name "#" means that the symbols that follow are
		 * kernel resident.
		 */
		if (ksym->name[0] == '#') {
			curr_module = ksym;
			continue;
		}

		if (defsym(ksym->name, ksym->value, N_ABS | N_EXT,
		/* this is safe since curr_module was initialized properly */
			(curr_module->name[1]) ?  TRANSIENT : RESIDENT)) {
			/* kludge: mark referenced modules */
			if (curr_module->name[1] && /* but not the kernel */
				(curr_module->name[0] == '#')) {
				curr_module->name[0] = '+';
				++module_refs;
			}
		}
	}

#ifndef HACKER_TOOL
#ifdef SHAME_ON_YOU
	/* Fake it, by reading a zSystem.map file */
	if (zfile) {
		FILE *zfp;
		char line[80];
		char dummy[80];
		struct kernel_sym zsym;

		if ((zfp = fopen(zfile, "r")) == (FILE *)0) {
			perror(zfile);
		}
		else {
			while (fgets(line, 80, zfp)) {
				sscanf(line, "%lx %s %s", &zsym.value, dummy, zsym.name);
				defsym(zsym.name, zsym.value, N_ABS | N_EXT, RESIDENT);
			}
		}
	}
#endif
#endif

	/* Change values according to the parameters to "insmod" */
	{
		struct symbol *change;
		char *param;
		char *val;
		char internal_name[SYM_MAX_NAME];
		int value;

		while (argc > 1) {
			param = *argv;
			++argv;
			--argc;

			if ((val = strchr(param, '=')) == NULL)
				continue;
			*val++ = '\0';
			if (*val < '0' || '9' < *val) {
				fprintf(stderr, "Symbol '%s' has an illegal value: %s\n", param, val);
				exit(2);
			}

			if (val[0] == '0') {
				if (val[1] == 'x')
					sscanf(val, "%x", &value);
				else
					sscanf(val, "%o", &value);
			}
			else
				sscanf(val, "%d", &value);

			sprintf(internal_name, "_%s", param);
			change = findsym(internal_name, NULL);
			if (change == NULL) {
				fprintf(stderr, "Symbol '%s' not found\n", param);
				exit(2);
			}
			*((int *)(textseg + change->n.n_value)) = value;
		}
	}

#ifdef HACKER_TOOL
	/* now resolve over zSystem.map */
	if (use_zSystem) {
	  struct zSystem_entry *zsym;
		
#if KERNEL_VERSION >= 1001076
	  check_system_map_name();
#endif
	  fprintf(stderr,"Now resolving from %s\n", zsystem_map_name);
	  zsym=build_zSystem_syms(zsystem_map_name);
	  if (!zsym) exit(2);
	  if (!force_load) {
	    if (!check_for_right_zSystem_map(zsym, ksymtab,  nksyms)) {
	      fprintf(stderr,"%s doesn\'t match actual kernel\n",zsystem_map_name);
              exit(2);
	    }
	  }
	  while (zsym) {
	    if (defsym(zsym->name, zsym->value, N_ABS | N_EXT,  RESIDENT )) {
	      fprintf(stderr,"resolved: %08x %s\n", zsym->value, zsym->name);
	    }
	    zsym=zsym->next;
	  }
	}
#endif

	/* allocate space for "common" symbols */
	/* and check for undefined symbols */
	fatal_error = 0;
	for (sp = symtab ; sp < symtab + nsymbols ; sp++) {
		if (sp->n.n_type == (N_UNDF | N_EXT) && sp->n.n_value != 0) {
			sp->n.n_type = N_BSS | N_EXT;
			len = sp->n.n_value;
			sp->n.n_value = progsize;
			progsize += len;
			progsize = (progsize + 3) &~ 03;
		} else if ((sp->n.n_type &~ N_EXT) == N_UNDF) {
			fprintf(stderr, "%s undefined\n", sp->n.n_un.n_name);
			fatal_error = 1;
		}
	}
	if (fatal_error)
		exit(2);

	/* create the module */
	errno = 0;
	/* We add "sizeof (int)" to skip over the use count */
	addr = create_module(modname, progsize) + sizeof (int);
	switch (errno) {
	case EEXIST:
		fprintf(stderr, "A module named %s already exists\n", modname);
		exit(2);
	case ENOMEM:
		fprintf(stderr, "Cannot allocate space for module\n");
		exit(2);
	case 0:
		break;
	default:
		perror("create_module");
		exit(2);
	}

	/* perform relocation */
	fseek(fp, N_TRELOFF(header), SEEK_SET);
	relocate(textseg, header.a_text, header.a_trsize, fp);
	relocate(dataseg, header.a_data, header.a_drsize, fp);
	init_func += addr;
	cleanup_func += addr;

	/* "hide" the module specific symbols, not to be exported! */
	hidesym("_cleanup_module");
	hidesym("_init_module");
	hidesym("_kernel_version");
	hidesym("_mod_use_count_");

	/* Build the module symbol table */
	/* abuse of n_other:
	 * 0	refer kernel resident
	 * 1	define module symbol, inserted in kernel syms
	 * 2	refer module symbol
	 * 3	won't happen (define resolved by other module?)
	 */

	/*
	 * Get size info for the new symbol table
	 * we already have module_refs
	 */
	for (sp = symtab ; export_flag && (sp < symtab + nsymbols) ; sp++) {
		if ((sp->n.n_type & N_EXT) && (sp->n.n_other & DEF_BY_MODULE)) {
			string_table_size += strlen(sp->n.n_un.n_name) + 1;
			++n_symboldefs;
		}
	}
	newtab = (struct symbol_table *)ckalloc(sizeof(struct symbol_table) +
		n_symboldefs * sizeof(struct internal_symbol) +
		module_refs * sizeof(struct module_ref) +
		string_table_size);

	newtab->size = sizeof(struct symbol_table) +
		n_symboldefs * sizeof(struct internal_symbol) +
		module_refs * sizeof(struct module_ref) +
		string_table_size;

	newtab->n_symbols = n_symboldefs;
	newtab->n_refs = module_refs;

	symp = &(newtab->symbol[0]);
	stringp = ((char *)symp) + 
		n_symboldefs * sizeof(struct internal_symbol) +
		module_refs * sizeof(struct module_ref);

	/* update the string pointer (to a string index) in the symbol table */
	for (sp = symtab ; export_flag && (sp < symtab + nsymbols) ; sp++) {
		if ((sp->n.n_type & N_EXT) && (sp->n.n_other & DEF_BY_MODULE)) {
			symp->addr = (void *)(sp->n.n_value + addr);
			symp->name = (char *)(stringp - (char *)newtab);
			strcpy(stringp, sp->n.n_un.n_name);
			stringp += strlen(sp->n.n_un.n_name) + 1;
			++symp;
		}
	}

	refp = (struct module_ref *)symp;
	for (ksym = ksymtab, i = 0 ; i < nksyms ; ++i, ksym++) {
		if (ksym->name[0] == '+') {
			refp->module = (struct module *)ksym->value;
			++refp;
		}
	}

	/* load the module into the kernel */
	/* NOTE: the symbol table == NULL if there are no defines/updates */
	routines.init = (int (*)(void)) init_func;
	routines.cleanup = (void (*)(void)) cleanup_func;

	if (init_module(modname, textseg, header.a_text + header.a_data,
		&routines,
		((n_symboldefs + module_refs)? newtab : NULL)) < 0) {

		if (errno == EBUSY) {
			fprintf(stderr, "Initialization of %s failed\n", modname);
		} else {
			perror("init_module");
		}
		delete_module(modname);
		exit(1);
	}

	/*
	 * Print a loadmap so that kernel panics can be identified.
	 * Load map option conceived by Derek Atkins <warlord@MIT.EDU>
	 */
	for (sp = symtab; loadmap && (sp < symtab + nsymbols) ; sp++) {
		char symtype = 'u';

		if ((sp->n.n_type & ~N_EXT) == N_ABS)
			continue;

		switch (sp->n.n_type & ~N_EXT) {
		case N_TEXT | N_EXT: symtype = 'T'; break;
		case N_TEXT: symtype = 't'; break;
		case N_DATA | N_EXT: symtype = 'D'; break;
		case N_DATA: symtype = 'd'; break;
		case N_BSS | N_EXT: symtype = 'B'; break;
		case N_BSS: symtype = 'b'; break;
		}

		printf("%08lx %c %s\n",
			sp->n.n_value + addr, symtype, sp->n.n_un.n_name);
	}

	if (nksyms > 0)
		free(ksymtab); /* it has done its job */

	exit(0);
}


void
relocate(char *seg, int segsize, int len, FILE *fp)
{
	struct relocation_info rel;
	unsigned long val;
	struct symbol *sp;

	while ((len -= sizeof rel) >= 0) {
		fread(&rel, sizeof rel, 1, fp);
#ifdef DEBUG
		printf("relocate %s:%u\n", seg == textseg? "text" : "data",
			rel.r_address);
#endif
		if (rel.r_address < 0 || rel.r_address >= segsize) {
			fprintf(stderr, "Bad relocation\n");
			exit(2);
		}
		if (rel.r_length != 2) {
			fprintf(stderr, "Unimplemented relocation:  r_length = %d\n", rel.r_length);
			exit(2);
		}
		val = * (long *) (seg + rel.r_address);
		if (rel.r_pcrel) {
			val -= addr;
#ifdef DEBUG
			printf("pc relative\n");
#endif
		}
		if (rel.r_extern) {
			if (rel.r_symbolnum >= nsymbols) {
				fprintf(stderr, "Bad relocation\n");
				exit(2);
			}
			sp = symtab + rel.r_symbolnum;
			val += sp->n.n_value;
			if ((sp->n.n_type &~ N_EXT) != N_ABS)
				val += addr;
		} else if (rel.r_symbolnum != N_ABS) {
			val += addr;
		}
		* (long *) (seg + rel.r_address) = val;
	}
}

void
hidesym(const char *name)
{
	struct symbol *sp;

	if ((sp = findsym(name, NULL)) != NULL)
		sp->n.n_type &= ~N_EXT;
}

int
defsym(const char *name, unsigned long value, int type, int source)
{
	struct symbol *sp;

	if ((sp = findsym(name, NULL)) == NULL)
		return 0; /* symbol not used */
	/* else */

	if (sp->n.n_type != (N_UNDF | N_EXT)) {
		/*
		 * Multiply defined?
		 * Well, this is what we _want_!
		 * I.e. a module _should_ be able to replace a kernel resident
		 * symbol, or even a symbol defined by another module!
		 * NOTE: The symbols read from the kernel (the transient ones)
		 * MUST be handled in the order:
		 *   last defined, ..., earlier defined, ...
		 */
		return 0; /* symbol not used */
	}
	/* else */
	sp->n.n_type = type;
	sp->n.n_value = value;
	/*
	 * n.n_other abused
	 * in order to discriminate between defines and references
	 * and resolves by kernel resident or transient symbols...
	 */
	sp->n.n_other |= source;

	return 1; /* symbol used */
}


/*
 * Look up an name in the symbol table.  If "add" is not null, add a
 * the entry to the table.  The table is stored as a splay tree.
 */
struct symbol *
findsym(const char *key, struct symbol *add)
{
	struct symbol *left, *right;
	struct symbol **leftp, **rightp;
	struct symbol *sp1, *sp2, *sp3;
	int cmp;
	int path1, path2;

	if (add) {
		add->n.n_un.n_name = (char *)key;
#if 0
		if ((add->n.n_type & N_EXT) == 0)
			return add;
#endif
	}
	sp1 = symroot;
	if (sp1 == NULL)
		return add? symroot = add : NULL;
	leftp = &left, rightp = &right;
	for (;;) {
		cmp = strncmp( sp1->n.n_un.n_name, key, SYM_MAX_NAME);
		if (cmp == 0)
			break;
		if (cmp > 0) {
			sp2 = sp1->child[0];
			path1 = 0;
		} else {
			sp2 = sp1->child[1];
			path1 = 1;
		}
		if (sp2 == NULL) {
			if (! add)
				break;
			sp2 = add;
		}
		cmp = strncmp( sp2->n.n_un.n_name, key, SYM_MAX_NAME);
		if (cmp == 0) {
one_level_only:
			if (path1 == 0) {	/* sp2 is left child of sp1 */
				*rightp = sp1;
				rightp = &sp1->child[0];
			} else {
				*leftp = sp1;
				leftp = &sp1->child[1];
			}
			sp1 = sp2;
			break;
		}
		if (cmp > 0) {
			sp3 = sp2->child[0];
			path2 = 0;
		} else {
			sp3 = sp2->child[1];
			path2 = 1;
		}
		if (sp3 == NULL) {
			if (! add)
				goto one_level_only;
			sp3 = add;
		}
		if (path1 == 0) {
			if (path2 == 0) {
				sp1->child[0] = sp2->child[1];
				sp2->child[1] = sp1;
				*rightp = sp2;
				rightp = &sp2->child[0];
			} else {
				*rightp = sp1;
				rightp = &sp1->child[0];
				*leftp = sp2;
				leftp = &sp2->child[1];
			}
		} else {
			if (path2 == 0) {
				*leftp = sp1;
				leftp = &sp1->child[1];
				*rightp = sp2;
				rightp = &sp2->child[0];
			} else {
				sp1->child[1] = sp2->child[0];
				sp2->child[0] = sp1;
				*leftp = sp2;
				leftp = &sp2->child[1];
			}
		}
		sp1 = sp3;
	}
	/*
	 * Now sp1 points to the result of the search.  If cmp is zero,
	 * we had a match; otherwise not.
	 */
	*leftp = sp1->child[0];
	*rightp = sp1->child[1];
	sp1->child[0] = left;
	sp1->child[1] = right;
	symroot = sp1;
	return cmp == 0? sp1 : NULL;
}


unsigned long
looksym(const char *name)
{
	struct symbol *sp;

	sp = findsym(name, NULL);
	if (sp == NULL) {
		fprintf(stderr, "%s undefined\n", name);
		exit(2);
	}
	return sp->n.n_value;
}
