dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([MSPACK], [libmspack],,
    AC_MSG_ERROR([libmspack development headers not found]))
AC_SUBST(MSPACK_CFLAGS)
AC_SUBST(MSPACK_LIBS)
AC_OUTPUT
