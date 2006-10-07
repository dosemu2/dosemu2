#include <stdlib.h>
#include <errno.h>
#include <setjmp.h>
#include <string.h>
#include "emu.h"
#include "translate.h"
#include "unicode_symbols.h"
#include "dosemu_debug.h"
#include "keyboard.h"

/*
 * Default/Dummy Charset operations
 * =========================
 */

static size_t charset_to_unicode_default(
	struct char_set_state *state, 
	struct char_set *set, int offset,
	t_unicode *symbol_out, const unsigned char *str, size_t in_len)
{
	if (in_len == 0) {
		errno = EINVAL;
	}
	else {
		errno = EILSEQ;
	}
	return -1;
}

static size_t unicode_to_charset_default(
	struct char_set_state *state, 
	struct char_set *set, int offset,
	t_unicode symbol, unsigned char *outbuf, size_t out_len)
{
	errno = EILSEQ;
	return -1;
}

static int init_charset_state_dummy(struct char_set_state *state)
{
	return 0;
}

static void cleanup_charset_state_dummy(struct char_set_state *state)
{
	return;
}

static void copy_charset_default(
	struct char_set_state *dst, const struct char_set_state *src)
{
	memcpy(&dst->u, &src->u, sizeof(src->u));
}

static void foreach_charset_default(struct char_set *set,
	int offset,
	void *callback_data, foreach_callback_t callback)
{
	/* I don't know anything so do this with brute force! */
	t_unicode i;
	unsigned char buff[100];

	/* Walk through all of the unicode characters and report
	 * if how they are mapped
	 */
	for(i = 0; i < 0xFFFF; i++) {
		size_t len;
		struct char_set_state state;
		init_charset_state(&state, set);
		len = unicode_to_charset(&state, i, buff, sizeof(buff));
		if (len > 0) {
			callback(callback_data, i, buff, len);
		}
		cleanup_charset_state(&state);
	}
}

struct char_set_operations dummy_charset_ops = {
	unicode_to_charset: &unicode_to_charset_default,
	charset_to_unicode: &charset_to_unicode_default,
	init:               &init_charset_state_dummy,
	cleanup:            &cleanup_charset_state_dummy,
        copy:               &copy_charset_default,
	foreach:            &foreach_charset_default,
};
/*
 * Default Primitive Charset operations
 * ======================================
 */
static size_t unicode_to_charset_primitive(
	struct char_set_state *state, 
	struct char_set *piece, int offset,
	t_unicode symbol, unsigned char *outbuf, size_t out_len)
{
	int i;
	unsigned char buff[3];
	size_t buff_len;

	buff_len = 0;

	for(i = 0; i < piece->chars_count; i++) {
		if (piece->chars[i] == symbol) {
			break;
		}
	}
	if (i < piece->chars_count) {
		buff_len = piece->bytes_per_char;
		if (buff_len == 1) {
			buff[0] = i + offset;
		}
		else if (buff_len == 2) {
			buff[0] = (i/piece->logical_chars_count) + offset;
			buff[1] = (i%piece->logical_chars_count) + offset;
		}
	} 
	if (buff_len > 0) {
		if (buff_len <= out_len) {
			memcpy(outbuf, buff, buff_len);
			return buff_len;
		}
		else {
			errno = -E2BIG;
			return -1;
		}
	}
	else {
		errno = EILSEQ;
		return -1;
	}
}

static size_t charset_to_unicode_primitive(
	struct char_set_state *state, 
	struct char_set *piece, int offset,
	t_unicode *symbol_out,
	const unsigned char *str, size_t in_len)
{
	int index;
	int max;

	max = offset + piece->logical_chars_count -1;
	index = piece->chars_count;

	if (in_len < piece->bytes_per_char) {
		errno = EINVAL;
		return -1;
	}
	if (piece->bytes_per_char == 1) {
		unsigned char ch = *str;
		if ((ch >= offset) && (ch <= max)) {
			index = ch - offset;
		}
#if 0
		u_printf("primitive: range:%02x-%02x %02x->%d\n",
			offset, max, ch, index);
#endif
	}
	else if (piece->bytes_per_char == 2) {
		if ((str[0] >= offset) && (str[0] <= max) &&
			(str[1] >= offset) && (str[1] <= max)) {
			index = ((str[0] - offset) * piece->logical_chars_count) +
				(str[1] - offset);
		}
#if 0
		u_printf("primitive: range:%02x-%02x %02x:%02x->%d\n",
			offset, max, str[0], str[1], index);
#endif
	}
	if (index < piece->chars_count) {
		*symbol_out = piece->chars[index];
		return piece->bytes_per_char;
	}
	else {
		errno = EILSEQ;
		return -1;
	}
}

