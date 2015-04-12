dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([serv_xlat.c])
AC_CONFIG_HEADER(include/_kbd_unicode_config.h)

AC_OUTPUT

AH_TOP([
#ifndef UNICODE_KEYB_CONFIG_H
#define UNICODE_KEYB_CONFIG_H

#define HAVE_UNICODE_KEYB 2

#ifdef VERSION_H
#if DOSEMU_VERSION_CODE < VERSION_OF(1,1,1,1)
  #error "Sorry, wrong DOSEMU version for keyboard unicode plugin, please upgrade"
#endif
#endif
])

AH_BOTTOM([
#endif /* UNICODE_KEYB_CONFIG_H */
])

sed 's/#define PACKAGE_.\+//' include/_kbd_unicode_config.h >include/kbd_unicode_config.h
