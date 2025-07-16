dnl Process this file with autoconf to produce a configure script.
AC_INIT

dnl Check for SDL3
PKG_CHECK_MODULES(SDL, [sdl3],, [
	    AC_MSG_ERROR([*** SDL3 not found!])
])

dnl Check for SDL3_ttf
PKG_CHECK_MODULES(SDL_TTF, [sdl3-ttf], [
  SDL_LIBS="$SDL_LIBS $SDL_TTF_LIBS"
  SDL_CFLAGS="$SDL_CFLAGS $SDL_TTF_CFLAGS -DHAVE_SDL_TTF"
], [
  AC_MSG_WARN([SDL3 TTF development package not installed])
])

dnl Check for fontconfig
PKG_CHECK_MODULES(FONTCONFIG, [fontconfig], [
  SDL_LIBS="$SDL_LIBS $FONTCONFIG_LIBS"
  SDL_CFLAGS="$SDL_CFLAGS $FONTCONFIG_CFLAGS -DHAVE_FONTCONFIG"
], [
  AC_MSG_WARN([Fontconfig development package not installed])
])

AC_CONFIG_FILES([Makefile.conf])

AC_OUTPUT
