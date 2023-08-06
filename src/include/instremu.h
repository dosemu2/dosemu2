#ifndef INSTREMU_H
#define INSTREMU_H

int instr_len(unsigned char *ptr, int is_32);
int decode_modify_segreg_insn(cpuctx_t *scp, int pmode,
    unsigned int *new_val);

#endif
