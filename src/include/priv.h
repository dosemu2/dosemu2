#ifndef __PRIV_H__
#define __PRIV_H__

#include "config.h"

extern int i_am_root;

void priv_init(void);
int internal_priv_on(void);
int internal_priv_off(void);
int priv_drop(void);

/* I've found about 99% of the places in dosemu where it is important
   that you be root.  I have done the important thing though and at
   least made the current usage consistent.  If someone else wants to
   actually make this work with RUN_AS_USER, it would probably benefit
   dosemu in the long run.  EB -- 11 Aug 1996
   The proper formant for using priv_on, priv_off, and priv_default
   is:
   priv_on();       -* If you need to be root to do what you are doing next 
   code();
   priv_default;

   priv_off();      -* If you definentily need to be the user who is running dosemu
   code();
   priv_default();
   */

/* Having a priv_default routine isn't quite as good as a
   the ability to arbitrarily nest which privs are in effect,
   but it is much cleaner then ending arbitrary procedures with priv_on,
   and possibly some better then ending them with priv_off.

   It at least simplifies concepts like RUN_AS_USER

   Please keep priv_on / priv_off statements that are added as few and
   as closely focused as possible so we can be assured that dosemu
   does the right thing.
   */

#ifdef RUN_AS_USER
#define priv_default()  internal_priv_off()
#define priv_on()       internal_priv_on()
#define priv_off()      internal_priv_off()
#else
#define priv_default()  internal_priv_on()
#define priv_on()       internal_priv_on()
#define priv_off()      internal_priv_off()
#endif /* RUN_AS_USER */

#endif /* PRIV_H */

