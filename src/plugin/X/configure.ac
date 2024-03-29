dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_HEADERS([_x_config.h])
AC_CONFIG_FILES([Makefile.conf])

PKG_CHECK_MODULES([X11], [x11],,
    AC_MSG_ERROR([X development libraries not installed]))
AC_MSG_NOTICE(Compiling with X support...)

dnl Check for the MITSHM extention to speed up VGAemu
PKG_CHECK_MODULES([XEXT], [xext],
     [
       AC_DEFINE(HAVE_MITSHM,1)
     ],
     AC_MSG_NOTICE(Compiling without the MITSHM X extension))

AC_ARG_ENABLE(vidmode,
          [  --enable-vidmode        use XF86 vidmode extension])
if test "$enable_vidmode" = "yes" ; then
  PKG_CHECK_MODULES([XVM], [xxf86vm],, enable_vidmode="no")
fi
if test "$enable_vidmode" = "yes"; then
    AC_MSG_NOTICE(Compiling with the XF86 video mode extension)
    AC_DEFINE(HAVE_XVIDMODE,1)
fi

dnl Check for the XKB extension to get reliable keyboard handling
AC_CHECK_HEADER(X11/XKBlib.h, AC_DEFINE(HAVE_XKB,1))

XORG_FONT_BDF_UTILS

AC_OUTPUT

AH_TEMPLATE([HAVE_MITSHM],
[ Define this if you want to use MITSHM ])

AH_TEMPLATE([HAVE_XVIDMODE],
[ Define this if you want to use the XF86 video mode extension ])

AH_TEMPLATE(HAVE_XKB,
[Define this if you have the XKB extension])

AH_TOP([
#ifndef X_CONFIG_H
#define X_CONFIG_H
])

AH_BOTTOM([
#endif /* X_CONFIG_H */
])

sed -E 's/#define PACKAGE_.+//' _x_config.h >x_config.hh
