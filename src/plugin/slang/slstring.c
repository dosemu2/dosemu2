/* Copyright (c) 1998, 1999, 2001, 2002 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */
#include "slinclud.h"

#include "slang.h"
#include "_slang.h"

typedef struct _SLstring_Type
{
   struct _SLstring_Type *next;
   unsigned int ref_count;
   char bytes [1];
}
SLstring_Type;

static SLstring_Type *String_Hash_Table [SLSTRING_HASH_TABLE_SIZE];
static char Single_Char_Strings [256 * 2];

#if _SLANG_OPTIMIZE_FOR_SPEED
#define MAX_FREE_STORE_LEN 32
static SLstring_Type *SLS_Free_Store [MAX_FREE_STORE_LEN];

# define NUM_CACHED_STRINGS 601
typedef struct 
{
   unsigned long hash;
   SLstring_Type *sls;
   unsigned int len;
}
Cached_String_Type;
static Cached_String_Type Cached_Strings [NUM_CACHED_STRINGS];

#define GET_CACHED_STRING(s) \
   (Cached_Strings + (unsigned int)(((unsigned long) (s)) % NUM_CACHED_STRINGS))

_INLINE_
static void cache_string (SLstring_Type *sls, unsigned int len, unsigned long hash)
{
   Cached_String_Type *cs;
   
   cs = GET_CACHED_STRING(sls->bytes);
   cs->sls = sls;
   cs->hash = hash;
   cs->len = len;
}

_INLINE_
static void uncache_string (char *s)
{
   Cached_String_Type *cs;
   
   cs = GET_CACHED_STRING(s);
   if ((cs->sls != NULL)
       && (cs->sls->bytes == s))
     cs->sls = NULL;
}
#endif



_INLINE_
unsigned long _SLstring_hash (unsigned char *s, unsigned char *smax)
{
   register unsigned long h = 0;
   register unsigned long sum = 0;
   unsigned char *smax4;

   smax4 = smax - 4;

   while (s < smax4)
     {
	sum += s[0];
	h = sum + (h << 1);
	sum += s[1];
	h = sum + (h << 1);
	sum += s[2];
	h = sum + (h << 1);
	sum += s[3];
	h = sum + (h << 1);
	
	s += 4;
     }

   while (s < smax)
     {
	sum += *s++;
	h ^= sum + (h << 3);	       /* slightly different */
     }

   return h;
}

unsigned long _SLcompute_string_hash (char *s)
{
#if _SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
   SLstring_Type *sls;

   cs = GET_CACHED_STRING(s);
   if (((sls = cs->sls) != NULL)
       && (sls->bytes == s))
     return cs->hash;
#endif
   return _SLstring_hash ((unsigned char *) s, (unsigned char *) s + strlen (s));
}

_INLINE_
/* This routine works with any (long) string */
static SLstring_Type *find_string (char *s, unsigned int len, unsigned long hash)
{
   SLstring_Type *sls;
   char ch;

   sls = String_Hash_Table [(unsigned int)(hash % SLSTRING_HASH_TABLE_SIZE)];

   if (sls == NULL)
     return NULL;

   ch = s[0];
   do
     {
	char *bytes = sls->bytes;

	/* Note that we need to actually make sure that bytes[len] == 0. 
	 * In this case, it is not enough to just compare pointers.  In fact,
	 * this is called from create_nstring, etc...  It is unlikely that the
	 * pointer is a slstring
	 */
	if ((/* (s == bytes) || */ ((ch == bytes[0])
		 && (0 == strncmp (s, bytes, len))))
	    && (bytes [len] == 0))
	  break;

	sls = sls->next;
     }
   while (sls != NULL);

   return sls;
}

_INLINE_
static SLstring_Type *find_slstring (char *s, unsigned long hash)
{
   SLstring_Type *sls;

   sls = String_Hash_Table [(unsigned int)(hash % SLSTRING_HASH_TABLE_SIZE)];
   while (sls != NULL)
     {
	if (s == sls->bytes)
	  return sls;

	sls = sls->next;
     }
   return sls;
}

_INLINE_
static SLstring_Type *allocate_sls (unsigned int len)
{
#if _SLANG_OPTIMIZE_FOR_SPEED
   SLstring_Type *sls;
   
   if ((len < MAX_FREE_STORE_LEN)
       && (NULL != (sls = SLS_Free_Store [len])))
     {
	SLS_Free_Store[len] = NULL;
	return sls;
     }
#endif
   /* FIXME: use structure padding */
   return (SLstring_Type *) SLmalloc (len + sizeof (SLstring_Type));
}

