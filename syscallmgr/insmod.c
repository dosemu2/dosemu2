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
 * added by Bjorn Ekwall <bj0rn@blox.se> in June 1994 (C)
 *
 * Load map option conceived by Derek Atkins <warlord@MIT.EDU>
 *
 * Support for versioned kernels and symbols: Bjorn Ekwall in December 1994
 *
 * Merged in ksyms and rmmod in December 1994: Bjorn Ekwall
 *
 * Support for ELF modules: Bjorn Ekwall in December 1994 after having
 *                          mangled sources from, and been enlightened
 *                          and supported by Eric Youngdale <eric@aib.com>
 *                          (the kludges are all mine, don't blame Eric...)
 *
 * Support for array initializers: Bjorn Ekwall in January 1995
 * Support for string initializers: Bjorn Ekwall in January 1995
 * Fixed major bug in a.out bss variable handling: March '95, Bas.
 * ELF fixes from H.J.Lu <hjl@nynexst.com>
 * Many ELF and other fixes from:
 * 	James Bottomley <J.E.J.Bottomley@damtp.cambridge.ac.uk>
 * Full support for MODPATH setting: Henrik Storner <storner@osiris.ping.dk>
 * Removed limitation of unversioned module vs. versioned kernel: Bjorn
 * Handle all combinations of ELF vs a.out kernels/modules: Bjorn
 * Added syslog error reporting with option "-s": Jacques Gelinas
 * Added MOD_AUTOCLEAN and option "-k" (for kerneld"): Bjorn and Jacques
 *
 * ---------------------------
 * Support for (z)System.map resolving of unresolved symbols (HACKER_TOOL)
 * added by Hans Lermen <lermen@elserv.ffm.fgan.de>.
 *
 * Changes:
 * 
 * August 1995,      (HACKER_TOOL-2)
 * - fixed 'magic' stuff for 1.3.x, added readonly (R) label types for ELF.
 *   ( Thanks to Eddie C. Dost <ecd@dressler.de> )
 *
 * October 22, 1995
 * - Increased 'magic' check length to allow 'nm -n vmlinux' generated
 *   System.maps be readable (we have this in Slackware 3.0)
 *
 * October 24, 1995  (pre HACKER_TOOL-3)
 * - Reorganized checking for right System.map, needed because
 *   in newer kernels some of the symbols are local (dynamically added
 *   via register_symtab().
 * - Will now silently load, except on errors or option -v.
 * - Also, now only 90% of the matches and 100% of mismatches are valid
 *   for the check (hint from John Michael Floyd <jmf@pwd.nsw.gov.au>).
 * - If option -f is set, then the case 'unversioned_kernel, versioned_module'
 *   can load, even if the versioned module has no symbol 'kernel_version'
 *   (bug found by Uwe Bonnes <bon@elektron.ikp.physik.th-darmstadt.de>).
 *   This bug bug also relates to the 'official' insmod.
 *   A hint for module programmers: Even if you compile with MODVERSIONS,
 *   please include the symbol kernel_version.
 *   This way it will load in an unversioned kernel without -f option.
 *
 * October 26, 1995  (HACKER_TOOL-3)
 * - Added option -p.
 *   Forces insmod to just check if the module matches the kernel.
 *   This option can be used to poll a couple of compiled modules
 *   and/or System.maps in order to find the one that matches.
 *
 * ---------------------------
 *
 * NOTE for HACKER_TOOL:
 *
 * 1.    This feature was added to insmod by the DOSEMU-team
 *       and released as part of the dosemu-distribution 
 *       much *earlier* then the release of modules-1.1.67.
 *       Unfortunately the meanings of the -m switch differs.
 *       Because of this I decided to take over the meaning from the official
 *       moduls release. This feature can be enabled via HACKER_TOOL.
 *       So, for old users of dosemu's insmod:  -m, -M  becomes -z, -Z ! Ok?
 *       Starting with Linux 1.1.76 the zSystem.map exists no longer,
 *       and is replaced by System.map (same file structure), but don't worry,
 *       if zSystem.map is not found, System.map is taken by default.
 *
 * 2.    You may wonder why the dosemu team did not take effort to get
 *       this feature part of the official modutils release ?
 *       In fact, we did (at state of Linux 1.1.83)! 
 *       Bjorn Ekwall, because he saw that the dosemu team needs this,
 *       put the HACKER_TOOL stuff into modutils-1.1.85, but on protest
 *       of Linus he had to remove it later for modutils-1.1.87.
 *       So, we have the ugly state to always repatch the modules stuff
 *       whenever a a new release comes up, ... sorry.
 *       To make this as easy as possible, the HACKER_TOOL routines are
 *       programmed as isolated as possible (the integrated stuff from
 *       modutils-1.1.85 was faster).
 */

#ifndef MOD_AUTOCLEAN /* defined in <linux/module.h> by the "kerneld"-patch */
#define MOD_AUTOCLEAN 0x40000000 /* big enough, but no sign problems... */
#endif

#ifdef HACKER_TOOL
  #ifdef WITHIN_DOSEMU
    #include "kversion.h"
  #else
    #include <linux/version.h>
    #define KERNEL_VERSION  (((LINUX_VERSION_CODE >> 16)*1000000) + (((LINUX_VERSION_CODE >> 8) & 255) *1000) + (LINUX_VERSION_CODE & 255) )
  #endif
  #include <ctype.h>
#endif


static char *default_path[] = {
	".", "/linux/modules",
	"/lib/modules/%s/fs",
	"/lib/modules/%s/net",
	"/lib/modules/%s/scsi",
	"/lib/modules/%s/misc",
	"/lib/modules/default/fs",
	"/lib/modules/default/net",
	"/lib/modules/default/scsi",
	"/lib/modules/default/misc",
	"/lib/modules/fs",
	"/lib/modules/net",
	"/lib/modules/scsi",
	"/lib/modules/misc",
	0
};

#define PATCH_STRINGS

#include "insmod.h"
#define is_global(sp) (aout_flag?(sp->u.n.n_type & N_EXT) : \
				(ELF32_ST_BIND(sp->u.e.st_info) == STB_GLOBAL))
#define is_undef(sp) (aout_flag?((sp->u.n.n_type & ~N_EXT) == N_UNDF) : \
			    (ELF32_ST_TYPE(sp->u.e.st_info) == STT_NOTYPE))

/* hack: sizeof(struct elfhdr) > sizeof(struct exec) */
Elf32_Ehdr header;
int aout_flag; /* to know the format of the module */
int elf_kernel = 0; /* to know the format of the kernel */

size_t codesize;
size_t progsize;
size_t bss1size;
size_t bss2size;

char *textseg;

int nsymbols;
struct symbol *symtab;
char *stringtab;
unsigned long addr;
int verbose = 0;

static int export_flag = 1; /* See comment att option handler in main() */
static int force_load = 0;
static int loadmap = 0;
static int versioned_kernel = 0;
static struct kernel_sym nullsym;
static struct symbol *symroot;
static struct utsname uts_info;

void *
ckalloc(size_t nbytes)
{
	void *p;

	if ((p = malloc(nbytes)) == NULL) {
	        perror("insmod: malloc failed");
		exit(2);
	}
	return p;
}

void *
ckrealloc(void *ptr, size_t nbytes)
{
        void *p;

	if ((p = realloc(ptr, nbytes)) == NULL) {
	        perror("insmod: realloc failed");
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
	unsigned long kernel_version;

	/* Check if module and kernel version match */
	kernel_version = looksym("kernel_version");
	if (strcmp( (char*) textseg + kernel_version, uts_info.release)) {
		insmod_error (
			"Error: The module was compiled on kernel version %s.\n"
			"       This kernel is version %s. They don't match!\n"
			"       Check that the module is usable with the current kernel,\n"
			"       recompile the module and try again.",
			(char*) textseg + kernel_version, uts_info.release);
		if (force_load)
			insmod_error("       Trying to load it anyway...");
		else
			exit( 2);
	}

}


#ifdef HACKER_TOOL   /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

struct zSystem_entry {
  struct zSystem_entry *next;
  int value;;
  char *name;
#ifdef __TESTKSYMS__
  char symtype;
#endif
};

int use_zSystem=0, use_zSystem_local=0, zsyms_valid=0;
int warnings=0;
int silent_poll_mode=0;

char *zsystem_map_name="/usr/src/linux/zSystem.map";

#if KERNEL_VERSION >= 1001076
char *system_map_name="/usr/src/linux/System.map";
static void check_system_map_name()
{
  struct stat buf;
  if (stat(zsystem_map_name, &buf)) zsystem_map_name=system_map_name; 
}
#endif

static int is_version_tail(const char *s)
{
  int i;
  if (*s++ != '_') return 0;
  if (*s++ != 'R') return 0;
  for (i=0; i<8; i++) if (!isxdigit(s[i])) return 0;
  if (s[8]) return 0;
  return 1;
}

/*
 * ignore version info on both sides
 */
int ignore_strncmp(const char *s1, const char *s2, size_t n)
{
  int i;
  for (i=0; i<n; i++) {
    if (s1[i] != s2[i]) {
      if (!s1[i] || !s2[i]) {
        if (!s1[i]) {
          if (is_version_tail(s2+i)) return 0;
          return strncmp("",s2+i,n-i);
        }
        if (is_version_tail(s1+i)) return 0;
        return strncmp(s1+i,"",n-i);
      }
      return strncmp(s1+i,s2+i,n-i);
    }
    if (!s1[i]) return 0;
  }
  return 0;
}

static struct kernel_sym *find_resident_sym(struct kernel_sym *ksym, int nksyms, char *name)
{
  for (; nksyms >0 ; --nksyms, ksym++) {
    if (!ignore_strncmp(ksym->name,name,SYM_MAX_NAME)) return ksym;
  }
  return 0;
}

static struct zSystem_entry *build_zSystem_syms(char *name, struct kernel_sym *ksymtab, int nksyms)
{
  FILE *f;
  struct zSystem_entry *table=0;
  struct zSystem_entry *p;
  char c;
  int i;
  char n[1024];
  static char magic[] ="00100000 t startup_32\n";
  static char magic2[]="00100000 T startup_32\n";
  struct kernel_sym *ksym, *p_;
  int matches,mismatches;

  f=fopen(name, "r");
  if (!f) {
    insmod_error("error reading zSystem.map, errno=%d",errno);
    return 0;
  }

  for (ksym = ksymtab, i = 0 ; i < nksyms ; ++i, ksym++) {
    if (ksym->name[0] == '#') {
      if (!ksym->name[1]) break;
    }
  }
  ksym++; 
  nksyms -= i+1;
  matches=0;
  mismatches=0;
  
  memset(n,' ',sizeof(n));
  i=fread(n,1,sizeof(n),f);
  n[sizeof(n)-1]=0;
  if ((i < sizeof(magic)) || (!strstr(n,magic)) ) {
    if (!strstr(n,magic2)) {
      insmod_error("wrong file format of %s",name);
      return 0;
    }
  } 
  fseek(f,0,SEEK_SET);
  while (!feof(f) && n[0]) {
    n[0]=0;
    p=0;
    fscanf(f, "%08x %c %s\n", &i, &c, n);
    switch (c) {
      case 't': case 'd': case 'b': case 'r': 
        if (!use_zSystem_local) break;
        /* else fall through */
      case 'T': case 'D': case 'B': case 'R':{
        p=malloc(sizeof(struct zSystem_entry));
        p->name=strdup(n);
        p->value=i;
        p->next=table;
#ifdef __TESTKSYMS__
        p->symtype=c;
#endif
        table=p;
        break;
      }
    }
    p_=find_resident_sym(ksym,nksyms,n);
    if (p_) {
      if (p_->value == i) matches++;
      else if (p) mismatches++;
    }
  }
  fclose(f);
  if (!n[0]) insmod_error("warning: file %s may be corrupt or empty",name);
  zsyms_valid=(!mismatches && ((matches*10) >= (nksyms*9)));
  return table;
}

#endif  /* HACKER_TOOL <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */



/*
 * unversioned kernel, versioned module
 */
int
m_strncmp(const char *tabentry, const char *lookfor, size_t n)
{
	int len = strlen(lookfor);
	int retval;

	if ((retval = strncmp(tabentry, lookfor, len)) != 0)
		return retval;
	/* else */
	if ((strncmp(tabentry + len, "_R", 2) == 0) &&
		(strlen(tabentry + len) == 10))
		return 0;
	else
		return retval;
}

/*
 * versioned kernel, unversioned module
 */
int
k_strncmp(const char *tabentry, const char *lookfor, size_t n)
{
	int len = strlen(tabentry);
	int retval;

	if ((retval = strncmp(tabentry, lookfor, len)) != 0)
		return retval;
	/* else */
	if ((strncmp(lookfor + len, "_R", 2) == 0) &&
		(strlen(lookfor + len) == 10))
		return 0;
	else
		return retval;
}

#ifdef PATCH_STRINGS
struct strpatch {
	int *where;
	int what;
};
static struct strpatch *stringpatches;
static int n_stringpatches;

static void
push_string(char *string, int *patchme)
{
	int len;
	unsigned int string_offset = progsize;
	char *p;

	if ((p = strchr(string, ',')) != (char *)0)
		len = p - string;
	else
		len = strlen(string);

	progsize += len + 1;
	/* JEJB: don't like this, but if we're copying strings, better make
	 * sure memory actually exists to copy them into */
	textseg = ckrealloc(textseg, progsize);
	strncpy(textseg + string_offset, string, len);
	*(textseg + string_offset + len) = '\0';

	if (n_stringpatches == 0)
		stringpatches = (struct strpatch *)ckalloc(sizeof(struct strpatch));
	else
		stringpatches = (struct strpatch *)ckrealloc(stringpatches,
			(n_stringpatches + 1) * sizeof(struct strpatch));

	(*(stringpatches + n_stringpatches)).where = patchme;
	(*(stringpatches + n_stringpatches)).what = string_offset;
	++n_stringpatches;
}
#endif

int
main(int argc, char **argv)
{
	FILE *fp;
	struct exec *aouthdr = (struct exec *)&header;
	struct kernel_sym *curr_module = &nullsym;
	struct kernel_sym *ksymtab = NULL;
	struct kernel_sym *ksym = NULL;
	struct kernel_sym *resident_ksym_start = NULL;
	struct mod_routines routines;
	struct symbol *sp;
	unsigned long init_func, cleanup_func;
	int (*tabcomp) (const char *, const char *, size_t) = strncmp;
	int fatal_error;
	int i;
	int nksyms;
	int versioned_module;
	int found_resident = -1;
	int autoclean = 0; /* for insertions from kerneld: option "-k" */
	int bss_offset;

	struct symbol_table *newtab;
	int module_refs = 0; /* number of references modules */
	int n_symboldefs = 0; /* number of defined symbols in the module */
	int string_table_size = 0; /* size of the new symbol table string table */
	struct internal_symbol *symp;
	struct module_ref *refp;
	char *stringp;

	char *filename;
	char spare_path[200]; /* just testing... */
	char *modname = NULL;
	char *otextseg; /* JEJB: store the initial textseg, so we get offset */
	char *p;

	/* find basename */
	
	if ((p = strrchr(argv[0], '/')) != (char *)0)
		++p;
	else
		p = argv[0];

	if (strcmp(p, "rmmod") == 0)
		return rmmod(argc, argv);

	if (strcmp(p, "ksyms") == 0)
		return ksyms(argc, argv);

	/* else  this is insmod! */

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

			case 'X': /* _do_ export externs */
				export_flag = 1;
				break;

			case 'x': /* do _not_ export externs */
				export_flag = 0;
				break;

			case 'm': /* generate loadmap */
				loadmap = 1;
				break;

			case 'k': /* module loaded by kerneld, auto-cleanable */
				autoclean = MOD_AUTOCLEAN;
				break;

			case 'v': /* verbose output */
				verbose = 1;
				break;
			case 's':
				insmod_setsyslog("insmod");
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
			case 'w': /* output warnings */
				warnings = 1;
				break;

			case 'p': /* silent poll mode */
				silent_poll_mode = 1;
				break;
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
		      "insmod [-f] [-x] [-o name] [-msvwl] [-z | -Z mapfile] module [[sym=value]...]\n"
		      "\n"
		      "  module     Filename of a loadable kernel module (*.o)\n"
		      "  -o name    Set internal modulname to name\n"
		      "  -x         do *not* export externs\n"
		      "  -m         generate loadmap (so crashes can be traced down)\n"
		      "  -v         verbose output\n"
		      "  -s         set insmod syslogging\n"
		      "  -w         print warnings\n"
		      "  -f         Force loading under wrong kernel version\n"
		      "             (together with -z also disables mapfile check)\n"
		      "  -z         At last use /usr/src/linux/zSystem.map to resolve\n"
		      "             (on Linux > 1.1.76:  System.map)\n" 
		      "  -Z mapfile As -z, but use mapfile\n"
		      "  -l         Together with -z, -Z, also take local symbols from map\n"
		      "             (But note, local symbols can be multiple defined, last one is taken)\n"
		      "  -p         silent poll mode, just check if the module matches the kernel\n"
		      , stderr);
#else
		fputs("Usage: insmod [-f] [-x] [-o name] [-m] [-s] [-v] module "
			"[[sym=value]...]\n", stderr);
#endif
		exit(2);
	}

	uname(&uts_info);

	filename = argv[1];
	argv += 2;
	--argc;

	/* get the size of the current kernel symbol table */
	nksyms = get_kernel_syms(NULL);

	if (nksyms < 0) {
		insmod_error("get_kernel_sys failed: Cannot find Kernel symbols!");
		exit(2);
	}

	if (nksyms) {
		ksymtab = (struct kernel_sym *) ckalloc(nksyms * sizeof *ksymtab);
		/* NOTE!!! The order of the symbols is important */
		if (get_kernel_syms(ksymtab) != nksyms) {
			insmod_error ("Kernel symbol problem");
			exit(2);
		}
	}

	/* Is this a kernel with appended CRC-versions? */
	for (ksym = ksymtab, i = 0 ; i < nksyms ; ++i, ksym++) {
		if (found_resident == -1 && strcmp(ksym->name, "#") == 0) {
			resident_ksym_start = ksym;
			found_resident = i;
			break;
		}
	}

	/* look only in residents */
	if (found_resident != -1) {
		++ksym;
		/* The first symbol in a versioned kernel is _Using_Versions */
		if ((strcmp(ksym->name, "_Using_Versions") == 0) ||
		    (strcmp(ksym->name, "Using_Versions") == 0))
	    		versioned_kernel = 1;

		/* look at a "normal" symbol: */
		++ksym;
		if (((i + 1) < nksyms) && (ksym->name[0] != '_'))
			elf_kernel = 1;
	}

	/* construct the module name */
	if (modname == NULL) {
		int len;

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

	if ((strchr(filename, '/') == 0) && (strchr(filename, '.') == 0)) {
		struct stat dummy;
		char **defp;
		char *path;
		char *onepath;
		char *p;

		/* Look in the path given by MODPATH environment,
		 * if not found, look in the default directories.
                 * <storner@osiris.ping.dk> 950319 + Bjorn
		 */
		if ((path = getenv("MODPATH")) == 0)  {
			onepath = ckalloc(2000); /* or whatever */
			onepath[0] = '\0';
			for (defp = default_path; *defp; ++defp) {
				sprintf(spare_path, *defp, uts_info.release);
				strcat(onepath, spare_path);
				strcat(onepath, ":");
			}
		}
		else {
			onepath = ckalloc(strlen(path)+2);
			sprintf(onepath, "%s:", path);
		}

		for (path = onepath; (*path != '\0'); path += strlen(path)+1) {
			for (p = path; (*p != ':') && (*p != '\0'); p++)
				; 
			*p = '\0';
			sprintf(spare_path, "%s/%s", path, filename);
			if (stat(spare_path, &dummy) >= 0) 
				break;
			/* else */
			strcat(spare_path, ".o");
			if (stat(spare_path, &dummy) >= 0) 
				break;
		}
		free(onepath);
		filename = spare_path;
	}

	/* open file and read header */
	if ((fp = fopen(filename, "r")) == NULL) {
		insmod_error ("Cannot open %s", filename);
		exit(2);
	}

	/* sizeof(struct elfhdr) > sizeof(struct exec) */
	fread(&header, sizeof(Elf32_Ehdr), 1, fp);
	if (feof(fp) || ferror(fp)) {
		insmod_error ("Could not read header of %s", filename);
		exit(2);
	}

	symtab = (struct symbol *)0;
	nsymbols = 0;

	if (N_MAGIC((*aouthdr)) == OMAGIC) {
		char *errstr;

		if ((errstr = load_aout(fp)) != (char *)0) {
			insmod_error ("%s: %s", filename, errstr);
			exit(2);
		}
	}
	else if ((header.e_ident[0] == 0x7f) &&
		 (strncmp(&header.e_ident[1], "ELF",3) == 0) &&
		 (header.e_type == ET_REL) &&
		 ((header.e_machine == 3) || (header.e_machine == 6))
		) {
			char *errstr;

			if ((errstr = load_elf(fp)) != (char *)0) {
				insmod_error ("%s: %s", filename, errstr);
				exit(2);
			}
	}
	else {
		insmod_error ("%s: not an object file", filename);
		exit(2);
	}

	/* JEJB: now we have textseg fixed after the load, so save value
	 * for later finding offset if realloc forces a move */
	otextseg = textseg;

	if (findsym("Using_Versions", NULL, strncmp))
		versioned_module = 1;
	else
		versioned_module = 0;

	/* check version info */
        if (verbose) {
		insmod_error ("versioned kernel: %s\nversioned module: %s",
			versioned_kernel ? "yes" : "no",
			versioned_module ? "yes" : "no");
		insmod_error ("%s kernel\n%s module",
			elf_kernel ? "ELF" : "a.out",
			aout_flag ? "a.out" : "ELF");
	}
	/*
	 * Logic:
	 *
	 * versioned_kernel versioned_module	action
	 * ================ ================	=============================
	 *	no		no		same kernel and module version
	 *					all symbols must match
	 *	no		yes		same kernel and module version
	 *					ignore symbol version suffix
	 *	yes		no		same kernel and module version
	 *					ignore symbol version suffix
	 *	yes		yes		all symbols must match,
	 *					including the version suffix
	 */
	switch ((versioned_kernel << 1) + versioned_module) {
	case 0: /* unversioned_kernel, unversioned_module */
		check_version(force_load);
		tabcomp = strncmp;
		break;

	case 1: /* unversioned_kernel, versioned_module */
#ifdef HACKER_TOOL
		if (!force_load)
#endif
		check_version(force_load);
		tabcomp = m_strncmp;
		break;

	case 2: /* versioned_kernel,   unversioned_module */
		check_version(force_load);
		tabcomp = k_strncmp;
		break;

	case 3: /* versioned_kernel,   versioned_module */
#ifdef HACKER_TOOL
		if (use_zSystem) check_version(force_load);
#endif
		tabcomp = strncmp;
		break;

	}

	/* get initialization and cleanup routines */
	init_func = looksym("init_module");
	cleanup_func = looksym("cleanup_module");

	/* bind undefined symbols (ELF-stuff hidden in defsym()) */
	defsym(strncmp, "mod_use_count_", 0 - sizeof (int), N_BSS | N_EXT, TRANSIENT);

	/* First: resolve symbols using kernel transient symbols */
	/* Then: use resident kernel symbols to resolve the last ones... */
	for (ksym = ksymtab, i = 0 ; i < nksyms ; ++i, ksym++) {
		/* Magic in this version of the new get_kernel_syms:
		 * Every module is sent along as a symbol,
		 * where the module name is represented as "#module", and
		 * the address of the module struct is stuffed into the value.
		 * The name "#" means that the symbols that follow are
		 * kernel resident.
		 */
		if (ksym->name[0] == '#') {
			curr_module = ksym;
			continue;
		}

		if (defsym(tabcomp, ksym->name + (elf_kernel?0:1),
			ksym->value, N_ABS | N_EXT,
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

#ifdef HACKER_TOOL
	/* now resolve over zSystem.map */
	if (use_zSystem) {
	  struct zSystem_entry *zsym;
	  char *name;
		
#if KERNEL_VERSION >= 1001076
	  check_system_map_name();
#endif
	  if (verbose) insmod_error("Now resolving from %s", zsystem_map_name);
	  zsym=build_zSystem_syms(zsystem_map_name, ksymtab,  nksyms);
	  if (!zsym) exit(2);
	  if (!force_load) {
	    if (!zsyms_valid) {
	      insmod_error("%s doesn\'t match actual kernel",zsystem_map_name);
              exit(2);
	    }
	  }
	  while (zsym) {
	    name = zsym->name;
             /* NOTE: Symbols in the module which are not in ksyms
                      always are *unversioned* (as are the symbols in System.map).
                      So we can use strncmp to compare.
              */
	    if (!elf_kernel && (name[0] == '_')) name++;
	    if (defsym(strncmp, name, zsym->value, N_ABS | N_EXT,  RESIDENT )) {
	      if (verbose) insmod_error("resolved: %08x %s", zsym->value, zsym->name);
	    }
	    zsym=zsym->next;
	  }
	}
#endif

	/* allocate space for "common" symbols */
	/* and check for undefined symbols */
	fatal_error = 0;
	bss_offset = codesize + bss1size;
	/*
	 * Scan symbol table to determine global bss size
	 */
	for (sp = symtab ; sp < symtab + nsymbols ; sp++) {
		if ((symname(sp) == (char *)0) || (*symname(sp) == '\0'))
			continue;

		if ( ((aout_flag && (sp->u.n.n_type == (N_UNDF | N_EXT))) ||
			(!aout_flag && is_global(sp) && is_undef(sp)))
			&& (symvalue(sp) != 0)) {
			int len;

			if (aout_flag) {
				sp->u.n.n_type = N_BSS | N_EXT;
				len = symvalue(sp);
 				if (verbose)
 				        insmod_error (
 					"bss2 sym %08x, size =%7d: %s",
 			                bss_offset, len, symname( sp));
			}
			else { /* I'm not sure I understand this... */
				sp->u.e.st_info = (STB_GLOBAL << 4)| STT_OBJECT;
				len = (sp->u.e.st_size)?(sp->u.e.st_size):4;
			}
 			symvalue(sp) = bss_offset;
 			bss_offset += len;
		} else if (is_undef(sp)) {
			if (versioned_module) {
				char *v = strrchr(symname(sp), '_');
				if (v && (strncmp(v, "_R", 2) == 0) &&
					(strlen(v) == 10))
					*v = '\0';
				insmod_error ("%s: wrong version",
					symname(sp));
				if (v && !(*v))
					*v = '_';
			}
			else
				insmod_error ("%s undefined", symname(sp));
			fatal_error = 1;
		}
	}
	if (fatal_error) {
		insmod_error ("Failed to load module! The symbols from kernel %s don't match %s",
			(char*) textseg + looksym("kernel_version"),
			uts_info.release);
		exit(2);
	}

	/* Change values according to the parameters to "insmod" */
	{
		struct symbol *change;
		int *patchme;
		char *param;
		char *val;
		int value;

		while (argc > 1) {
			param = *argv;
			++argv;
			--argc;

			/*
			 * This makes the following contruct legal:
			 *    insmod module.o -o new_name
			 * This is used by the "options" configuration
			 * lines for modprobe.
			 */
			if (strcmp(param, "-o") == 0) {
				modname = (char*) ckalloc(strlen(argv[0]) + 1);
				strcpy(modname, argv[0]);
				--argc;
				++argv;
				continue;
			}
			if ((val = strchr(param, '=')) == NULL)
				continue;
			*val = '\0'; /* mark end of tag */

			if (versioned_module)
				change = findsym(param, NULL, m_strncmp);
			else
				change = findsym(param, NULL, strncmp);
			if (change == NULL) {
				insmod_error ("Symbol '%s' not found", param);
				exit(2);
			}
			patchme = (int *)(textseg + symvalue(change));

			do {
				++val;
				if (*val < '0' || '9' < *val) {
#ifdef PATCH_STRINGS
					if (*val != ',')
						push_string(val, patchme);
#else
					insmod_error ("Symbol '%s' has an illegal value: %s", param, val);
					exit(2);
#endif
				}
				else { /* numerical */
					if (val[0] == '0') {
						if (val[1] == 'x')
							sscanf(val, "%x", &value);
						else
							sscanf(val, "%o", &value);
					}
					else
						sscanf(val, "%d", &value);
					*patchme = value;
				}
				++patchme;
			} while ((val = strchr(val, ',')) != (char *)0);
		}
	}

#ifdef HACKER_TOOL
	if (silent_poll_mode) {
		/* if we are here,
		 * we assume that the module matches the kernel.
		 */
		exit(0);
	}
#endif
	/* create the module */
	errno = 0;
	/* make sure we have enough memory malloc'd for copy which kernel
	 * will perform */
	textseg = ckrealloc(textseg, progsize);
	/* We add "sizeof (int)" to skip over the use count */
	addr = create_module(modname, progsize) + sizeof (int);

	switch (errno) {
	case EEXIST:
		insmod_error ("A module named %s already exists", modname);
		exit(2);
	case ENOMEM:
		insmod_error ("Cannot allocate space for module");
		exit(2);
	case 0:
		break;
	default:
		perror("create_module");
		exit(2);
	}

	/* perform relocation */
	if (aout_flag)
		relocate_aout(fp, textseg - otextseg);
	else
		relocate_elf(fp, textseg - otextseg);

#ifdef PATCH_STRINGS
	{
		struct strpatch *p = stringpatches;

		while (--n_stringpatches >= 0) {
			*(p->where) = (int)addr + p->what;
			++p;
		}

		free(stringpatches);
		n_stringpatches = 0;
	}
#endif
	init_func += addr;
	cleanup_func += addr;

	/* "hide" the module specific symbols, not to be exported! */
	hidesym("cleanup_module");
	hidesym("init_module");
	hidesym("kernel_version");
	hidesym("mod_use_count_");

	/* Build the module symbol table */
	/* abuse of *_other:
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
		if (is_global(sp) && (symother(sp) & DEF_BY_MODULE)) {
			string_table_size += strlen(symname(sp)) + 1;
			if (!elf_kernel)
				++string_table_size;
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
		if (is_global(sp) && (symother(sp) & DEF_BY_MODULE)) {
			symp->addr = (void *)(symvalue(sp) + addr);
			symp->name = (char *)(stringp - (char *)newtab);
			if (!elf_kernel) {
				strcpy(stringp, "_");
				++stringp;
			}
			strcpy(stringp, symname(sp));
			stringp += strlen(symname(sp)) + 1;
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

	if (init_module(modname, textseg,
		/* An autoclean marker for a non-kerneld-aware kernel
		 * will result in a rejection of the module.
		 * This is what we want, since the "-k" option is useful
		 * only with kerneld aware kernels...
		 */
		autoclean | progsize /*codesize*/, &routines,
		((n_symboldefs + module_refs)? newtab : NULL)) < 0) {

		if (errno == EBUSY) {
			insmod_error ("Initialization of %s failed", modname);
		} else {
			perror("init_module");
		}
		delete_module(modname);
		exit(1);
	}
#ifdef DEBUG_DISASM
dis(textseg, codesize);
#endif
	/*
	 * Print a loadmap so that kernel panics can be identified.
	 * Load map option conceived by Derek Atkins <warlord@MIT.EDU>
	 */
	for (sp = symtab; loadmap && (sp < symtab + nsymbols) ; sp++) {
		char symtype = 'u';

		if (aout_flag) {
			if ((sp->u.n.n_type & ~N_EXT) == N_ABS)
				continue;

			switch (sp->u.n.n_type & ~N_EXT) {
			case N_TEXT | N_EXT: symtype = 'T'; break;
			case N_TEXT: symtype = 't'; break;
			case N_DATA | N_EXT: symtype = 'D'; break;
			case N_DATA: symtype = 'd'; break;
			case N_BSS | N_EXT: symtype = 'B'; break;
			case N_BSS: symtype = 'b'; break;
			}
		}
		else
			symtype = ' '; /* until someone does the work... */

		printf("%08lx %c %s\n",
			symvalue(sp) + addr, symtype, symname(sp));
	}

	if (nksyms > 0)
		free(ksymtab); /* it has done its job */

	exit(0);
}

void
hidesym(const char *name)
{
	struct symbol *sp;

	if ((sp = findsym(name, NULL, strncmp)) != NULL) {
		if (aout_flag)
			sp->u.n.n_type &= ~N_EXT;
		else
			sp->u.e.st_info = (STB_LOCAL << 4) |
		    			  (ELF32_ST_TYPE(sp->u.e.st_info));
	}
}

int
defsym(int (*pfi) (const char *, const char *, size_t),
	const char *name, unsigned long value, int type, int source)
{
	struct symbol *sp;
	/*
	 * field "*_other" abused in the symbol table
	 * in order to discriminate between defines and references
	 * and resolves by kernel resident or transient symbols...
	 */

	if ((sp = findsym(name, NULL, pfi)) == NULL)
		return 0; /* symbol not used */
	/* else */

	/*
	 * Multiply defined?
	 * Well, this is what we _want_!
	 * I.e. a module _should_ be able to replace a kernel resident
	 * symbol, or even a symbol defined by another module!
	 * NOTE: The symbols read from the kernel (the transient ones)
	 * MUST be handled in the order:
	 *   last defined, ..., earlier defined, ...
	 */
	if (aout_flag) {
		if (sp->u.n.n_type != (N_UNDF | N_EXT))
			return 0; /* symbol not used */
		sp->u.n.n_type = type;
	}
	else { /* elf */
		if ((ELF32_ST_BIND(sp->u.e.st_info) != STB_GLOBAL) ||
		    (ELF32_ST_TYPE(sp->u.e.st_info) != STT_NOTYPE))
			return 0; /* symbol not used */
		/* hack follows... */
		if (type & N_ABS)
			sp->u.e.st_info = (STB_GLOBAL << 4);
		else
			sp->u.e.st_info = (STB_LOCAL << 4);
		sp->u.e.st_info |= STT_OBJECT; /* or whatever */
	}

	symother(sp) |= source;
	symvalue(sp) = value;

	return 1; /* symbol used */
}


/*
 * Look up an name in the symbol table.  If "add" is not null, add a
 * the entry to the table.  The table is stored as a splay tree.
 */
struct symbol *
findsym(const char *key, struct symbol *add,
	int (*pfi) (const char *, const char *, size_t))
{
	struct symbol *left, *right;
	struct symbol **leftp, **rightp;
	struct symbol *sp1, *sp2, *sp3;
	int cmp;
	int path1, path2;

	if (add) {
		if (aout_flag)
			add->u.n.n_un.n_name = (char *)key;
		else
			add->u.e.st_name = (Elf32_Word)key;
	}
	sp1 = symroot;
	if (sp1 == NULL)
		return add? symroot = add : NULL;
	leftp = &left, rightp = &right;
	for (;;) {
		cmp = (*pfi)(symname(sp1), key, SYM_MAX_NAME);
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
		cmp = (*pfi)(symname(sp2), key, SYM_MAX_NAME);
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

	sp = findsym(name, NULL, strncmp);
	if (sp == NULL) {
		insmod_error ("%s needed, but can't be found", name);
		exit(2);
	}
	return symvalue(sp);
}

/*
 * This is (was) rmmod, now merged with insmod!
 *
 * Original author: Jon Tombs <jon@gtex02.us.es>,
 * and extended by Bjorn Ekwall <bj0rn@blox.se> in 1994 (C).
 * See the file COPYING for your rights (GNU GPL)
 */

#define WANT_TO_REMOVE 1
#define CAN_REMOVE 2

struct ref {
	int modnum;
	struct ref *next;
};

struct mod {
	int status; /* or of: WANT_TO_REMOVE, CAN_REMOVE */
	char name[MOD_MAX_NAME];
	struct ref *ref;
};

struct mod *loaded;
int current = -1;

/* build the references as shown in /proc/ksyms */
void
get_stacks()
{
	FILE *fp;
	struct ref *rp;
	int i;
	char line[200]; /* or whatever... */
	char *p;
	char *r;

	if ((fp = fopen("/proc/modules", "r")) == (FILE *)0) {
		perror("/proc/modules");
		exit(1);
	}

	while (fgets(line, 200, fp)) {
		if (loaded) {
			++current;
			loaded = (struct mod *)ckrealloc(loaded,
				(current + 1) * sizeof(struct mod));
		} else {
			current = 0;
			loaded = (struct mod *)ckalloc(sizeof(struct mod));
		}

		loaded[current].status = 0;
		loaded[current].ref = (struct ref *)0;

		/* save the module name */
		p = strchr(line, ' ');
		*p = '\0';
		strcpy(loaded[current].name, line);

		/* any references? */
		if ((p = strchr(p + 1, '['))) {
			*(strrchr(p + 1, ']')) = '\0';
			do {
				r = p + 1;
				if ((p = strchr(r, ' ')))
					*p = '\0';
				for (i = 0; i < current; ++i) {
					if (strcmp(loaded[i].name, r) == 0)
						break;
				}

				if (i == current) { /* not found! */
					insmod_error (
					"Strange reference in "
					"/proc/modules: '%s' used by '%s'?",
					loaded[current].name, r);
					exit(1);
				}

				rp = (struct ref *)ckalloc(sizeof(struct ref));
				rp->modnum = i;
				rp->next = loaded[current].ref;
				loaded[current].ref = rp;
			} while (p);
		}
	}

	fclose(fp);
}

int
rmmod(int argc, char **argv)
{
	struct ref *rp;
	int i, j;
	int count;
	char **list;
	char *p;

	if (argc == 1) {
		fprintf(stderr, "usage: rmmod [-r] [-s] module ...\n");
		exit(1);
	}
	/* else */

	if (strcmp(argv[1], "-r") != 0) {
		if (strcmp(argv[1], "-a") == 0) {
			/* delete all unused modules and stacks */
			delete_module(NULL);
			exit(0);
		}
		/* else */
		if (strcmp(argv[1],"-s")==0){
			insmod_setsyslog("rmmod");
			--argc;
			++argv;
		}
		while (argc > 1) {
			if ((p = strrchr(argv[1], '.')) &&
				((strcmp(p, ".o") == 0) ||
				 (strcmp(p, ".mod") == 0))) 
					*p = '\0';
			if (delete_module(argv[1]) < 0) {
				perror(argv[1]);
			}
			--argc;
			++argv;
		}
		exit(0);
	}
	/* else recursive removal */

	count = argc - 2;
	list = &(argv[2]);
	if (count <= 0) {
		insmod_error ("usage: rmmod [-r] module ...");
		exit(1);
	}

	get_stacks();

	for (i = 0; i < count; ++i) {
		for (j = 0; j <= current; ++j) {
			if (strcmp(loaded[j].name, list[i]) == 0) {
				loaded[j].status = WANT_TO_REMOVE;
				break;
			}
		}
		if (j > current)
			insmod_error ("module '%s' not loaded", list[i]);
	}

	for (i = 0; i <= current; ++i) {
		if (loaded[i].ref || (loaded[i].status == WANT_TO_REMOVE))
			loaded[i].status |= CAN_REMOVE;

		for (rp = loaded[i].ref; rp; rp = rp->next) {
			switch (loaded[rp->modnum].status) {
			case CAN_REMOVE:
			case WANT_TO_REMOVE | CAN_REMOVE:
				break;

			case WANT_TO_REMOVE:
				if (loaded[rp->modnum].ref == (struct ref *)0)
					break;
				/* else fallthtough */
			default:
				loaded[i].status &= ~CAN_REMOVE;
				break;
			}
		}

		switch (loaded[i].status) {
		case CAN_REMOVE:
		case WANT_TO_REMOVE | CAN_REMOVE:
			if (delete_module(loaded[i].name) < 0) {
				perror(loaded[i].name);
			}
			break;

		case WANT_TO_REMOVE:
			insmod_error ("module '%s' is in use!",
				loaded[i].name);
			break;
		}
	}

	return 0;
}


/*
 * Used to be ksyms.c, but merged as well...
 *
 * Get kernel symbol table(s).
 *
 * Bjorn Ekwall <bj0rn@blox.se> in 1994 (C)
 * See the file COPYING for your rights (GNU GPL)
 */

int
ksyms(int argc, char **argv)
{
	struct kernel_sym *ksymtab = NULL;
	struct kernel_sym *ksym = NULL;
	struct module module_struct;
	int nksyms;
	int i;
	int kmem = 0;
	int allsyms = 0;
	int show_header = 1;
	char *p;
	char *module_name = "";

	while (argc > 1 && (argv[1][0] == '-')) {
		p = &(argv[1][1]);
		while (*p) {
			switch (*p) {
			case 'a':
				allsyms = 1;
				break;

			case 'm':
				if ((kmem = open("/dev/kmem", O_RDONLY)) < 0) {
					perror("/dev/kmem");
					exit(2);
				}
				break;

			case 'h':
				show_header = 0;
				break;
			}
			++p;
		}
		--argc;
		++argv;
	}

	if (argc < 1) {
		fputs("Usage: ksyms [-a] [-h]\n", stderr);
		exit(2);
	}

	/* get the size of the current kernel symbol table */
	nksyms = get_kernel_syms(NULL);

	if (nksyms < 0) {
		insmod_error ("get_kernel_sys failed: Cannot find Kernel symbols!");
		exit(2);
	}

	if (nksyms) {
		ksymtab = (struct kernel_sym *) ckalloc(nksyms * sizeof *ksymtab);
		/* NOTE!!! The order of the symbols is important */
		if (get_kernel_syms(ksymtab) != nksyms) {
			insmod_error ("Kernel symbol problem");
			exit(2);
		}
	}

	if (show_header)
		printf("Address  Symbol    \tDefined by\n");

	for (ksym = ksymtab, i = 0 ; i < nksyms ; ++i, ksym++) {
		/* Magic in this version of the new get_kernel_syms:
		 * Every module is sent along as a symbol,
		 * where the module name is represented as "#module", and
		 * the address of the module struct is stuffed into the value.
		 * The name "#" means that the symbols that follow are
		 * kernel resident.
		 */
		if (ksym->name[0] == '#') {
			module_name = ksym->name + 1;
			if (!allsyms && (*module_name == '\0'))
				break;
			/* else */
			if (kmem) {
				if (
		(lseek(kmem, (off_t)ksym->value, SEEK_SET) > 0) &&
		(read(kmem, (char *)&module_struct, sizeof(struct module)) ==
		sizeof(struct module))) {
					printf("%08lx --- (%dk) ---\t[%s]\n",
					(long)module_struct.addr,
					module_struct.size * 4,
					module_name);
				}
				else {
					perror("/dev/kmem");
					exit(2);
				}
			}
			continue;
		}

		printf("%08lx %s", ksym->value, ksym->name);
		if (*module_name)
			printf("\t[%s]", module_name);
		printf("\n");
	}

	return 0;
}
