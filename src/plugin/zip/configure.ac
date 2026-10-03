dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([LIBZIP], [libzip >= 1.2],,
    AC_MSG_ERROR([libzip development headers not found or too old]))
AC_OUTPUT