static void foreach_primitive(struct char_set *piece, 
	int offset, 
	void *callback_data, foreach_callback_t callback)
{
	unsigned char buff[2];
	int i;
	for(i = 0; i < piece->chars_count; i++) {
		if (piece->bytes_per_char == 1) {
			buff[0] = i + offset;
		}
		else if (piece->bytes_per_char == 2) {
			buff[0] = (i/piece->logical_chars_count) + offset;
			buff[1] = (i%piece->logical_chars_count) + offset;
		}
		callback(callback_data,
			piece->chars[i], buff, piece->bytes_per_char);
	}
}


struct char_set_operations primitive_charset_ops =
{
	unicode_to_charset: &unicode_to_charset_primitive,
	charset_to_unicode: &charset_to_unicode_primitive,
	init:               &init_charset_state_dummy,
	cleanup:            &cleanup_charset_state_dummy,
	copy:               &copy_charset_default,
	foreach:            &foreach_primitive,
};

/* 
 * Default Compound Charset operations
 * =====================================
 */

static size_t attempt_piece(struct char_set_state *state,
	struct char_set *piece, int offset,
	t_unicode symbol,
	unsigned char *outbuf, size_t out_len)
{
	if (piece == NULL) {
		return -1;
	}
	if (piece->logical_chars_count == 94) {
		offset++;
	}
	return piece->ops->unicode_to_charset(state, piece, offset, 
		symbol, outbuf, out_len);
}

static size_t unicode_to_compound_charset(
	struct char_set_state *ch_state, 
	struct char_set *set, int offset,
	t_unicode symbol,
	unsigned char *outbuf, size_t out_len)
{
	size_t result;

	result = -1;
	errno = EILSEQ;

	if (set->g0->logical_chars_count == 94) {
		if (symbol == U_SPACE) {
			outbuf[0] = 0x20;
			result = 1;
		}
		else if (symbol == U_DELETE) {
			outbuf[0] = 0x7f;
			result = 1;
		}
	}
	if ((result == -1) && (errno == EILSEQ)) {
		result = attempt_piece(ch_state, set->g0, 0x20, symbol, outbuf, out_len);
	}
	if ((result == -1) && (errno == EILSEQ)) {
		result = attempt_piece(ch_state, set->g1, 0xA0, symbol, outbuf, out_len);
	}
	if ((result == -1) && (errno == EILSEQ)) {
		result = attempt_piece(ch_state, set->c0, 0x00, symbol, outbuf, out_len);
	}
	if ((result == -1) && (errno == EILSEQ)) {
		result = attempt_piece(ch_state, set->c1, 0x80, symbol, outbuf, out_len);
	}
	return result;
}


static size_t compound_charset_to_unicode(
	struct char_set_state *state, 
	struct char_set *set, int offset, 
	t_unicode *symbol_out,
	const unsigned char *str, size_t in_len)
{
	struct char_set *piece;
	if (set->g0->logical_chars_count == 94) {
		if (str[0] == 0x20) {
			*symbol_out = U_SPACE;
			return 1;
		}
		else if (str[0] == 0x7f) {
			*symbol_out = U_DELETE;
			return 1;
		}
	}
	if ((str[0] & 0x7f) <= 0x1F) {
		offset = str[0] & 0x80;
		piece = (offset < 0x80)?set->c0:set->c1;
	}
	else {
		offset = (str[0] & 0x80) + 0x20;
		piece = (offset < 0x80)?set->g0:set->g1;
		if (piece && piece->logical_chars_count == 94) {
			offset++;
		}
	}
	if (piece == NULL) {
		return -1;
	}
	return piece->ops->charset_to_unicode(
		state, piece, offset, symbol_out, str, in_len);
}


