dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_HEADER(_fdppconf.h)
AC_CONFIG_FILES([Makefile.conf])
PKG_PROG_PKG_CONFIG
AC_ARG_WITH([fdpp-build-path], AS_HELP_STRING([--with-fdpp-build-path=DIR],
        [use alternate fdpp dir]),
  [
    export PKG_CONFIG_PATH="$withval"
    export PKG_CONFIG="$PKG_CONFIG --define-variable=prefix=$withval \
        --define-variable=includedir=$withval \
        --define-variable=fdpplibdir=$withval"
    AC_DEFINE_UNQUOTED(FDPP_KERNEL_DIR, "$withval")
  ],
  [
    if test "x$PKG_CONFIG_PATH" = x
    then
       export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
    else
       export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"
    fi
  ]
)
PKG_CHECK_MODULES([FDPP], [fdpp >= 0.1beta8],,
    AC_MSG_ERROR([fdpp installation not found or too old]))
if test -n "$with_fdpp_build_path"; then
    CPPFLAGS="$CPPFLAGS $FDPP_CFLAGS"
    AC_CHECK_HEADER([fdpp/thunks.h],,
        AC_MSG_ERROR([fdpp headers not found]))
fi
AC_SUBST(FDPP_CFLAGS)
AC_SUBST(FDPP_LIBS)
PKG_CHECK_MODULES([VALGRIND], [valgrind], AC_DEFINE(HAVE_VALGRIND),
  AC_MSG_NOTICE([valgrind support disabled]))
AH_TEMPLATE([HAVE_VALGRIND],
[Define this if you want valgrind integration])
AH_TEMPLATE([FDPP_KERNEL_DIR],
[Define this to your custom fdpp location])
AC_OUTPUT

sed 's/#define PACKAGE_.\+//' _fdppconf.h >fdppconf.hh
