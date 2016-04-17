#ifndef INSTREMU_H
#define INSTREMU_H

int instr_len(unsigned char *, int);
int instr_emu(struct sigcontext *scp, int pmode, int cnt);
int decode_modify_segreg_insn(struct sigcontext *scp, int pmode,
    unsigned int *new_val);
unsigned char instr_binary_byte(unsigned char op, unsigned char op1,
                                   unsigned char op2, unsigned long *eflags);
unsigned instr_binary_word(unsigned op, unsigned op1,
                                   unsigned op2, unsigned long *eflags);
unsigned instr_binary_dword(unsigned op, unsigned op1,
                                   unsigned op2, unsigned long *eflags);

#endif
