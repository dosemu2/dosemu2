/*
 * Lermen's Tiny Thread package, file lt-threads.c
 *
 * (C) 1997 under GPL, Hans Lermen <lermen@fgan.de>
 *
 * This thread package does _NOT_  POSIX compatible threading.
 * As its name says: it is a tiny fast alternative using Linux cloning.
 * The aim was to avoid as much unnecessary sys_calls as possible.
 * Locking is _NOT_ done via IPC semaphores, instead we use a user level
 * atomic technique, when the resource is free, there will be no sys_call atall.
 *
 * Each thread has its own 'Thread Control Block' (TCB) at the bottom of its
 * stack. Hence to identify itself it doesn't need to go over getpid().
 *
 * However, there are restrictions given by the technique used:
 *
 *   - The total maximum number of threads is 27.
 *   - The size of the stack must be at power of 2 and is equal
 *     for all threads. It is not a problem to make the stack area huge,
 *     because only those pages actually used will be allocated (paged in)
 *     by the system. Though you will be estonished what /proc/<pid>/status
 *     is telling you about stack consumption, ... just ignore it;-)
 *   - You have to lock/unlock resource_libc when using non-reentrant parts
 *     of libc (malloc _is_ no-reentrant).
 *   - You must not use atexit, exit, _exit atall. instead use the techniques
 *     and functions supplied by lt-threads.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <linux/unistd.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <setjmp.h>
#include <time.h>


#define LT_THREADS_ITSELF
#include "lt-threads.h"

/* ---------------------for name list stuff */
static int find_deleted_name_list_entry(struct name_list *list);

/* --------------------- for termination stuff */

static void sigterm_handler(int signum);
static void init_sigterm();

/* --------------------- for suspend/resume */

#define SIG_SUSPEND_WAKEUP SIGUSR1

static sigset_t sigmask_suspend;
static sigset_t sigmask_resume;

static void init_suspend();

/* --------------------- for locking */
static void init_resources();

/* --------------------- for messages stuff */

static void init_message_system();
static mbox_handle get_local_mailbox(char *name);
static mbox_handle get_global_mailbox(char *name);
static void sendmessage_local(mbox_handle mbx, struct msg *msg);
static struct msg *receivemessage_local(mbox_handle mbx);

/* --------------------- TCB stuff */

static struct queue_entry tcb_free_pool = {&tcb_free_pool, &tcb_free_pool};
static struct tcb *thread0_tcb=0;
static struct tcb *allocate_tcb_space();
static void free_tcb_space(struct tcb *tcb);
static void thread_cleanup_stuff(void);
static int sys_thread_start(struct tcb *tcb);

/* --------------------- for stack alloc stuff */
#define STACK_TOP		0xc0000000U
#define STACK_BOTTOM		0xa0000000U
#define STACK_TOP_PADDING	  TCB_GRAN

static unsigned int stack_pool_bottom=0;
static char *force_stack_expand(unsigned long address);
static void *alloc_stack_page(int size, int gran);


/* --------------------- stack alloc stuff ------------------ */

static char *force_stack_expand(unsigned long address)
{
	/* The kernel has a feature, such that it doesn't
	 * allow grow down the stack below the current stack pointer.
	 * This makes sense in single threaded applications,
	 * but is bad for threading purposes:
	 *   1.	You are forced to put the stacks into the heap (below 1st giga)
	 *	hence it is less protected against overwriting then normally.
	 *   2. The 1 giga stack address space isn't used any more, and
	 *	you may have problems with address space, on huge programs
	 *	that use huge arrays (data bases, matrices).
	 * We trick out the kernel by expanding the stack vma to a given
	 * value (setting ESP to the bottom and thouching it).
	 * ( look at do_page_fault in arch/i386/mm/fault.c how it
	 *   treats growing down of stack )
	 */
	__asm__ volatile ("
		xchgl	%%esp,%%eax
		movb	$1,(%%esp)
		xchgl	%%esp,%%eax
		": : "a" (address)
	);
	return (char *)address;
}

static void *alloc_stack_page(int size, int gran)
{
	if (!stack_pool_bottom) {
		stack_pool_bottom = STACK_TOP - STACK_TOP_PADDING;
		if ((__ESP-256) < (STACK_TOP - STACK_TOP_PADDING))
			stack_pool_bottom = (__ESP-256-size) & (-gran);
	}
	stack_pool_bottom = (stack_pool_bottom - size) & (-gran);
	if (stack_pool_bottom < STACK_BOTTOM)
		return 0;
	return (void *)force_stack_expand((unsigned long)stack_pool_bottom);
}


