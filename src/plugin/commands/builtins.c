#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "builtins.h"
#include "dos2linux.h"
#include "cpu.h"
#include "emu.h"
#include "int.h"
#include "utilities.h"
#include "zalloc.h"

/* hope this will be removed soon */
#include "coop_compat.h"

/* hope 2K is enough */
#define LOWMEM_POOL_SIZE 0x800
#define BUF_SIZE LOWMEM_POOL_SIZE

static char scratch[BUF_SIZE];
static char scratch2[BUF_SIZE];
static char *builtin_name;

unsigned char _osmajor;
unsigned char _osminor;
int com_errno;

static MemPool mp;
static char *lowmem_pool;

int com_vsprintf(char *str, char *format, va_list ap)
{
	char *s = str;
	int i, size;

	size = vsnprintf(scratch, BUF_SIZE, format, ap);
	for (i=0; i < size; i++) {
		if (scratch[i] == '\n') *s++ = '\r';
		*s++ = scratch[i];
	}
	*s = 0;
	return s - str;
}

int com_sprintf(char *str, char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vsprintf(str, format, ap);
	va_end(ap);
	return ret;
}

int com_vfprintf(int dosfilefd, char *format, va_list ap)
{
	int size;

	size = com_vsprintf(scratch2, format, ap);
	if (!size) return 0;
	return com_doswrite(dosfilefd, scratch2, size);
}

int com_vprintf(char *format, va_list ap)
{
	return com_vfprintf(1, format, ap);
}

int com_fprintf(int dosfilefd, char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vfprintf(dosfilefd, format, ap);
	va_end(ap);
	return ret;
}

int com_printf(char *format, ...)
{
	va_list ap;
	int ret;
	va_start(ap, format);

	ret = com_vfprintf(1, format, ap);
	va_end(ap);
	return ret;
}

int com_puts(char *s)
{
	return com_printf("%s", s);
}

char *skip_white_and_delim(char *s, int delim)
{
	while (*s && isspace(*s)) s++;
	if (*s == delim) s++;
	while (*s && isspace(*s)) s++;
	return s;
}

void call_msdos(void)
{
	do_intr_call_back(0x21, 0);
}

void call_msdos_interruptible(void)
{
	do_intr_call_back(0x21, 1);
}

int com_doswrite(int dosfilefd, char *buf32, int size)
{
	char *s;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_alloc(size);
	if (!s) return -1;
	memcpy(s, buf32, size);
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	LWORD(ds) = FP_SEG32(s);
	LWORD(edx) = FP_OFF32(s);
	LWORD(eax) = 0x4000;	/* write handle */
	call_msdos_interruptible();	/* call MSDOS */
	lowmem_free(s, size);
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		return -1;
	}
	return  LWORD(eax);
}

int com_dosread(int dosfilefd, char *buf32, int size)
{
	char *s;

	if (!size) return 0;
	com_errno = 8;
	s = lowmem_alloc(size);
	if (!s) return -1;
	LWORD(ecx) = size;
	LWORD(ebx) = dosfilefd;
	LWORD(ds) = FP_SEG32(s);
	LWORD(edx) = FP_OFF32(s);
	LWORD(eax) = 0x3f00;	/* read handle */
	call_msdos_interruptible();	/* call MSDOS */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		lowmem_free(s, size);
		return -1;
	}
	memcpy(buf32, s, LWORD(eax));
	lowmem_free(s, size);
	return  LWORD(eax);
}

char *com_getenv(char *keyword)
{
	struct PSP  *psp = COM_PSP_ADDR;
	char *env = SEG2LINEAR(psp->envir_frame);
	char key[128];
	int len;

	len = strlen(keyword);
	memcpy(key, keyword, len+1);
	strupr(key);
	while (*env) {
		if (!strncmp(key, env, len) && (env[len] == '=')) {
			return env + len + 1;
		}
		env += strlen(env) + 1;
	}
	return 0;
}

