dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
AC_CHECK_LIB(mt32emu, mt32emu_create_context,,
    AC_MSG_ERROR([munt development libs not found]))
AC_CHECK_HEADER([mt32emu/c_interface/c_interface.h],,
    AC_MSG_ERROR([munt development headers not found]))

AC_SUBST(LIBS)
AC_OUTPUT
