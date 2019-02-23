dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
if test "x$PKG_CONFIG_PATH" = x
then
   export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
else
   export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"
fi
PKG_CHECK_MODULES([FDPP], [fdpp >= 0.1beta5],,
    AC_MSG_ERROR([fdpp installation not found or too old]))
AC_SUBST(FDPP_CFLAGS)
AC_SUBST(FDPP_LIBS)
AC_OUTPUT
