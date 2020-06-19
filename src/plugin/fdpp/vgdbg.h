#ifndef VGDBG_H
#define VGDBG_H

#include <fdpp/thunks.h>

void fdpp_mark_mem(fdpp_far_t p, uint16_t size, int type);
void fdpp_prot_mem(fdpp_far_t p, uint16_t size, int type);

#endif
