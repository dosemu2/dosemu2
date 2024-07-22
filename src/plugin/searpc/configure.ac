dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])

PKG_CHECK_MODULES([GLIB2], [glib-2.0], [
    CFLAGS="$CFLAGS $GLIB2_CFLAGS"
    LIBS="$LIBS $GLIB2_LIBS"
  ],
  [ AC_MSG_ERROR(glib2-devel not found) ]
)

PKG_CHECK_MODULES([SEARPC], [libsearpc], [
    CFLAGS="$CFLAGS $SEARPC_CFLAGS"
    LIBS="$LIBS $SEARPC_LIBS"
  ],
  [ AC_MSG_ERROR(libsearpc-devel not found) ]
)

AC_PATH_PROGS([RPCGEN], [searpc-codegen searpc-codegen.py],
  [AC_MSG_ERROR(searpc-codegen not found)])
AC_SUBST(RPCGEN)

AC_OUTPUT
