#ifndef INSTR_DEC_H
#define INSTR_DEC_H

int decode_segreg(sigcontext_t *scp);
uint16_t decode_selector(sigcontext_t *scp);
int decode_memop(sigcontext_t *scp, uint32_t *op);

#endif
