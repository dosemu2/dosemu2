/* Keymap routines for SLang.  The role of these keymap routines is simple:
 * Just read keys from the tty and return a pointer to a keymap structure.
 * That is, a keymap is simple a mapping of strings (keys from tty) to
 * structures.  Also included are routines for managing the keymaps.
 */
/* Copyright (c) 1992, 1999, 2001, 2002 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */

#include "slinclud.h"

#include "slang.h"
#include "_slang.h"

/* We need a define a rule for upperand lower case chars that user cannot
   change!  This could be a problem for international chars! */

#define UPPER_CASE_KEY(x) (((x) >= 'a') && ((x) <= 'z') ? (x) - 32 : (x))
#define LOWER_CASE_KEY(x) (((x) >= 'A') && ((x) <= 'Z') ? (x) + 32 : (x))

int SLang_Key_TimeOut_Flag = 0;	       /* true if more than 1 sec has elapsed
                                          without key in multikey sequence */

int SLang_Last_Key_Char;

SLKeyMap_List_Type SLKeyMap_List[SLANG_MAX_KEYMAPS];

static SLang_Key_Type *malloc_key(unsigned char *str)
{
   SLang_Key_Type *neew;

   if (NULL == (neew = (SLang_Key_Type *) SLmalloc(sizeof(SLang_Key_Type))))
     return NULL;

   SLMEMSET ((char *) neew, 0, sizeof (SLang_Key_Type));
   SLMEMCPY((char *) neew->str, (char *) str, (unsigned int) *str);
   return(neew);
}

static SLKeyMap_List_Type *add_keymap (char *name, SLang_Key_Type *map)
{
   int i;

   for (i = 0; i < SLANG_MAX_KEYMAPS; i++)
     {
	if (SLKeyMap_List[i].keymap == NULL)
	  {
	     if (NULL == (name = SLang_create_slstring (name)))
	       return NULL;

	     SLKeyMap_List[i].keymap = map;
	     SLKeyMap_List[i].name = name;
	     return &SLKeyMap_List[i];
	  }
     }
   SLang_Error = SL_UNKNOWN_ERROR;
   /* SLang_doerror ("Keymap quota exceeded."); */
   return NULL;
}

FVOID_STAR SLang_find_key_function(char *name, SLKeyMap_List_Type *keymap)
{
   SLKeymap_Function_Type *fp = keymap -> functions;
   char ch = *name;

   while ((fp != NULL) && (fp->name != NULL))
     {
	if ((ch == *fp->name)
	    && (0 == strcmp(fp->name, name)))
	  return (FVOID_STAR) fp->f;

	fp++;
     }
   return NULL;
}

#ifdef REAL_UNIX_SYSTEM
/* Expand termcap string specified by s.  s as passed will have the format:
 *   "XY)..."  where XY represents a termcap keyname.
 */
static char *process_termcap_string (char *s, char *str, int *ip, int imax)
{
   char c[3], *val;
   int i;

   if ((0 == (c[0] = s[0]))
       || (0 == (c[1] = s[1]))
       || (s[2] != ')'))
     {
	SLang_verror (SL_SYNTAX_ERROR, "setkey: ^(%s is badly formed", s);
	return NULL;
     }
   s += 3;

   c[2] = 0;
   if ((NULL == (val = SLtt_tgetstr (c)))
       || (*val == 0))
     return NULL;

   i = *ip;
   while ((i < imax) && (*val != 0))
     {
	str[i++] = *val++;
     }
   *ip = i;

   return s;
}
#endif

/* convert things like "^A" to 1 etc... The 0th char is the strlen INCLUDING
 * the length character itself.
 */
