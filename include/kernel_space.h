/*
 *  This is file kernel_space.h 
 *  In conjunction with dosemu/emumodule it grants direct access to
 *  kernel space without mapping.
 *
 *  Mai,12 1995, Hans Lermen <lermen@elserv.ffm.fgan.de>
 */
 
#ifndef _KERNEL_SPACE_H_
#define _KERNEL_SPACE_H_

static inline void memcopy_from_huge(void *target, unsigned source_selector, void *source, int count)
{
__asm__("
	cld
	push %%ds
	push %%ecx
	mov %%ax,%%ds
	shrl $2,%%ecx  /* NOTE: carry contains bit 1 of ecx */
	rep ; movsl
	jnc 1f
	movsw
	1:
	pop %%ecx
	testb $1,%%cl
	je 2f
	movsb
	2:
	pop %%ds
	"
	: /* no outputs */
	:"a" (source_selector), "c" (count),"D" ((long) target),"S" ((long) source)
	:"di","si");
}


static inline void memcopy_to_huge(void *source, unsigned target_selector, void *target, int count)
{
__asm__("
	cld
	push %%es
	push %%ecx
	mov %%ax,%%es
	shrl $2,%%ecx  /* NOTE: carry contains bit 1 of ecx */
	rep ; movsl
	jnc 1f
	movsw
	1:
	pop %%ecx
	testb $1,%%cl
	je 2f
	movsb
	2:
	pop %%es
	"
	: /* no outputs */
	:"a" (target_selector), "c" (count),"D" ((long) target),"S" ((long) source)
	:"di","si");
}

static inline long get_huge_long(int selector, int *addr)
{
	 unsigned long ret;
	__asm__("push %%fs; mov %%dx,%%fs; movl %%fs:%1,%0; pop %%fs"
	        :"=r" (ret):"m" (*addr), "d" (selector));  
	return ret;
}


static inline unsigned short get_huge_word(int selector, int *addr)
{
	 unsigned short ret;
	__asm__("push %%fs; mov %%dx,%%fs; movw %%fs:%1,%0; pop %%fs"
	        :"=r" (ret):"m" (*addr), "d" (selector));  
	return ret;
}


static inline unsigned char get_huge_byte(int selector, int *addr)
{
	  register unsigned char ret;
	__asm__("push %%fs; mov %%dx,%%fs; movb %%fs:%1,%0; pop %%fs"
	        :"=q" (ret):"m" (*addr), "d" (selector));  
	return ret;
}


static inline void put_huge_long(unsigned long val, int selector, int *addr)
{
	__asm__("push %%fs; mov %%ax,%%fs; movl %0,%%fs:%1; pop %%fs"
	        : /* no outputs */ :"ir" (val),"m" (*addr), "a" (selector));  
}


static inline void put_huge_word(unsigned short val, int selector, int *addr)
{
	__asm__("push %%fs; mov %%ax,%%fs; movw %0,%%fs:%1; pop %%fs"
	        : /* no outputs */ :"ir" (val),"m" (*addr), "a" (selector));  
}


static inline void put_huge_byte(char val, int selector, int *addr)
{
	__asm__("push %%fs; mov %%ax,%%fs; movb %0,%%fs:%1; pop %%fs"
	        : /* no outputs */ :"iq" (val),"m" (*addr), "a" (selector));  
}



#endif
/*
 *  This is file kernel_space.h 
 *  In conjunction with dosemu/emumodule it grants direct access to
 *  kernel space without mapping.
 *
 *  Mai,12 1995, Hans Lermen <lermen@elserv.ffm.fgan.de>
 */
 
#ifndef _KERNEL_SPACE_H_
#define _KERNEL_SPACE_H_

static inline void memcopy_from_huge(void *target, unsigned source_selector, void *source, int count)
{
__asm__("
	cld
	push %%ds
	push %%ecx
	mov %%ax,%%ds
	shrl $2,%%ecx  /* NOTE: carry contains bit 1 of ecx */
	rep ; movsl
	jnc 1f
	movsw
	1:
	pop %%ecx
	testb $1,%%cl
	je 2f
	movsb
	2:
	pop %%ds
	"
	: /* no outputs */
	:"a" (source_selector), "c" (count),"D" ((long) target),"S" ((long) source)
	:"di","si");
}


static inline void memcopy_to_huge(void *source, unsigned target_selector, void *target, int count)
{
__asm__("
	cld
	push %%es
	push %%ecx
	mov %%ax,%%es
	shrl $2,%%ecx  /* NOTE: carry contains bit 1 of ecx */
	rep ; movsl
	jnc 1f
	movsw
	1:
	pop %%ecx
	testb $1,%%cl
	je 2f
	movsb
	2:
	pop %%es
	"
	: /* no outputs */
	:"a" (target_selector), "c" (count),"D" ((long) target),"S" ((long) source)
	:"di","si");
}

static inline long get_huge_long(int selector, int *addr)
{
	 unsigned long ret;
	__asm__("push %%fs; mov %%dx,%%fs; movl %%fs:%1,%0; pop %%fs"
	        :"=r" (ret):"m" (*addr), "d" (selector));  
	return ret;
}


static inline unsigned short get_huge_word(int selector, int *addr)
{
	 unsigned short ret;
	__asm__("push %%fs; mov %%dx,%%fs; movw %%fs:%1,%0; pop %%fs"
	        :"=r" (ret):"m" (*addr), "d" (selector));  
	return ret;
}


static inline unsigned char get_huge_byte(int selector, int *addr)
{
	  register unsigned char ret;
	__asm__("push %%fs; mov %%dx,%%fs; movb %%fs:%1,%0; pop %%fs"
	        :"=q" (ret):"m" (*addr), "d" (selector));  
	return ret;
}


static inline void put_huge_long(unsigned long val, int selector, int *addr)
{
	__asm__("push %%fs; mov %%ax,%%fs; movl %0,%%fs:%1; pop %%fs"
	        : /* no outputs */ :"ir" (val),"m" (*addr), "a" (selector));  
}


static inline void put_huge_word(unsigned short val, int selector, int *addr)
{
	__asm__("push %%fs; mov %%ax,%%fs; movw %0,%%fs:%1; pop %%fs"
	        : /* no outputs */ :"ir" (val),"m" (*addr), "a" (selector));  
}


static inline void put_huge_byte(char val, int selector, int *addr)
{
	__asm__("push %%fs; mov %%ax,%%fs; movb %0,%%fs:%1; pop %%fs"
	        : /* no outputs */ :"iq" (val),"m" (*addr), "a" (selector));  
}



#endif