static int load_and_run_DOS_program(char *command, char *cmdline)
{
	struct param4a {
		unsigned short envframe;
		FAR_PTR cmdline;
		FAR_PTR fcb1;
		FAR_PTR fcb2;
	} __attribute__((packed));
	struct PSP  *psp = COM_PSP_ADDR;
	char *cmd, *cmdl;
	struct param4a *pa4;

	pa4 = (struct param4a *)lowmem_alloc(sizeof(struct param4a));
	if (!pa4) return -1;

	cmd = com_strdup(command);
	if (!cmd) {
		com_errno = 8;
		return -1;
	}
	cmdl = lowmem_alloc(256);
	if (!cmdl) {
		com_strfree(cmd);
		com_errno = 8;
		return -1;
	}
	if (!cmdline) cmdline = "";
	snprintf(cmdl, 256, "%c %s\r", (char)(strlen(cmdline)+1), cmdline);

	/* prepare param block */
	pa4->envframe = 0; // ctcb->envir_frame;
	pa4->cmdline = MK_FP(FP_SEG32(cmdl), FP_OFF32(cmdl));
	pa4->fcb1 = MK_FP(FP_SEG32(psp->FCB1), FP_OFF32(psp->FCB1));
	pa4->fcb2 = MK_FP(FP_SEG32(psp->FCB2), FP_OFF32(psp->FCB2));
	LWORD(es) = FP_SEG32(pa4);
	LWORD(ebx) = FP_OFF32(pa4);

	/* path of programm to load */
	LWORD(ds) = FP_SEG32(cmd);
	LWORD(edx) = FP_OFF32(cmd);
	
	LWORD(eax) = 0x4b00;
	real_run_int(0x21);

	return 0;
}

int com_system(char *command)
{
	char *program = com_getenv("COMSPEC");
	char cmdline[256];

	snprintf(cmdline, sizeof(cmdline), "/C %s", command);
	if (!program) program = "\\COMMAND.COM";
	return load_and_run_DOS_program(program, cmdline);
}

static char * com_dosallocmem(int size)
{
	/* size in bytes */
	LWORD(ebx) = (size +15) >> 4;
	HI(ax) = 0x48;
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) {
		com_errno = LWORD(eax);
		error("lowmem_alloc failed for %i\n", size);
		return NULL;
	}
	return (char *)SEG2LINEAR(LWORD(eax));
}

#ifdef NEED_DOSFREEMEM
static void com_dosfreemem(char *p)
{
	if (!p) {
		error("Attempt to free NULL\n");
		return;
	}
	if (FP_OFF32(p)) {
		error("Attempt to free addr %p not from malloc\n", p);
		return;
	}
	LWORD(es) = FP_SEG32(p);
	HI(ax) = 0x49;
	call_msdos();    /* call MSDOS */
	if (LWORD(eflags) & CF) {
		error("lowmem_free failed for %p\n", p);
		com_errno = LWORD(eax);
	}
	return;
}
#endif

char * lowmem_alloc(int size)
{
	char *ptr = znalloc(&mp, size);
	if (!ptr) {
		error("builtin %s OOM\n", builtin_name);
		leavedos(86);
	}
	if (size > 1024) {
		/* well, the lowmem heap is limited, let's be polite! */
		error("builtin %s requests too much of a heap: 0x%x\n",
			builtin_name);
	}
	return ptr;
}

void lowmem_free(char *p, int size)
{
	return zfree(&mp, p, size);
}

char * com_strdup(char *s)
{
	struct lowstring *p;
	int len = strlen(s);
	if (len > 254) {
		error("lowstring too long: %i bytes. builtin: %s\n",
			len, builtin_name);
		len = 254;
	}

	p = (void *)lowmem_alloc(len + 1 + sizeof(struct lowstring));
	if (!p) return 0;
	p->len = len;
	memcpy(p->s, s, len + 1);
	p->s[len] = 0;
	return p->s;
}

void com_strfree(char *s)
{
	struct lowstring *p = (void *)(s - 1);
	lowmem_free((char *)p, p->len);
}