_INLINE_
static void free_sls (SLstring_Type *sls, unsigned int len)
{
#if _SLANG_OPTIMIZE_FOR_SPEED
   if ((len < MAX_FREE_STORE_LEN)
       && (SLS_Free_Store[len] == NULL))
     {
	SLS_Free_Store [len] = sls;
	return;
     }
#else
   (void) len;
#endif
   SLfree ((char *)sls);
}

_INLINE_
static char *create_long_string (char *s, unsigned int len, unsigned long hash)
{
   SLstring_Type *sls;

   sls = find_string (s, len, hash);

   if (sls != NULL)
     {
	sls->ref_count++;
	s = sls->bytes;

#if _SLANG_OPTIMIZE_FOR_SPEED
	cache_string (sls, len, hash); 
#endif
	return s;
     }

   sls = allocate_sls (len);
   if (sls == NULL)
     return NULL;

   strncpy (sls->bytes, s, len);
   sls->bytes[len] = 0;
   sls->ref_count = 1;

#if _SLANG_OPTIMIZE_FOR_SPEED
   cache_string (sls, len, hash);
#endif

   hash = hash % SLSTRING_HASH_TABLE_SIZE;
   sls->next = String_Hash_Table [(unsigned int)hash];
   String_Hash_Table [(unsigned int)hash] = sls;

   return sls->bytes;
}

_INLINE_
static char *create_short_string (char *s, unsigned int len)
{
   char ch;

   /* Note: if len is 0, then it does not matter what *s is.  This is
    * important for SLang_create_nslstring.
    */
   if (len) ch = *s; else ch = 0;

   len = 2 * (unsigned int) ((unsigned char) ch);
   Single_Char_Strings [len] = ch;
   Single_Char_Strings [len + 1] = 0;
   return Single_Char_Strings + len;
}

/* s cannot be NULL */
_INLINE_
static char *create_nstring (char *s, unsigned int len, unsigned long *hash_ptr)
{
   unsigned long hash;

   if (len < 2)
     return create_short_string (s, len);

   hash = _SLstring_hash ((unsigned char *) s, (unsigned char *) (s + len));
   *hash_ptr = hash;

   return create_long_string (s, len, hash);
}

char *SLang_create_nslstring (char *s, unsigned int len)
{
   unsigned long hash;
   return create_nstring (s, len, &hash);
}

char *_SLstring_make_hashed_string (char *s, unsigned int len, unsigned long *hashptr)
{
   unsigned long hash;

   if (s == NULL) return NULL;

   hash = _SLstring_hash ((unsigned char *) s, (unsigned char *) s + len);
   *hashptr = hash;

   if (len < 2)
     return create_short_string (s, len);

   return create_long_string (s, len, hash);
}

char *_SLstring_dup_hashed_string (char *s, unsigned long hash)
{
   unsigned int len;
#if _SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
   SLstring_Type *sls;
   
   if (s == NULL) return NULL;
   if (s[0] == 0)
     return create_short_string (s, 0);
   if (s[1] == 0)
     return create_short_string (s, 1);
     
   cs = GET_CACHED_STRING(s);
   if (((sls = cs->sls) != NULL)
       && (sls->bytes == s))
     {
	sls->ref_count += 1;
	return s;
     }
#else
   if (s == NULL) return NULL;
#endif

   len = strlen (s);
#if !_SLANG_OPTIMIZE_FOR_SPEED
   if (len < 2) return create_short_string (s, len);
#endif

   return create_long_string (s, len, hash);
}

char *_SLstring_dup_slstring (char *s)
{
   SLstring_Type *sls;
   unsigned int len;
   unsigned long hash;
#if _SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
   
   cs = GET_CACHED_STRING(s);
   if (((sls = cs->sls) != NULL)
       && (sls->bytes == s))
     {
	sls->ref_count += 1;
	return s;
     }
#endif
   
   if ((s == NULL) || ((len = strlen (s)) < 2))
     return s;

   hash = _SLstring_hash ((unsigned char *)s, (unsigned char *)(s + len));

   sls = find_slstring (s, hash);
   if (sls == NULL)
     {
	SLang_Error = SL_INTERNAL_ERROR;
	return NULL;
     }
   
   sls->ref_count++;
#if _SLANG_OPTIMIZE_FOR_SPEED
   cache_string (sls, len, hash);
#endif
   return s;
}

