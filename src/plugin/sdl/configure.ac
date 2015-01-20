dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([sdl.c])
AC_CONFIG_HEADER([include/_sdl_config.h])
AC_CONFIG_AUX_DIR([../../..])

dnl Check for SDL
SDL_VERSION=2.0.2
AM_PATH_SDL2($SDL_VERSION,
	    [
	    LIBS="$LIBS $SDL_LIBS"
	    CFLAGS="$CFLAGS $SDL_CFLAGS"
	    AC_DEFINE(SDL_SUPPORT)
	    SDL_SUPPORT="SDL_SUPPORT=1"],
	    AC_MSG_WARN([*** SDL version $SDL_VERSION not found!])
)

AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)
AC_SUBST(SDL_SUPPORT)

AC_CONFIG_FILES([Makefile.conf])

AC_OUTPUT

AH_TOP([
#ifndef SDL_CONFIG_H
#define SDL_CONFIG_H
])

AH_TEMPLATE([SDL_SUPPORT],
[ DEFINE this, if you want SDL support ])

AH_BOTTOM([
#endif /* SDL_CONFIG_H */
])

sed 's/#define PACKAGE_.\+//' include/_sdl_config.h >include/sdl_config.h