static int com_argparse(char *s, char **argvx, int maxarg)
{
   int mode = 0;
   int argcx = 0;
   char delim = 0;
   char *p;

   p = strchr(s+1, '\r');
   if (p && ((p - (s+1)) < 128)) {
     *p = 0;
   }
   else s[1+(unsigned char)s[0]] = 0;
   s++;

   /* transform:
    *    dir/p to dir /p
    *    cd\ to cd \
    *    cd.. to cd ..
    */
   p = s + strcspn(s, "\\/. ");
   if (*p && *p != ' ' && (*p != '.' || (p[1] && strchr("\\/.", p[1])))) {
      memmove(p+1, p, s [-1] - (p - s) + 1/*NUL*/);
      *p = ' ';
      s[-1]++; /* update length */
   }

   maxarg --;
   for ( ; *s; s++) {
      if (!mode) {
         if (*s > ' ') {
            mode = 1;
            switch (*s) {
              case '"':
              case '\'':
                delim = *s;
                mode = 2;
                argvx[argcx++] = s+1;
                break;
              default:
                argvx[argcx++] = s;
                break;
            }
            if (argcx >= maxarg)
               break;
         }
      } else if (mode == 1) {
         if (*s <= ' ') {
            mode = 0;
            *s = 0;
         }
      } else {
         if (*s == delim) {
           *s = 0;
           mode = 1;
         }
      }
   }
   argvx[argcx] = 0;
   return(argcx);
}

int com_dosgetdrive(void)
{
        HI(ax) = 0x19;
        call_msdos();    /* call MSDOS */
        return LO(ax);  /* 0=A, 1=B, ... */
}

int com_dossetdrive(int drive)
{
        HI(ax) = 0x0e;
        LO(dx) = drive; /* 0=A, 1=B, ... */
        call_msdos();    /* call MSDOS */
        return LO(ax);  /* number of available logical drives */
}

int com_dossetcurrentdir(char *path)
{
        /*struct com_starter_seg  *ctcb = owntcb->params;*/
        char *s = com_strdup(path);

        com_errno = 8;
        if (!s) return -1;
        HI(ax) = 0x3b;
        LWORD(ds) = FP_SEG32 (s) /*COM_SEG*/;
        LWORD(edx) = FP_OFF32 (s) /*COM_OFFS_OF(s)*/;
        call_msdos();    /* call MSDOS */
        com_strfree(s);
        if (LWORD(eflags) & CF) return -1;
        return 0;
}

struct REGPACK regs_to_regpack(struct vm86_regs *regs)
{
	struct REGPACK regpack;

	regpack.r_ax =  regs->eax;
	regpack.r_bx =  regs->ebx;
	regpack.r_cx =  regs->ecx;
	regpack.r_dx =  regs->edx;
	regpack.r_bp =  regs->ebp;
	regpack.r_si =  regs->esi;
	regpack.r_di =  regs->edi;
	regpack.r_ds =  regs->ds;
	regpack.r_es =  regs->es;
	regpack.r_flags =  regs->eflags;

	return regpack;
}

struct vm86_regs regpack_to_regs(struct REGPACK *regpack)
{
	struct vm86_regs regs = _regs;

	regs.eax = regpack->r_ax;
	regs.ebx = regpack->r_bx;
	regs.ecx = regpack->r_cx;
	regs.edx = regpack->r_dx;
	regs.ebp = regpack->r_bp;
	regs.esi = regpack->r_si;
	regs.edi = regpack->r_di;
	regs.ds = regpack->r_ds;
	regs.es = regpack->r_es;
	regs.eflags = regpack->r_flags;

	return regs;
}

void com_intr(int intno, struct REGPACK *regpack)
{
	struct vm86_regs saved_regs = _regs;
	_regs = regpack_to_regs(regpack);

	do_intr_call_back(intno, 0);

	*regpack = regs_to_regpack(&_regs);
	_regs = saved_regs;
}

void dos_helper_r(struct REGPACK *regpack)
{
	struct vm86_regs saved_regs = _regs;
	_regs = regpack_to_regs(regpack);

	dos_helper();

	*regpack = regs_to_regpack(&_regs);
	_regs = saved_regs;
}

int com_int86x(int intno, union com_REGS *inregs,
		union com_REGS *outregs, struct SREGS *segregs)
{
	struct vm86_regs saved_regs = _regs;

