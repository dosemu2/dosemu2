#ifndef SEGREG_H
#define SEGREG_H

int msdos_fault(struct sigcontext *scp);

typedef struct x86_ins {
  int _32bit:1;	/* 16/32 bit code */
  unsigned address_size; /* in bytes so either 4 or 2 */
  unsigned operand_size;
  int rep;
  int ds:1, es:1, fs:1, gs:1, cs:1, ss:1;
} x86_ins;

int x86_handle_prefixes(struct sigcontext *scp, unsigned cs_base,
	x86_ins *x86);
uint32_t x86_pop(struct sigcontext *scp, x86_ins *x86);

#endif
