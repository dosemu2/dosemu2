/*
 * Lermen's Tiny Thread package, file lt-threads.h
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
 
#ifndef LT_THREADS_H
#define LT_THREADS_H

#include <sys/types.h>
#include <setjmp.h>


#define LT_THREADS_VERSION	0x000201	/* 0.2.1 */

#define MAX_THREADS	27	/* NOTE: don't make it > 27
				 * atomic_reserve/free rely on that
				 */

/* --------------------- for clone stuff */
#include <linux/version.h>
#if LINUX_VERSION_CODE < 0x020100
 /* Note: Linux-2.1.x don't like that :-( */
 #include <linux/sched.h>
#else
 #define CSIGNAL         0x000000ff      /* signal mask to be sent at exit */
 #define CLONE_VM        0x00000100      /* set if VM shared between processes */
 #define CLONE_FS        0x00000200      /* set if fs info shared between processes */
 #define CLONE_FILES     0x00000400      /* set if open files shared between processes */
 #define CLONE_SIGHAND   0x00000800      /* set if signal handlers shared */
#endif


/* --------------------- for stack alloc stuff */

int make_stack_unlimited(int drop_privs);

/* --------------------- for queue stuff */

#define QUEUE_IS_EMPTY(head) \
( ((struct queue_entry *)(head)) == ((struct queue_entry *)(head))->next )

struct queue_entry {
	struct queue_entry *next;
	struct queue_entry *last;
};

void append_to_queue(struct queue_entry *existing, struct queue_entry *new);
void remove_from_queue(struct queue_entry *entry);


/* --------------------- for malloc stuff */

#ifndef PAGE_SIZE
  #define PAGE_SIZE 0x1000
#endif

void *locked_malloc(size_t size);
void locked_free(void *p);
void *page_malloc(size_t size);
void page_free(void *ptr);


/* ---------------------for name list stuff */

struct name_list_entry {
	char *name;
	union {
		void *p;
		struct tcb *tcb;
		struct lock_struct *sem;
		struct mbox *mbox;
		int idata;
	} u;
};

struct name_list {
	int size;
	int count;
	struct name_list_entry list[0];
};

struct name_list *create_namelist(int numentries);
int lookup_name_list(struct name_list *list, char *name);
void * get_name_list_value(struct name_list *list, char *name);
int set_name_list_value(struct name_list *list, char *name, void *value);
int insert_name_list_entry(struct name_list *list, char *name, void *value);

/* --------------------- for termination stuff */

void exit_thread(struct tcb *tcb);
void exit_all(int exit_code);

/* --------------------- for suspend/resume */

#define SIG_SUSPEND_WAKEUP SIGUSR1

void suspend_thread();
void resume_thread(struct tcb *tcb);

/* --------------------- for locking */

#define RESOURCES(x) (resource_list->list[x].u.sem)
#define MAX_RESOURCES 32  /* may be a multiple of 32 */

struct lock_struct {
	int used;
	int id;
	int owner_count;
	struct tcb *owner;
	int successor_id;
};

#ifdef LT_THREADS_ITSELF
  struct name_list *resource_list = 0;
  struct lock_struct *resource_libc = 0;
  struct lock_struct *resource_sys = 0;
  struct lock_struct *resource_mbox = 0;
  struct name_list *mbox_list = 0;
#else
  extern struct name_list *resource_list;
  extern struct lock_struct *resource_libc;
  extern struct lock_struct *resource_sys;
  extern struct lock_struct *resource_mbox;
  extern struct name_list *mbox_list;
#endif

struct lock_struct *create_resource(char *name);
void lock_resource(struct lock_struct *sem);
void unlock_resource(struct lock_struct *sem);
void transfer_resource(struct lock_struct *sem, int successor_id);

/* --------------------- for timed sleep stuff */

#define ONE_SECOND	1000000
#define DECI_SECOND	 100000
#define MILLI_SECOND	   1000

void thread_usleep(int useconds);

/* --------------------- for messages stuff */

#define MAILBOXES(x) (mbox_list->list[x].u.mbox)
#define MAX_MAILBOXES 32  /* may be a multiple of 32 */

#define MBOX_ID_OF_HANDLE(mbh) (((union mbox_handle_s)mbh).s.local_id)
#define MBOX_NETPART_OF_HANDLE(mbh) (((union mbox_handle_s)mbh).s.netpart)
#define MBOX_IS_LOCAL(mbh) (MBOX_NETPART_OF_HANDLE(mbh) == 0)
#define MBOX_LOCALPTR_OF_HANDLE(mbh) MAILBOXES(MBOX_ID_OF_HANDLE(mbh))
#define MBOX_INC_QUEUE_PTR(mbx,ptr) ({\
	int ret = mbx->ptr++; \
	if (mbx->ptr >= mbx->msg_max) mbx->ptr = 0; \
	ret; })