/*
 * When running as user, current Linux has a stacklimit of 8Mb.
 * ( no way to change that via bash ulimit )
 * This is enough for normal stacksizes, however, if you need more
 * You need some way to set it to 'unlimited'. This only can be done
 * as root, hence setting the suid bit is needed.
 * The below function sets the limit high, and (in case drop_privs)
 * will drop root priviledges before return.
 */
int make_stack_unlimited(int drop_privs)
{
	struct rlimit rlim;
	uid_t uid = getuid();
	int ret;

	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	ret=setrlimit(RLIMIT_STACK, &rlim);
	if (drop_privs) setreuid(uid,uid);
	return ret;
}

/* ------------------------ queue stuff --------------------- */


/*
 * Note:
 * An 'existing' entry _must_ have valid pointers in 'next', 'last'.
 * We build the head of a queue with an empty 'queue_entry' haveing
 * 'next', 'last' poining to itself.
 */
void append_to_queue(struct queue_entry *existing, struct queue_entry *new)
{
	new->next = existing->next;
	existing->next = new;
	new->last = existing;
	new->next->last = new;
}

void remove_from_queue(struct queue_entry *entry)
{
	entry->last->next = entry->next;
	entry->next->last = entry->last;
}


/* ----------------------- malloc stuff --------------------- */


void *locked_malloc(size_t size)
{
	void *p;
	lock_resource(resource_libc);
	p = malloc(size);
	unlock_resource(resource_libc);
	return p;
}

void locked_free(void *p)
{
	lock_resource(resource_libc);
	free(p);
	unlock_resource(resource_libc);
}

void *page_malloc(size_t size)
{
	/* NOTE:
	 * As of man page, malloc() always returns a value,
	 * which is suitably aligned for any kind of variable,
	 * hence we atleast will be aligned at 8.
	 * So, after _our_ alignment, we atleast have space for
	 * the saved original pointer at p[-1];
	 */
	void **p, *porg;
	size += PAGE_SIZE;
	p = porg =malloc(size);
	p = (void *)((((int)porg) + PAGE_SIZE) & (-PAGE_SIZE));
	p[-1] = porg;
	return p;
}

void page_free(void *ptr)
{
	int *p = ptr;
	free(((void **)p)[-1]);
}



/* ------------------------ name lists ---------------------- */

struct name_list *create_namelist(int numentries)
{
	struct name_list *p;

	numentries = (numentries + 31) & ~31; /* aligned up */
	p = locked_malloc(sizeof(struct name_list) + numentries*sizeof(struct name_list_entry));
	if (!p) return 0;
	p->size = numentries;
	p->count = 0;
	return p;
}

int lookup_name_list(struct name_list *list, char *name)
{
	int i;
	for (i=0; i< list->count; i++) {
		if ( list->list[i].name
		     && !strcmp(name, list->list[i].name)) return i;
	}
	return -1;
}

void * get_name_list_value(struct name_list *list, char *name)
{
	int i;
	if ((i=lookup_name_list(list, name))>=0) return list->list[i].u.p;
	return 0;
}

int set_name_list_value(struct name_list *list, char *name, void *value)
{
	int i;
	if ((i=lookup_name_list(list, name)) <0) return -1;
	list->list[i].u.p = value;
	return i;
}

static int find_deleted_name_list_entry(struct name_list *list)
{
	int i;
	for (i=0; i < list->count; i++)
		if (!list->list[i].name) return i;
	return -1;
}

int insert_name_list_entry(struct name_list *list, char *name, void *value)
{
	struct name_list_entry *p;
	int i;

	lock_resource(resource_sys);
	if ((i=lookup_name_list(list, name)) < 0) {
		if (list->count >= list->size) {
			i = find_deleted_name_list_entry(list);
			if (i<0) {
				unlock_resource(resource_sys);
				return -1;
			}
		}
		else i = list->count++;
	}
	p = &list->list[i];
	p->name = name;
	p->u.p = value;
	unlock_resource(resource_sys);
	return i;
}

int delete_name_list_entry(struct name_list *list, char *name)
{
	int i;

	if ((i=lookup_name_list(list, name)) < 0) return -1;
	list->list[i].name = 0;
	return i;
}