static void foreach_piece(
	struct char_set *piece,	int offset,
	void *callback_data, foreach_callback_t callback)
{
	if (piece->logical_chars_count == 94) {
		offset++;
	}
	piece->ops->foreach(piece, offset, callback_data, callback);
}
	
static void foreach_compound_charset(struct char_set *set, 
	int offset,
	void *callback_data, foreach_callback_t callback)
{
	foreach_piece(set->c0, 0x00, callback_data, callback);
	foreach_piece(set->g0, 0x20, callback_data, callback);
	foreach_piece(set->c1, 0x80, callback_data, callback);
	foreach_piece(set->g1, 0xA0, callback_data, callback);
}


struct char_set_operations compound_charset_ops = {
	unicode_to_charset: &unicode_to_compound_charset,
	charset_to_unicode: &compound_charset_to_unicode,
	init:               &init_charset_state_dummy,
	cleanup:            &cleanup_charset_state_dummy,
	copy:               &copy_charset_default,
	foreach:            &foreach_compound_charset,
};

/*
 * General Charset operations
 * ============================
 */

static void initialize_charset_piece(struct char_set *piece)
{
	if (!piece) {
		return;
	}
	if (!piece->next) {
		register_charset(piece);
	}
}

static void initialize_charset_ops(struct char_set *set)
{
	if (!set->ops) {
		if (set->c0 || set->c1 || set->g0 || set->g1) { 
			set->ops = &compound_charset_ops;
		}
		else if (set->chars) {
			set->ops = &primitive_charset_ops;
		}
		else {
			set->ops = &dummy_charset_ops;
		}
	}
	if (!set->ops->unicode_to_charset) {
		set->ops->unicode_to_charset = dummy_charset_ops.unicode_to_charset;
	}
	if (!set->ops->charset_to_unicode) {
		set->ops->charset_to_unicode = dummy_charset_ops.charset_to_unicode;
	}
	if (!set->ops->init) {
		set->ops->init = dummy_charset_ops.init;
	}
	if (!set->ops->cleanup) {
		set->ops->cleanup = dummy_charset_ops.cleanup;
	}
	if (!set->ops->copy) {
		set->ops->copy = dummy_charset_ops.copy;
	}
	if (!set->ops->foreach) {
		set->ops->foreach = dummy_charset_ops.foreach;
	}
}

static void initialize_charset(struct char_set *set)
{
	static const char null_string[] = "";
#if 0
	fprintf(stderr, "++initializing char_set(%p): %s\n",
		set, set->names[0]);
#endif
	if (!set->chars) {
		set->chars_count = 0;
	}
	if (!set->final_chars) {
		set->final_chars = null_string;
	}
	initialize_charset_ops(set);
	initialize_charset_piece(set->c0);
	initialize_charset_piece(set->c1);
	initialize_charset_piece(set->g0);
	initialize_charset_piece(set->g1);
#if 0
	fprintf(stderr, "--finished char_set(%p): %s\n",
		set, set->names[0]);
#endif
}

static struct char_set *char_set_list_head = 0;

struct char_set *lookup_charset(char *name)
{
	struct char_set *list = char_set_list_head;
#if 0
	fprintf(stderr, "Lookup_charset:%s\n", name);
#endif
	for(;list; list = list->next) {
		const char **ptr;
		for(ptr = list->names; *ptr; ptr++) {
			if (strcmp(*ptr, name) == 0) {
#if 0
				fprintf(stderr, "found charset:%s\n", name);
#endif
				return list;
			}
		}
	}
#if 0
	fprintf(stderr, "Counldn't find charset:%s\n", name);
#endif
	return 0;
}

struct char_set *lookup_charset_piece(
	const char *final_chars, int len, int chars_count, int bytes_per_char)
{
	struct char_set *list = char_set_list_head;
	for(; list; list = list->next) {
		if ((strlen(list->final_chars) == len) &&
			(memcmp(list->final_chars, final_chars, len) == 0) &&
			(list->logical_chars_count == chars_count) &&
			(list->bytes_per_char == bytes_per_char)) {
			return list;
		}
	}
	return 0;
}

void traverse_charsets(void *callback_data, charsets_callback_t callback)
{
	struct char_set *list = char_set_list_head;

	for(; list; list = list->next) {
		callback(callback_data, list);
	}
}