#define MBOX_HANDLE_OF(mbx) (mbx->mbhs.handle)

typedef unsigned long long mbox_handle;

struct msg {
	unsigned long size;
	char content[0];
};

union mbox_handle_s {
	struct {
		long local_id;
		long netpart;
	} s;
	mbox_handle handle;
};

struct mbox {
	union mbox_handle_s mbhs;
	int msg_max;
	int wptr;
	int rptr;
	int sendqueue;
	int receivequeue;
	struct msg *msgqueue[0];
};


mbox_handle create_local_mailbox(char *name, int numentries);
mbox_handle get_mailbox(char *name);
mbox_handle get_mailbox_wait(char *name);
void sendmessage(mbox_handle mbx, struct msg *msg);
struct msg *receivemessage(mbox_handle mbx);
int mailbox_is_empty(mbox_handle mbx);


/* --------------------- TCB stuff */

/*
 * All treads are functions of this type,
 * The 'params' pointer can be passed on create_tread()
 */
typedef void thread_function_type(void *params);

/*
 * A thread must _not_ use atexit() to register a exit-function
 * (problems with non reentrancy of libc).
 * Instead, it may use the tcb->exit_func pointer point to a function
 * of below type. However, the old value of tcb->exit_func must
 * be saved on a _static_ place and restored within the exit function.
 * This way a a chain of exit functions will be called on a
 * 'last-in first-out' policy.
 */
typedef void tread_exit_function_type(void);

/* flags within tcb.threadflags */
#define TCB_F_STARTUP		1
#define TCB_F_TERMREQUEST_BIT	1
#define TCB_F_TERMREQUEST	(1 << TCB_F_TERMREQUEST_BIT)

/* TCB (Thread Control Block */
struct tcb {
	struct queue_entry link;
	pid_t pid;
	int tcb_id;
	int threadflags;
	struct tcb *parent;
	thread_function_type *thread_code;
	void *params;
	unsigned long stack_size;
	int suspend_count;
	char owning_locks[MAX_RESOURCES>>3];
	jmp_buf exit_jmpbuf;
	tread_exit_function_type *exit_func;
	int exit_code;
};

#define TCB_GRAN_DEFAULT	0x20000U	/* 128K address space granularity */
#ifdef LT_THREADS_ITSELF
  unsigned int tcb_gran = TCB_GRAN_DEFAULT;
  unsigned int tcb_gran_mask = ~(TCB_GRAN_DEFAULT-1);
  struct tcb *thread_list[MAX_THREADS] = {0};
#else
  extern unsigned int tcb_gran;
  extern unsigned int tcb_gran_mask;
  extern struct tcb *thread_list[MAX_THREADS];
#endif

#define TCB_GRAN	tcb_gran
#define TCB_GRAN_MASK	tcb_gran_mask
#define __ESP ( (unsigned long) ({ \
	unsigned long r; \
	__asm__("movl %%esp,%0" :"=a" (r) ); \
	r; \
}))
#define OWN_TCB		((struct tcb *)(__ESP & TCB_GRAN_MASK))
#define FOR_ALL_TCB(id,tcb) \
	for (id=0, tcb=thread_list[0]; id < MAX_THREADS; id++, tcb=thread_list[id]) \
		if ((tcb !=0) && ((int)tcb != -1))

static __inline__ struct tcb *get_tcb_from_id(int tcb_id)
{
	if (tcb_id >= MAX_THREADS || tcb_id < 0) return 0;
	return thread_list[tcb_id];
}

struct tcb *create_thread(thread_function_type *thread_code, void *params);
struct tcb *init_zero_thread(int stacksize);


/* --------------------- inline assembly ------------------------ */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy { unsigned long a[100]; };
#define NOPT_ADDR(x) (*(struct __dummy *) x)
#define NOPT_CONST_ADDR (*(const struct __dummy *) addr)



/*
 * This function atomically reserves 'resource' and queues ID.
 * When the reservation was successfull (i.e. if 'resource' was -1 before)
 * the function returns -1, otherwise '0'.
 * Id must be a number in the range 0..27.
 * In any case bit 'idnum' is set in resource. Due to the technique used
 * for being atomic, only 27 of 32 bits in the integer can be used.
 */
