/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 
   Unix SMB/Netbios implementation.
   Version 1.9.
   Copyright (C) Andrew Tridgell 1992,1993,1994
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
Modified by O.V.Zhirov, July 1998

*/


#ifdef DOSEMU
#include "config.h"
#include "utilities.h"
#include "mangle.h"
#include "mfs.h"
#include "dos2linux.h"
#include "emu.h"
#include "translate.h"
#include <wctype.h>
#else
#include "includes.h"
#include "loadparm.h"
#endif

/****************************************************************************
provide a checksum on a string
****************************************************************************/
static int str_checksum(char *s)
{
  int res = 0;
  int c;
  int i=0;
  while (*s)
    {
      c = *s;
      res ^= (c << (i % 15)) ^ (c >> (15-(i%15)));
      s++; i++;
    }
  return(res);
}

/****************************************************************************
check if a name is a special msdos reserved name:
the name is either a full Unix name or an 8 character candidate
****************************************************************************/
unsigned long is_dos_device(const char *fname)
{
  char *p;
  unsigned char *dev;
  unsigned long devfar;
  int i;

  /*
   * LPTx e.t.c. is reserved no matter the path (e.g. .\LPT1 _is_ reserved),
   * but mfs.c sends an entire path.
   *             -- Rob Clark <rclark@list-clark.com> 2000/05/09
   */
  if (fname[0] == '/')
  {
    d_printf("MFS: is_msdos_device %s\n", fname);
    p = strrchr(fname,'/');
    if (p) fname = p+1;	/* strip the directory part */
  }
  else
  {
    d_printf("MFS: is_msdos_device %.8s\n", fname);
  }

  /*
   * The below (now #ifdef'ed out) code strips the extension off for checking.
   * But the assumption, that file names with a reserved basename remain
   * reserved even if they have an extension, seems not to be true.
   * For example CLIPPER-programs _are_ using file names such as
   * 'LPTx.PRN' for internal spooling purposes.
   *             -- Bernd Schueler b.schueler@gmx.de 2001/02/23
   * well, as far as I know the extension can be stripped off;
   * most likely the bug was somewhere else - we must not strip
   * the extension off for further processing Bart 2002/08/12
   */

  /* walk the chain of DOS devices; see also FreeDOS kernel code */
  dev = &lol[lol_nuldev_off];
  devfar = (((unsigned long)dev - 0x26) << 12) | 0x26;
  do
  {
    for (i = 0; i < 8; i++)
    {
      char c1 = fname[i];
      /* hopefully no device names with cyrillic characters exist ... */
      if ((unsigned char)c1 >= 128) break;
      if ((unsigned char)dev[0xa + i] >= 128) break;
      if (c1 == '.' || c1 == '\0')
      {
        /* check if remainder of device name consists of spaces or nulls */
        for (; i < 8; i++)
        {
          char c2 = dev[0xa + i];
          if (c2 != ' ' && c2 != '\0')
            break;
        }
        break;
      }
      if (toupper(c1) != toupper(dev[0xa + i]))
        break;
    }
    if (i == 8)
      return devfar;
    if (dev[0] == 0xff || dev[1] == 0xff)
      return 0;
    memcpy(&devfar, dev, 4);
    dev = MK_FP32((dev[3] << 8) | dev[2], (dev[1] << 8) | dev[0]);
  } while (1);
}


