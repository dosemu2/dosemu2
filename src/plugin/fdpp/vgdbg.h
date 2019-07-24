#ifndef VGDBG_H
#define VGDBG_H

void fdpp_mark_mem(uint16_t seg, uint16_t off, uint16_t size, int type);
void fdpp_prot_mem(uint16_t seg, uint16_t off, uint16_t size, int type);

#endif
