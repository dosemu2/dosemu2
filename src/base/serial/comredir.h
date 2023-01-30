#ifndef COMREDIR_H
#define COMREDIR_H

void comredir_init(void);
void comredir_setup(int num, int num_wr, unsigned flags);
void comredir_reset(void);

#endif
