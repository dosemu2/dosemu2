/* Copyright (c) 1992, 1999, 2001, 2002 John E. Davis
 * This file is part of the S-Lang library.
 *
 * Trimmed down for DOSEMU
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */
#define _GNU_SOURCE
#include "slinclud.h"

#include <ctype.h>

#include "slang.h"
#include "_slang.h"

/* p and ch may point to the same buffer */
char *_SLexpand_escaped_char(char *p, char *ch)
{
   int i = 0;
   int max = 0, num, base = 0;
   char ch1;

   ch1 = *p++;

   switch (ch1)
     {
      default: num = ch1; break;
      case 'n': num = '\n'; break;
      case 't': num = '\t'; break;
      case 'v': num = '\v'; break;
      case 'b': num = '\b'; break;
      case 'r': num = '\r'; break;
      case 'f': num = '\f'; break;
      case 'E': case 'e': num = 27; break;
      case 'a': num = 7;
	break;

	/* octal */
      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7':
	max = '7';
	base = 8; i = 2; num = ch1 - '0';
	break;

      case 'd':			       /* decimal -- S-Lang extension */
	base = 10;
	i = 3;
	max = '9';
	num = 0;
	break;

      case 'x':			       /* hex */
	base = 16;
	max = '9';
	i = 2;
	num = 0;
	break;
     }

   while (i--)
     {
	ch1 = *p;

	if ((ch1 <= max) && (ch1 >= '0'))
	  {
	     num = base * num + (ch1 - '0');
	  }
	else if (base == 16)
	  {
	     ch1 |= 0x20;
	     if ((ch1 < 'a') || ((ch1 > 'f'))) break;
	     num = base * num + 10 + (ch1 - 'a');
	  }
	else break;
	p++;
     }

   *ch = (char) num;
   return p;
}

#if !defined(HAVE_ISSETUGID) && defined(__GLIBC__) && (__GLIBC__ >= 2) && 0
extern int __libc_enable_secure;
# define HAVE___LIBC_ENABLE_SECURE 1
#endif

int _SLsecure_issetugid (void)
{
#ifdef HAVE_ISSETUGID
   return (1 == issetugid ());
#else
# ifdef HAVE___LIBC_ENABLE_SECURE
   return __libc_enable_secure;
# else
#  if defined(HAVE_GETUID) && defined(HAVE_GETEUID) && defined(HAVE_GETGID) && defined(HAVE_GETEUID)
   static int enable_secure;
   if (enable_secure == 0)
    {
       if ((getuid () != geteuid ()) 
	   || (getgid () != getegid ()))
	 enable_secure = 1;
       else
	 enable_secure = -1;
    }
   return (enable_secure == 1);
#  else
   return 0;
#  endif
# endif
#endif
}

/* Like getenv, except if running as setuid or setgid, returns NULL */
char *_SLsecure_getenv (char *s)
{
   if (_SLsecure_issetugid ())
     return NULL;
   return getenv (s);
}
