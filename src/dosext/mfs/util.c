#include "config.h"
#include "emu.h"
#include "mangle.h"
#include "translate/translate.h"
#include "dos2linux.h"
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
unsigned char lowerDOS_table[256];
static void init_upperlowerDOS_table(void)
{
  struct char_set_state dos_state;
  t_unicode symbol, symbolc;
  int i, result;

  for (i = 0; i < 128; i++) {
    /* force English ASCII handling for 0 -- 127 to avoid problems
       with the Turkish dotless i */
    upperDOS_table[i] = lowerDOS_table[i] = i;
    if (i >= 'a' && i <= 'z')
      upperDOS_table[i] = i - ('a' - 'A');
    else if (i >= 'A' && i <= 'Z')
      lowerDOS_table[i] = i + ('a' - 'A');
  }
  for (i = 128; i < 256; i++) {
    upperDOS_table[i] = lowerDOS_table[i] = i;
    init_charset_state(&dos_state, trconfig.dos_charset);
    result = charset_to_unicode(&dos_state, &symbol, &upperDOS_table[i], 1);
    cleanup_charset_state(&dos_state);
    if (result == 1) {
      symbolc = towupper(symbol);
      init_charset_state(&dos_state, trconfig.dos_charset);
      result = unicode_to_charset(&dos_state, symbolc, &upperDOS_table[i], 1);
      cleanup_charset_state(&dos_state);
      if (result != 1)
	upperDOS_table[i] = i;
      symbolc = towlower(symbol);
      init_charset_state(&dos_state, trconfig.dos_charset);
      result = unicode_to_charset(&dos_state, symbolc, &lowerDOS_table[i], 1);
      cleanup_charset_state(&dos_state);
      if (result != 1)
	lowerDOS_table[i] = i;
    }
  }
}

void init_all_DOS_tables(void)
{
  valid_initialise();
  init_unicode_to_dos_table();
  init_dos_to_unicode_table();
  init_upperlowerDOS_table();
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
static BOOL strnequalDOS(const char *name, const char *upname, int len)
{
  const char *n, *un;
  for (n = name, un = upname; *n && *un && len; n++, un++, len--)
    if (toupperDOS((unsigned char)*n) != (unsigned char)*un)
      return FALSE;
  if (!len || (! *n && ! *un))
    return TRUE;
  return FALSE;
}

BOOL strequalDOS(const char *name, const char *upname)
{
  return strnequalDOS(name, upname, strlen(upname) + 1);
}

char *strstrDOS(char *haystack, const char *upneedle)
{
  char *p = haystack;
  while (*p) {
    if (strnequalDOS(p, upneedle, strlen(upneedle)))
      return p;
    p++;
  }
  return NULL;
}

BOOL strhasupperDOS(char *s)
{
  struct char_set_state dos_state;

  t_unicode symbol;
  size_t len = strlen(s);
  int result = -1;

  init_charset_state(&dos_state, trconfig.dos_charset);

  while (*s) {
    result = charset_to_unicode(&dos_state, &symbol, (unsigned char *)s, len);
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

char *strupperDOS(char *src)
{
  char *s = src;
  for (; *src; src++)
    *src = toupperDOS(*src);
  return s;
}

char *strlowerDOS(char *src)
{
  char *s = src;
  for (; *src; src++)
    *src = tolowerDOS(*src);
  return s;
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


