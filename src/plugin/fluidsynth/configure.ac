dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([FLUS], [fluidsynth],,
    AC_MSG_ERROR([fluidsynth development libs not found]))
AC_SUBST(FLUS_CFLAGS)
AC_SUBST(FLUS_LIBS)
AC_OUTPUT
