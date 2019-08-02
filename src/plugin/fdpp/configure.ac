dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_HEADER(_fdppconf.h)
AC_CONFIG_FILES([Makefile.conf])
if test "x$PKG_CONFIG_PATH" = x
then
   export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
else
   export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"
fi
PKG_CHECK_MODULES([FDPP], [fdpp >= 0.1beta6],,
    AC_MSG_ERROR([fdpp installation not found or too old]))
AC_SUBST(FDPP_CFLAGS)
AC_SUBST(FDPP_LIBS)
PKG_CHECK_MODULES([VALGRIND], [valgrind], AC_DEFINE(HAVE_VALGRIND),
  AC_MSG_NOTICE([valgrind support disabled]))
AH_TEMPLATE([HAVE_VALGRIND],
[Define this if you want valgrind integration])
AC_OUTPUT

sed 's/#define PACKAGE_.\+//' _fdppconf.h >fdppconf.hh
