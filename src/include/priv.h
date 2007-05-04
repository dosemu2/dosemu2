/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef __PRIV_H__
#define __PRIV_H__

#include "config.h"

/* set, we either are logged in under root or are suid root
 * Means: we can do stuff that needs root access (iopl, ioperm, e.t.c.)
 * Dos NOT mean, that _have_ that rights already, it needs enter_priv_on
 * anyway.
 */ 
EXTERN int can_do_root_stuff INIT(0);

/* set, if dosemu was started from a 'root login',
 * unset, if dosemu was started from a 'user login'
 */
EXTERN int under_root_login INIT(0);
/* set, if dosemu was started as root via 'sudo'
 */
EXTERN int using_sudo INIT(0);
/* gives the current i/o privilege level without needing to ask
   the kernel */
EXTERN int current_iopl INIT(0);

typedef int saved_priv_status;

#define PRIV_MAGIC 0x56697250	/* "PriV" */
#define PRIV_LOCAL_VAR		OdDnAmE_dont_forget_priv_save_area
#define PRIV_SAVE_AREA		saved_priv_status PRIV_LOCAL_VAR = PRIV_MAGIC;
#define enter_priv_on()		real_enter_priv_on(&PRIV_LOCAL_VAR)
#define enter_priv_off()	real_enter_priv_off(&PRIV_LOCAL_VAR)
#define leave_priv_setting()	real_leave_priv_setting(&PRIV_LOCAL_VAR)

void priv_init(void);
void priv_drop_total(void);
int real_enter_priv_on(saved_priv_status *);
int real_enter_priv_off(saved_priv_status *);
int real_leave_priv_setting(saved_priv_status *);
int priv_iopl(int pl);     /* do iopl() under forced priv_on */
uid_t get_cur_uid(void);   /* get the current active uid */
uid_t get_cur_euid(void);  /* get the current active euid */
gid_t get_cur_egid(void);  /* get the current active egid */
uid_t get_orig_uid(void);  /* get the uid that was present at start of dosemu */
uid_t get_orig_euid(void); /* get the euid that was present at start of dosemu */
gid_t get_orig_gid(void);  /* get the gid that was present at start of dosemu */
int priv_drop(void);
int is_in_groups(gid_t gid);

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
      local variable on the stack and later restored from that on
      leave_priv_setting(). This variable is has to be defined at
      entry of each function (or block), that uses a enter/leave_priv bracket.
      To avoid errors it has to be defined via the macro PRIV_SAVE_AREA.
      The 'stack depth' is just _one_ and is checked to not overflow.
      The enter/leave_priv_* in fact are macros, that pass a pointer to
      the local privs save area to the appropriate 'real_' functions.
      ( this way, we can't forget to include priv.h and PRIV_SAVE_AREA )
      Hence, you never again have to worry about previous priv settings,
      and whenever you feel you need to switch off or on privs, you can do it
      without coming into trouble.

   3. We now have the system calls (getuid, setreuid, e.t.c) _only_ in
      src/base/misc/priv.c. We cash the setting and don't do unnecessary
      systemcalls. Hence NEVER call 'getuid', 'setreuid' e.t.c yourself,
      instead use the above supplied functions. The only places where I
      broke this 'holy law' myself was when printing the log, showing both
      values (the _real_ and the cashed on).

   4. In case dosemu was started out of a root login (or non-suid-root
      out of a user login), we skip _all_ priv-settings.
      There is a new variable 'under_root_login'
      which is only set when dosemu is started from a root login.

   5. On all places were iopl() is called outside a enter_priv...leave_priv
      bracket, the new priv_iopl() function is to be used in order to
      force privileges.

   6. 'running as user' just needs to do _one_ priv_off at start of dosemu,
      if we don't do it we are 'running as root' ;-)

   -- Hans 970405
   -- Updated: Eric Biederman 30 Nov 1997
   -- Updated: Hans 971210
*/

#endif /* PRIV_H */

