dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
AC_CHECK_LIB(fdpp, FdppThunkCall,,
    AC_MSG_ERROR([fdpp development libs not found]))
AC_CHECK_HEADER([fdpp/thunks.h],,
    AC_MSG_ERROR([fdpp development headers not found]))
AC_SUBST(LIBS)
AC_OUTPUT
