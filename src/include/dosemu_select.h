/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * (C) Copyright 1998  Florian La Roche
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 * this include file replaces asm constructs for gcc 2.8.x and
 * egcs 1.1 or newer. it needs to replace them for libc5 and
 * glibc 2.0. glibc 2.1 should be ok.
 */

#if defined(__linux__) && (GCC_VERSION_CODE > 2007)

/* check for libc5/kernel-2.0 and replace the macros in <asm/posix_types.h> */
#if GLIBC_VERSION_CODE < 2000
#undef	__FD_SET
#undef	__FD_CLR
#undef	__FD_ISSET
#undef	__FD_ZERO
#define __FD_SET(fd,fdsetp) \
		__asm__ __volatile__ ("btsl %1,%0"			\
			:"=m" (*(__kernel_fd_set *) (fdsetp))		\
			:"r" ((int) (fd))				\
			:"cc","memory")
#define __FD_CLR(fd,fdsetp) \
		__asm__ __volatile__("btrl %1,%0"			\
			: "=m" (*(__kernel_fd_set *) (fdsetp))		\
			: "r" ((int) (fd))				\
			: "cc","memory")
#define __FD_ISSET(fd,fdsetp) \
	(__extension__							\
	({ unsigned char __result;					\
		__asm__ __volatile__("btl %1,%2 ; setcb %b0"		\
			: "=q" (__result)				\
			: "r" ((int) (fd)),				\
			  "m" (*(__kernel_fd_set *) (fdsetp))		\
			: "cc");					\
		__result; }))
#define __FD_ZERO(fdsetp) \
	do {								\
		int __d0, __d1;						\
		__asm__ __volatile__("cld ; rep ; stosl"		\
			:"=m" (*(__kernel_fd_set *) (fdsetp)),		\
			 "=c" (__d0), "=D" (__d1)			\
			:"a" (0), "1" (__FDSET_LONGS),			\
			 "2" ((__kernel_fd_set *) (fdsetp))		\
			: "memory");					\
	} while (0)
#endif /* GLIBC_VERSION_CODE < 2000 */

/* fix for glibc 2.0 <selectbits.h> */
#if GLIBC_VERSION_CODE == 2000
#undef	__FD_ZERO
#define __FD_ZERO(fdsetp) \
  do {									\
	int __d0, __d1;							\
	__asm__ __volatile__("cld ; rep ; stosl"			\
		:"=m" ((fdsetp)->fds_bits[__FDELT (__FD_SETSIZE)]),	\
		 "=c" (__d0), "=D" (__d1)				\
		:"a" (0), "1" (sizeof (__fd_set) / sizeof (__fd_mask)),	\
		 "2" (&(fdsetp)->fds_bits[0])				\
		: "memory");						\
  } while (0)
#endif

#endif /* GCC_VERSION_CODE > 2007 */