/****************************************************************************
return True if a name is in 8.3 dos format
****************************************************************************/
static BOOL is_8_3(char *fname)
{
  int len = strlen(fname);
  char *dot_pos = strchr(fname,'.');
  int l;

  DEBUG(5,("checking %s for 8.3\n",fname));

  /* can't be longer than 12 chars */
  if (len == 0 || len > 12)
    return(False);

  /* can't contain invalid dos chars */
  /* Windows use the ANSI charset.
     But filenames are translated in the PC charset.
     This Translation may be more or less relaxed depending
     the Windows application. */

  /* %%% A nice improvment to name mangling would be to translate
     filename to ANSI charset on the smb server host */

  {
    char *p = fname;

#ifdef KANJI
    dot_pos = 0;
    while (*p)
      {
	  if (is_shift_jis (*p)) {
	      p += 2;
	  } else if (is_kana (*p)) {
	      p ++;
	  } else {
	      if (*p == '.' && !dot_pos)
		  dot_pos = (char *) p;
	      if (!VALID_DOS_PCHAR(p))
		  return(False);
	      p++;
	  }
      }      
#else
    while (*p)
      {
	if (!VALID_DOS_PCHAR(p))
	  return(False);
	p++;
      }      
#endif /* KANJI */
  }

  /* no dot and less than 9 means OK */
  if (!dot_pos)
    return(len <= 8);
	
  l = PTR_DIFF(dot_pos,fname);

  /* base must be at least 1 char except special cases . and .. */
  if (l == 0)
    return(strcmp(fname,".") == 0 || strcmp(fname,"..") == 0);

  /* base can't be greater than 8 */
  if (l > 8)
    return(False);

  if (lp_strip_dot() && 
      len - l == 1 &&
      !strchr(dot_pos+1,'.'))
    {
      *dot_pos = 0;
      return(True);
    }

  /* extension must be between 1 and 3 */
  if ( (len - l < 2 ) || (len - l > 4) )
    return(False);

  /* extension can't have a dot */
  if (strchr(dot_pos+1,'.'))
    return(False);

  /* must be in 8.3 format */
  return(True);
}



/*
keep a stack of name mangling results - just
so file moves and copies have a chance of working
*/
fstring *mangled_stack = NULL;
int mangled_stack_size = 0;
int mangled_stack_len = 0;

/****************************************************************************
create the mangled stack
****************************************************************************/
static void create_mangled_stack(int size)
{
  if (mangled_stack)
    {
      free(mangled_stack);
      mangled_stack_size = 0;
      mangled_stack_len = 0;
    }
  if (size > 0)
    mangled_stack = (fstring *)malloc(sizeof(fstring)*size);
  if (mangled_stack) mangled_stack_size = size;
}

/****************************************************************************
push a mangled name onto the stack
****************************************************************************/
static void push_mangled_name(char *s)
{
  int i;
  char *p;

  if (!mangled_stack)
    {
      create_mangled_stack(MANGLED_STACK);
      if (!mangled_stack)
	return;
    }

  for (i=0;i<mangled_stack_len;i++)
    if (strcmp(s,mangled_stack[i]) == 0)
      {
	array_promote(mangled_stack[0],sizeof(fstring),i);	
	return;
      }

  safe_memcpy(mangled_stack[1],mangled_stack[0],
	      sizeof(fstring)*min(mangled_stack_len,mangled_stack_size-1));
  strcpy(mangled_stack[0],s);
  p = strrchr(mangled_stack[0],'.');
  if (p && (!strhasupperDOS(p+1)) && (strlen(p+1) < 4))
    *p = 0;
  mangled_stack_len = min(mangled_stack_size,mangled_stack_len+1);
}

/****************************************************************************
check for a name on the mangled name stack
****************************************************************************/
BOOL check_mangled_stack(char *s, char *MangledMap)
{
  int i;
  pstring tmpname;
  char extension[5]="";
  char *p = strrchr(s,'.');
  BOOL check_extension = False;

  if (!mangled_stack) return(False);

  if (p)
    {
      check_extension = True;
      StrnCpy(extension,p,4);
    }

  for (i=0;i<mangled_stack_len;i++)
    {
      strcpy(tmpname,mangled_stack[i]);
      mangle_name_83(tmpname, MangledMap);
      if (strequal(tmpname,s))
	{
	  strcpy(s,mangled_stack[i]);
	  break;
	}
      if (check_extension && !strchr(mangled_stack[i],'.'))
	{
	  strcpy(tmpname,mangled_stack[i]);
	  strcat(tmpname,extension);
	  mangle_name_83(tmpname, MangledMap);
	  if (strequal(tmpname,s))
	    {
	      strcpy(s,mangled_stack[i]);
	      strcat(s,extension);
	      break;
	    }	  
	}
    }

  if (i < mangled_stack_len)
    {
      DEBUG(3,("Found %s on mangled stack as %s\n",s,mangled_stack[i]));
      array_promote(mangled_stack[0],sizeof(fstring),i);
      return(True);      
    }

  return(False);
}	