int register_charset(struct char_set *set)
{
	struct char_set *list = char_set_list_head;
	int result = 0;
#if 0
	fprintf(stderr, "+registering char_set(%p): %s\n",
		set, set->names[0]);
#endif
	if (set->next == 0) {
		if (list == 0) {
			char_set_list_head = set;
		} else {
			while (list->next) {
				list = list->next;
			}
			list->next = set;
		}
		initialize_charset(set);
	}
	else {
		result = -1;
	}
#if 0
	fprintf(stderr, "-finished char_set(%p): %s   -->%d\n",
		set, set->names[0], result);
#endif
	return result;
}

int unregister_charset(struct char_set *set)
{
	struct char_set *list = char_set_list_head;
	if (list == 0) {
		return -1;
	}
	if (list == set) {
		char_set_list_head = list->next;
	} else {
		while ((list->next != set) && (list->next != 0)) {
			list = list->next;
		}
		if (list->next == set) {
			list->next = set->next;
			set->next = 0;
		} else {
			return -1;
		}
	}
	return 0;
}

struct unicode_to_charset_state {
	jmp_buf jmp_env;
	t_unicode symbol;
	struct char_set_state *ostate;
	struct char_set *chars;
	unsigned char *outbuf;
	size_t out_bytes_left;
	size_t result;
};

static void unicode_to_charset_callback(void *p, t_unicode approximation)
{
	struct unicode_to_charset_state *state = p;

	if (debug_level('u') > 1) 
		u_printf("U: symbol %04x approximation: %04x\n",
		state->symbol, approximation);
	state->result = state->chars->ops->unicode_to_charset(
		state->ostate, state->chars, 0, approximation, 
		state->outbuf, state->out_bytes_left);

	/* done searching? */
	if ((state->result != -1) || (errno != EILSEQ)) {
		longjmp(state->jmp_env, 1);
	}
}

size_t unicode_to_charset(struct char_set_state *ostate, t_unicode symbol,
	unsigned char *outbuf, size_t out_bytes_left)
{
	struct unicode_to_charset_state state;

	if (out_bytes_left == 0) {
		errno = E2BIG;
		return -1; 
	}
	if (!ostate) {
		errno = EBADF;
		return -1;
	}
	state.chars = ostate->chars;
	if (!state.chars) {
		errno = EBADF;
		return -1;
	}

	state.result = state.chars->ops->unicode_to_charset(
		ostate, state.chars, 0, symbol, outbuf, out_bytes_left);
	
	if ((state.result == -1) && (errno == EILSEQ)) {
		state.symbol = symbol;
		state.ostate = ostate;
		state.outbuf = outbuf;
		state.out_bytes_left = out_bytes_left;
		if (setjmp(state.jmp_env) == 0) {
			traverse_approximations(state.symbol, &state,
				unicode_to_charset_callback);
		}
	}
	if ((state.result == -1) && (errno == EILSEQ)) {
		state.symbol = U_REPLACEMENT_CHARACTER;
		state.ostate = ostate;
		state.outbuf = outbuf;
		state.out_bytes_left = out_bytes_left;
		if (setjmp(state.jmp_env) == 0) {
			traverse_approximations(state.symbol, &state,
				unicode_to_charset_callback);
		}
	}

	if (debug_level('u') > 1) {
		int i;
		u_printf("U: unicode->charset charset:%s symbol:%04x -> char:",
			state.chars->names[0], symbol);
		for(i = 0; (state.result != -1) && (i < state.result); i++) {
			u_printf("%s%02x", ((i != 0)?",":""), outbuf[i]);
		}
		u_printf("...%zu", state.result);
		if (state.result == -1) {
			u_printf(":%d(%s)", errno, strerror(errno));
		}
		u_printf("\n");
	}

	return state.result;
}

size_t charset_to_unicode(struct char_set_state *state, 
	t_unicode *symbol,
	const unsigned char *inbuf, size_t in_bytes_left)
{
	struct char_set *chars;
	size_t result;

	if (!state) {
		errno = EBADF;
		return -1;
	}
	chars = state->chars;
	if (!chars || !inbuf) {
		errno = EBADF;
		return -1;
	}
	if (in_bytes_left <= 0) {
		errno = EINVAL;
		return -1;
	}
	result = chars->ops->charset_to_unicode(
		state, chars, 0, symbol, inbuf, in_bytes_left);
	if (debug_level('u') > 1) {
		int i;
		u_printf("U: charset->unicode charset:%s ", chars->names[0]);
		for(i = 0; (result != -1) && (i < result); i++) {
			u_printf("%s%02x", ((i != 0)?",":""), inbuf[i]);
		}
		u_printf(" -> symbol:%04x...%zu",
			*symbol, result);
		if (result == -1) {
			u_printf(":%d(%s)", errno, strerror(errno));
		}
		u_printf("\n");
	}
	return result;
}

