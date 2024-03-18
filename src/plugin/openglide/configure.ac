dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])

AC_CHECK_LIB(glide2x, grGlideInit, [
  OPENGLIDE_LIBS="-lglide2x"
], [
  AC_MSG_ERROR(Please install OpenGLide for GLide passthrough plugin)
])
CPPFLAGS="$CPPFLAGS -I/usr/include/openglide"
AC_CHECK_HEADER(sdk2_glide.h, [
  OPENGLIDE_CFLAGS="$OPENGLIDE_CFLAGS -I/usr/include/openglide"
], [
  AC_MSG_ERROR(Can't find OpenGLide headers)
])
AC_SUBST(OPENGLIDE_LIBS)
AC_SUBST(OPENGLIDE_CFLAGS)

AC_OUTPUT
