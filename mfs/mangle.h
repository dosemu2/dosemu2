/* header file to make mangle.c fit with dosemu 

Andrew Tridgell
March 1995
*/

#if defined(__linux__) || defined(__NetBSD__)
#define DOSEMU 1		/* this is a port to dosemu */
#endif


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>


/* no debugging - the code is perfect! */
#define DEBUG(level,message)
#define PTR_DIFF(p1,p2) ((ptrdiff_t)(((char *)(p1)) - (char *)(p2)))

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#define strnorm(s) strlower(s)
#define strisnormal(s) (!strhasupper(s))

#define safe_memcpy(x,y,s) memmove(x,y,s)

#define lp_strip_dot() 0

#define BOOL int
#define True 1
#define False 0


#define CASE_LOWER 0
#define CASE_UPPER 1


typedef char fstring[100];
typedef char pstring[1024];


/* prototypes */
void mangle_name_83(char *s, char *MangledMap);
static BOOL do_fwd_mangled_map(char *s, char *MangledMap);


#ifndef MANGLE
#define MANGLE 1
#endif

#ifndef MANGLED_STACK
#define MANGLED_STACK 50
#endif
