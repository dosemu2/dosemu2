/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Stupid Simple Cooperative Thread Dispatcher (SSCTD;-)
 *
 * Copyright (c) 1994 .. 2001 Hans Lermen <lermen@fgan.de>
 *
 */

#ifndef COOPTHREADS_H
#define COOPTHREADS_H 1

#define lowmem_alloc coop_lowmem_alloc
#define lowmem_free coop_lowmem_free
#define call_msdos coop_call_msdos
#define com_dosallocmem coop_com_dosallocmem
#define com_dosfreemem coop_com_dosfreemem
#define com_getenv coop_com_getenv
#define com_system coop_com_system
#define com_intr coop_com_intr
#define com_int86x coop_com_int86x
#define com_int86 coop_com_int86
#define register_com_program coop_register_com_program
#define com_doswrite coop_com_doswrite
#define com_dosread coop_com_dosread
#define com_strdup coop_com_strdup
#define com_strfree coop_com_strfree
#define com_vsprintf coop_com_vsprintf
#define com_sprintf coop_com_sprintf
#define com_vprintf coop_com_vprintf
#define com_vfprintf coop_com_vfprintf
#define com_fprintf coop_com_fprintf
#define com_printf coop_com_printf
#define com_puts coop_com_puts

#if 0
#include <unistd.h>
#define COM_CHECK_PERMISSIONS { \
  uid_t uid = getuid(); \
  uid_t euid = geteuid(); \
  if (uid != euid) { \
    error( \
	"You started DOSEMU suid-root. However, there is code configured in,\n" \
	"which is not yet audited enough to let it run suid-root.\n" \
	"Either remove the s-bit from the binary, or run as user root, \n" \
	"or recompile DOSEMU without the involved experimental code.\n\n" \
    ); \
    leavedos(99); \
  } \
}
#else
#define COM_CHECK_PERMISSIONS
#endif

#define YUP fprintf(stderr,
#define CALLER(firstarg) *(((unsigned *)(&firstarg))-1)

#include <stdarg.h>

#define DOS_HELPER_COOP		0xc0

/* ============ thread switcher part =================== */

register unsigned long STACKPTR asm("%esp");
typedef void thread_function_type(void *params);
typedef void com_hook_call_type(int param);

struct tcb {
	void *params;
	thread_function_type *thread_code;
	char *stack;
	unsigned long stack_size;
	jmp_buf context;
	jmp_buf exit_context;
	/* local data parts,
	 * we have these here (and not in ctcb) because of security reasons.
	 */
	struct com_program_entry *pent;
	com_hook_call_type **hook_functions;
};

#ifdef COOPTHREADS_H_ITSELF
  #define COOPEXTRN
  #define NONVOLATILE_OWNTCB ((struct tcb *)owntcb)
  volatile struct tcb *owntcb = 0;
  struct tcb *tcb0 = 0;
#else
#  define COOPEXTRN extern
  COOPEXTRN struct tcb *owntcb;
  COOPEXTRN struct tcb *tcb0;
#endif

void switch_to(struct tcb* new);

/* ============ com thread part =================== */

typedef int com_program_type(int argc, char **argv);
typedef void function_call_type(void);

#define COM_THREAD_MAGIC	0xd0c0

struct com_program_entry {
	struct com_program_entry *next;
	char * name;
	com_program_type *program;
	int stacksize;
	int heapsize;
	function_call_type **functions;
	int num_functions;
};

#define NUM_COM_HOOKS	29	/* just fits into 128 bytes */

struct com_hook_entry {
	char nearcallop;
	short distpl;
	unsigned char param;
} __attribute__((packed));


