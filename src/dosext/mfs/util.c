/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "config.h"
#include "emu.h"
#include "mangle.h"
#include "translate.h"
#include <wctype.h>
#include <errno.h>

int case_default=-1;
BOOL case_mangle=False;
int DEBUGLEVEL=0;

/****************************************************************************
initialise the valid dos char array
****************************************************************************/
BOOL valid_dos_char[256];
static void valid_initialise(void)
{
  int i;

  for (i=0;i<256;i++)
    valid_dos_char[i] = is_valid_DOS_char(i);
}

#ifdef HAVE_UNICODE_TRANSLATION

/* this is the fast table for single byte DOS character sets,
   used to avoid expensive translate calls */
unsigned char unicode_to_dos_table[0x10000];
static void init_unicode_to_dos_table(void)
{
  struct char_set_state dos_state;
  unsigned char *dest;
  t_unicode symbol;
  int result;

  dest = unicode_to_dos_table;

  /* these are either invalid or ascii: no replacement '_' ! */
  for (symbol = 0; symbol <= 0x7f; symbol++)
    *dest++ = symbol;

  for (symbol = 0x80; symbol <= 0xffff; symbol++) {
    init_charset_state(&dos_state, trconfig.dos_charset);
    result = unicode_to_charset(&dos_state, symbol, dest, 1);
    if (result == -1 && errno == -E2BIG)
      error("BUG: Internal multibyte character sets can't happen\n");
    if (result != 1 || *dest == '?')
      *dest = '_';
    cleanup_charset_state(&dos_state);
    dest++;
  }
}

unsigned short dos_to_unicode_table[0x100];
static void init_dos_to_unicode_table(void)
{
  struct char_set_state dos_state;
  unsigned short *dest;
  int i;
  t_unicode symbol;
  int result;

  dest = dos_to_unicode_table;

  for (i = 0; i < 0x100; i++) {
    unsigned char ch = i;
    init_charset_state(&dos_state, trconfig.dos_charset);
    result = charset_to_unicode(&dos_state, &symbol, &ch, 1);
    *dest = symbol;
    if (result != 1)
      *dest = '?';
    cleanup_charset_state(&dos_state);
    dest++;
  }
}

/* uppercase table for DOS characters */
unsigned char upperDOS_table[256];
static void init_upperDOS_table(void)
{
  struct char_set_state dos_state;
  t_unicode symbol;
  int i, result;

  for (i = 0; i < 256; i++) {
    upperDOS_table[i] = i;
    init_charset_state(&dos_state, trconfig.dos_charset);
    result = charset_to_unicode(&dos_state, &symbol, &upperDOS_table[i], 1);
    if (result == 1) {
      symbol = towupper(symbol);
      result = unicode_to_charset(&dos_state, symbol, &upperDOS_table[i], 1);
      if (result != 1)
	upperDOS_table[i] = i;
    }
    cleanup_charset_state(&dos_state);
  }
}

void init_all_DOS_tables(void)
{
  valid_initialise();
  init_unicode_to_dos_table();
  init_dos_to_unicode_table();
  init_upperDOS_table();
}

#else

int codepage=CODEPAGE;

/**********************************************************************
Modified by O.V.Zhirov July 1998

Below I implement some kind of national language support.

Note: codepages in Linux and DOS can be different -
      e.g. in russian Linux koi8-r is tradtional,
      while DOS uses alternative coding cp866

Warning: current linux locale setting could be irrelevant!!!

Below I implement DOS counterpart of locale-dependent functions.
Main assumption: all codepages differ in upper half of the charset
table ( 127<c<=256 )

Currently codepage 866 only is supported;
some support of Western Europe codepages: 850,... etc.
probably works (In fact, original approach by Andrew Tridgell
is left intact, but it needs some revision. May be someone, who
knows these codings better makes this job).
********************************************************************/

/*******************************************************************
  convert a string to upper case
********************************************************************/
void strupperDOS(char *s)
{
  while (*s)
    {
      if (islowerDOS(*s))
	*s = toupperDOS(*s);
      s++;
    }
}

