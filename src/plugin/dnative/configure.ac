dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])

# for dumpmachine
AC_PROG_CC
AC_CHECK_HEADERS([sys/syscall.h],, [AC_MSG_ERROR([no sys/syscall.h])])
CONFIG_HOST=`uname -s`
CONFIG_OS=`uname -o`
machine=`$CC -dumpmachine | cut -d- -f1 | sed 's/i.86/i386/'`
if test "$CONFIG_HOST" != "Linux"; then
  AC_MSG_ERROR([dnative only supported on linux])
fi
if test "$machine" != "i386" -a "$machine" != "x86_64"; then
  AC_MSG_ERROR([dnative only supported on x86])
fi
if test "$CONFIG_OS" = "Android"; then
  AC_MSG_ERROR([dnative not supported on android (no modify_ldt)])
fi
AC_OUTPUT
