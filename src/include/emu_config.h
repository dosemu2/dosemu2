#ifndef EMU_CONFIG_H
#define EMU_CONFIG_H

#include "config.hh"

#ifdef __clang__
#define HAS_FEATURE(x) __has_feature(x)
#else
#define HAS_FEATURE(x) 0
#endif
#if (defined(__SANITIZE_ADDRESS__) || HAS_FEATURE(address_sanitizer)) && \
  defined(HAVE_SANITIZER_ASAN_INTERFACE_H)
#define USE_ASAN 1
#else
#define USE_ASAN 0
#endif

#endif
