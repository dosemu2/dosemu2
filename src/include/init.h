#ifndef DOSEMU_INIT_H
#define DOSEMU_INIT_H

#include "config.h"

#define CONSTRUCTOR(X) X __attribute__((constructor)); X
#define DESTRUCTOR(X) X __attribute__((destructor)); X

#endif /* DOSEMU_INIT_H */