static void free_sls_string (SLstring_Type *sls, char *s, unsigned int len, 
			     unsigned long hash)
{
   SLstring_Type *sls1, *prev;

#if _SLANG_OPTIMIZE_FOR_SPEED
   uncache_string (s);
#else
   (void) s;
#endif

   hash = hash % SLSTRING_HASH_TABLE_SIZE;

   sls1 = String_Hash_Table [(unsigned int) hash];

   prev = NULL;

   /* This should not fail. */
   while (sls1 != sls)
     {
	prev = sls1;
	sls1 = sls1->next;
     }

   if (prev != NULL)
     prev->next = sls->next;
   else
     String_Hash_Table [(unsigned int) hash] = sls->next;

   free_sls (sls, len);
}

_INLINE_
static void free_long_string (char *s, unsigned int len, unsigned long hash)
{
   SLstring_Type *sls;

   if (NULL == (sls = find_slstring (s, hash)))
     {
	SLang_doerror ("Application internal error: invalid attempt to free string");
	return;
     }

   sls->ref_count--;
   if (sls->ref_count != 0)
     {
#if _SLANG_OPTIMIZE_FOR_SPEED
	/* cache_string (sls, len, hash); */
#endif
	return;
     }
   

   free_sls_string (sls, s, len, hash);
}

/* This routine may be passed NULL-- it is not an error. */
void SLang_free_slstring (char *s)
{
   unsigned long hash;
   unsigned int len;
#if _SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
   SLstring_Type *sls;

   cs = GET_CACHED_STRING(s);
   if (((sls = cs->sls) != NULL)
       && (sls->bytes == s))
     {
	if (sls->ref_count <= 1)
	  free_sls_string (sls, s, cs->len, cs->hash);
	else
	  sls->ref_count -= 1;
	return;
     }
#endif

   if (s == NULL) return;

   if ((len = strlen (s)) < 2)
     return;

   hash = _SLstring_hash ((unsigned char *)s, (unsigned char *) s + len);
   free_long_string (s, len, hash);
}

char *SLang_create_slstring (char *s)
{
   unsigned long hash;
#if _SLANG_OPTIMIZE_FOR_SPEED
   Cached_String_Type *cs;
   SLstring_Type *sls;

   cs = GET_CACHED_STRING(s);
   if (((sls = cs->sls) != NULL)
       && (sls->bytes == s))
     {
	sls->ref_count += 1;
	return s;
     }
#endif

   if (s == NULL) return NULL;
   return create_nstring (s, strlen (s), &hash);
}

void _SLfree_hashed_string (char *s, unsigned int len, unsigned long hash)
{
   if ((s == NULL) || (len < 2)) return;
   free_long_string (s, len, hash);
}


char *_SLallocate_slstring (unsigned int len)
{
   SLstring_Type *sls = allocate_sls (len);
   if (sls == NULL)
     return NULL;

   return sls->bytes;
}

void _SLunallocate_slstring (char *s, unsigned int len)
{
   SLstring_Type *sls;
   
   if (s == NULL)
     return;
   
   sls = (SLstring_Type *) (s - offsetof(SLstring_Type,bytes[0]));
   free_sls (sls, len);
}

char *_SLcreate_via_alloced_slstring (char *s, unsigned int len)
{   
   unsigned long hash;
   SLstring_Type *sls;

   if (s == NULL)
     return NULL;
   
   if (len < 2)
     {
	char *s1 = create_short_string (s, len);
	_SLunallocate_slstring (s, len);
	return s1;
     }

   /* s is not going to be in the cache because when it was malloced, its
    * value was unknown.  This simplifies the coding.
    */
   hash = _SLstring_hash ((unsigned char *)s, (unsigned char *)s + len);
   sls = find_string (s, len, hash);
   if (sls != NULL)
     {
	sls->ref_count++;
	_SLunallocate_slstring (s, len);
	s = sls->bytes;

#if _SLANG_OPTIMIZE_FOR_SPEED
	cache_string (sls, len, hash); 
#endif
	return s;
     }
	
   sls = (SLstring_Type *) (s - offsetof(SLstring_Type,bytes[0]));
   sls->ref_count = 1;

#if _SLANG_OPTIMIZE_FOR_SPEED
   cache_string (sls, len, hash);
#endif

   hash = hash % SLSTRING_HASH_TABLE_SIZE;
   sls->next = String_Hash_Table [(unsigned int)hash];
   String_Hash_Table [(unsigned int)hash] = sls;

   return s;
}

/* Note, a and b may be ordinary strings.  The result is an slstring */
char *SLang_concat_slstrings (char *a, char *b)
{
   unsigned int lena, len;
   char *c;

   lena = strlen (a);
   len = lena + strlen (b);

   c = _SLallocate_slstring (len);
   if (c == NULL)
     return NULL;

   strcpy (c, a);
   strcpy (c + lena, b);

   return _SLcreate_via_alloced_slstring (c, len);
}

