dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([JSONC], [json-c >= 0.13],,
    AC_MSG_ERROR([json-c development headers not found or too old]))
AC_OUTPUT
