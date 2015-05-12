dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([ALSA], alsa,,
    AC_MSG_ERROR([alsa development libs not found]))
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
AC_OUTPUT