int init_charset_state(struct char_set_state *state, struct char_set *chars)
{
	if (!chars || !state) {
		errno = EINVAL;
		return -1;
	}
	memset(state, '\0', sizeof(*state));
	state->chars = chars;
	return chars->ops->init(state);
}

void copy_charset_state(struct char_set_state *dst, 
	const struct char_set_state *src)
{
	if (!src || !dst) {
		return;
	}
	dst->chars = src->chars;
	src->chars->ops->copy(dst, src);
	return;
}

void cleanup_charset_state(struct char_set_state *state)
{
	if (!state) {
		return;
	}
	if (state->chars->ops && state->chars->ops->cleanup) {
		state->chars->ops->cleanup(state);
	}
}

void foreach_character_mapping(struct char_set *set, 
	void *callback_data, foreach_callback_t callback)
{
	set->ops->foreach(set, 0, callback_data, callback);
}



CONSTRUCTOR(static void init(void))
{
	register_debug_class('u', 0, "Unicode translation");
}

#define TRANSLATE_DEBUG 0

#if TRANSLATE_DEBUG
#include "video.h"
#include <string.h>
/* koi8-r -> cp866 */
const u_char koi8_to_dos[] = {
    0xa0, 0xa1, 0xa2, 0xf1, 0xa4, 0xa5, 0xa6, 0xa7,  /* A0-A7 */
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,  /* A8-AF */
    0xb0, 0xb1, 0xb2, 0xf0, 0xb4, 0xb5, 0xb6, 0xb7,  /* B0-B7 */
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,  /* B8-BF */
    0xee, 0xa0, 0xa1, 0xe6, 0xa4, 0xa5, 0xe4, 0xa3,  /* C0-C7 */
    0xe5, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,  /* C8-CF */
    0xaf, 0xef, 0xe0, 0xe1, 0xe2, 0xe3, 0xa6, 0xa2,  /* D0-D7 */
    0xec, 0xeb, 0xa7, 0xe8, 0xed, 0xe9, 0xe7, 0xea,  /* D8-DF */
    0x9e, 0x80, 0x81, 0x96, 0x84, 0x85, 0x94, 0x83,  /* E0-E7 */
    0x95, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,  /* E8-EF */
    0x8f, 0x9f, 0x90, 0x91, 0x92, 0x93, 0x86, 0x82,  /* F0-F7 */
    0x9c, 0x9b, 0x87, 0x98, 0x9d, 0x99, 0x97, 0x9a   /* F8-FF */
};
/* cp866 -> koi8-r */
static const u_char dos_to_koi8[] = {
  0xe1, 0xe2, 0xf7, 0xe7, 0xe4, 0xe5, 0xf6, 0xfa, /* 80-87 */
  0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, /* 88-8F */
  0xf2, 0xf3, 0xf4, 0xf5, 0xe6, 0xe8, 0xe3, 0xfe, /* 90-97 */
  0xfb, 0xfd, 0xff, 0xf9, 0xf8, 0xfc, 0xe0, 0xf1, /* 98-9F */
  0xc1, 0xc2, 0xd7, 0xc7, 0xc4, 0xc5, 0xd6, 0xda, /* A0-A7 */
  0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, /* A8-AF */
  0x90, 0x91, 0x92, 0x81, 0x87, 0xb2, 0xb4, 0xa7, /* B0-B7 */
  0xa6, 0xb5, 0xa1, 0xa8, 0xae, 0xad, 0xac, 0x83, /* B8-BF */
  0x84, 0x89, 0x88, 0x86, 0x80, 0x8a, 0xaf, 0xb0, /* C0-C7 */
  0xab, 0xa5, 0xbb, 0xb8, 0xb1, 0xa0, 0xbe, 0xb9, /* C8-CF */
  0xba, 0xb6, 0xb7, 0xaa, 0xa9, 0xa2, 0xa4, 0xbd, /* D0-D7 */
  0xbc, 0x85, 0x82, 0x8d, 0x8c, 0x8e, 0x8f, 0x8b, /* D8-DF */
  0xd2, 0xd3, 0xd4, 0xd5, 0xc6, 0xc8, 0xc3, 0xde, /* E0-E7 */
  0xdb, 0xdd, 0xdf, 0xd9, 0xd8, 0xdc, 0xc0, 0xd1, /* E8-EF */
  0xb3, 0xa3, 0x99, 0x98, 0x93, 0x9b, 0x9f, 0x97, /* F0-F7 */
  0x9c, 0x95, 0x9e, 0x96, 0xbf, 0x9d, 0x94, 0x9a  /* F8-FF */
};
/* iso-8859-1 -> cp437 */
const unsigned char latin_to_dos[] = {
    0,    0xad, 0x9b, 0x9c, 0,    0x9d, 0x7c, 0x15,  /* A0-A7 */
    0x22, 0,    0xa6, 0xae, 0xaa, 0x2d, 0,    0,     /* A8-AF */
    0xf8, 0xf1, 0xfd, 0xfc, 0x27, 0xe6, 0x14, 0xf9,  /* B0-B7 */
    0x2c, 0,    0xa7, 0xaf, 0xac, 0xab, 0,    0xa8,  /* B8-BF */
    0,    0,    0,    0,    0x8e, 0x8f, 0x92, 0x80,  /* C0-C7 */
    0,    0x90, 0,    0,    0,    0,    0,    0,     /* C8-CF */
    0,    0xa5, 0,    0,    0,    0,    0x99, 0,     /* D0-D7 */
    0xed, 0,    0,    0,    0x9a, 0,    0,    0xe1,  /* D8-DF */
    0x85, 0xa0, 0x83, 0,    0x84, 0x86, 0x91, 0x87,  /* E0-E7 */
    0x8a, 0x82, 0x88, 0x89, 0x8d, 0xa1, 0x8c, 0x8b,  /* E8-EF */
    0,    0xa4, 0xa2, 0x95, 0x93, 0,    0x94, 0xf6,  /* F0-F7 */
    0xed, 0x97, 0xa3, 0x96, 0x81, 0,    0,    0x98   /* F8-FF */
};