char *SLang_process_keystring(char *s)
{
   /* FIXME: v2.0, make this thread safe */
   static char str[32];
   unsigned char ch;
   int i;

   i = 1;
   while (*s != 0)
     {
	ch = (unsigned char) *s++;
	if (ch == '^')
	  {
	     ch = *s++;
	     if (ch == 0)
	       {
		  if (i < 32)
		    str[i++] = '^';
		  break;
	       }
#ifdef REAL_UNIX_SYSTEM
	     if (ch == '(')
	       {
		  s = process_termcap_string (s, str, &i, 32);
		  if (s == NULL)
		    {
		       str[0] = 1;
		       return str;
		    }
		  continue;
	       }
#endif
	     ch = UPPER_CASE_KEY(ch);
	     if (ch == '?') ch = 127; else ch = ch - 'A' + 1;
	  }

	if (i >= 32) break;
	str[i++] = ch;
     }

   if (i > SLANG_MAX_KEYMAP_KEY_SEQ)
     {
	SLang_verror (SL_INVALID_PARM, "Key sequence is too long");
	return NULL;
     }

   str[0] = i;
   return(str);
}

static int key_string_compare (unsigned char *a, unsigned char *b, unsigned int len)
{
   unsigned char *amax = a + len;
   int cha, chb, cha_up, chb_up;

   while (a < amax)
     {
	cha = *a++;
	chb = *b++;

	if (cha == chb) continue;

	cha_up = UPPER_CASE_KEY(cha);
	chb_up = UPPER_CASE_KEY(chb);

	if (cha_up == chb_up)
	  {
	     /* Use case-sensitive result. */
	     return cha - chb;
	  }
	/* Use case-insensitive result. */
	return cha_up - chb_up;
     }
   return 0;
}

static char *Define_Key_Error = "Inconsistency in define key.";

/* This function also performs an insertion in an ordered way. */
static int find_the_key (char *s, SLKeyMap_List_Type *kml, SLang_Key_Type **keyp)
{
   unsigned char ch;
   unsigned int str_len;
   SLang_Key_Type *key, *last, *neew;
   unsigned char *str;

   *keyp = NULL;

   if (NULL == (str = (unsigned char *) SLang_process_keystring(s)))
     return -2;

   if (1 == (str_len = str[0]))
     return 0;

   ch = str[1];
   key = kml->keymap + ch;

   if (str_len == 2)
     {
	if (key->next != NULL)
	  {
	     SLang_doerror (Define_Key_Error);
	     return -2;
	  }

	if (key->type == SLKEY_F_INTERPRET)
	  SLang_free_slstring (key->f.s);

	key->str[0] = str_len;
	key->str[1] = ch;

	*keyp = key;
	return 0;
     }

   /* insert the key definition */
   while (1)
     {
	int cmp;
	unsigned int key_len, len;

	last = key;
	key = key->next;

	if ((key != NULL) && (key->str != NULL))
	  {
	     len = key_len = key->str[0];
	     if (len > str_len) len = str_len;

	     cmp = key_string_compare (str + 1, key->str + 1, len - 1);

	     if (cmp > 0)
	       continue;

	     if (cmp == 0)
	       {
		  if (key_len != str_len)
		    {
		       SLang_doerror (Define_Key_Error);
		       return -2;
		    }

		  if (key->type == SLKEY_F_INTERPRET)
		    SLang_free_slstring (key->f.s);

		  *keyp = key;
		  return 0;
	       }
	     /* Drop to cmp < 0 case */
	  }

	if (NULL == (neew = malloc_key(str))) return -1;

	neew -> next = key;
	last -> next = neew;

	*keyp = neew;
	return 0;
     }
}

/* returns -2 if inconsistent, -1 if malloc error, 0 upon success */
int SLkm_define_key (char *s, FVOID_STAR f, SLKeyMap_List_Type *kml)
{
   SLang_Key_Type *key;
   unsigned int type = SLKEY_F_INTRINSIC;
   int ret;

   ret = find_the_key (s, kml, &key);
   if ((ret != 0) || (key == NULL))
     return ret;

   key->type = type;
   key->f.f = f;
   return 0;
}

