dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
AC_CHECK_LIB(ieee1284, ieee1284_open,,
    AC_MSG_ERROR([ieee1284 development lib not found]))
AC_CHECK_HEADER(ieee1284.h,,
    AC_MSG_ERROR([ieee1284 development header not found]))
AC_SUBST(LIBS)
AC_OUTPUT
