#ifndef __PRIV_H__
#define __PRIV_H__

#include "config.h"

#ifndef i_am_root
  #define i_am_root 1
#endif

extern int under_root_login;

void priv_init(void);
int enter_priv_on(void);
int enter_priv_off(void);
int leave_priv_setting(void);
int priv_iopl(int pl);     /* do iopl() under forced priv_on */
uid_t get_cur_uid(void);   /* get the current active uid */
uid_t get_cur_euid(void);  /* get the current active euid */
gid_t get_cur_egid(void);  /* get the current active egid */
uid_t get_orig_uid(void);  /* get the uid that was present at start of dosemu */
uid_t get_orig_euid(void); /* get the euid that was present at start of dosemu */
gid_t get_orig_gid(void);  /* get the gid that was present at start of dosemu */
int priv_drop(void);


/*
   The 'priv stuff' works as follows:

   1. All settings have to be done in 'pairs' such as

        enter_priv_on();   / * need to have root access for 'do_something' * /
        do_something();
        leave_priv_setting();
      or
        enter_priv_off();  / * need pure user access for 'do_something' * /
        do_something();
        leave_priv_setting();

   2. On enter_priv_XXX() the current state will be saved (pushed) on a
      socalled 'privilege stack' and restored (popped) by leave_priv_setting();
      Hence, you never again have to worry about previous priv settings,
      and whenever you feel you need to switch off or on privs, you can do it
      without coming into trouble.

   3. We now have the system calls (getuid, setreuid, e.t.c) _only_ in
      src/base/misc/priv.c. We cash the setting and don't do unnecessary
      systemcalls. Hence NEVER call 'getuid', 'setreuid' e.t.c yourself,
      instead use the above supplied functions. The only places where I
      broke this 'holy law' myself was when printing the log, showing both
      values (the _real_ and the cashed on).

   4. In case of dosemu was startet out of a root login, we skip 
      _all_ priv-settings. There is a new variable 'under_root_login'
      which is only set when dosemu is started from a root login.

   5. On all places were iopl() is called outside a enter_priv...leave_priv
      bracket, the new priv_iopl() function is to be used in order to
      force privileges.

   6. 'running as user' just needs to do _one_ priv_off at start of dosemu,
      if we don't do it we are 'running as root' ;-)

   -- Hans 970405
*/

#endif /* PRIV_H */

