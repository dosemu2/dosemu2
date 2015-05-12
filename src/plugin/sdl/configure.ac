dnl Process this file with autoconf to produce a configure script.
AC_INIT

dnl Check for SDL
SDL_VERSION=2.0.2
PKG_CHECK_MODULES([SDL], sdl2 >= $SDL_VERSION,,
	    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)

AC_CONFIG_FILES([Makefile.conf])

AC_OUTPUT
