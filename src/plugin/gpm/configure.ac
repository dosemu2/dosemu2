dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
AC_CHECK_HEADER(gpm.h,,
    AC_MSG_ERROR([gpm development headers not found]))
AC_CHECK_LIB(gpm, Gpm_Open,,
    AC_MSG_ERROR([gpm development libs not found]))
AC_SUBST(LIBS)
AC_OUTPUT
