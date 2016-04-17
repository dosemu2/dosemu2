dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_FILES([Makefile.conf])
AC_CHECK_LIB(vga, vga_simple_init,,
    AC_MSG_ERROR([svgalib development libs not found]))
AC_CHECK_HEADER([vga.h],,
    AC_MSG_ERROR([svgalib development headers not found]))
AC_MSG_CHECKING(for svgalib version >= 1.9.21)
svgalib_ver=`printf '#include <vga.h>\nSVGALIB_VER\n' | \
	${CC-cc} -E - | tail -n 1 | cut -dx -f2`
svgalib_ver_a=`expr $svgalib_ver / 10000`
svgalib_ver_b=`expr $svgalib_ver / 100 % 100`
svgalib_ver_c=`expr $svgalib_ver % 100`
if test $svgalib_ver -ge 010921; then
    use_svgalib="yes"
else
    use_svgalib="no"
fi
AC_MSG_RESULT([$use_svgalib, found ${svgalib_ver_a}.${svgalib_ver_b}.${svgalib_ver_c}])
if test "$use_svgalib" = "no"; then
    AC_MSG_ERROR([Your svgalib is too old.])
fi

AC_SUBST(LIBS)
AC_OUTPUT