/* this is the magic char used for mangling */
char magic_char = '~';


/****************************************************************************
determine whther is name could be a mangled name
****************************************************************************/
BOOL is_mangled(char *s)
{
  char *m = strchr(s,magic_char);
  if (!m) return(False);

  /* we use two base 36 chars before the extension */
  if (m[1] == '.' || m[1] == 0 ||
      m[2] == '.' || m[2] == 0 ||
      (m[3] != '.' && m[3] != 0))
    return(is_mangled(m+1));

  /* it could be */
  return(True);
}



/****************************************************************************
return a base 36 character. v must be from 0 to 35.
****************************************************************************/
static char base36(int v)
{
  v = v % 36;
  if (v < 10)
    return('0'+v);
  return('A' + (v-10));
}

/****************************************************************************
do the actual mangling to 8.3 format
****************************************************************************/
void mangle_name_83(char *s, char *MangledMap)
{
  int csum = str_checksum(s);
  char *p;
  char extension[4] = "";
  char base[9]="";
  int baselen = 0;
  int extlen = 0;

#ifndef DOSEMU
  if (MangledMap && *MangledMap) {
    if (do_fwd_mangled_map(s, MangledMap))
      return;
  }
#endif

  p = strrchr(s,'.');
  if (p && (strlen(p+1)<4) )
    {
      BOOL all_normal = (strisnormal(p+1)); /* XXXXXXXXX */
      if (all_normal && p[1] != 0)
	{
	  *p = 0;
	  csum = str_checksum(s);
	  *p = '.';
	}
    }

  DEBUG(5,("Mangling name %s to ",s));

  if (p)
    {
      if (p == s)
	strcpy(extension,"___");                   /* ??? */
      else
	{
	  *p++ = 0;
	  while (*p && extlen < 3)
	    {
	      if (VALID_DOS_PCHAR(p) && *p != '.')
		extension[extlen++] = *p;
	      p++;
	    }
	  extension[extlen] = 0;
	}
    }

  p = s;

  while (*p && baselen < 5)
    {
      if (VALID_DOS_PCHAR(p) && *p != '.')
	base[baselen++] = *p;
      p++;
    }
  base[baselen] = 0;

  csum = csum % (36*36);

    sprintf(s,"%s%c%c%c",base,magic_char,base36(csum/36),base36(csum%36));

  if (*extension)
    {
      strcat(s,".");
      strcat(s,extension);
    }
  DEBUG(5,("%s\n",s));
}


/****************************************************************************
convert a filename from a UNIX to a DOS character set.
****************************************************************************/

BOOL name_ufs_to_dos(char *dest, const char *src)
{
  mbstate_t unix_state;

  int retval = 1;

  wchar_t symbol;
  size_t slen = strlen(src), result;
  wchar_t wdst[slen + 1], *wdest = wdst;

  memset(&unix_state, 0, sizeof unix_state);

  *dest = '\0';
  result = mbsrtowcs(wdest, &src, slen + 1, &unix_state);
  if (result == (size_t) -1)
    return 0;

  while (*wdest) {
    symbol = *wdest++;
    if (symbol > 0xffff)
      symbol = '_';
    *dest = unicode_to_dos_table[symbol];
    if (!VALID_DOS_PCHAR(dest) && strchr(" +,;=[]",*dest)==0)
      retval = 0;
    dest++;
  }
  *dest = '\0';
  return retval;
}

/****************************************************************************
convert a filename to 8.3 format. return True if successful.
****************************************************************************/
BOOL name_convert(char *Name, BOOL mangle)
{
  /* check if it's already in 8.3 format */
  if (is_8_3(Name))
    return(True);

  if (!mangle)
    return(False);

  DEBUG(5,("Converted name %s",Name));

  /* mangle it into 8.3 */
  push_mangled_name(Name);
  mangle_name_83(Name, NULL);

  DEBUG(5,("to %s\n",Name));
  
  return(True);
}

