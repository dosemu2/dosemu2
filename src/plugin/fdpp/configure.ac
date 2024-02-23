dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_HEADERS([_fdppconf.h])
AC_CONFIG_FILES([Makefile.conf])
PKG_PROG_PKG_CONFIG
AC_ARG_WITH([fdpp-lib-dir], AS_HELP_STRING([--with-fdpp-lib-dir=DIR],
        [use alternate fdpp dir]),
  [
    fdpp_lib_dir="$withval"
    AC_CHECK_FILE([$fdpp_lib_dir/libfdpp.so],,
        AC_MSG_ERROR([$fdpp_lib_dir incorrect]))
    AC_ARG_WITH([fdpp-include-dir],
        AS_HELP_STRING([--with-fdpp-include-dir=DIR],
            [set fdpp include dir]),
            [fdpp_inc_dir="$withval"
             AC_CHECK_FILE([$fdpp_inc_dir/fdpp/thunks.h],,
                AC_MSG_ERROR([$fdpp_inc_dir incorrect]))],
        AC_MSG_ERROR([use --with-fdpp-include-dir]))
    AC_ARG_WITH([fdpp-kernel-dir],
        AS_HELP_STRING([--with-fdpp-kernel-dir=DIR],
            [set fdpp kernel dir]),
            fdpp_kern_dir="$withval",
            fdpp_kern_dir="$fdpp_lib_dir")
    AC_ARG_WITH([fdpp-pkgconf-dir],
        AS_HELP_STRING([--with-fdpp-pkgconf-dir=DIR],
            [set fdpp pkgconf dir]),
        [export PKG_CONFIG_PATH="$withval"],
        [export PKG_CONFIG_PATH="$fdpp_lib_dir"])

    AC_DEFINE_UNQUOTED(FDPP_KERNEL_DIR, "$fdpp_kern_dir")
    FDPP_RPATH="$fdpp_lib_dir"
    FDPP_INCLUDES=-I"$fdpp_inc_dir"
    FDPP_LFLAGS=-L"$fdpp_lib_dir"
  ],
  [
    if test "x$prefix" = xNONE; then
      if test "x$PKG_CONFIG_PATH" = x
      then
        export PKG_CONFIG_PATH="$ac_default_prefix/lib/pkgconfig"
      else
        export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$ac_default_prefix/lib/pkgconfig"
      fi
      AC_ARG_WITH([fdpp-rpath], AS_HELP_STRING([--with-fdpp-rpath=DIR],
        [add alternate fdpp rpath]),
        [FDPP_RPATH="$fdpp_rpath"],
        [FDPP_RPATH="/usr/lib/fdpp"
         FDPP_RPATH2="/usr/lib64/fdpp"]
      )
    else
      FDPP_RPATH="$prefix/lib/fdpp"
      FDPP_RPATH2="$prefix/lib64/fdpp"
    fi
    FDPP_RPATH3="$ac_default_prefix/lib/fdpp"
    FDPP_RPATH4="$ac_default_prefix/lib64/fdpp"
  ]
)

AC_SUBST(FDPP_RPATH)
AC_SUBST(FDPP_RPATH2)
AC_SUBST(FDPP_RPATH3)
AC_SUBST(FDPP_RPATH4)
AC_SUBST(FDPP_INCLUDES)
AC_SUBST(FDPP_LFLAGS)

PKG_CHECK_MODULES([FDPP], [fdpp >= 1.7],,
    AC_MSG_ERROR([fdpp installation not found or too old]))
PKG_CHECK_MODULES([VALGRIND], [valgrind], AC_DEFINE(HAVE_VALGRIND),
  AC_MSG_NOTICE([valgrind support disabled]))
AH_TEMPLATE([HAVE_VALGRIND],
[Define this if you want valgrind integration])
AH_TEMPLATE([FDPP_KERNEL_DIR],
[Define this to your custom fdpp location])
AC_OUTPUT

sed -E 's/#define PACKAGE_.+//' _fdppconf.h >fdppconf.hh
