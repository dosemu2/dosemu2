#include "dpmi.h"
#include "dpmiops.h"

#define DD(r, n, a, ...) \
  .__##n =  ___##n,
#define DDv(r, n) \
  .__##n =  ___##n,
#define vDD(n, a, ...) \
  .__##n =  ___##n,
#define vDDv(n) \
  .__##n =  ___##n,

struct dpmi_ops dpmiops = {
#include "dpmi_inc.h"
};
