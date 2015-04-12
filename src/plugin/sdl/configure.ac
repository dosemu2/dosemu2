dnl Process this file with autoconf to produce a configure script.
AC_INIT

dnl Check for SDL
SDL_VERSION=2.0.2
AM_PATH_SDL2($SDL_VERSION,
	    [SDL_SUPPORT="SDL_SUPPORT=1"],
	    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)

AC_SUBST(SDL_SUPPORT)

AC_CONFIG_FILES([Makefile.conf])

AC_OUTPUT
