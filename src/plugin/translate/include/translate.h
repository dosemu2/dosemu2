#ifndef DOSEMU_TRANSLATE_H
#define DOSEMU_TRANSLATE_H

#include <stdlib.h>
#include <wchar.h>

#include "init.h"

typedef unsigned int t_unicode;

/* 0xFFFD is the unicode undefined character */

/* An invalid unicode value */
#ifndef U_VOID
#define U_VOID 0xFFFF
#endif

typedef void (*approximations_callback_t)(void *p, t_unicode approximation);
typedef void (*approximation_list_callback_t)(void *p, t_unicode symbol, t_unicode approximation);

void traverse_approximations(t_unicode symbol, void *p, approximations_callback_t callback);
void traverse_approximation_list(void *p, approximation_list_callback_t callback);

struct char_set_operations;

struct char_set {
	const int bytes_per_char;
	int chars_count;
	const t_unicode *chars;
	const int registration_no; /* iso2022 registration number */
	const char *final_chars; /* iso2022 & relatives final characters */
	const int prefered_side; /* 0 == left , 1 == right */
	const int logical_chars_count; /* The size of the whole this character set fills */

	struct char_set *c0;
	struct char_set *g0;
	struct char_set *c1;
	struct char_set *g1;

	const char *names[10];
	struct char_set_operations *ops;
	struct char_set *next;
};

struct iso2022_state {
	struct char_set *g[4];
	int gl, gr;
	int flags;
};

struct char_set_state {
	struct char_set *chars;
	union {
		mbstate_t mb_state;
		struct iso2022_state iso2022_state;
		void *extra;
	} u;
};

typedef size_t (*charset_to_unicode_t)(
	struct char_set_state *state, 
	struct char_set *set, int offset,
	t_unicode *symbol_out, const unsigned char *str, size_t in_len);
typedef size_t (*unicode_to_charset_t)(
	struct char_set_state *state, 
	struct char_set *set, int offset,
	t_unicode symbol, unsigned char *str, size_t out_len);
typedef int (*init_charset_state_t)(struct char_set_state *state);
typedef void (*cleanup_charset_state_t)(struct char_set_state *state);
typedef void (*copy_charset_state_t)(
	struct char_set_state *dst, const struct char_set_state *src);
typedef void (*foreach_callback_t)(void *callback_data, 
	t_unicode symbol, const unsigned char *bytes, size_t byte_len);
typedef void (*foreach_t)(struct char_set *set, int offset,
	void *callback_data, foreach_callback_t callback);
	
struct char_set_operations
{
	unicode_to_charset_t unicode_to_charset;
	charset_to_unicode_t charset_to_unicode;
	init_charset_state_t init;
	cleanup_charset_state_t cleanup;
	copy_charset_state_t copy;
	foreach_t foreach;
};


/* errno values:
    `EILSEQ'
          The conversion stopped because of an invalid byte sequence in
          the input.  After the call `*INBUF' points at the first byte
          of the invalid byte sequence.

    `E2BIG'
          The conversion stopped because it ran out of space in the
          output buffer.

    `EINVAL'
          The conversion stopped because of an incomplete byte sequence
          at the end of the input buffer.

    `EBADF'
          The CD argument is invalid.

*/

size_t unicode_to_charset(struct char_set_state *state, t_unicode key,
	unsigned char *outbuf, size_t out_bytes_left);
size_t charset_to_unicode(struct char_set_state *state, t_unicode *symbol,
	const unsigned char *inbuf, size_t in_bytes_left);

int init_charset_state(struct char_set_state *state, struct char_set *char_set);
void cleanup_charset_state(struct char_set_state *state);
void copy_charset_state(struct char_set_state *dst, 
	const struct char_set_state *src);
void foreach_character_mapping(struct char_set *set, 
	void *callback_data, foreach_callback_t callback);

/* char set registration & lookup */
struct char_set *lookup_charset(char *name);
struct char_set *lookup_charset_piece(
	const char *final_chars, int len, int chars_count, int bytes_per_char);
int register_charset(struct char_set *set);
int unregister_charset(struct char_set *set);

typedef void (*charsets_callback_t)(void *callback_data, struct char_set *piece);
void traverse_charsets(void *callback_data, charsets_callback_t callback);

/* widely used character set pieces */
extern struct char_set ascii_c0;
extern struct char_set ascii_g0;
extern struct char_set ascii_c1;
extern struct char_set ibm_ascii_c0;
extern struct char_set ibm_ascii_g0;

/* default character set operations */
extern struct char_set_operations dummy_charset_ops;
extern struct char_set_operations primitive_charset_ops;
extern struct char_set_operations compound_charset_ops;

#define CHARS(X) (sizeof(X)/sizeof((X)[0])),(X)

/* 
 * utility functions 
 * ====================
 */

/* number of characters in a possibly multibyte string */
size_t character_count(const struct char_set_state *in_state, const char *str,
	size_t max_str_len);

/* convert a possibly multibyte string to unicode */
size_t charset_to_unicode_string(
	struct char_set_state *state,
	t_unicode *dst,
	const char **src, size_t src_len);


/* return the number of unicode character in str */
size_t unicode_len(t_unicode *str);

/* convert a unicode string value into a number: see strtol */
extern long int unicode_to_long (const t_unicode *ptr,
	t_unicode **endptr, int base);


struct translate_config_t {
       struct char_set *video_mem_charset;  /* character set emulated dos display is in (single byte) */
       struct char_set *keyb_config_charset;  /* character set keypresses are translated into (single byte)*/
       struct char_set *output_charset;  /* character set users terminal is in (single byte) */
       struct char_set *paste_charset;  /* character set paste data comes in */
       struct char_set *keyb_charset;  /* character set keyboard input comes in */
       struct char_set *dos_charset;   /* character set used for DOS filesystem */
};

extern struct translate_config_t trconfig;


#endif /* DOSEMU_TRANSLATE_H */
