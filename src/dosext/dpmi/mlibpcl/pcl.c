#include "pcl_ctx.h"
#include "pcl.h"
#define _S(x) #x
#define S(x, y) _S(x/y)
#include S(LIBPCL_DIR, pcl.c)
