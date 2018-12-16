dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
AC_CHECK_TOOL([AS], [i386-elf-as])
if test -z "$AS"; then
  AC_CHECK_TOOL([AS], [as])
fi
if test -z "$AS"; then
  AC_MSG_ERROR(as not found)
fi
AC_CHECK_TOOL([AS_LD], [i386-elf-ld])
if test -z "$AS_LD"; then
  AC_CHECK_TOOL([AS_LD], [ld])
fi
if test -z "$AS_LD"; then
  AC_MSG_ERROR(ld not found)
fi

AC_SUBST(AS)
AC_SUBST(AS_LD)
AC_OUTPUT