int SLang_define_key (char *s, char *funct, SLKeyMap_List_Type *kml)
{
   SLang_Key_Type *key;
   FVOID_STAR f;
   int ret;

   ret = find_the_key (s, kml, &key);
   if ((ret != 0) || (key == NULL))
     return ret;

   f = SLang_find_key_function(funct, kml);

   if (f == NULL)                      /* assume interpreted */
     {
	char *str = SLang_create_slstring (funct);
	if (str == NULL) return -1;
	key->type = SLKEY_F_INTERPRET;
	key->f.s = str;
     }
   else
     {
	key->type = SLKEY_F_INTRINSIC;
	key->f.f = f;
     }
   return 0;
}

int SLkm_define_keysym (char *s, unsigned int keysym, SLKeyMap_List_Type *kml)
{
   SLang_Key_Type *key;
   int ret;

   ret = find_the_key (s, kml, &key);

   if ((ret != 0) || (key == NULL))
     return ret;

   key->type = SLKEY_F_KEYSYM;
   key->f.keysym = keysym;
   return 0;
}

SLang_Key_Type *SLang_do_key(SLKeyMap_List_Type *kml, int (*getkey)(void))
{
   register SLang_Key_Type *key, *next, *kmax;
   unsigned int len;
   unsigned char input_ch;
   register unsigned char chup;
   unsigned char key_ch = 0;

   SLang_Last_Key_Char = (*getkey)();
   SLang_Key_TimeOut_Flag = 0;

   if (SLANG_GETKEY_ERROR == (unsigned int) SLang_Last_Key_Char)
     return NULL;

   input_ch = (unsigned char) SLang_Last_Key_Char;

   key = (SLang_Key_Type *) &((kml->keymap)[input_ch]);

   /* if the next one is null, then we know this MAY be it. */
   while (key->next == NULL)
     {
	if (key->type != 0)
	  return key;

	/* Try its opposite case counterpart */
	if (input_ch == LOWER_CASE_KEY(input_ch))
	  input_ch = UPPER_CASE_KEY(input_ch);

	key = kml->keymap + input_ch;
	if (key->type == 0)
	  return NULL;
     }

   /* It appears to be a prefix character in a key sequence. */

   len = 1;			       /* already read one character */
   key = key->next;		       /* Now we are in the key list */
   kmax = NULL;			       /* set to end of list */

   while (1)
     {
	SLang_Key_TimeOut_Flag = 1;
	SLang_Last_Key_Char = (*getkey)();
	SLang_Key_TimeOut_Flag = 0;

	len++;

	if ((SLANG_GETKEY_ERROR == (unsigned int) SLang_Last_Key_Char)
	    || SLKeyBoard_Quit)
	  break;

	input_ch = (unsigned char) SLang_Last_Key_Char;

	chup = UPPER_CASE_KEY(input_ch);

	while (key != kmax)
	  {
	     if (key->str[0] > len)
	       {
		  key_ch = key->str[len];
		  if (chup == UPPER_CASE_KEY(key_ch))
		    break;
	       }
	     key = key->next;
	  }

	if (key == kmax) break;

	/* If the input character is lowercase, check to see if there is
	 * a lowercase match.  If so, set key to it.  Note: the
	 * algorithm assumes the sorting performed by key_string_compare.
	 */
	if (input_ch != key_ch)
	  {
	     next = key->next;
	     while (next != kmax)
	       {
		  if (next->str[0] > len)
		    {
		       unsigned char next_ch = next->str[len];
		       if (next_ch == input_ch)
			 {
			    key = next;
			    break;
			 }
		       if (next_ch != chup)
			 break;
		    }
		  next = next->next;
	       }
	  }

	/* Ok, we found the first position of a possible match.  If it
	 * is exact, we are done.
	 */
	if ((unsigned int) key->str[0] == len + 1)
	  return key;

	/* Apparantly, there are some ambiguities. Read next key to resolve
	 * the ambiguity.  Adjust kmax to encompass ambiguities.
	 */

	next = key->next;
	while (next != kmax)
	  {
	     if ((unsigned int) next->str[0] > len)
	       {
		  key_ch = next->str[len];
		  if (chup != UPPER_CASE_KEY(key_ch))
		    break;
	       }
	     next = next->next;
	  }
	kmax = next;
     }

   return NULL;
}

