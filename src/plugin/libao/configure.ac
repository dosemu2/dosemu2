dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
AC_CHECK_HEADER(ao/ao.h,,
    AC_MSG_ERROR([libao development headers not found]))
AC_CHECK_LIB(ao, ao_initialize,,
    AC_MSG_ERROR([libao development libs not found]))
AC_SUBST(LIBS)
AC_OUTPUT
