#ifndef INSTR_DEC_H
#define INSTR_DEC_H

int decode_segreg(cpuctx_t *scp);
uint16_t decode_selector(cpuctx_t *scp);
int decode_memop(cpuctx_t *scp, uint32_t *op, dosaddr_t cr2);

void instrdec_init(void);
void instrdec_done(void);

#endif