/****************************************************************************
does a string have any uppercase chars in it?
****************************************************************************/
BOOL strhasupperDOS(char *s)
{
  while (*s) 
    {
      if (isupperDOS(*s)) return(True);
      s++;
    }
  return(False);
}
/****************************************************************************
does a string have any lowercase chars in it?
****************************************************************************/
BOOL strhaslowerDOS(char *s)
{
  while (*s) 
    {
      if (islowerDOS(*s)) return(True);
      s++;
    }
  return(False);
}

BOOL isupperDOS(int c)     /* isupper() */
{ unsigned char u=(unsigned char)c; /* convert to ascii */
  if (u>127)
   { switch (codepage)
      { case 866: if (u<160 || u==240) return(True); break;
      };
     return(False);
   }
  else return(isupper(u));
}

int toupperDOS(int c)     /* toupper() */
{ unsigned char u=(unsigned char)c; /* convert to ascii */
  if (u>127)
   { switch (codepage)
      { case 866: if (u>=160 && u<=175) { u-=32; break;};
                  if (u>=224 && u<=239) { u-=80; break;};
         if (u==241) { u-=1; break;};
                  break;
      };
     return((char)u);
   }
  else return(toupper(u));
}

BOOL islowerDOS(int c)     /* islower() */
{ unsigned char u=(unsigned char)c; /* convert to ascii */
  if (u>127)
   { switch (codepage)
      { case 866: if ((u>=160 && u<=175) ||
                      (u>=224 && u<=239) || u==241)
            return(True); break;
      };
     return(False);
   }
  else return(islower(u));
}

int tolowerDOS(int c)     /* tolower() */
{ unsigned char u=(unsigned char)c; /* convert to ascii */
  if (u>127)
   { switch (codepage)
      { case 866: if (u>=128 && u<=143) { u+=32; break;};
                  if (u>=144 && u<=159) { u+=80; break;};
         if (u==240) { u+=1; break;};
                  break;
      };
     return((char)u);
   }
  else return(tolower(u));
}

/*******************************************************************
  convert a string to lower case
********************************************************************/
void strlowerDOS(char *s)
{
  while (*s)
    {
      if (isupperDOS(*s))
     *s = tolowerDOS(*s);
      s++;
    }
}

BOOL isalphaDOS(int c)
{ if(c>=0) { return(isalpha(c));}
  else return(islowerDOS(c) || isupperDOS(c));
}

BOOL isalnumDOS(int c)
{ if(c>=0) { return(isalnum(c));}
  else return(islowerDOS(c) || isupperDOS(c));
}
#endif

BOOL is_valid_DOS_char(int c)
{ unsigned char u=(unsigned char)c; /* convert to ascii */
#if HAVE_UNICODE_TRANSLATION
  if (u >= 128 || isalnum(u)) return(True);
#else
  if(isalnum(c)) return(True);
#endif

  /* now we add some extra special chars  */
  if(strchr("._^$~!#%&-{}()@'`",c)!=0) return(True); /* general for
                                                    any codepage */
#ifndef HAVE_UNICODE_TRANSLATION
  if(isalnumDOS(c)) return(True);
  switch (codepage)
  {
    case 866: return(False); break;
    /* here one can add something I have missed  */
  }
/* now we implement the national support for some Western Europe
   codepages, a la Andrew Tridgell. Sorry for such an ugly solution,
   but I hope it is temporary
*/

  /*    When the special char exists both cases, windows uses
   the uppercase code for DOS filename */
  switch (u)
  {
  /* any code page, low and uppercase, valid for all DOS PC */

  case 142: return(True); break;     /* A trema      */
  case 143: return(True); break;     /* A o          */
  case 144: return(True); break;     /* E '          */
  case 146: return(True); break;     /* AE           */
  case 153: return(True); break;     /* O trema      */
  case 154: return(True); break;     /* U trema      */
  case 165: return(True); break;     /* N tilda      */

  /* any code page, valid for all PC */
  case 128: return(True); break;     /* C cedille    */
  case 156: return(True); break;     /* Pound        */

  /* code page 850 (most common)*/
  case 183: return(True); break;     /* A `     (WIN)*/
  case 157: return(True); break;     /* Phi     (WIN)*/
  case 212: return(True); break;     /* E`      (WIN)*/
  }
#endif
/* no match is found, then    */
  return(False);
}

