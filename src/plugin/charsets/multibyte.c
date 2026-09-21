#include <wchar.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "init.h"
#include "translate/translate.h"

/* I assume wchar_t/wint_t is in unicode ... */

/* State management... */

static int init_multibyte_state(struct char_set_state *state)
{
	memset(&state->u.mb_state,  '\0', sizeof(state->u.mb_state));
	return 0;
}

static void cleanup_multibyte_state(struct char_set_state *state)
{
	/* noop */
}

/* Multibyte translation... */
static size_t unicode_to_multibyte(struct char_set_state *state,
	struct char_set *set, int offset,
	t_unicode symbol, unsigned char *outbuf, size_t out_bytes_left)
{
	size_t result;
	if (MB_CUR_MAX > out_bytes_left) {
		errno = E2BIG;
		return -1;
	}
	result = wcrtomb((char *)outbuf, symbol, &state->u.mb_state);
	return result;
}

static size_t multibyte_to_unicode(struct char_set_state *state,
	struct char_set *set, int offset,
	t_unicode *symbol, const unsigned char *inbuf, size_t in_bytes_left)
{
	wchar_t wide_symbol;
	size_t result;
	mbstate_t saved = state->u.mb_state;

	*symbol = U_VOID;
	result = mbrtowc(&wide_symbol, (const char *)inbuf, in_bytes_left,
		&state->u.mb_state);
	if (result == -2) {
		/* mbrtowc() has swallowed the bytes into its state and wants
		 * the next ones, but our callers hand the whole character in
		 * again once the rest of it arrives.  Put the state back, or
		 * the second try sees the same bytes as a continuation and
		 * calls the character invalid. */
		state->u.mb_state = saved;
		errno = EINVAL;
		result = -1;
	}
	if (result != -1) {
		*symbol = wide_symbol;
	}
	return result;
}

static struct char_set_operations multibyte_ops = {
	.unicode_to_charset = unicode_to_multibyte,
	.charset_to_unicode = multibyte_to_unicode,
	.init = init_multibyte_state,
	.cleanup = cleanup_multibyte_state,
};

static struct char_set multibyte = {
	.names = { "default", "multibyte", 0 },
	.ops = &multibyte_ops,
};

CONSTRUCTOR(static void init(void))
{
	register_charset(&multibyte);
}
