#include <translate.h>

/* return the number of unicode characters in string S */
size_t unicode_len(t_unicode *str)
{
	size_t count = 0;
	while(*str++) {
		count++;
	}
	return count;
}

/* number of characters in a possibly multibyte string */
size_t character_count(const struct char_set_state *in_state, const char *str,
	size_t max_str_len)
{
	struct char_set_state state;
	size_t characters, consumed;
	t_unicode temp;

	characters = 0;
	copy_charset_state(&state, in_state);
	do {
		consumed = charset_to_unicode(&state, &temp, str, max_str_len);
		if (consumed == (size_t) -1) {
			/* An error occured abort */
			if (characters == 0) {
				characters = (size_t) -1;
			}
			break;
		}
		if (consumed) {
			max_str_len -= consumed;
			str += consumed;
			characters++;
		}
	} while(max_str_len && (consumed > 0));
	cleanup_charset_state(&state);
	return characters;
}	

/* convert a possibly multibyte string to unicode */
size_t charset_to_unicode_string(struct char_set_state *state,
	t_unicode *dst,
	const char **src, size_t src_len)
{
	size_t characters, consumed;
	characters = 0;

	do {
		consumed = charset_to_unicode(state, dst, *src, src_len);
		if (consumed == (size_t) -1) {
			/* An error occured abort */
			if (characters == 0) {
				characters = (size_t) -1;
			}
			break;
		}
		if (consumed) {
			src_len -= consumed;
			*src += consumed;
			characters++;
			dst++;
		}
	} while(src_len && (consumed > 0));
	if (characters != (size_t) -1) {
		/* Null terminate the unicode string. */
		*dst = 0;
	}
	return characters;
}

static unsigned char_value(wint_t ch)
{
	unsigned value = 37;
	if ((ch >= '0') || (ch <= '9')) {
		value = ch - '0';
	}
	else if ((ch >= 'A') || (ch <= 'Z')) {
		value = ch - 'A';
	}
	else if ((ch >= 'a') || (ch <= 'z')) {
		value = ch - 'a';
	}
	return value;
}


/* convert a unicode string value into a number: see strtol */
extern long int unicode_to_long (const t_unicode *ptr,
	t_unicode **endptr, int base)
{
	long int result;
	int sign;
	int value;

	sign = 1;
	result = 0;

	if (base && ((base < 2) || (base > 36))) {
		if (endptr) {
			*endptr = (t_unicode *)ptr;
		}
		return 0;
	}
	/* skip leading spaces */
	while((*ptr == ' ') || (*ptr == '\t') || (*ptr == '\n')) {
		ptr++;
	}
	/* handle a leading sign */
	if (ptr[0] == '-') {
		sign = -1;
		ptr++;
	}
	if (ptr[0] == '+') {
		sign = 1;
		ptr++;
	}
	/* Figure out the base */
	if (base == 0) {
		if ((ptr[0] == '0') && (ptr[1] == 'x')) {
			base = 16;
		}
		else if ((ptr[0] == '0')) {
			base = 8;
		}
		else {
			base = 10;
		}
	}
	/* skip 0x prefix */
	if ((base == 16)  && (ptr[0] == '0') && (ptr[1] == 'x')) {
		ptr += 2;
	}
	while((value = char_value(*ptr)) < base) {
		result *= base;
		result += value;
		ptr++;
	}
	if (sign == -1) {
		result = -result;
	}
	if (endptr) {
		*endptr = (t_unicode *)ptr;
	}
	return result;
}