/* ------------------------ termination stuff ---------------- */
#if 0
/* just an example how to setup a sigchild handler */
void sigchild_handler(int signum)
{
	int pid;
	int status;
	while (1) {
		pid = waitpid(WAIT_ANY, &status, WNOHANG);
		if (pid <= 0) break;
		/* notify_termination(pid, status) */
		??????
	}
}
#endif

static void sigterm_handler(int signum)
{
	struct tcb *tcb = OWN_TCB;

	if (!atomic_bitset(&tcb->threadflags, TCB_F_TERMREQUEST_BIT)) {
		longjmp(tcb->exit_jmpbuf,1);
	}
	/* else it was set before and we have nothing to do */
}

static void init_sigterm()
{
	struct sigaction new;

	new.sa_handler = sigterm_handler;
	sigfillset(&new.sa_mask);	/* block all SIGs while in the handler */
	new.sa_flags = 0;
	new.sa_restorer = 0;
	sigaction(SIGTERM, &new, 0);
}


void exit_thread(struct tcb *tcb)
{
	if (!tcb)  tcb = OWN_TCB;
	if (tcb->threadflags & TCB_F_TERMREQUEST) {
		if (tcb == OWN_TCB) {
			/* well, this should not happen :-(
			 * we do a _real_ exit, because longjmp would lead
			 * to a loop
		 	*/
			_exit(99);
		}
		return;
	}
	if (tcb == OWN_TCB) {
		atomic_bitset(&tcb->threadflags, TCB_F_TERMREQUEST_BIT);
		longjmp(tcb->exit_jmpbuf,1);
	}
	kill(tcb->pid, SIGTERM);
}


void exit_all(int exit_code)
{
	if (!thread0_tcb) exit(exit_code);
	thread0_tcb->exit_code = exit_code;
	exit_thread(thread0_tcb);
	exit_thread(0);
}

/* -------------------------- suspend/resume ----------------- */


static void suspend_handler(int signum)
{
	struct tcb *tcb = OWN_TCB;
	atomic_inc(&tcb->suspend_count, 1);
}

/* The below need to be call at startup code of _every_ thread
 * It sets the signal handler for awake from a suspend.
 * Suspend is used a lot in conjunction with locking of resources.
 */
static void init_suspend()
{
	struct sigaction new;

	OWN_TCB->suspend_count = 0;
	new.sa_handler = suspend_handler;
	sigfillset(&new.sa_mask);	/* block all SIGs while in the handler */
	new.sa_flags = 0;
	new.sa_restorer = 0;
	sigemptyset(&sigmask_suspend); sigaddset(&sigmask_suspend, SIG_SUSPEND_WAKEUP);
	sigfillset(&sigmask_resume);
	sigdelset(&sigmask_resume, SIG_SUSPEND_WAKEUP);
	sigdelset(&sigmask_resume, SIGTERM);
					/* block SIG_SUSPEND_WAKEUP */
	sigprocmask(SIG_BLOCK, &sigmask_suspend, 0);
					/* set the suspend handler */
	sigaction(SIG_SUSPEND_WAKEUP, &new, 0);
}

void suspend_thread()
{
	struct tcb *owntcb = OWN_TCB;
	while (!owntcb->suspend_count) sigsuspend(&sigmask_resume);
	atomic_dec(&owntcb->suspend_count, 1);
}

void resume_thread(struct tcb *tcb)
{
	kill(tcb->pid, SIG_SUSPEND_WAKEUP);
}


/* -------------------------- locking ------------------------ */

/*
 * The below locking scheme implements userspace semaphores,
 * that in the most frequent cases do _not_ enter the kernel.
 * ( doesn't use IPC or kernel semphores )
 * It depends on the atomic_reserv/free() algorithme defined above
 * and won't work with other locking strategies, that cannot
 * 'reserve and queue' with _one_ atomic operation.
 *
 * The win of this algorithme is _much_ more speed,
 * the disadvantage is that you can't have more then 27 threads.
 * (though, on 64-bit machines it could be 58 threads)
 */

static void init_resources()
{
	/* This must be called _before_ use of locking.
	 */
	resource_list = create_namelist(MAX_RESOURCES);
	resource_sys = create_resource("sys");
	resource_libc = create_resource("libc");
}


struct lock_struct *create_resource(char *name)
{
	int i = insert_name_list_entry(resource_list, name, 0);
	struct lock_struct *sem;

