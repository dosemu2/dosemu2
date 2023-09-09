dnl Process this file with autoconf to produce a configure script.
AC_INIT

dnl Check for SDL
SDL_MIN_VERSION=2.0.6
PKG_CHECK_MODULES(SDL, [sdl2 >= $SDL_MIN_VERSION],, [
	    AC_MSG_ERROR([*** SDL version $SDL_MIN_VERSION not found!])
])
m4_define(SDL_VERSION, [2$1[]26$1[]0])
m4_define(SDL_VER, [SDL_VERSION(.)])
PKG_CHECK_MODULES(sdl2_[]SDL_VERSION(_), [sdl2 >= SDL_VER],, [
	    AC_MSG_WARN([sdl2 version SDL_VER not found])
])

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
