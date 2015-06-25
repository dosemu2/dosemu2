dnl Process this file with autoconf to produce a configure script.
AC_INIT
dnl Check for SDL
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION,,
	    AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
)

AC_CONFIG_FILES([Makefile.conf])

AC_OUTPUT