/* cp437 -> iso8859-1 */
const unsigned char dos_to_latin[] = {
   0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,  /* 80-87 */ 
   0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,  /* 88-8F */
   0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9,  /* 90-97 */
   0xff, 0xd6, 0xdc, 0xa2, 0xa3, 0xa5, 0x00, 0x00,  /* 98-9F */
   0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,  /* A0-A7 */
   0xbf, 0x00, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,  /* A8-AF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* B0-B7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* B8-BF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* C0-C7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* C8-CF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* D0-D7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* D8-DF */
   0x00, 0xdf, 0x00, 0x00, 0x00, 0x00, 0xb5, 0x00,  /* E0-E7 */
   0x00, 0x00, 0x00, 0xf0, 0x00, 0xd8, 0x00, 0x00,  /* E8-EF */
   0x00, 0xb1, 0x00, 0x00, 0x00, 0x00, 0xf7, 0x00,  /* F0-F7 */
   0xb0, 0xb7, 0x00, 0x00, 0xb3, 0xb2, 0x00, 0x00   /* F8-FF */
};  

/* iso-8859-1 -> cp850 */
const unsigned char latin1_to_dos[] = {
    0xff, 0xad, 0xbd, 0x9c, 0xcf, 0xbe, 0xdd, 0xf5,  /* A0-A7 */
    0xf9, 0xb8, 0xa6, 0xae, 0xaa, 0xf0, 0xa9, 0xee,  /* A8-AF */
    0xf8, 0xf1, 0xfd, 0xfc, 0xef, 0xe6, 0xf4, 0xfa,  /* B0-B7 */
    0xf7, 0xfb, 0xa7, 0xaf, 0xac, 0xab, 0xf3, 0xa8,  /* B8-BF */
    0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,  /* C0-C7 */
    0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,  /* C8-CF */
    0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0x9e,  /* D0-D7 */
    0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe8, 0xe1,  /* D8-DF */
    0x85, 0xa0, 0x83, 0xc6, 0x84, 0x86, 0x91, 0x87,  /* E0-E7 */
    0x8a, 0x82, 0x88, 0x89, 0x8d, 0xa1, 0x8c, 0x8b,  /* E8-EF */
    0xd0, 0xa4, 0x95, 0xa2, 0x93, 0xe4, 0x94, 0xf6,  /* F0-F7 */
    0xed, 0x97, 0xa3, 0x96, 0x81, 0xec, 0xe7, 0x98   /* F8-FF */
};

