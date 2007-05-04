/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* header file to make mangle.c fit with dosemu 

Andrew Tridgell
March 1995

Modified by O.V.Zhirov, July 1998
*/

#if defined(__linux__)
#define DOSEMU 1		/* this is a port to dosemu */
#endif


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>


/* no debugging - the code is perfect! */
#define DEBUG(level,message)
#define PTR_DIFF(p1,p2) ((ptrdiff_t)(((char *)(p1)) - (char *)(p2)))

#define strnorm(s) strlowerDOS(s)
#define strisnormal(s) (!strhasupperDOS(s))

#define safe_memcpy(x,y,s) memmove(x,y,s)

#define lp_strip_dot() 1

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
extern unsigned long is_dos_device(const char *fname);
extern void mangle_name_83(char *s, char *MangledMap);
extern BOOL name_ufs_to_dos(char *dest, const char *src);
extern BOOL do_fwd_mangled_map(char *s, char *MangledMap);
extern BOOL name_convert(char *Name,BOOL mangle);
extern BOOL is_mangled(char *s);
extern BOOL check_mangled_stack(char *s, char *MangledMap);

/* prototypes, found in util.c */
void init_all_DOS_tables(void);
extern unsigned char unicode_to_dos_table[0x10000];
extern unsigned short dos_to_unicode_table[0x100];
extern unsigned char upperDOS_table[0x100];

BOOL isupperDOS(int c);
#define toupperDOS(c) (upperDOS_table[(unsigned char)(c)])
BOOL islowerDOS(int c);
int  tolowerDOS(int c);
void strupperDOS(char *s);
BOOL strhasupperDOS(char *s);
BOOL strhaslowerDOS(char *s);
void strlowerDOS(char *s);
BOOL isalphaDOS(int c);
BOOL isalnumDOS(int c);
BOOL is_valid_DOS_char(int c);
int chrcmpDOS(int c1, int c2);
int strncmpDOS(char *s1, char *s2,int n);
int strcmpDOS(char *s1, char *s2);
int strncasecmpDOS(char *s1, char *s2,int n);
int strcasecmpDOS(char *s1, char *s2);

char *StrnCpy(char *dest,const char *src,int n);
void array_promote(char *array,int elsize,int element);
BOOL strequalDOS(const char *s1, const char *s2);
BOOL strequal(char *s1,char *s2);


extern BOOL valid_dos_char[256];

#define VALID_DOS_PCHAR(p) (valid_dos_char[*(unsigned char *)(p)])


#ifndef MANGLE
#define MANGLE 1
#endif

#ifndef MANGLED_STACK
#define MANGLED_STACK 150
#endif

#ifndef CODEPAGE
#define CODEPAGE 866
#endif


