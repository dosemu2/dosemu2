#include <errno.h>
#include <setjmp.h>
#include <string.h>
#include "translate.h"
#include "unicode_symbols.h"

/* TODO:
 * implement multibyte final characters.
 * implement utf8 in iso2022
 * implement constraints on character sets.
 */
static int init_iso2022_state(struct char_set_state *ch_state)
{
	struct iso2022_state *state = &ch_state->u.iso2022_state;
	memset(state, '\0', sizeof(*state));
	/* initialize to iso8859-1 */
	state->g[0] = lookup_charset_piece("B", 1, 94, 1);
	state->g[1] = lookup_charset_piece("A", 1, 96, 1);
	state->g[2] = 0;
	state->g[3] = 0;
	state->gl = 0;
	state->gr = 1;
	state->flags = 0;
	return 0;
}

static size_t iso2022_to_unicode(
	struct char_set_state *ch_state, 
	struct char_set *set, int offset,
	t_unicode *symbol_out, const unsigned char *str, size_t str_len)
{
#define NEXT_CHAR() \
do { \
	if (consumed < str_len) { \
		ch = str[consumed++]; \
	} else { \
		goto bad_length; \
	} \
} while(0)

#define SI  0x0F /* one byte ^O */
#define SO  0x0E /* two byte ^N */
#define ESC 0x1B
#define SS2 0x8E /* also 0x1B 0x4E ... <ESC> N */
#define SS3 0x8F /* also 0x1B 0x4F ... <ESC> O */
#define CSI 0x9B

	unsigned char ch;
	size_t consumed;
	struct iso2022_state state[1];
	struct char_set *piece;

	state[0] = ch_state->u.iso2022_state;
	consumed = 1;
	ch = *str;
	if (ch == ESC) {
		/* Handle escapes to set the characters */
		int chars_count = 94;
		int bytes_per_char = 1;
		int gn = 0;
		struct char_set *iso2022_piece;
		
		NEXT_CHAR();
		
		switch (ch) {
		case '$': {
			NEXT_CHAR();
			bytes_per_char = 2;
			switch (ch) {
			case '(': gn = 0; chars_count = 94; goto set_gn;
			case ')': gn = 1; chars_count = 94; goto set_gn;
			case '*': gn = 2; chars_count = 94; goto set_gn;
			case '+': gn = 3; chars_count = 94; goto set_gn;
			case ',': gn = 0; chars_count = 96; goto set_gn;
			case '-': gn = 1; chars_count = 96; goto set_gn;
			case '.': gn = 2; chars_count = 96; goto set_gn;
			case '/': gn = 3; chars_count = 96; goto set_gn;
			case 'B': gn = 0; chars_count = 94; goto set_gn_this_char;
			default: goto bad_escape;
			}
		}
		case '(': gn = 0; chars_count = 94; goto set_gn;
		case ')': gn = 1; chars_count = 94; goto set_gn;
		case '*': gn = 2; chars_count = 94; goto set_gn;
		case '+': gn = 3; chars_count = 94; goto set_gn;
		case ',': gn = 0; chars_count = 96; goto set_gn;
		case '-': gn = 1; chars_count = 96; goto set_gn;
		case '.': gn = 2; chars_count = 96; goto set_gn;
		case '/': gn = 3; chars_count = 96; goto set_gn;
		case 'N': goto ss2;
		case 'O': goto ss3;
		default: goto bad_escape;
		}
	bad_escape:
		/* Since I can't make sense of the escape sequence
		 * reset the state, and return the characters
		 * individually, starting with the escape char.
		 */
		state[0] = ch_state->u.iso2022_state;
		consumed = 1;
		ch = *str;
		goto control_character;
	set_gn:
		NEXT_CHAR();
	set_gn_this_char:
		iso2022_piece = lookup_charset_piece(&ch, 1, chars_count, bytes_per_char);
		if (iso2022_piece) {
			state->g[gn] = iso2022_piece;
		} 
		else {
			goto bad_escape;
		}
		*symbol_out = U_VOID;
	} 
	else if (ch == SI) {
		state->gl = 0;
		*symbol_out = U_VOID;
	}
	else if (ch == SO) {
		state->gl = 1;
		*symbol_out = U_VOID;
	}
	else if (ch == SS2) {
	ss2:
		NEXT_CHAR();
		if ((ch & 0x7F) >= 0x20) {
			piece = state->g[2];
			offset = (ch & 0x80) + 0x20;
			goto lookup_char;
		} 
		else {
			goto bad_escape;
		}
	}
	else if (ch == SS3) {
	ss3:
		NEXT_CHAR();
		if ((ch & 0x7F) >= 0x20) {
			piece = state->g[3];
			offset = (ch & 0x80) + 0x20;
			goto lookup_char;
		} 
		else {
			goto bad_escape;
		}
		
	}
	else if (ch == CSI) {
		/* For now I don't understand any of these
		 * extra escapes so don't even try...
		 */
		goto bad_escape;
#if 0
		/* pretend to have a clue about directioinality escapes */
		NEXT_CHAR();
#endif
	}
	else {
		size_t result;
	control_character:
		if ((ch & 0x7f) <= 0x1f) {
			offset = ch & 0x80;
			piece = (offset < 0x80)?set->c0:set->c1;
		}
		else {
			offset = (ch & 0x80) + 0x20;
			piece = state->g[(offset < 0x80)?state->gl:state->gr];
		}
	lookup_char:
		if (piece) {
			if (piece->logical_chars_count == 94) {
				offset++;
			}
			result = piece->ops->charset_to_unicode(
				ch_state, piece, offset, symbol_out, 
				str + consumed -1, str_len - consumed +1);
		}
		else {
			errno = EILSEQ;
			result = -1;
		}
		if (result == -1) {
			goto bad_call;
		}
		consumed += result -1;
	}
	ch_state->u.iso2022_state = state[0];
	return consumed;
#if 0	
 bad_args:
	errno = EBADF;
	return -1;
#endif	
#if 0	
 bad_string:
	errno = EILSEQ;
	return -1;
#endif	
 bad_length:
	errno = EINVAL;
	return -1;
 bad_call:
	return -1;

#undef NEXT_CHAR
#undef SI
#undef SO
#undef ESC
#undef SS2
#undef SS3
#undef CSI
}