/* cp850 -> iso-8859-1 */
const unsigned char dos_to_latin1[] = {
   0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,  /* 80-87 */
   0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,  /* 88-8F */
   0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9,  /* 90-97 */
   0xff, 0xd6, 0xdc, 0xa2, 0xa3, 0xd8, 0xd7, 0x00,  /* 98-9F */
   0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,  /* A0-A7 */
   0xbf, 0xae, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,  /* A8-AF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0xc1, 0xc2, 0xc0,  /* B0-B7 */
   0xa9, 0x00, 0x00, 0x00, 0x00, 0xa2, 0xa5, 0xac,  /* B8-BF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe3, 0xc3,  /* C0-C7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa4,  /* C8-CF */
   0xf0, 0xd0, 0xca, 0xcb, 0xc8, 0x00, 0xcd, 0xce,  /* D0-D7 */
   0xcf, 0x00, 0x00, 0x00, 0x00, 0xa6, 0xcc, 0x00,  /* D8-DF */
   0xd3, 0xdf, 0xd4, 0xd2, 0xf5, 0xe5, 0xb5, 0xdc,  /* E0-E7 */
   0xfc, 0xda, 0xdb, 0xd9, 0xfd, 0xdd, 0xad, 0xb4,  /* E8-EF */
   0xad, 0xb1, 0x00, 0xbe, 0xb6, 0xa7, 0xf7, 0xb8,  /* F0-F7 */
   0xb0, 0xa8, 0xb7, 0xb9, 0xb3, 0xb2, 0x00, 0xa0   /* F8-FF */
};

/* iso-8859-2 -> cp852 */
const unsigned char latin2_to_dos[] = {
    0,    0xa4, 0xf4, 0x9d, 0xcf, 0x95, 0x97, 0xf5,  /* A0-A7 */
    0xf9, 0xe6, 0xb8, 0x9b, 0x8d, 0xf0, 0xa6, 0xbd,  /* A8-AF */
    0xf8, 0xa5, 0xf7, 0x88, 0xef, 0x96, 0x98, 0xf3,  /* B0-B7 */
    0xf2, 0xe7, 0xad, 0x9c, 0xab, 0xf1, 0xa7, 0xbe,  /* B8-BF */
    0xe8, 0xb5, 0xb6, 0xc6, 0x8e, 0x91, 0x8f, 0x80,  /* C0-C7 */
    0xac, 0x90, 0xa8, 0xd3, 0xb7, 0xd6, 0xd7, 0xd2,  /* C8-CF */
    0xd1, 0xe3, 0xd5, 0xe0, 0xe2, 0x8a, 0x99, 0x9e,  /* D0-D7 */
    0xfc, 0xde, 0xe9, 0xeb, 0x9a, 0xed, 0xdd, 0xe1,  /* D8-DF */
    0xea, 0xa0, 0x83, 0xc7, 0x84, 0x92, 0x86, 0x87,  /* E0-E7 */
    0x9f, 0x82, 0xa9, 0x89, 0xd8, 0xa1, 0x8c, 0xd4,  /* E8-EF */
    0xd0, 0xe4, 0xe5, 0xa2, 0x93, 0x8b, 0x94, 0xf6,  /* F0-F7 */
    0xfd, 0x85, 0xa3, 0xfb, 0x81, 0xec, 0xee, 0xfa   /* F8-FF */
};