#ifndef DOSEMU
static char *mangled_match(char *s, /* This is null terminated */
                           char *pattern, /* This isn't. */
                           int len) /* This is the length of pattern. */
{
  static pstring matching_bit;  /* The bit of the string which matches */
                                /* a * in pattern if indeed there is a * */
  char *sp;                     /* Pointer into s. */
  char *pp;                     /* Pointer into p. */
  char *match_start;            /* Where the matching bit starts. */
  pstring pat;

  StrnCpy(pat, pattern, len);   /* Get pattern into a proper string! */
  strcpy(matching_bit,"");      /* Match but no star gets this. */
  pp = pat;                     /* Initialise the pointers. */
  sp = s;
  if ((len == 1) && (*pattern == '*')) {
    return NULL;                /* Impossible, too ambiguous for */
                                /* words! */
  }

  while ((*sp)                  /* Not the end of the string. */
         && (*pp)               /* Not the end of the pattern. */
         && (*sp == *pp)        /* The two match. */
         && (*pp != '*')) {     /* No wildcard. */
    sp++;                       /* Keep looking. */
    pp++;
  }
  if (!*sp && !*pp)             /* End of pattern. */
    return matching_bit;        /* Simple match.  Return empty string. */
  if (*pp == '*') {
    pp++;                       /* Always interrested in the chacter */
                                /* after the '*' */
    if (!*pp) {                 /* It is at the end of the pattern. */
      StrnCpy(matching_bit, s, sp-s);
      return matching_bit;
    } else {
      /* The next character in pattern must match a character further */
      /* along s than sp so look for that character. */
      match_start = sp;
      while ((*sp)              /* Not the end of s. */
             && (*sp != *pp))   /* Not the same  */
        sp++;                   /* Keep looking. */
      if (!*sp) {               /* Got to the end without a match. */
        return NULL;
      } else {                  /* Still hope for a match. */
        /* Now sp should point to a matching character. */
        StrnCpy(matching_bit, match_start, sp-match_start);
        /* Back to needing a stright match again. */
        while ((*sp)            /* Not the end of the string. */
               && (*pp)         /* Not the end of the pattern. */
               && (*sp == *pp)) { /* The two match. */
          sp++;                 /* Keep looking. */
          pp++;
        }
        if (!*sp && !*pp)       /* Both at end so it matched */
          return matching_bit;
        else
          return NULL;
      }
    }
  }
  return NULL;                  /* No match. */
}


