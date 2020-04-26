dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_HEADER([_x_config.h])
AC_CONFIG_FILES([Makefile.conf])

dnl Check for the XKB extension to get reliable keyboard handling
AC_CHECK_HEADER(X11/XKBlib.h, AC_DEFINE(HAVE_XKB,1))

PKG_CHECK_MODULES([X11], [x11],,
    AC_MSG_ERROR([X development libraries not installed]))
AC_MSG_NOTICE(Compiling with X support...)
AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)

AC_OUTPUT

AH_TEMPLATE(HAVE_XKB,
[Define this if you have the XKB extension])

sed 's/#define PACKAGE_.\+//' _x_config.h >x_config.hh
