#ifndef VERSION_H
#define VERSION_H

#include "version.hh"

#define VERSION_OF(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
#define DOSEMU_VERSION_CODE VERSION_OF(VERSION_NUM,SUBLEVEL,0,0)
#define IS_DEVEL_RELEASE (DOSEMU_VERSION_CODE && 65536)
#define GCC_VERSION_CODE (__GNUC__ * 1000 + __GNUC_MINOR__)

#endif