BOOL do_fwd_mangled_map(char *s, char *MangledMap)
{
  /* MangledMap is a series of name pairs in () separated by spaces.
   * If s matches the first of the pair then the name given is the
   * second of the pair.  A * means any number of any character and if
   * present in the second of the pair as well as the first the
   * matching part of the first string takes the place of the * in the
   * second.
   *
   * I wanted this so that we could have RCS files which can be used
   * by UNIX and DOS programs.  My mapping string is (RCS rcs) which
   * converts the UNIX RCS file subdirectory to lowercase thus
   * preventing mangling.
   */
  char *start=MangledMap;       /* Use this to search for mappings. */
  char *end;                    /* Used to find the end of strings. */
  char *match_string;
  pstring new_string;           /* Make up the result here. */
  char *np;                     /* Points into new_string. */

  DEBUG(5,("Mangled Mapping '%s' map '%s'\n", s, MangledMap));
  while (*start) {
    while ((*start) && (*start != '('))
      start++;
    start++;                    /* Skip the ( */
    if (!*start)
      continue;                 /* Always check for the end. */
    end = start;                /* Search for the ' ' or a ')' */
    DEBUG(5,("Start of first in pair '%s'\n", start));
    while ((*end) && !((*end == ' ') || (*end == ')')))
      end++;
    if (!*end) {
      start = end;
      continue;                 /* Always check for the end. */
    }
    DEBUG(5,("End of first in pair '%s'\n", end));
    if ((match_string = mangled_match(s, start, end-start))) {
      DEBUG(5,("Found a match\n"));
      /* Found a match. */
      start = end+1;            /* Point to start of what it is to become. */
      DEBUG(5,("Start of second in pair '%s'\n", start));
      end = start;
      np = new_string;
      while ((*end)             /* Not the end of string. */
             && (*end != ')')   /* Not the end of the pattern. */
             && (*end != '*'))  /* Not a wildcard. */
        *np++ = *end++;
      if (!*end) {
        start = end;
        continue;               /* Always check for the end. */
      }
      if (*end == '*') {
        strcpy(np, match_string);
        np += strlen(match_string);
        end++;                  /* Skip the '*' */
        while ((*end)             /* Not the end of string. */
               && (*end != ')')   /* Not the end of the pattern. */
               && (*end != '*'))  /* Not a wildcard. */
          *np++ = *end++;
      }
      if (!*end) {
        start = end;
        continue;               /* Always check for the end. */
      }
      *np++ = '\0';             /* NULL terminate it. */
      DEBUG(5,("End of second in pair '%s'\n", end));
      strcpy(s, new_string);    /* Substitute with the new name. */
      strupperDOS(s);
      DEBUG(5,("s is now '%s'\n", s));
      return True;              /* DONE! */
    } else {
      start = end;              /* Skip a bit which cannot be wanted */
                                /* anymore. */
    }
    start++;
  }
  return False;
}

/******************************************************/


static BOOL do_rev_mangled_map(char *s, char *MangledMap)
{
  /* Does the reverse of do_fwd_mangled_map().  Look in there for
   * details.
   */
  /* Look for the start and end of each of the mapping strings using
   * these...
   */
  char *start_first=MangledMap;
  char *end_first;
  char *start_second=MangledMap;
  char *end_second;
  char *match_string;
  char *ss;                     /* Pointer into s. */
  char *pp;                     /* Pointer into pattern. */
  pstring pattern;

  DEBUG(5,("Mangled Mapping '%s' map '%s'\n", s, MangledMap));
  while (*start_first) {
    while ((*start_first) && (*start_first != '('))
      start_first++;
    start_first++;              /* Skip the ( */
    if (!*start_first)
      continue;                 /* Always check for the end. */
    end_first = start_first;    /* Search for the ' ' or a ')' */
    DEBUG(5,("Start of first in pair '%s'\n", start_first));
    while ((*end_first) && !((*end_first == ' ') || (*end_first == ')')))
      end_first++;
    if (!*end_first) {
      start_first = end_first;
      continue;                 /* Always check for the end. */
    }
    DEBUG(5,("End of first in pair '%s'\n", end_first));
    /* Now look for the second pair. */
    start_second = end_first+1; /* Point to start of second string in */
                                /* pair. */
    DEBUG(5,("Start of second in pair '%s'\n", start_second));
    end_second = start_second;
    /* Find the end of the second of the pair. */
    while ((*end_second) && (*end_second != ')'))
      end_second++;
    if (!*end_second) {
      start_first = end_second;
      continue;               /* Always check for the end. */
    }
    DEBUG(5,("End of second in pair '%s'\n", end_second));
    if ((match_string=mangled_match(s, start_second, end_second-start_second)))
      {
	DEBUG(5,("Found a match\n"));
	/* Substitute with the new name. */
	ss = s;
	StrnCpy(pattern, start_first, end_first-start_first);
	pp = pattern;
	while ((*pp)              /* Not end of pattern. */
	       && (*pp != '*'))   /* Not the wild bit. */
	  *ss++ = *pp++;
	if (!*pp) {               /* The end of the pattern. */
	  DEBUG(5,("s is now '%s'\n", s));
	  return True;
	}
	/* Must have hit a star. */
	pp++;                     /* Skip the star. */
	strcat(ss, match_string); /* Bung in the matching bit. */
	ss += strlen(match_string);
	while ((*ss++ = *pp++))     /* Not end of pattern. */
	  ;
	DEBUG(5,("s is now '%s'\n", s));
	return True;              /* DONE! */
    } else {
      start_first = end_second; /* Skip a bit which cannot be wanted */
      /* anymore. */
    }
    start_first++;
  }
  return False;
}


