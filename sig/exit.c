/*
 *  linux/kernel/exit.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/resource.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>

#undef for_each_task
#define for_each_task(p) \
        for (p = current ; (p = p->next_task) != current ; )

/*
 * kill_pg() sends a signal to a process group: this is what the tty
 * control characters do (^C, ^Z etc)
 */
int kill_pg(int pgrp, int sig, int priv)
{
	struct task_struct *p=NULL;
	int err,retval = -ESRCH;
	int found = 0;

	if (sig<0 || sig>32 || pgrp<=0)
		return -EINVAL;
	for_each_task(p) {
		if (p->pgrp == pgrp) {
			if ((err = send_sig(sig,p,priv)) != 0)
				retval = err;
			else
				found++;
		}
	}
	return(found ? 0 : retval);
}
int kill_proc(int pid, int sig, int priv)
{
 	struct task_struct *p=NULL;

	if (sig<0 || sig>32)
		return -EINVAL;
	if (current->pid == pid) {
		return send_sig(sig,current,priv);
	}
	for_each_task(p) {
		if (p && p->pid == pid) {
			return send_sig(sig,p,priv);
		}
	}
	return(-ESRCH);
}