struct unicode_to_iso2022_state
{
	jmp_buf jmp_env;
	struct char_set_state *ch_state;
	struct iso2022_state state;
	struct char_set *piece;
	unsigned char outbuf[10];
	size_t out_len;
	t_unicode symbol;
	size_t result;
};

static void digup_symbol_callback(void *p, struct char_set *piece)
{
	struct unicode_to_iso2022_state *state = p;

	if (!piece) {
		return;
	}
	state->result = piece->ops->unicode_to_charset(
		state->ch_state, piece, 0,
		state->symbol, state->outbuf, state->out_len);
	
	/* success? */
	if (state->result != -1) {
		state->piece = piece;
	}
	/* done searching? */
	if ((state->result != -1) || (errno != EILSEQ)) {
		longjmp(state->jmp_env, 1);
	}
}


static void digup_symbol(struct unicode_to_iso2022_state *state)
{
	int i;
	
	if (setjmp(state->jmp_env) == 0) {
		/* first try the currently mapped character sets... */
		for(i = 0; i < 4; i++) {
			digup_symbol_callback(state, state->state.g[i]);
		}
		/* Then try everything... */
		traverse_charsets(state, digup_symbol_callback);
	}
}

static size_t unicode_to_iso2022(struct char_set_state *ch_state, 
	struct char_set *set, int offset,
	t_unicode symbol, unsigned char *outbuf, size_t out_bytes_left)
{
	size_t length;
	unsigned char data[10];
	struct unicode_to_iso2022_state state;

	/* Make a private copy of the state */
	state.state = ch_state->u.iso2022_state;

	/* set the default to no output string */
	length = 0;
	
	/* Look for a suitable translation */
	state.ch_state = ch_state;
	state.piece = 0;
	state.out_len = sizeof(state.outbuf);
	state.symbol = symbol;
	digup_symbol(&state);

	/* If found, output a translation */
	if (state.piece)  {
		const unsigned char *final_chars = state.piece->final_chars;
		int offset = 0x20;
		int out_index = 0;
		unsigned char destination;
		/* output a prefix if necessary */
		if (state.state.g[state.state.gl] == state.piece) {
			offset = 0x20;
		}
		else if (state.state.g[state.state.gr] == state.piece) {
			offset = 0xA0;
		}
		else {
			int gn;
			data[out_index++] = 0x1B;
			if (state.piece->bytes_per_char > 1) {
				data[out_index++] = '$';
			}
			if (state.piece->prefered_side == 1) {
				offset += 0x80;
			}
			destination = '(';
			if (state.piece->logical_chars_count == 96) {
				destination = ',';
			}
			if (state.piece->prefered_side != 0) {
				destination += 1;
			}
			data[out_index++] = destination;
			while(*final_chars) {
				data[out_index++] = *final_chars;
				final_chars++;
			}
			gn = state.piece->prefered_side;
			state.state.g[gn] = state.piece;
			if (offset < 0x80) {
				state.state.gl = gn;
			} else {
				state.state.gr = gn;
			}
		}
		if (state.piece->logical_chars_count == 94) {
			offset++;
		}
		state.result = state.piece->ops->unicode_to_charset(
			state.ch_state, state.piece, offset,
			state.symbol, 
			data + out_index, sizeof(data) - out_index);
		if (state.result == -1) {
			return -1;
		}
		out_index += state.result;
		length = out_index;
	}
	else {
		/* I can't translate the character; */
		goto bad_data;
	}
	if (out_bytes_left < length) {
		goto too_little_space;
	}
	/* Only if we write something save the state change */
	memcpy(outbuf, data, length);
	ch_state->u.iso2022_state = state.state;
	return length;

 too_little_space:
	errno = E2BIG;
	return -1;
 bad_data:
	errno = EILSEQ;
	return -1;
}


/* A default iso2022 implementation */
struct char_set_operations iso2022_ops = {
	.charset_to_unicode = &iso2022_to_unicode,
	.unicode_to_charset = &unicode_to_iso2022,
	.init = &init_iso2022_state,
/* 	foreach: */
};
struct char_set iso2022 = {
	.names = { "iso2022", "ctext", 0 },
	.ops = &iso2022_ops,
};

CONSTRUCTOR(static void init(void))
{
	register_charset(&iso2022);
}
