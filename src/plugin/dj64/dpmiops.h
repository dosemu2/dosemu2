#ifndef DPMIOPS_H
#define DPMIOPS_H

#define DD(r, n, a, ...) \
  r (*__##n) a;
#define DDv(r, n) \
  r (*__##n)(void);
#define vDD(n, a, ...) \
  void (*__##n) a;
#define vDDv(n) \
  void (*__##n)(void);

struct dpmi_ops {
#include "dpmi_inc.h"
};

#undef DD
#undef DDv
#undef vDD
#undef vDDv

extern struct dpmi_ops dpmiops;

#endif
