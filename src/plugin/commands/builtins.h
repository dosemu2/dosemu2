#ifndef __BUILTINS_H
#define __BUILTINS_H

#include "emu.h"
#include "bios.h"
#include "dos2linux.h"

#define COM_PSP_SEG	(REG(es))
#define COM_PSP_ADDR	((struct PSP *)LINEAR2UNIX(SEG2LINEAR(COM_PSP_SEG)))

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

/* ============= libc equlivalents ============== */

#define com_stdin	0
#define com_stdout	1
#define com_stderr	2

int com_error(char *format, ...);
char *com_getenv(char *keyword);
int com_system(char *command, int quit);
char * com_strdup(char *s);
unsigned short get_dos_ver(void);
void com_strfree(char *s);
int com_dosgetdrive(void);
int com_dossetdrive(int drive);
int com_dossetcurrentdir(char *path);
void com_intr(int intno, struct REGPACK *regpack);
void call_msdos(void);
void call_msdos_interruptible(void);
char *lowmem_alloc(int size);
void lowmem_free(char *p, int size);
void register_com_program(char *name, com_program_type *program);
char *skip_white_and_delim(char *s, int delim);
struct REGPACK regs_to_regpack(struct vm86_regs *regs);
struct vm86_regs regpack_to_regs(struct REGPACK *regpack);

extern int com_errno;

#endif
