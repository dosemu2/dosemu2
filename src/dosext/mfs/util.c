/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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

BOOL is_valid_DOS_char(int c)
{ unsigned char u=(unsigned char)c; /* convert to ascii */
  if (u >= 128 || isalnum(u)) return(True);

  /* now we add some extra special chars  */
  if(strchr("._^$~!#%&-{}()@'`",c)!=0) return(True); /* general for
                                                    any codepage */
/* no match is found, then    */
  return(False);
}

/* case insensitive comparison of two DOS names */
/* upname is always already uppercased          */
BOOL strequalDOS(const char *name, const char *upname)
{
  const unsigned char *n, *un;
  for (n = name, un = upname; *n || *un; n++, un++)
    if (toupperDOS(*n) != *un)
      return FALSE;
  return TRUE;
}

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

  return(strcasecmp(s1,s2)==0);
}


