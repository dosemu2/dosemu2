dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])

PKG_PROG_PKG_CONFIG
AC_ARG_WITH([dj64-lib-dir], AS_HELP_STRING([--with-dj64-lib-dir=DIR],
        [use alternate dj64dev dir]),
  [
    dj64_lib_dir="$withval"
    AC_CHECK_FILE([$dj64_lib_dir/libdjdev64.so],,
        AC_MSG_ERROR([$dj64_lib_dir incorrect]))
    AC_ARG_WITH([dj64-include-dir],
        AS_HELP_STRING([--with-dj64-include-dir=DIR],
            [set dj64 include dir]),
            [dj64_inc_dir="$withval"
             AC_CHECK_FILE([$dj64_inc_dir/djdev64/stub.h],,
                AC_MSG_ERROR([$dj64_inc_dir incorrect]))],
        AC_MSG_ERROR([use --with-dj64-include-dir]))
    AC_ARG_WITH([dj64-pkgconf-dir],
        AS_HELP_STRING([--with-dj64-pkgconf-dir=DIR],
            [set dj64 pkgconf dir]),
        [export PKG_CONFIG_PATH="$withval"],
        AC_MSG_ERROR([use --with-dj64-pkgconf-dir]))
    DJ64_INCLUDES="-I$dj64_inc_dir"
    DJ64_LDFLAGS="-L$dj64_lib_dir -Wl,-rpath,$dj64_lib_dir"
  ]
)

AC_SUBST(DJ64_INCLUDES)
AC_SUBST(DJ64_LDFLAGS)
PKG_CHECK_MODULES([DJDEV64], [djdev64],,
    AC_MSG_ERROR([djdev64 development libs not found]))
PKG_CHECK_MODULES([DJSTUB64], [djstub64],,
    AC_MSG_ERROR([djstub64 development libs not found]))
AC_OUTPUT
