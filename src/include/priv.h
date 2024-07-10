#ifndef __PRIV_H__
#define __PRIV_H__


/* set, we either are logged in under root or are suid root
 * Means: we can do stuff that needs root access (iopl, ioperm, e.t.c.)
 * Dos NOT mean, that _have_ that rights already, it needs enter_priv_on
 * anyway.
 */
extern int can_do_root_stuff;

/* set, if dosemu was started from a 'root login',
 * unset, if dosemu was started from a 'user login'
 */
extern int under_root_login;
/* set, if dosemu was started as root via 'sudo'
 */
extern int using_sudo;
/* gives the current i/o privilege level without needing to ask
   the kernel */
extern int current_iopl;

#define PRIV_SAVE_AREA
#define enter_priv_on()		do { real_enter_priv_on()
#define leave_priv_setting()	real_leave_priv_setting(); } while(0)

void priv_init(void);
void priv_drop_total(void);
int real_enter_priv_on(void);
int real_leave_priv_setting(void);
int priv_iopl(int pl);     /* do iopl() under forced priv_on */
uid_t get_orig_uid(void);  /* get the uid that was present at start of dosemu */
gid_t get_orig_gid(void);  /* get the gid that was present at start of dosemu */
uid_t get_suid(void);
gid_t get_sgid(void);
int running_suid_orig(void);
int running_suid_changed(void);
void priv_drop_root(void);
void priv_drop(void);

#endif /* PRIV_H */

