#define HAVE_KEYBOARD_V1 1
#if DOSEMU_VERSION_CODE < VERSION_OF(1,1,1,1)
  #error "Sorry, wrong DOSEMU version for keyboard plugin, please upgrade"
#endif
