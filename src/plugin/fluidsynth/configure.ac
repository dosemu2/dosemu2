dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
PKG_CHECK_MODULES([FLUS], [fluidsynth],,
    AC_MSG_ERROR([fluidsynth development libs not found]))
dnl need to see if new_fluid_midi_parser() is exported - it is private
dnl in some versions of fluidsynth
#AC_CHECK_LIB([fluidsynth], new_fluid_midi_parser,,
#    AC_MSG_ERROR([fluidsynth is found but new_fluid_midi_parser() \
#is not exported]))
AC_SUBST(FLUS_CFLAGS)
AC_SUBST(FLUS_LIBS)
AC_OUTPUT
