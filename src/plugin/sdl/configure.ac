dnl Process this file with autoconf to produce a configure script.
AC_INIT

dnl Check for SDL
SDL_VERSION=2.0.6
AM_PATH_SDL2($SDL_VERSION,,
	    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)

dnl Check for SDL2_ttf
PKG_CHECK_MODULES(SDL2_TTF, [SDL2_ttf], [
  SDL_LIBS+=" $SDL2_TTF_LIBS"
  SDL_CFLAGS+=" $SDL2_TTF_CFLAGS -DHAVE_SDL2_TTF"
], [
  AC_MSG_WARN([SDL2 TTF development package not installed])
])

dnl Check for fontconfig
PKG_CHECK_MODULES(FONTCONFIG, [fontconfig], [
  SDL_LIBS+=" $FONTCONFIG_LIBS"
  SDL_CFLAGS+=" $FONTCONFIG_CFLAGS -DHAVE_FONTCONFIG"
], [
  AC_MSG_WARN([Fontconfig development package not installed])
])

AC_CONFIG_FILES([Makefile.conf])

AC_OUTPUT
