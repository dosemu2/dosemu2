#ifndef INSTREMU_H
#define INSTREMU_H

int instr_len(unsigned char *ptr, int is_32);
int decode_modify_segreg_insn(cpuctx_t *scp, int pmode,
    unsigned int *new_val);
unsigned char instr_binary_byte(unsigned char op, unsigned char op1,
                                   unsigned char op2, unsigned *eflags);
unsigned instr_binary_word(unsigned op, unsigned op1,
                                   unsigned op2, unsigned *eflags);
unsigned instr_binary_dword(unsigned op, unsigned op1,
                                   unsigned op2, unsigned *eflags);
#endif
