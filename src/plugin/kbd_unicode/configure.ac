dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([serv_xlat.c])
AC_CONFIG_HEADER(include/kbd_unicode_config.h)

dnl Check for the XKB extension to get reliable keyboard handling
AC_CHECK_HEADER(X11/XKBlib.h, have_xkb="yes", have_xkb="no")
if test "$have_xkb" = "yes" ; then
	AC_DEFINE(HAVE_XKB,1)
fi

AC_OUTPUT

AH_TOP([
#ifndef UNICODE_KEYB_CONFIG_H
#define UNICODE_KEYB_CONFIG_H

#define HAVE_UNICODE_KEYB 2

#if DOSEMU_VERSION_CODE < VERSION_OF(1,1,1,1)
  #error "Sorry, wrong DOSEMU version for keyboard unicode plugin, please upgrade"
#endif
])

AH_TEMPLATE(HAVE_XKB,
[Define this if you have the XKB extension])

AH_BOTTOM([
#endif /* UNICODE_KEYB_CONFIG_H */
])