struct com_starter_seg {
	unsigned short	opint20;	/* 0x00 */
	unsigned short	memend_frame;	/* 0x02 */
	unsigned char	dos_reserved4;	/* 0x04 */
	unsigned char	cpm_function_entry[0xa-0x5];	/* 0x05 */
	FAR_PTR		int22_copy;	/* 0x0a */
	FAR_PTR		int23_copy;	/* 0x0e */
	FAR_PTR		int24_copy;	/* 0x12 */
	unsigned short	parent_psp;	/* 0x16 */
	unsigned char	file_handles[20];	/* 0x18 */
	unsigned short	envir_frame;	/* 0x2c */
	FAR_PTR		system_stack;	/* 0x2e */
	unsigned short	max_open_files;	/* 0x32 */
	FAR_PTR		file_handles_ptr;	/* 0x34 */
	unsigned char	dos_reserved38[0x50-0x38];	/* 0x38 */
	unsigned char	high_language_dos_call[0x53-0x50];	/* 0x50 */
	unsigned char	dos_reserved53[0x5c-0x53];	/* 0x53 */
	unsigned char	FCB1[0x6c-0x5c];	/* 0x5c */
	unsigned char	FCB2[0x80-0x6c];	/* 0x6c */
	unsigned char	DTA[0x100-0x80];	/* 0x80 */
			/* PSP above, our header below */
	unsigned char	jumparound[3];	/* jmp start*/
	unsigned char	mode_flags;	/* 0x103 */
#				define CDEBUG 1		/* within mode_flags */
	unsigned short	magic;		/* 0x104 */
	unsigned short	heapstart;	/* 0x106 */
	unsigned short  saved_ax;	/* 0x108 */
	unsigned short  real_psp;	/* 0x10a */
	unsigned short	far_sp,far_ss;	/* 0x10c */
	unsigned short	saved_sp,saved_ss; /* 0x110 */
	unsigned short	oldip, oldcs;	/* 0x114 */
	unsigned short	oldsp;		/* 0x118 */
	unsigned short	heapend;	/* 0x11a */
	unsigned short	heapptr;	/* 0x11c */
	unsigned short	brkpoint;	/* 0x11e */
	unsigned long	userdata;	/* 0x120 */
	char		intop, intnum;	/* 0x124 */
	char		rethook[9];	/* 0x126 */
	char		align120[1];	/* 0x12f, for future development */
	FAR_PTR		saved_int23;	/* 0x130 */
	FAR_PTR		saved_int24;	/* 0x134 */
	struct com_hook_entry *hooks;	/* 0x138 */
	struct tcb 	*tcb;		/* 0x13c */
	char		heapmap[64];	/* 0x140 */
	char		comstarts_here;	/* 0x180 */
} __attribute__((packed));

#define SEG2LINEAR(seg)	((void *)  ( ((unsigned int)(seg)) << 4)  )
#define COM_SEG		((unsigned short)((unsigned int)ctcb >> 4))
#define COM_OFFS_OF(x)	((unsigned short)(((char *)x) - ((char *)ctcb)))
#define CTCB		((struct com_starter_seg *)owntcb->params)
#define UDATAPTR	((void *)(CTCB->userdata))
//((void *)(((char *)owntcb->params) + CTCB->userdata))

#define LOWMEM_HEAP_SHIFT	7
#define LOWMEM_HEAP_GRAN	(1<<LOWMEM_HEAP_SHIFT)
#define MIN_STACK_HEAP_PADDING	512
#define roundup_lowmem_heap_size(size) ((size + LOWMEM_HEAP_GRAN -1) \
	& (-((int)LOWMEM_HEAP_GRAN)))

#define MK_FP16(s,o)	((((unsigned long)s) << 16) | (o & 0xffff))
#define MK_FP			MK_FP16
#define FP_OFF16(far_ptr)	((int)far_ptr & 0xffff)
#define FP_SEG16(far_ptr)	(((unsigned int)far_ptr >> 16) & 0xffff)
#define MK_FP32(s,o)		((void *)SEGOFF2LINEAR(s,o))
#define FP_OFF32(void_ptr)	((unsigned int)void_ptr & 15)
#define FP_SEG32(void_ptr)	(((unsigned int)void_ptr >> 4) & 0xffff)
#define rFAR_PTR(type,far_ptr) ((type)((FP_SEG16(far_ptr) << 4)+(FP_OFF16(far_ptr))))

void register_com_program(char *name, com_program_type *program,
							char *flags, ...);
void call_vm86_from_com_thread(int seg, int offs);
void com_delete_other_thread(struct com_starter_seg *other);
void call_msdos(void);
char *lowmem_alloc(int size);
void lowmem_free(char *p, int size);
FAR_PTR register_com_hook_function(int intnum,
		com_hook_call_type *funct, int num, int param);

/* ============= MDOS functions (and equivalent) calls ============== */

extern unsigned char _osmajor;
extern unsigned char _osminor;
extern int com_errno;

#define _psp (((struct com_starter_seg  *)(owntcb->params))->real_psp)

struct com_MCB {
	char id;			/* 0 */
	unsigned short owner_psp;	/* 1 */
	unsigned short size;		/* 3 */
	char align8[3];			/* 5 */
	char name[8];			/* 8 */
} __attribute__((packed));

struct	REGPACK {
	unsigned short r_ax, r_bx, r_cx, r_dx;
	unsigned short r_bp, r_si, r_di, r_ds, r_es, r_flags;
} __attribute__((packed));

struct WORDREGS {
	unsigned short ax, bx, cx, dx, si, di, cflag, flags;
} __attribute__((packed));

struct BYTEREGS {
	unsigned char al, ah, bl, bh, cl, ch, dl, dh;
} __attribute__((packed));

union com_REGS {
	struct WORDREGS x;
	struct BYTEREGS h;
} __attribute__((packed));

struct SREGS {
	unsigned short es;
	unsigned short cs;
	unsigned short ss;
	unsigned short ds;
} __attribute__((packed));

