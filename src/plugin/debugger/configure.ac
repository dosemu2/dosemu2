dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])

AC_CHECK_LIB(readline, main, [
  DOSDEBUG_LIBS="-lreadline"
  DOSDEBUG_DEFS="-DHAVE_LIBREADLINE"
], [
  AC_MSG_ERROR(Please install libreadline-dev for readline support in dosdebug)
])
AC_CHECK_LIB(readline, rl_copy_text, [], [
  CPPFLAGS="$CPPFLAGS -I/usr/local/opt/readline/include"
  LDFLAGS="$LDFLAGS -L/usr/local/opt/readline/lib"
  AS_UNSET([ac_cv_lib_readline_rl_copy_text])
  AC_CHECK_LIB(readline, rl_copy_text, [
    DOSDEBUG_LIBS="$DOSDEBUG_LIBS -L/usr/local/opt/readline/lib"
    DOSDEBUG_DEFS="$DOSDEBUG_DEFS -I/usr/local/opt/readline/include"
  ], [
    AC_MSG_ERROR(If using a Mac, please brew install readline)
  ])
])
AC_SUBST(DOSDEBUG_LIBS)
AC_SUBST(DOSDEBUG_DEFS)

AC_OUTPUT
