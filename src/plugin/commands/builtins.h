#ifndef __BUILTINS_H
#define __BUILTINS_H

#include "emu.h"
#include "bios.h"

#define COM_PSP_SEG	(REG(es))
#define COM_PSP_ADDR	((struct PSP *)SEG2LINEAR(COM_PSP_SEG))

typedef int com_program_type(int argc, char **argv);

struct com_program_entry {
	struct com_program_entry *next;
	char * name;
	com_program_type *program;
};

struct	REGPACK {
	unsigned short r_ax, r_bx, r_cx, r_dx;
	unsigned short r_bp, r_si, r_di, r_ds, r_es, r_flags;
} __attribute__((packed));
#define REGPACK_INIT (regs_to_regpack(&_regs))

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

void dos_helper_r(struct REGPACK *regpack);

/* ============= libc equlivalents ============== */

#define com_stdin	0
#define com_stdout	1
#define com_stderr	2

int com_vsprintf(char *str, char *format, va_list ap);
int com_sprintf(char *str, char *format, ...);
int com_vfprintf(int dosfilefd, char *format, va_list ap);
int com_vprintf(char *format, va_list ap);
int com_fprintf(int dosfilefd, char *format, ...);
int com_printf(char *format, ...);
int com_puts(char *s);
int com_doswrite(int dosfilefd, char *buf32, int size);
int com_dosread(int dosfilefd, char *buf32, int size);
char *com_getenv(char *keyword);
int com_system(char *command);
char * com_strdup(char *s);
unsigned short get_dos_ver(void);
void com_strfree(char *s);
void com_intr(int intno, struct REGPACK *regpack);
int com_int86x(int intno, union com_REGS *inregs,
		union com_REGS *outregs, struct SREGS *segregs);
int com_int86(int intno, union com_REGS *inregs, union com_REGS *outregs);
void call_msdos(void);
void call_msdos_interruptible(void);
char *lowmem_alloc(int size);
void lowmem_free(char *p, int size);
void register_com_program(char *name, com_program_type *program);
char *skip_white_and_delim(char *s, int delim);
struct REGPACK regs_to_regpack(struct vm86_regs *regs);
struct vm86_regs regpack_to_regs(struct REGPACK *regpack);

extern unsigned char _osmajor;
extern unsigned char _osminor;
extern int com_errno;

#endif
