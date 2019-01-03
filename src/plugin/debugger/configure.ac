dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])

AC_CHECK_LIB(readline, main, [
  DOSDEBUG_LIBS="-lreadline"
  DOSDEBUG_DEFS="-DHAVE_LIBREADLINE"
], [
  AC_MSG_ERROR(Please install libreadline-dev for readline support in dosdebug)
])
AC_SUBST(DOSDEBUG_LIBS)
AC_SUBST(DOSDEBUG_DEFS)

AC_OUTPUT