	if (i <0) return 0;
	sem = RESOURCES(i) = locked_malloc(sizeof(struct lock_struct));
	sem->successor_id = sem->used = -1;
	sem->owner = 0;
	sem->owner_count =0;
	sem->id = i;
	set_name_list_value(resource_list, name, sem);
	return sem;
}

void lock_resource(struct lock_struct *sem)
{
	struct tcb *owntcb = OWN_TCB;

	if (owntcb->threadflags & TCB_F_STARTUP)
		/* we are in the startphase, locking not yet working */
		 return;
	if (sem->owner == owntcb) {
		sem->owner_count++;
		return;
	}
	if (atomic_reserv(&sem->used, owntcb->tcb_id)) {
		atomic_bitset(&owntcb->owning_locks,sem->id);
		sem->owner = owntcb;
		sem->owner_count = 0;
		return;
	}

	/* we didn't get the resource, but we queued ourself with the
	 * above 'atomic_reserv', hence we just need to supend ourself
	 * and will get awaken by the semaphore owning process
	 */
	suspend_thread();
	/* when we are coming here, we own the resource */
}

void unlock_resource(struct lock_struct *sem)
{
	struct tcb *owntcb = OWN_TCB;
	struct tcb *othertcb;
	int otherid;
	int succid;

	if (owntcb->threadflags & TCB_F_STARTUP)
		/* we are in the startphase, locking not yet working */
		 return;

	if (sem->owner != owntcb) return;
	if (sem->owner_count--) return; 

	sem->owner = 0;
	atomic_bitclear(&owntcb->owning_locks,sem->id);

	succid = sem->successor_id;
	if (succid >=0) {
		/* We have to transfer the resource to a given successor.
		 * We assume that the successor task did _not_ try to lock
		 * the resource, hence _we_ now do do it for it.
		 */
		sem->successor_id = -1;
		if (get_tcb_from_id(succid))
			 atomic_reserv(&sem->used, succid);
	}

	if (atomic_free(&sem->used, owntcb->tcb_id)) {
		/* there's nobody else waiting in the queue
		 * and the above atomic_free did remove ourself from
		 * the queue, hence nothing else to do
		 */
		return;
	}

	/* there is atleast _one_ thread waiting for the resource.
	 * We need to awake it. There will be no race condition even
	 * if this thread may not have reached his suspend state,
	 * because we have SIG_SUSPEND_WAKEUP always blocked outside of
	 * of sigsuspend().
	 *
	 * We now extract the highest waiting priority tcb_id
	 * (or the one that was given as successor) and
	 * just awake that thread. The tcb_id _remains_ in the semaphore
	 * until the awoken process removes it itself.
	 * We also have to take care about deleted threads as well.
	 */
	if (succid < 0)
		otherid = get_lowest_waiting_id_from_resource(sem->used);
	else	otherid = succid;

	while (otherid >=0) {
		othertcb = get_tcb_from_id(otherid);
		if (othertcb) {
			/* There is a possible (but not very likely) race
			 * here, when the tread after the get_tcb_from_id()
			 * above got deleted.
			 * We have to avoid that on 'exit_thread' side.
			 */
			sem->owner = othertcb;
			sem->owner_count = 0;
			resume_thread(othertcb);
			return;
		}
		else {
			/* hmm..., we come into trouble here.
			 * Maybe the waiting tread just gots deleted,
			 * so we remove it from the queue and try the next one
		 	 */
			if (atomic_free(&sem->used, otherid)) {
				/* nothing else to do */
				return;
			}
		}
		otherid = get_lowest_waiting_id_from_resource(sem->used);
	}
}

void transfer_resource(struct lock_struct *sem, int successor_id)
{
	if (sem->owner != OWN_TCB) return;
	/* Note: only the owner of a resource may dedicate the ownership
	 * to another thread. And because transfer_resource() is called
	 * only by the owner, the below is save.
	 */
	sem->successor_id = successor_id;
	unlock_resource(sem);
}


/* ------------------------ end locking ------------------------*/

/* --------------------- timed sleep stuff --------------------*/

/*
 * Note: we use our own usleep to avoid
 *  A) problems with signal stuff
 *  B) problems with libc
 */

static inline int libless_nanosleep(const struct timespec *req, struct timespec *rem)
{
	int __res;
	__asm__ __volatile__("int $0x80\n"
	:"=a" (__res):"a" ((int)__NR_nanosleep), "b" ((int)req), "c" ((int)rem) );
	return __res;
}

