dnl Process this file with autoconf to produce a configure script.
AC_INIT

AM_PATH_GLIB_2_0(2.0.0,, AC_MSG_ERROR([*** glib2 not found!]))
AC_CHECK_HEADER(ladspa.h,, AC_MSG_ERROR([*** ladspa not found!]))

AC_CONFIG_FILES([Makefile.conf])

AC_OUTPUT
