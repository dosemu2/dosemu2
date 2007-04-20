#include <errno.h>
#include <string.h>
#include "translate.h"
/* utf8 */

static size_t utf8_to_unicode(
	struct char_set_state *state, 
	struct char_set *set, int offset,
	t_unicode *symbol, const unsigned char *str, size_t len)
{
	unsigned char ch;
	t_unicode result;
	size_t bytes_desired = 0;
	size_t i;

	*symbol = result = U_VOID;
	if (len < 1) {
		goto bad_length;
	} else {
		ch = *str;
	}
	if (ch <= 0x7f) {
		bytes_desired = 1;
		result = ch;
	}
	else if ((ch >= 0xC0) && (ch <= 0xDF)) {
		bytes_desired = 2;
		result = ch - 0xC0;
	}
	else if ((ch >= 0xE0) && (ch <= 0xEF)) {
		bytes_desired = 3;
		result = ch - 0xE0;
	}
	else if ((ch >= 0xF0) && (ch <= 0xF7)) {
		bytes_desired = 4;
		result = ch - 0xF0;
	}
	else if ((ch >= 0xF8) && (ch <= 0xFB)) {
		bytes_desired = 5;
		result = ch - 0xF8;
	}
	else if ((ch >= 0xFC) && (ch <= 0xFD)) {
		bytes_desired = 6;
		result = ch - 0xFC;
	} 
	else {
		goto bad_string;
	}
	if (len >= bytes_desired) {
		for(i = 1; i < bytes_desired; i++) {
			ch = str[i];
			if ((ch >= 0x80) && (ch <= 0xBF)) {
				result <<= 6;
				result += ch & 0x3F;
			} else {
				goto bad_string;
			}
		}
	} else {
		goto bad_length;
	}
	*symbol = result;
	return bytes_desired;
 bad_string:
	errno = EILSEQ;
	return -1;
 bad_length:
	errno = EINVAL;
	return -1;
}



static size_t unicode_to_utf8(struct char_set_state *state,
	struct char_set *set, int offset,
	t_unicode value,
	unsigned char *out_str, size_t out_len)
{
	int length = 0;
	char data[6];
	/* Never output resevered values */
	if (((value >= 0xD800) && (value <= 0xDFFF)) ||
	    ((value >= 0xFFFE) && (value <= 0xFFFF)) ||
	    (value > 0x7FFFFFFF)) {
		goto bad_data;
	}
	else if ((value >= 0x0000) && (value <= 0x007F)) {
		length = 1;
		data[0] = value;
	}
	else if ((value >= 0x0080) && (value <= 0x07FF)) {
		length = 2;
		data[0] = 0xC0 + (value >> 6);
		data[1] = 0x80 + ((value >> 0) & 0x3F);
	}
	else if ((value >= 0x0800) && (value <= 0xFFFF)) {
		length = 3;
		data[0] = 0xE0 + (value >> 12);
		data[1] = 0x80 + ((value >> 6) & 0x3F);
		data[2] = 0x80 + ((value >> 0) & 0x3F);
	}
	else if ((value >= 0x10000) && (value <= 0x1FFFFF)) {
		length = 4;
		data[0] = 0xF0 + (value >> 18);
		data[1] = 0x80 + ((value >> 12) & 0x3F);
		data[2] = 0x80 + ((value >> 6) & 0x3F);
		data[3] = 0x80 + ((value >> 0) & 0x3F);
	}
	else if ((value >= 0x200000) && (value <= 0x03FFFFFF)) {
		length = 5;
		data[0] = 0xF8 + (value >> 24);
		data[1] = 0x80 + ((value >> 18) & 0x3F);
		data[2] = 0x80 + ((value >> 12) & 0x3F);
		data[3] = 0x80 + ((value >> 6) & 0x3F);
		data[4] = 0x80 + ((value >> 0) & 0x3F);
	}
	else if ((value >= 0x04000000) && (value <= 0x7FFFFFFF)) {
		length = 6;
		data[0] = 0xFC + (value >> 30);
		data[1] = 0x80 + ((value >> 24) & 0x3F);
		data[2] = 0x80 + ((value >> 18) & 0x3F);
		data[3] = 0x80 + ((value >> 12) & 0x3F);
		data[4] = 0x80 + ((value >> 6) & 0x3F);
		data[5] = 0x80 + ((value >> 0) & 0x3F);
	} 
	if (out_len < length) {
		goto too_little_space;
	}
	memcpy(out_str, data, length);
	return length;

 too_little_space:
	errno = E2BIG;
	return -1;
 bad_data:
	errno = EILSEQ;
	return -1;
}

struct char_set_operations utf8_ops = {
	.unicode_to_charset = &unicode_to_utf8,
	.charset_to_unicode = &utf8_to_unicode,
};

struct char_set utf8 = {
	.bytes_per_char =      0,
	.registration_no =     196,
	.final_chars =         "G",
	.prefered_side =       0,
	.logical_chars_count = 0x100,  /* all of them */

	.names = { "utf8", 0 },
	.ops = &utf8_ops,
};

CONSTRUCTOR(static void init(void))
{
	register_charset(&utf8);
}