/****************************************************************************
search for a name in a directory which when 83 mangled gives the specified
string
****************************************************************************/
void mangle_search_83(char *s,char *home,char *dir, char *MangledMap)
{
  void *dirptr;
  char *dname;
  pstring tmpname;

  DEBUG(3,("mangle search - searching for %s in %s\n",s,dir));

  if (check_mangled_stack(s, MangledMap))
    return;

  strcpy(tmpname,home);
  if (home[strlen(home)-1] != '/' && *dir != '/')
    strcat(tmpname,"/");
  strcat(tmpname,dir);
  if (tmpname[strlen(tmpname)-1] == '/')
    strcat(tmpname,".");

  dirptr = OpenDir(tmpname);

  if (!dirptr)
    return;

  
  for (dname = ReadDirName(dirptr); dname; dname = ReadDirName(dirptr))
    {
      strcpy(tmpname,dname);
      mangle_name_83(tmpname, MangledMap);
      DEBUG(5,("Mangle: Trying %s gave %s\n",dname,tmpname));
      if (strcmp(tmpname,s) == 0)
	{
	  strcpy(s,dname);
	  push_mangled_name(s);
	  CloseDir(dirptr);
	  return;
	}
    }
  
  /* didn't find it */
  strnorm(s);

  CloseDir(dirptr);
}



/****************************************************************************
convert a dos name to a unix name - possibly unmangling
****************************************************************************/
void unix_convert_83(char *s,char *home,BOOL mangle, char *MangledMap)
{
  char *p;
  unix_format(s);

  strnorm(s);

  if (!mangle) 
    return;

  if (MangledMap && *MangledMap) {
    /* I now have to scan the components of this file path to see if */
    /* they need to be reversed mapped. */
    char *d;                    /* Destination string pointer. */
    char *start;
    pstring component;
    *component = 0;
    p = s;                      /* Use p to scan the string. */

    while (*p) {
      if ((*p == '/') || (p == s)) { /* Path separator or start of string. */
        if (*p == '/')
          p++;                  /* get to the name bit. */
        start = p;              /* And remember its start position. */
        d = component;
        while (*p && (*p != '/')) /* And copy it into component. */
          *d++ = *p++;
        *d++ = '\0';
        if (do_rev_mangled_map(component, MangledMap)) {
          pstring new_s;
          StrnCpy(new_s, s, start-s);
          strcat(new_s, component);
          strcat(new_s, p);
          strcpy(s, new_s);
          p = start + strlen(component);
        }
      } else {
        p++;                    /* I shouldn't get here. */
      }
    }
  }

  DEBUG(5,("Converting name %s ",s));

  unix_clean_name(s);

  /* Now need to do a reverse lookup in MangledMap as I will have got
   * the second of the pair from the client and I want to map to the
   * first.
   */

  while ((p = strchr(s,magic_char)))
    {
      char *p2;

      pstring directory;
      pstring name;
      pstring rest;

      *directory = *name = *rest = 0;

      /* pull off the rest of the name */      
      if ((p2 = strchr(p,'/')))
	{
	  *p2 = 0;
	  strcpy(rest,p2+1);
	}

      /* pull out the name itself */
      if ((p2 = strrchr(s,'/')))
	{
	  *p2 = 0;
	  strcpy(name,p2+1);
	  strcpy(directory,s);
	}
      else
	strcpy(name,s);

      strupperDOS(name);
      /* now search through the directory for a matching name */
      mangle_search_83(name,home,directory, MangledMap);
      string_replace(name,magic_char,1);

      /* and construct the name again */
      strcpy(s,directory);
      if (*directory)
	strcat(s,"/");
      strcat(s,name);
      if (*rest)
	strcat(s,"/");
      strcat(s,rest);
    }
  string_replace(s,1,magic_char);
  DEBUG(5,("to %s\n",s));
}

#endif
