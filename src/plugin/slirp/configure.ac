dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([SLIRP], [slirp],,
    AC_MSG_ERROR([slirp development library not installed]))
AC_SUBST(LIBS)
AC_OUTPUT
