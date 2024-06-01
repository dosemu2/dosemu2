dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([DJDEV64], [djdev64],,
    AC_MSG_ERROR([djdev64 development libs not found]))
PKG_CHECK_MODULES([DJSTUB64], [djstub64],,
    AC_MSG_ERROR([djstub64 development libs not found]))
AC_OUTPUT