void thread_usleep(int useconds)
{
	struct timespec req, rem;
	req.tv_sec = useconds / 1000000;
	req.tv_nsec = (useconds % 1000000) * 1000;
	while (libless_nanosleep(&req, &rem)) {
		req = rem;
	}
}

/* ----------------------- messages stuff ----------------------*/


static void init_message_system()
{
	/* This must be called _before_ use of messages.
	 */
	mbox_list = create_namelist(MAX_MAILBOXES);
	resource_mbox = create_resource("mbox");
	create_local_mailbox("dummy",1); /* to avoid ZERO-id */
}


mbox_handle create_local_mailbox(char *name, int numentries)
{
	int i;
	struct mbox *mbx;

	lock_resource(resource_mbox);
	i = insert_name_list_entry(mbox_list, name, 0);
	if (i <0) {
		unlock_resource(resource_mbox);
		return 0;
	}
	mbx = MAILBOXES(i) = locked_malloc(sizeof(struct mbox) + (numentries * sizeof(void *)));
	memset(mbx, 0, sizeof(struct mbox));
	mbx->msg_max = numentries;
	mbx->sendqueue = mbx->receivequeue = -1;
	mbx->mbhs.s.local_id = i;
	set_name_list_value(mbox_list, name, mbx);
	unlock_resource(resource_mbox);
	return MBOX_HANDLE_OF(mbx);
}

static mbox_handle get_local_mailbox(char *name)
{
	int i;

	lock_resource(resource_mbox);
	i = lookup_name_list(mbox_list, name);
	if (i <0) {
		unlock_resource(resource_mbox);
		return 0;
	}
	unlock_resource(resource_mbox);
	return MBOX_HANDLE_OF(MAILBOXES(i));
}

static mbox_handle get_global_mailbox(char *name)
{
	/* not implemented in this version */
	return 0;
}

mbox_handle get_mailbox(char *name)
{
	mbox_handle mbh;
	mbh = get_local_mailbox(name);
	if (mbh) return mbh;
	return get_global_mailbox(name);
}

mbox_handle get_mailbox_wait(char *name)
{
	mbox_handle mbh;
	int poll = 1;

	while (1) {
		mbh = get_mailbox(name);
		if (mbh) return mbh;
		/* Now, the mailbox isn't yet available.
		 * As this is maybe a long period wait, we simply
		 * do 'poll and wait' at a decreasing pool rate.
		 */
		thread_usleep(poll * DECI_SECOND);
		if (poll < 16) poll <<=1;
	}
}


static void sendmessage_local(mbox_handle mbx, struct msg *msg)
{
	struct mbox *mbox = MBOX_LOCALPTR_OF_HANDLE(mbx);
	struct tcb *tcb = OWN_TCB;
	int recid, old_wptr;

	lock_resource(resource_mbox);
	/* first we queue the message */
	old_wptr = MBOX_INC_QUEUE_PTR(mbox, wptr);
	if (mbox->wptr ==  mbox->rptr) {
		/* mbox queue is full, have to sleep as sender,
		 * but before that we have to restore the old wptr.
		 */
		mbox->wptr = old_wptr;
		atomic_reserv(&mbox->sendqueue, tcb->tcb_id);
		unlock_resource(resource_mbox);
		suspend_thread();
		atomic_free(&mbox->sendqueue, tcb->tcb_id);
		/* Ok, we got awoken by a receiving thread
		 * (which made place on the queue),
		 * and that one passed the resource_mbox lock
		 * to us. Hence we now can queue the message.
		 */
		old_wptr = MBOX_INC_QUEUE_PTR(mbox, wptr);
	}
	/* Ok, we have room and can queue */
	mbox->msgqueue[old_wptr] = msg;

	/* now look if a thread is waiting on the mbox */
	recid = get_lowest_waiting_id_from_resource(mbox->receivequeue);
	if (recid != -1) {
		/* there is atleast one thread waiting,
		 * we pass the mbox-lock to it. The receiver will
		 * clear its bit itself.
		 */
		transfer_resource(resource_mbox, recid);
	}
	else
		unlock_resource(resource_mbox);
}

static struct msg *receivemessage_local(mbox_handle mbx)
{
	struct mbox *mbox = MBOX_LOCALPTR_OF_HANDLE(mbx);
	struct tcb *tcb = OWN_TCB;
	struct msg *msg = 0;
	int sendid;

