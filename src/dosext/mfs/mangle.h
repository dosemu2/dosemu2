/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

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
#include <ctype.h>


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
#ifndef True
#define True 1
#define False 0
#endif


#define CASE_LOWER 0
#define CASE_UPPER 1


typedef char fstring[100];
typedef char pstring[1024];


/* prototypes */
void mangle_name_83(char *s, char *MangledMap);
extern BOOL do_fwd_mangled_map(char *s, char *MangledMap);
extern BOOL name_convert(char *OutName,char *InName,BOOL mangle, char *MangledMap);
extern BOOL is_mangled(char *s);
extern BOOL check_mangled_stack(char *s, char *MangledMap);

/* prototypes, found in util.c */
char *StrnCpy(char *dest,char *src,int n);
void strupper(char *s);
BOOL strhasupper(char *s);
BOOL strhaslower(char *s);
void array_promote(char *array,int elsize,int element);
void strlower(char *s);
BOOL strequal(char *s1,char *s2);



#ifndef MANGLE
#define MANGLE 1
#endif

#ifndef MANGLED_STACK
#define MANGLED_STACK 50
#endif
