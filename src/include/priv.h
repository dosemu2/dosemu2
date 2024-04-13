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

typedef int saved_priv_status;

#define PRIV_MAGIC 0x56697250	/* "PriV" */
#define PRIV_LOCAL_VAR		OdDnAmE_dont_forget_priv_save_area
#define PRIV_SAVE_AREA		saved_priv_status PRIV_LOCAL_VAR = PRIV_MAGIC;
#define enter_priv_on()		real_enter_priv_on(&PRIV_LOCAL_VAR)
#define leave_priv_setting()	real_leave_priv_setting(&PRIV_LOCAL_VAR)

void priv_init(void);
void priv_drop_total(void);
int real_enter_priv_on(saved_priv_status *);
int real_leave_priv_setting(saved_priv_status *);
int priv_iopl(int pl);     /* do iopl() under forced priv_on */
uid_t get_orig_uid(void);  /* get the uid that was present at start of dosemu */
gid_t get_orig_gid(void);  /* get the gid that was present at start of dosemu */
int priv_drop(void);

#endif /* PRIV_H */

