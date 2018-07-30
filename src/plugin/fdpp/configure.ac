dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([FDPP], [fdpp],,
    AC_MSG_ERROR([fdpp installation not found]))
AC_SUBST(FDPP_LIBS)
AC_OUTPUT