/* cp852 -> iso-8859-2 */
const unsigned char dos_to_latin2[] = {
   0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xf9, 0xe6, 0xe7,  /* 80-87 */
   0xb3, 0xeb, 0xf5, 0xd5, 0xee, 0xac, 0xc4, 0xc6,  /* 88-8F */
   0xc9, 0xc5, 0xe5, 0xf4, 0xf6, 0xa5, 0xb5, 0xa6,  /* 90-97 */
   0xb6, 0xd6, 0xdc, 0xab, 0xbb, 0xa3, 0xd7, 0xe8,  /* 98-9F */
   0xe1, 0xed, 0xf3, 0xfa, 0xa1, 0xb1, 0xae, 0xbe,  /* A0-A7 */
   0xca, 0xea, 0x00, 0xbc, 0xc8, 0xba, 0xab, 0xbb,  /* A8-AF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0xc1, 0xc2, 0xcc,  /* B0-B7 */
   0xaa, 0x00, 0x00, 0x00, 0x00, 0xaf, 0xbf, 0x00,  /* B8-BF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc3, 0xe3,  /* C0-C7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa4,  /* C8-CF */
   0xf0, 0xd0, 0xcf, 0xcb, 0xef, 0xd2, 0xcd, 0xce,  /* D0-D7 */
   0xec, 0x00, 0x00, 0x00, 0x00, 0xde, 0xd9, 0x00,  /* D8-DF */
   0xd3, 0xdf, 0xd4, 0xd1, 0xf1, 0xf2, 0xa9, 0xb9,  /* E0-E7 */
   0xc0, 0xda, 0xe0, 0xdb, 0xfd, 0xdd, 0xfe, 0xb4,  /* E8-EF */
   0xad, 0xbd, 0xb2, 0xb7, 0xa2, 0xa7, 0xf7, 0xb8,  /* F0-F7 */
   0xb0, 0xa8, 0xff, 0xfb, 0xd8, 0xf8, 0x00, 0xa0   /* F8-FF */
};
int lookup_character(int index, const unsigned char *table)
{
	int result = -1;
	if (index >= 0x80) {
		result = table[index - 0x80];
	} else if (index == 124) {
		result = 0xa6;
	} else if (index == 21) {
		result = 0xa7;
	} else if (index == 20) {
		result = 0xb6;
	} else {
		result = index;
	}
	return result;
}


int reverse_lookup_character(int index, const unsigned char *table)
{
	int result = -1;
	if (index >= 0xa0) {
		result = table[index - 0xa0];
	} else {
		result = index;
	}
	return result;
}

void translate_test(void)
{
	unsigned char test_charset_new[256], test_charset_old[256];
	unsigned char reverse_charset_new[256], reverse_charset_old[256];
	const unsigned char *old_charset, *old_reverse_charset;
	struct char_set *paste_charset, *video_charset;
	int i;
	memset(test_charset_new, 0, 256);
	memset(test_charset_old, 0, 256);
	memset(reverse_charset_new, 0, 256);
	memset(reverse_charset_old, 0, 256);
	paste_charset = lookup_charset("iso8859_1_terminal");
	video_charset = lookup_charset("cp437_display");
	old_charset = dos_to_latin;
	old_reverse_charset = latin_to_dos;
	/* Build the translate tables */	
	for(i = 0; i < 256; i++) {
		t_unicode uni;
		uni = charset_to_unicode(video_charset, i);
		test_charset_new[i] = unicode_to_charset(paste_charset, uni);
		test_charset_old[i] = lookup_character(i, old_charset);
		v_printf("f:%d->%04x:%02x\n",
			i, uni, test_charset_new[i]);
	}
	for(i = 0; i < 256; i++) {
		t_unicode uni;
		uni = charset_to_unicode(paste_charset, i);
		reverse_charset_new[i] = unicode_to_charset(video_charset, uni);
		reverse_charset_old[i] = reverse_lookup_character(i, old_reverse_charset);
		v_printf("r:%d->%04x:%02x\n",
			i, uni, reverse_charset_new[i]);
	}
	/* Now spot differences */
	for(i = 0; i < 256; i++) {
		if (test_charset_new[i] != test_charset_old[i]) {
			v_printf("f: at: %d was: %02x is: %02x\n",
				i, test_charset_old[i], test_charset_new[i]);
		}
	}
	for(i = 0; i < 256; i++) {
		if (reverse_charset_new[i] != reverse_charset_old[i]) {
			v_printf("r: at: %d was: %02x is: %02x\n",
				i, reverse_charset_old[i], reverse_charset_new[i]);
		}
	}

}

#endif /* TRANSLATE_DEBUG */
