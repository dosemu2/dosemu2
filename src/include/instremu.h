#ifndef INSTREMU_H
#define INSTREMU_H

#ifdef INSTREMU
int instr_len(unsigned char *ptr, int is_32);
int instr_emu(cpuctx_t *scp, int pmode, int cnt);
int decode_modify_segreg_insn(cpuctx_t *scp, int pmode,
    unsigned int *new_val);
unsigned char instr_binary_byte(unsigned char op, unsigned char op1,
                                   unsigned char op2, unsigned *eflags);
unsigned instr_binary_word(unsigned op, unsigned op1,
                                   unsigned op2, unsigned *eflags);
unsigned instr_binary_dword(unsigned op, unsigned op1,
                                   unsigned op2, unsigned *eflags);
#else
static inline int instr_len(unsigned char *ptr, int is_32)
{
    return 0;
}
static inline int instr_emu(cpuctx_t *scp, int pmode, int cnt)
{
    return 0;
}
static inline unsigned char instr_binary_byte(unsigned char op,
                                   unsigned char op1,
                                   unsigned char op2, unsigned *eflags)
{
    return 0;
}
static inline unsigned short instr_binary_word(unsigned op, unsigned op1,
                                   unsigned op2, unsigned *eflags)
{
    return 0;
}
static inline unsigned instr_binary_dword(unsigned op, unsigned op1,
                                   unsigned op2, unsigned *eflags)
{
    return 0;
}
#endif

#endif