	lock_resource(resource_mbox);
	/* first look if a message is queued */
	if (mbox->wptr ==  mbox->rptr) {
		/* mbox queue is empty, have to sleep as receiver
		 */
		atomic_reserv(&mbox->receivequeue, tcb->tcb_id);
		unlock_resource(resource_mbox);
		suspend_thread();
		atomic_free(&mbox->receivequeue, tcb->tcb_id);
		/* Ok, we got awoken by a sending thread
		 * (which put a message on the queue),
		 * and that one passed the resource_mbox lock
		 * to us. Hence we now can unqueue the message.
		 */
	}
	/* Ok, we have a message and can dequeue it */
	msg = mbox->msgqueue[MBOX_INC_QUEUE_PTR(mbox, rptr)];

	/* now look if a thread is waiting on the mbox */
	sendid = get_lowest_waiting_id_from_resource(mbox->sendqueue);
	if (sendid != -1) {
		/* there is atleast one thread waiting,
		 * we pass the mbox-lock to it. The sender will
		 * clear its bit itself.
		 */
		transfer_resource(resource_mbox, sendid);
	}
	else
		unlock_resource(resource_mbox);
	return msg;
}

static inline int mailbox_is_empty_local(mbox_handle mbx)
{
	struct mbox *mbox = MBOX_LOCALPTR_OF_HANDLE(mbx);
	int ret;

	lock_resource(resource_mbox);
	ret = ((mbox->wptr == mbox->rptr) && (mbox->sendqueue == -1));
	unlock_resource(resource_mbox);
	return ret;
}


void sendmessage(mbox_handle mbx, struct msg *msg)
{
	if (!MBOX_IS_LOCAL(mbx)) {
		/* not implemented in this version */
		exit_all(99);
	}
	sendmessage_local(mbx, msg);
}

struct msg *receivemessage(mbox_handle mbx)
{
	if (!MBOX_IS_LOCAL(mbx)) {
		/* not implemented in this version */
		exit_all(99);
	}
	return receivemessage_local(mbx);
}

int mailbox_is_empty(mbox_handle mbx)
{
	if (!MBOX_IS_LOCAL(mbx)) {
		/* not implemented in this version */
		exit_all(99);
	}
	return mailbox_is_empty_local(mbx);
}

/* -------------------- tcb and thread stuff -------------------*/


static struct tcb *allocate_tcb_space()
{
	struct tcb *tcb;

	lock_resource(resource_sys);
	if (QUEUE_IS_EMPTY(&tcb_free_pool)) {
		/* we allocate new space on the stack */
		tcb = alloc_stack_page(TCB_GRAN, TCB_GRAN);
	}
	else {
		/* we get space from the pool */
		tcb = (struct tcb *)tcb_free_pool.next;
		remove_from_queue(tcb_free_pool.next);
	}
	unlock_resource(resource_sys);
	memset(tcb, 0, sizeof(struct tcb));
	tcb->stack_size = TCB_GRAN;
	return tcb;
}

static void free_tcb_space(struct tcb *tcb)
{
	lock_resource(resource_sys);
	page_free(tcb);
	append_to_queue(tcb_free_pool.last, &tcb->link);
	unlock_resource(resource_sys);
}

/*
 * NOTE: this must _not_ be called other then from thread_stub_function()
 *	 and from init_zero_thread()
 */
static void thread_cleanup_stuff(void)
{
	struct tcb *tcb = OWN_TCB;
	int i;

	/* first free any locks we may own */
	while((i=search_lowest_bit(&tcb->owning_locks, MAX_RESOURCES)) >=0) {
		unlock_resource(RESOURCES(i));
	}

	/* call any registered exit_functions  */
	while (tcb->exit_func) (*(tcb->exit_func))();
}

static void thread_stub_function(void)
{
	struct tcb *tcb = OWN_TCB;

	/* set an emergence address, in case we terminate early */
	if (setjmp(tcb->exit_jmpbuf)) {
		tcb->exit_code = 99;
		return;
	}

	init_sigterm();
	init_suspend();
	/* waiting for parent to complete tcb */
	suspend_thread();
	/* from know on the thread is fully functionaly */

	if (!setjmp(tcb->exit_jmpbuf)) (*(tcb->thread_code))(tcb->params);

	/* we come here on normal return from the thread function
	 * _and_ from tread_exit() (via longjmp)
	 */

	thread_cleanup_stuff();
	/* ... and return, the clone asm wrapper will call exit() */
}

struct tcb *create_thread(thread_function_type *thread_code, void *params)
{
	struct tcb *tcb = 0;
	int i;
	pid_t pid;

