#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "unicode_symbols.h"
#include "translate.h"
#include "dosemu_charset.h"

/*
 * Terminal Charset operations
 * ============================
 */
/* The terminal charset ops
 * A terminal character set is defined as one where
 * characters 0x00 - 0x1f & 0x7f are identity mapped with ascii.
 *
 * Unicode has this property (except for character size), allowing
 * for an implementation with no lookup tables.
 *
 * All other characters are forwarded to the original character set.
 */
static size_t terminal_to_unicode(struct char_set_state *state,
	struct char_set *set, int offset,
	t_unicode *symbol_out, const unsigned char *str, size_t in_len)
{
	size_t result;

	if ((str[0] <= 0x20) || (str[0] == 0x7f)) {
		*symbol_out = str[0];
		result = 1;
	}
	else {
		struct char_set *piece;
		piece = set->c0;
		result = piece->ops->charset_to_unicode(
			state, piece, offset,
			symbol_out, str, in_len);
	}
	return result;
}

static size_t unicode_to_terminal(struct char_set_state *state,
	struct char_set *set, int offset,
	t_unicode symbol, unsigned char *str, size_t out_len)
{
	size_t result;

	if ((symbol <= 0x20) || (symbol == 0x7f)) {
		str[0] = symbol;
		result = 1;
	}
	else {
		struct char_set *piece;
		piece = set->c0;
		result = piece->ops->unicode_to_charset(
			state, piece, offset,
			symbol, str, out_len);
		if ((result == 1) && ((str[0] <= 0x20) || (str[0] == 0x7f))) {
			/* Ditch reserved characters */
			errno = EILSEQ;
			result = -1;
		}
	}
	return result;
}

static int init_terminal_charset_state(struct char_set_state *state)
{
	struct char_set *set;
	set = state->chars;
	return set->c0->ops->init(state);
}

static void cleanup_terminal_charset_state(struct char_set_state *state)
{
	struct char_set *set;
	set = state->chars;
	return set->c0->ops->cleanup(state);
}
static void copy_terminal_charset_state(
	struct char_set_state *dst, const struct char_set_state *src)
{
	struct char_set *set;
	set = src->chars;
	set->c0->ops->copy(dst, src);
}

struct foreach_terminal_state {
	void *callback_data;
	foreach_callback_t callback;
};
static void foreach_terminal_callback(void *callback_data,
	t_unicode symbol, const unsigned char *bytes, size_t byte_len)
{
	struct foreach_terminal_state *state = callback_data;
	if ((byte_len == 1) && ((bytes[0] <= 0x20) || (bytes[0] == 0x7f))) {
		/* supress translations for the control characters.. */
		return;
	}
	state->callback(state->callback_data, symbol, bytes, byte_len);
}

static void foreach_terminal(struct char_set *set, int offset,
	void *callback_data, foreach_callback_t callback)
{
	static const unsigned char delete[] = {0x7f};
	struct foreach_terminal_state state;
	int i;

	state.callback_data = callback_data;
	state.callback = callback;
	for(i = 0; i <= 0x20; i++) {
		char buff[1];
		buff[0] = i;
		state.callback(state.callback_data, i, buff, 1);
	}
	state.callback(state.callback_data, U_DELETE, delete, 1);
	set->c0->ops->foreach(set, offset, &state, foreach_terminal_callback);
}

struct char_set_operations terminal_charset_ops = {
	unicode_to_charset: &unicode_to_terminal,
	charset_to_unicode: &terminal_to_unicode,
	init:               &init_terminal_charset_state,
	cleanup:            &cleanup_terminal_charset_state,
	copy:               &copy_terminal_charset_state,
	foreach:            &foreach_terminal,
};

/* 
 * Terminal Charset managment
 * ===========================
 */

int is_terminal_charset(struct char_set *set)
{
	/* Verify ^A - ^Z are handled correctly */
	unsigned char buff[3];
	t_unicode symbol;
	struct char_set_state test;
	int is_terminal;
	int i;

	is_terminal = 1;
	for(i = 0; is_terminal && (i <= 0x20); i++) {
		/* First the character we want translated */
		buff[0] = i;
		/* Then extra characters that can't be a part
		 * of any sane escape code...
		 */
		buff[1] = 0;
		buff[2] = 0;
		init_charset_state(&test, set);
		if ((charset_to_unicode(&test, &symbol, buff, sizeof(buff)) != 1) ||
			(symbol != i)) {
			is_terminal = 0;
		}
		cleanup_charset_state(&test);
	}
	buff[0] = 0x7f;
	buff[1] = 0;
	buff[2] = 0;
	init_charset_state(&test, set);
	if ((charset_to_unicode(&test, &symbol, buff, sizeof(buff)) != 1) ||
		(symbol != U_DELETE)) {
		is_terminal = 0;
	}
	cleanup_charset_state(&test);

	return is_terminal;
}

struct char_set *get_terminal_charset(struct char_set *set)
{
	struct char_set *result;
	if (!set) {
		return 0;
	}
	if (is_terminal_charset(set)) {
		result = set;
	}
	else {
		static const char *prefix = "terminal_";
		char *name;
		name = malloc(strlen(prefix) + strlen(set->names[0]) + 1);
		sprintf(name, "%s%s", prefix, set->names[0]);
		result = lookup_charset(name);
		if (result) {
			free(name);
		}
		else {
			result = malloc(sizeof(*result));
			memset(result, '\0', sizeof(*result));
			result->names[0] = name;
			result->names[1] = 0;
			result->c0 = set;
			result->ops = &terminal_charset_ops;
			register_charset(result);
		}
	}
	return result;
}

/*
 * Display Charset operations
 * ============================
 */

struct is_display_charset_state {
	int stateful;
	int out_of_range;
	int control;
	unsigned char byte[256];
};

static void is_display_charset_callback(void *callback_data,
	t_unicode symbol, const unsigned char *bytes, size_t byte_len)
{
	struct is_display_charset_state *state = callback_data;
	if (((symbol >= 0x00) && (symbol < 0x1F)) || 
		((symbol >= 0x80) && (symbol <= 0x9F))) {
		state->control = 1;
	}
	if (byte_len != 1) {
		state->out_of_range = 1;
	} 
	else {
		/* If a character is repeated
		 * take that as evidence of a
		 * stateful character set...
		 */
		if (state->byte[bytes[0]]) {
			state->stateful = 1;
		}
		state->byte[bytes[0]] = 1;
	}
}

int is_display_charset(struct char_set *set)
{
	struct is_display_charset_state state;
	int is_display;
	int i;

	/* A display character set (i.e. one that reads from video memory) 
	 * should have the property of having 256 characters, no state,
	 * and no control characters...
	 */

	memset(&state, '\0', sizeof(state));
	state.stateful = state.out_of_range = state.control = 0;

	foreach_character_mapping(set, &state, is_display_charset_callback);

	is_display = 1;
	for(i = 0; i < 256; i++) {
		is_display &= !!state.byte[i];
	}
	is_display &= !state.stateful;
	is_display &= !state.out_of_range;
	is_display &= !state.control;
	return is_display;
}
