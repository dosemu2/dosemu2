/* include/kbd_unicode_config.h.  Generated automatically by configure.  */
/* include/kbd_unicode_config.h.in.  Generated automatically from configure.in by autoheader.  */
#ifndef UNICODE_KEYB_CONFIG_H
#define UNICODE_KEYB_CONFIG_H

#define HAVE_UNICODE_KEYB 2

#if DOSEMU_VERSION_CODE < VERSION_OF(1,1,1,1)
  #error "Sorry, wrong DOSEMU version for keyboard unicode plugin, please upgrade"
#endif


/* Define this if you have the XKB extension */
#define HAVE_XKB 1

#endif /* UNICODE_KEYB_CONFIG_H */
