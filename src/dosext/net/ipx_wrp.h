/* ipx.h compatibility wrapper */

#ifdef HAVE_NETIPX_IPX_H
#include <netipx/ipx.h>
#define IPX 1
#else
#ifdef HAVE_LINUX_IPX_H
#include <linux/ipx.h>
#define IPX 1
#define IPX_TYPE 1
#define SOL_IPX 256
#else
#warning no ipx.h
#endif
#endif