void SLang_undefine_key(char *s, SLKeyMap_List_Type *kml)
{
   int n, i;
   SLang_Key_Type *key, *next, *last, *key_root, *keymap;
   unsigned char *str;

   keymap = kml -> keymap;
   if (NULL == (str = (unsigned char *) SLang_process_keystring(s)))
     return;

   if (0 == (n = *str++ - 1)) return;
   i = *str;

   last = key_root = (SLang_Key_Type *) &(keymap[i]);
   key = key_root->next;

   while (key != NULL)
     {
	next = key->next;
	if (0 == SLMEMCMP ((char *)(key->str + 1), (char *) str, n))
	  {
	     if (key->type == SLKEY_F_INTERPRET)
	       SLang_free_slstring (key->f.s);

	     SLfree((char *) key);
	     last->next = next;
	  }
	else last = key;
	key = next;
     }

   if (n == 1)
     {
	*key_root->str = 0;
	key_root->f.f = NULL;
	key_root->type = 0;
     }
}

char *SLang_make_keystring(unsigned char *s)
{
   static char buf [3 * SLANG_MAX_KEYMAP_KEY_SEQ + 1];
   char *b;
   int n;

   n = *s++ - 1;

   if (n > SLANG_MAX_KEYMAP_KEY_SEQ)
     {
	SLang_verror (SL_INVALID_PARM, "Key sequence is too long");
	return NULL;
     }

   b = buf;
   while (n--)
     {
	if (*s < 32)
	  {
	     *b++ = '^';
	     *b++ = *s + 'A' - 1;
	  }
	else *b++ = *s;
	s++;
     }
   *b = 0;
   return(buf);
}

static SLang_Key_Type *copy_keymap(SLKeyMap_List_Type *kml)
{
   int i;
   SLang_Key_Type *neew, *old, *new_root, *km;

   if (NULL == (new_root = (SLang_Key_Type *) SLcalloc(256, sizeof(SLang_Key_Type))))
     return NULL;

   if (kml == NULL) return new_root;
   km = kml->keymap;

   for (i = 0; i < 256; i++)
     {
	old = &(km[i]);
	neew = &(new_root[i]);

	if (old->type == SLKEY_F_INTERPRET)
	  neew->f.s = SLang_create_slstring (old->f.s);
	else
	  neew->f.f = old->f.f;

	neew->type = old->type;
	SLMEMCPY((char *) neew->str, (char *) old->str, (unsigned int) *old->str);

	old = old->next;
	while (old != NULL)
	  {
	     neew->next = malloc_key((unsigned char *) old->str);
	     neew = neew->next;

	     if (old->type == SLKEY_F_INTERPRET)
	       neew->f.s = SLang_create_slstring (old->f.s);
	     else
	       neew->f.f = old->f.f;

	     neew->type = old->type;
	     old = old->next;
	  }
	neew->next = NULL;
     }
   return(new_root);
}

SLKeyMap_List_Type *SLang_create_keymap(char *name, SLKeyMap_List_Type *map)
{
   SLang_Key_Type *neew;
   SLKeyMap_List_Type *new_map;

   if ((NULL == (neew = copy_keymap(map)))
       || (NULL == (new_map = add_keymap(name, neew)))) return NULL;

   if (map != NULL) new_map -> functions = map -> functions;

   return new_map;
}

SLKeyMap_List_Type *SLang_find_keymap(char *name)
{
   SLKeyMap_List_Type *kmap, *kmap_max;

   kmap = SLKeyMap_List;
   kmap_max = kmap + SLANG_MAX_KEYMAPS;

   while (kmap < kmap_max)
     {
	if ((kmap->name != NULL)
	    && (0 == strcmp (kmap->name, name)))
	  return kmap;

	kmap++;
     }
   return NULL;
}
