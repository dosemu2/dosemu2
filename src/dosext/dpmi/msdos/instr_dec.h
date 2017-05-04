#ifndef INSTR_DEC_H
#define INSTR_DEC_H

int decode_segreg(struct sigcontext *scp);
uint16_t decode_selector(struct sigcontext *scp);
int decode_memop(struct sigcontext *scp, uint32_t *op);

#endif
