dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([sdl.c])
AC_CONFIG_AUX_DIR([../../..])

dnl Check for SDL
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION,
	    CFLAGS="$CFLAGS $SDL_CFLAGS"
	    LIBS="$LIBS $SDL_LIBS"
	    AC_DEFINE(SDL_SUPPORT)
	    SDL_SUPPORT="SDL_SUPPORT=1",
	    AC_MSG_WARN([*** SDL version $SDL_VERSION not found!])
)

AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)
AC_SUBST(SDL_SUPPORT)

AC_CONFIG_FILES([Makefile.conf])

AC_OUTPUT