	lock_resource(resource_sys);
	for (i=0; i < MAX_THREADS; i++) if (!thread_list[i]) break;
	if (!thread_list[i]) {
		/*
		 * tell the other processes: slot taken, keep away ...
		 */
		thread_list[i] = (void *)-1;
		/*
		 * ... and free the system resources,
		 * a later write to thread_list[i] will be atomic.
		 */
		unlock_resource(resource_sys);
		tcb = allocate_tcb_space();
		tcb->tcb_id = i;
		tcb->thread_code = thread_code;
		tcb->params = params;
		pid = sys_thread_start(tcb);
		if (pid < 0) {
			free_tcb_space(tcb);
			thread_list[i] = tcb = 0;
		}
		else {
			tcb->pid = pid;
			thread_list[i] = tcb;
			resume_thread(tcb);
		}
	}
	else unlock_resource(resource_sys);
	return tcb;
}

static void thread0_exit_stuff(void)
{
	struct tcb *tcb = OWN_TCB;
	struct tcb *othertcb;

	int id, status;
	/* first we do what all other threads do on exit */
	thread_cleanup_stuff();
	/* we have to kill also all our children */
	FOR_ALL_TCB(id, othertcb) {
		if (othertcb != tcb) {
			exit_thread(othertcb);
		}
	}
	/* now wait for all children to exit */
	while (waitpid(WAIT_ANY, &status, 0) >0);
	/* ... and exit ourself.
	 * We can salvely call exit(), because no child will
	 * will be hanging in libc ;-)
	 */
	exit(tcb->exit_code);
}

/*
 * "init_zero_thread()" makes the starting parent process behave
 * as a normal thread. All tcbs of the children will be
 * below our current stack, hence we don't wast heap size
 * and have the stack in the area were they belong too.
 */
struct tcb *init_zero_thread(int stacksize)
{
	struct tcb *tcb;

	if (stacksize) {
		tcb_gran = roundup_to_power_of_2(stacksize);
		tcb_gran_mask = ~(tcb_gran - 1);
	}

	/* first we have to make our stack a TCB, we leave the esp untouched
	 * but force the bottom to TCB_GRAN.
	 * NOTE: we need being called by the main process with esp
	 * beeing within our limits
	 */
	if ((__ESP-0x1000) < (STACK_TOP - TCB_GRAN) ) return 0;
	stack_pool_bottom = STACK_TOP;
	tcb = alloc_stack_page(TCB_GRAN, TCB_GRAN);
	if (!tcb) return 0;

	/* Ok, now we have our own TCB, we initialize it */
	memset(tcb, 0, sizeof(struct tcb));
	tcb->tcb_id = 0;	/* parent of parent is always 0 */
	tcb->stack_size = TCB_GRAN;
	tcb->thread_code = 0;	/* parent of parent has no thread function */
	tcb->pid = getpid();
	thread_list[0] = tcb;

	/* disbale locking during startup */
	tcb->threadflags |= TCB_F_STARTUP;

	/* now init the divers  thread functionality */
	init_resources();
	init_suspend();
	/* now we enable locking for thread0 */
	tcb->threadflags &= ~TCB_F_STARTUP;
	init_message_system();
	thread0_tcb = tcb;

	if (setjmp(tcb->exit_jmpbuf)) {
		/* here we come on thread exit of thread0
		 * because optimized GCC code messes up with local
		 * (register) variables after longjmp,
		 * we call a separate function,
		 * that reestablishs proper local variables
		 */
		 thread0_exit_stuff();
		 /* never comes here */
	}
	init_sigterm();
	return tcb;
}