static __inline__ int atomic_reserv(int *resource, int id)
{
	int res;
	id = (0x20 << id) | 1;
	__asm__ __volatile__("
		lock
		addl	%2,%1
		sbbl	%0,%0
	" : "=r" (res),  "=m" (NOPT_ADDR(resource)) : "r" (id));
	return res;
}

/*
 * This function frees a previously with atomic_reserv reserved resource
 * If there are still IDs queued, the function returns with 0, otherwise -1.
 */
static __inline__ int atomic_free(int *resource, int id)
{
	int res;
	id = (0x20 << id) | 1;
	__asm__ __volatile__("
		lock
		subl	%2,%1
		sbbl	%0,%0
	" : "=r" (res), "=m" (NOPT_ADDR(resource)) : "r" (id) );
	return res;
}

/*
 * This function return the highest priority queued ID in the resource
 * or -1, if none is queued.
 * Format of 'resource' is as in atomic_reserv and atomic_free.
 */
static __inline__ int get_lowest_waiting_id_from_resource(int resource)
{
	int res;
	if (resource == -1) return -1;
	resource &= ~0x1f;  /* mask out the used count */
	__asm__ __volatile__("
		bsfl	%2,%0
	" : "=r" (res) : "0" ((int)4), "r" ((int )resource));
	return res-5;
}

/*
 * This function increases 'flag' atomically and returns -1 if increasing
 * did result in transition from negativ to positive, else returns 0.
 */
static __inline__ int atomic_inc(int *flag, int increment)
{
	register int res;
	__asm__ __volatile__("
		lock
		addl	%2,%1
		sbbl	%0,%0
	" : "=r" (res), "=m" (NOPT_ADDR(flag)) : "r" (increment) ) ;
	return res;
}


/*
 * This function decreases 'flag' atomically and returns -1 if decreasing
 * did result in transition from positive to negative, else returns 0.
 */
static __inline__ int atomic_dec(int *flag, int decrement)
{
	register int res;
	__asm__ __volatile__("
		lock
		subl	%2,%1
		sbbl	%0,%0
	" : "=r" (res), "=m" (NOPT_ADDR(flag)) : "r" (decrement) ) ;
	return res;
}


/*
 * This function atomicaly tests and sets bit 'bitnum' in the bitfield
 * pointed to by 'addr'. It returns 0 if the bit was 0 before, else -1.
 */
static __inline__ int atomic_bitset(void *addr, int bitnum)
{
	register int res;
	__asm__ __volatile__("
		lock
		bts	%2,%1
		sbbl	%0,%0
	" : "=r" (res), "=m" (NOPT_ADDR(addr)) : "r" (bitnum) );
	return res;
}


/*
 * This function atomicaly tests and clears bit 'bitnum' in the bitfield
 * pointed to by 'addr'. It returns 0 if the bit was 0 before, else -1.
 */
static __inline__ int atomic_bitclear(void *addr, int bitnum)
{
	register int res;
	__asm__ __volatile__("
		lock
		btrl	%2,%1
		sbbl	%0,%0
	" : "=r" (res), "=m" (NOPT_ADDR(addr)) : "r" (bitnum) );
	return res;
}

/* returns index of lowest bit set or -1, if not found
 * 'fieldsize' must be multiple of 32
 */
static __inline__ int search_lowest_bit(void *addr, int fieldsize)
{
	register int res;
	__asm__ __volatile__("
		leal	31(%%ecx),%%ecx
		shrl	$5,%%ecx
		xorl	%%edx,%%edx
		movl	$-1,%0
	     1:
		bsfl	(%1,%%edx,1),%0
		jnz	2f
		leal	4(%%edx),%%edx
		loop	1b
		jmp	3f
	     2:
		shll	$3,%%edx
		addl	%%edx,%0
	     3:
	" : "=r" (res) : "r" ((int)addr), "c" (fieldsize) : "cx", "dx" );
	return res;
}

static __inline__ int roundup_to_power_of_2(int val)
{
	register int res;
	__asm__ __volatile__("
		bsrl	%2,%0
	" : "=r" (res) : "0" ((int)-1), "r" (val) );
	if (res <0) return 1;
	res = 1 << res;
	if (val > res) res <<= 1;
	return res;
}

/* -------------------- debugging aids -------------------*/

#ifdef LT_THREADS_ITSELF
  int do_print_tcb =1;
  int do_print_r =1;
#else
  extern int do_print_tcb;
  extern int do_print_r;
#endif

int locked_printf( const char *fmt, ...);
void print_tcb(struct tcb *tcb);
void print_resource(struct lock_struct *sem);
void print_mbox(mbox_handle mbx);

#endif /* LT_THREADS_H */