	LWORD(eax) = inregs->x.ax;
	LWORD(ebx) = inregs->x.bx;
	LWORD(ecx) = inregs->x.cx;
	LWORD(edx) = inregs->x.dx;
	LWORD(esi) = inregs->x.si;
	LWORD(edi) = inregs->x.di;
	LWORD(eflags) = inregs->x.flags;
	if (segregs) {
		LWORD(ds) = segregs->ds;
		LWORD(es) = segregs->es;
	}

	do_intr_call_back(intno, 0);

	outregs->x.ax =  LWORD(eax);
	outregs->x.bx =  LWORD(ebx);
	outregs->x.cx =  LWORD(ecx);
	outregs->x.dx =  LWORD(edx);
	outregs->x.si =  LWORD(esi);
	outregs->x.di =  LWORD(edi);
	outregs->x.flags =  LWORD(eflags);
	if (segregs) {
		segregs->ds =  LWORD(ds);
		segregs->es =  LWORD(es);
	}
	outregs->x.cflag = LWORD(eflags) & CF;

	_regs = saved_regs;

	return LWORD(eax);
}

int com_int86(int intno, union com_REGS *inregs, union com_REGS *outregs)
{
	return com_int86x(intno, inregs, outregs, 0);
}


static struct com_program_entry *com_program_list = 0;

static struct com_program_entry *find_com_program(char *name)
{
	struct com_program_entry *com = com_program_list;

	while (com) {
		if (!strcmp(com->name, name)) return com;
		com = com->next;
	}
	return 0;
}

void register_com_program(char *name, com_program_type *program)
{
	struct com_program_entry *com;

	if ((com = find_com_program(name)) == 0) {
		com = malloc(sizeof(struct com_program_entry));
		if (!com) return;
		com->next = com_program_list;
		com_program_list = com;
	}
	com->name = name;
	com->program = program;
}

static char *com_getarg0(void)
{
	char *env = SEG2LINEAR(COM_PSP_ADDR->envir_frame);
	return memchr(env, 1, 0x10000) + 2;
}

static void zpanic(const char *ctl, ...)
{
	error(ctl);
	error("\nzalloc failure for builtin %s\n", builtin_name);
	leavedos(85);
}

int commands_plugin_inte6(void)
{
#define MAX_ARGS 63
	char *args[MAX_ARGS + 1];
	struct PSP *psp;
	struct MCB *mcb;
	struct coop_com_header *com_header;
	struct com_program_entry *com;
	int argc;

	psp = COM_PSP_ADDR;
	mcb = SEG2LINEAR(COM_PSP_SEG - 1);
        com_header = (struct coop_com_header *)((char *)psp + 0x100);

	if (HI(ax) == 0 && com_header->magic != COM_MAGIC)
		return 0;

	if ((HI(ax) != 0) ||
            strcasecmp(mcb->name, "comcom") == 0 ||
            strcmp((char *)psp + com_header->heapstart, ",,,comcom") == 0)
                return coopthreads_plugin_inte6();

	if (!(lowmem_pool = com_dosallocmem(LOWMEM_POOL_SIZE))) {
		error("Unable to allocate memory pool\n");
		error("You should upgrade generic.com, lredir.com and so on\n");
		return 0;
	}
	zinitPool(&mp, "lowmem_pool", zpanic, znot,
		lowmem_pool, LOWMEM_POOL_SIZE);
	zclearPool(&mp);

	/* first parse commandline */
	args[0] = strlower(com_strdup(com_getarg0()));
	argc = com_argparse(&psp->cmdline_len, &args[1], MAX_ARGS - 1) + 1;
	builtin_name = strlower(com_strdup(mcb->name));
	builtin_name[8] = 0;

	/* now set the DOS version */
	HI(ax) = 0x30;
	call_msdos();
	_osmajor = LO(ax);
	_osminor = HI(ax);

	com = find_com_program(builtin_name);
	if (com) {
		com->program(argc, args);
	} else {
		error("inte6: unknown builtin: %s\n",builtin_name);
	}

	/*
	 * NOTE: We cannot free the lowmem pool here or "unix -e" will fail.
	 * We leave that responsibility to DOS - it will free the mem when
	 * generic.S exits.
	 */

	return 1;
}
