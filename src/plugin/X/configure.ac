dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([X.c])
AC_CONFIG_HEADER([include/_x_config.h])
AC_CONFIG_AUX_DIR([../../..])

AC_CONFIG_FILES([Makefile.conf])

AC_CHECK_LIB(X11, XOpenDisplay,,
    AC_MSG_ERROR([X development libraries not installed]))
AC_MSG_NOTICE(Compiling with X support...)
X_SUPPORT="X_SUPPORT=1"
AC_SUBST(X_SUPPORT)

dnl Check for the MITSHM extention to speed up VGAemu
AC_CHECK_LIB(Xext, XShmQueryExtension,, have_mitshm=no)
if test "$have_mitshm" != "no" ; then
  AC_CHECK_HEADER(X11/extensions/XShm.h,
     [AC_DEFINE(HAVE_MITSHM,1)],
     AC_MSG_NOTICE(Compiling without the MITSHM X extension),
     [#include <X11/Xlib.h>])
fi

AC_ARG_ENABLE(vidmode,
          [  --enable-vidmode        use XF86 vidmode extension])
if test "$enable_vidmode" = "yes" ; then
  AC_CHECK_LIB(Xxf86vm, XF86VidModeSwitchToMode,, enable_vidmode="no")
fi
if test "$enable_vidmode" != "yes"; then
    AC_MSG_NOTICE(Compiling without the XF86 video mode extension)
else
    AC_CHECK_HEADER(X11/extensions/xf86vmode.h,
     [AC_DEFINE(HAVE_XVIDMODE,1)],
      AC_MSG_NOTICE(Compiling without the XF86 video mode extension),
     [#include <X11/Xlib.h>])
fi

AC_CHECK_PROG([BDFTOPCF], [bdftopcf], ["yes"], ["no"])
AC_CHECK_PROG([MKFONTDIR], [mkfontdir], ["yes"], ["no"])
if test "$MKFONTDIR" = "no" -o "$BDFTOPCF" = "no"; then
    AC_MSG_ERROR([
Your system doesn't seem to have bdftopcf or mkfontdir available.
Please install these packages or a package named like 'xorg-x11-font-utils'
or 'xfonts-utils' and retry.])
fi

AC_SUBST(LIBS)

AC_OUTPUT

AH_TEMPLATE([HAVE_MITSHM],
[ Define this if you want to use MITSHM ])

AH_TEMPLATE([HAVE_XVIDMODE],
[ Define this if you want to use the XF86 video mode extension ])

AH_TOP([
#ifndef X_CONFIG_H
#define X_CONFIG_H
])

AH_BOTTOM([
#endif /* X_CONFIG_H */
])

sed 's/#define PACKAGE_.\+//' include/_x_config.h >include/x_config.h