#ifndef HAVE_UNICODE_TRANSLATION
/***************************************************************************
 locale-dependent string operations
****************************************************************************/
int chrcmpDOS(int c1, int c2)
{
  unsigned char u1,u2;
  u1=(unsigned char)c1; /* convert to ascii */
  u2=(unsigned char)c2; /* convert to ascii */
  if(u1==u2) return(0);
  switch(codepage)  /* exceptional cases */
  {
   case 866:    /*russian letter  'yo' */
             if (u1==240)
             { if(u2<=133) {return(134-u2);}
          else return(133-u2);
        }
        else
          if (u2==240)
          { if(u1<=133) { return(u1-134);}
            else return(u1-133);
          };
             if (u1==241)
             { if (u2<=165) {return(166-u2);}
          else
            if (u2==240) { return(1);}
            else return(165-u2);
        }
        else
          if (u2==241)
          { if (u1<=165) { return(u1-166);}
            else
          if (u1==240) { return(-1);}
          else return(u1-165);
          };
             break;
  /* everybody can add needed rules for other codepages   */
  };
  return(u1-u2);
}

int strncmpDOS(char *s1, char *s2,int n)
{ char *p1=s1, *p2=s2;
  int match=0;
  while (n--)
   { if((match=chrcmpDOS(*p1,*p2))!=0 || !(*p1) || !(*p2)) return(match);
     p1++; p2++;
   };
   return(match);
}

int strcmpDOS(char *s1, char *s2)
{
  char *p1=s1, *p2=s2;
  int match=0;
  while (1)
   { if((match=chrcmpDOS(*p1,*p2))!=0 || !(*p1) || !(*p2)) return(match);
     p1++; p2++;
   };
   return(match);
}

int strncasecmpDOS(char *s1, char *s2, int n)
{
  char *p1,*p2;
  int match;

  p1=strdup(s1);
  strupperDOS(p1);
  p2=strdup(s2);
  strupperDOS(p2);
  match=strncmpDOS(p1, p2, n);
  free(p1);
  free(p2);
  return match;
};
#endif

#ifdef HAVE_UNICODE_TRANSLATION
/* this is better called "strcasecmpUNIX" since it
   operates on the UNIX charset; */
BOOL strhasupperDOS(char *s)
{
  struct char_set_state dos_state;

  t_unicode symbol;
  size_t len = strlen(s);
  int result = -1;

  init_charset_state(&dos_state, trconfig.dos_charset);

  while (*s) {
    result = charset_to_unicode(&dos_state, &symbol, s, len);
    if (result == -1)
      break;
    if (iswupper(symbol))
      break;
    len -= result;
    s += result;
  }
  cleanup_charset_state(&dos_state);
  return(result != -1 && iswupper(symbol));
}

void strupperDOS(char *src)
{
  for (; *src; src++)
    *src = toupperDOS(*src);
}
#else
int strcasecmpDOS(char *s1, char *s2)
{ char *p1,*p2;
  int match;

  p1=strdup(s1);
  strupperDOS(p1);
  p2=strdup(s2);
  strupperDOS(p2);
  match=strcmpDOS(p1, p2);
  free(p1);
  free(p2);
  return match;
};

#endif

/* locale-independent routins      */

/***************************************************************************
line strncpy but always null terminates. Make sure there is room!
****************************************************************************/
char *StrnCpy(char *dest,const char *src,int n)
{
  char *d = dest;
  while (n-- && (*d++ = *src++)) ;
  *d = 0;
  return(dest);
}

/****************************************************************************
prompte a dptr (to make it recently used)
****************************************************************************/
void array_promote(char *array,int elsize,int element)
{
  char *p;
  if (element == 0)
    return;

  p = (char *)malloc(elsize);

  if (!p)
    {
      DEBUG(5,("Ahh! Can't malloc\n"));
      return;
    }
  memcpy(p,array + element * elsize, elsize);
  safe_memcpy(array + elsize,array,elsize*element);
  memcpy(array,p,elsize);
  free(p);
}

/*******************************************************************
  compare 2 strings 
********************************************************************/
BOOL strequal(char *s1,char *s2)
{
  if (!s1 || !s2) return(False);

#ifdef HAVE_UNICODE_TRANSLATION
  return(strcasecmp(s1,s2)==0);
#else
  return(strcasecmpDOS(s1,s2)==0);
#endif
}


