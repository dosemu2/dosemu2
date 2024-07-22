dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([SLANG], [slang],,
    AC_MSG_ERROR([slang development headers not found]))
AC_CHECK_LIB(b64, base64_encode_block, [
  AC_CHECK_HEADER([b64/cencode.h], [
    CLIP_TERM=1
  ], [AC_MSG_WARN([b64 development headers not found])])
], [AC_MSG_WARN([b64 libs not found])])
AC_SUBST(CLIP_TERM)
AC_OUTPUT