static int sys_thread_start(struct tcb *tcb)
{
	long retval;
	unsigned long *newstack = (unsigned long *)(((char *)tcb) + tcb->stack_size);

#if 0	/* this pushes a param for thread_stub_function, but we don't need */
	*--newstack = (unsigned long)tcb;
#endif

	/* (NOTE: the below is the original comment from Linus )
	 *
	 * Do clone() system call. We need to do the low-level stuff
	 * entirely in assembly as we're returning with a different
	 * stack in the child process and we couldn't otherwise guarantee
	 * that the program doesn't use the old stack incorrectly.
	 *
	 * Parameters to clone() system call:
	 *	%eax - __NR_clone, clone system call number
	 *	%ebx - clone_flags, bitmap of cloned data
	 *	%ecx - new stack pointer for cloned child
	 *
	 * In this example %ebx is CLONE_VM | CLONE_FS | CLONE_FILES |
	 * CLONE_SIGHAND which shares as much as possible between parent
	 * and child. (We or in the signal to be sent on child termination
	 * into clone_flags: SIGCHLD makes the cloned process work like
	 * a "normal" unix child process)
	 *
	 * The clone() system call returns (in %eax) the pid of the newly
	 * cloned process to the parent, and 0 to the cloned process. If
	 * an error occurs, the return value will be the negative errno.
	 *
	 * In the child process, we will do a "jsr" to the requested function
	 * and then do a "exit()" system call which will terminate the child.
	 */
	__asm__ __volatile__(
		"int $0x80\n\t"		/* Linux/i386 system call */
		"testl %0,%0\n\t"	/* check return value */
		"jne 1f\n\t"		/* jump if parent */
		"call *%3\n\t"		/* start subthread function */
		"movl %2,%0\n\t"
		"int $0x80\n"		/* exit system call: exit subthread */
		"1:\t"
		:"=a" (retval)
		:"0" (__NR_clone),"i" (__NR_exit),
		 "r" ((int)thread_stub_function),
		 "b" (CLONE_VM | CLONE_FS | CLONE_FILES | /*CLONE_SIGHAND |*/ SIGCHLD),
		 "c" (newstack));

	if (retval < 0) {
		errno = -retval;
		retval = -1;
	}
	return retval;
}

/* -------------------- debugging aids -------------------*/

int locked_printf( const char *fmt, ...)
{
	va_list args;
	int ret;
	va_start(args, fmt);
	lock_resource(resource_libc);
	ret = vprintf(fmt, args);
	va_end(args);
	fflush(stdout);
	unlock_resource(resource_libc);
	return ret;
}


void print_tcb(struct tcb *tcb)
{
	char lck[MAX_RESOURCES>>3];
	int i;

	if (!do_print_tcb) return;
	if (!tcb) tcb = OWN_TCB;
	lock_resource(resource_libc);
	printf(
	  "thread % 2d, pid % 6d, tcb at %p, tcbsize %08lx\n"
	  "  flags %08x, parent %p, code %p, params %p\n"
	  "  suspend_count % 2d, owning locks: %08lx",
	  tcb->tcb_id, tcb->pid, tcb, tcb->stack_size,
	  tcb->threadflags, tcb->parent, tcb->thread_code, tcb->params,
	  tcb->suspend_count,*(long *)tcb->owning_locks
	);
	memcpy(lck,tcb->owning_locks,MAX_RESOURCES>>3);
	while((i=search_lowest_bit(&lck, MAX_RESOURCES)) >=0) {
		printf(" %s", resource_list->list[i].name);
		atomic_bitclear(&lck,i);
	}
	printf("\n");
	fflush(stdout);
	unlock_resource(resource_libc);
}

void print_resource(struct lock_struct *sem)
{
	if (!do_print_r) return;
	lock_resource(resource_libc);
	printf(
	  "resource: %s, id %d, at %p, used %08x\n",
	  resource_list->list[sem->id].name, sem->id, sem, sem->used
	);
	if (sem->owner) {
	  	printf("  owned by thread %d count %d\n",
	  		sem->owner->tcb_id, sem->owner_count);
	}
	else {
		if (sem->used != -1) {
			printf(" in transition phase to thread %d\n",
				sem->owner_count);
		}
		else {
			printf("  not owned\n");
		}
	}
	unlock_resource(resource_libc);
}

void print_mbox(mbox_handle mbx)
{
	struct mbox *mbox;
	int id = MBOX_ID_OF_HANDLE(mbx);

	lock_resource(resource_libc);
	if (!MBOX_IS_LOCAL(mbx)) {
		printf("Global mailbox, netpart=0x%lx, local_id=0x%x\n",
			MBOX_NETPART_OF_HANDLE(mbx), id);
	}
	else {
		struct mbox aux;
		mbox = MBOX_LOCALPTR_OF_HANDLE(mbx);
		memcpy(&aux, mbox, sizeof(struct mbox));
		printf(
		  "Local mailbox %s id %d\n"
		  "Queue-length=%d, wptr=%d, rptr=%d\n"
		  "sendqueue=%08x, receivequeue=%08x\n",
		  mbox_list->list[id].name, id,
		  mbox->msg_max, mbox->wptr, mbox->rptr,
		  mbox->sendqueue, mbox->receivequeue
		);
	}
	unlock_resource(resource_libc);
}
