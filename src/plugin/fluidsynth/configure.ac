dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
AC_CHECK_HEADER(fluidsynth.h,,
    AC_MSG_ERROR([fluidsynth development headers not found]))
AC_CHECK_LIB(fluidsynth, new_fluid_synth,,
    AC_MSG_ERROR([fluidsynth development libs not found]))
AC_SUBST(LIBS)
AC_OUTPUT