struct findfirstnext_dta {
	/* offsets relative to ctcb, so may overlay it */
	char align95[0x80+0x15];
	unsigned char	attrib;		/* 0x95 */
			#define DOS_ATTR_NORM	0x00
			#define	DOS_ATTR_RDONLY	0x01
			#define DOS_ATTR_HIDDEN	0x02
			#define DOS_ATTR_SYSTEM	0x04
			#define DOS_ATTR_VOLID	0x08
			#define DOS_ATTR_DIR	0x10
			#define DOS_ATTR_ARCH	0x20
			#define DOS_ATTR_FILE	0x27
			#define DOS_ATTR_ALL	0x37
			#define DOS_ATTR_PROTECTED 0x7
	unsigned short	filetime;	/* 0x96 */
	unsigned short	filedate;	/* 0x98 */
	unsigned long	filesize;	/* 0x9a */
	char		name[13];	/* 0x9e */
} __attribute__((packed));

/* use anyDTA and PSP_DTA _only_ for save/restore purposes,
 * but for nothing else.
 */
typedef struct {
	char DTA[128];
} anyDTA;
#define PSP_DTA (*((anyDTA *)(&CTCB->DTA)))

void com_intr(int intno, struct REGPACK *regpack);
int com_int86(int intno, union com_REGS *inregs, union com_REGS *outregs);
int com_int86x(int intno, union com_REGS *inregs, union com_REGS *outregs,
	struct SREGS *segregs);
int com_intdos(union com_REGS *inregs, union com_REGS *outregs);
int com_intdosx(union com_REGS *inregs, union com_REGS *outregs,
	struct SREGS *segregs);

int com_dosallocmem(int size);	/* size in bytes (not frames!) */
int com_dosfreemem(int frame);
void com_free_unused_dosmem(void);
int com_dosreallocmem(int frame, int newsize);

int com_dosopen(char *fname, int access);
#define DOS_ACCESS_CODE		0x000F	/* mask */
#define DOS_ACCESS_SHARE	0x0070	/* mask */
#define DOS_ACCESS_INHERIT	0x0080	/* bit, 1 = inherit */
#define DOS_ACCESS_CREATE	0x1000	/* bit, 1 = use 'create' instead of 'open' */
#define DOS_ACCESS_ATTR		0x0F00	/* mask	*/
int com_dosclose(int dosfilefd);
int com_doscreatetempfile(char *pathtemplate /* needs 13 bytes tail behind */);
int com_dosrenamefile(char *old, char *new);
int com_dosdeletefile(char *file);
int com_doswrite(int dosfilefd, char *buf32, int size);
int com_doswriteconsole(char *string);
int com_dosread(int dosfilefd, char *buf32, int size);
int com_doscopy(int writefd, int readfd, int size, int textmode);
long com_dosseek(int dosfilefd, unsigned long offset, int whence);
long com_seektotextend(int fd);
int com_dosreadtextline(int fd, char * buf, int bsize, long *lastseek);
int com_dosfindfirst(char *path, int attr);
int com_dosfindnext(void);
int com_dosgetdrive(void);
int com_dossetdrive(int drive);
int com_dosgetfreediskspace(int drive, unsigned result[]);
int com_dosgetcurrentdir(int drive, char *buf);
int com_dossetcurrentdir(char *path);
int com_dosgetsetfilestamp(int fd, int ftime);
int com_doscanonicalize(char *buf, char *path);
int com_getdriveandpath(int drive, char *buf);
int com_dosmkrmdir(char *path, int remove);
int com_dosduphandle(int fd);
int com_dosforceduphandle(int open_fd, int new_fd);

int com_dosgetdate(void);	/* returned format: 0x0YYYWMDD */
int com_dossetdate(int date);	/* input format:    0x0YYY0MDD */
int com_dosgettime(void);	/* returned format: 0xHHMMSSss */
int com_dossettime(int time);	/* Input format:    0xHHMMSSss */

unsigned short com_peek(int seg, int off);
int load_and_run_DOS_program(char *command, char *cmdline);
int com_argparse(char *s, char **argvx, int maxarg);

int com_bioskbd(int ax_value);
int com_biosvideo(int ax_value);



/* ============= libc equlivalents ============== */

#define com_stdin	0
#define com_stdout	1
#define com_stderr	2

void com_exit(int exitcode);
int com_vsprintf(char *str, char *format, va_list ap);
int com_sprintf(char *str, char *format, ...);
int com_vfprintf(int dosfilefd, char *format, va_list ap);
int com_vprintf(char *format, va_list ap);
int com_fprintf(int dosfilefd, char *format, ...);
int com_printf(char *format, ...);
int com_puts(char *s);

char * com_strdup(char *s);
void com_strfree(char *s);
char * strupr(char *s);
char * strlower(char *s);

char *com_getenv(char *key);
int com_system(char *command);

#endif /* COOPTHREADS_H */
