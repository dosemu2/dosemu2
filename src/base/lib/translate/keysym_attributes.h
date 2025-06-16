#ifndef keysym_attributes_h
#define keysym_attributes_h

#include "keyboard/keyboard.h"

extern unsigned char keysym_attributes[];
#define KEYSYM_UNKNOWN			0x00
#define KEYSYM_LETTER			0x01
#define KEYSYM_COMBINING		0x02
#define KEYSYM_DEAD			0x02  /* A dosemu name (probably an abusive use) */
#define KEYSYM_CONTROL			0x03
#define KEYSYM_SPACE			0x04
#define KEYSYM_NUMBER			0x05
#define KEYSYM_PUNCTUATION		0x06
#define KEYSYM_SYMBOL			0x07
#define KEYSYM_INTERNAL_FUNCTION	0x08
#define KEYSYM_EXTERNAL_FUNCTION	0x09

#define is_keysym_letter(key)		(keysym_attributes[key] == KEYSYM_LETTER)
#define is_keysym_combining(key)	(keysym_attributes[key] == KEYSYM_COMBINING)
#define is_keysym_dead(key)		(keysym_attributes[key] == KEYSYM_DEAD)
#define is_keysym_control(key)		(keysym_attributes[key] == KEYSYM_CONTROL)
#define is_keysym_space(key)		(keysym_attributes[key] == KEYSYM_SPACE)
#define is_keysym_number(key)		(keysym_attributes[key] == KEYSYM_NUMBER)
#define is_keysym_punctuation(key)	(keysym_attributes[key] == KEYSYM_PUNCTUATION)
#define is_keysym_symbol(key)		(keysym_attributes[key] == KEYSYM_SYMBOL)
#define is_keysym_dosemu_key(key)	(keysym_attributes[key] == KEYSYM_INTERNAL_FUNCTION)
#define is_keysym_function(key)		(keysym_attributes[key] == KEYSYM_EXTERNAL_FUNCTION)

typedef void (*dead_keys_callback_t)(void *p, t_keysym dead_key, t_keysym in, t_keysym out);

t_keysym keysym_dead_key_translation(t_keysym dead_key, t_keysym in);
void traverse_dead_key_list(void *p, dead_keys_callback_t callback);

static inline int is_keypad_keysym(t_keysym key)
{
	int result = FALSE;
	switch(key) {
	case DKY_PAD_SLASH:	case DKY_PAD_AST:	case DKY_PAD_MINUS:
	case DKY_PAD_7:		case DKY_PAD_8:		case DKY_PAD_9:
	case DKY_PAD_4:		case DKY_PAD_5:		case DKY_PAD_6:	case DKY_PAD_PLUS:
	case DKY_PAD_1:		case DKY_PAD_2:		case DKY_PAD_3:
	case DKY_PAD_0:		case DKY_PAD_DECIMAL:	case DKY_PAD_ENTER:

#ifdef HAVE_UNICODE_KEYB
	case DKY_PAD_HOME:	case DKY_PAD_UP:	case DKY_PAD_PGUP:
	case DKY_PAD_LEFT:	case DKY_PAD_CENTER:	case DKY_PAD_RIGHT:
	case DKY_PAD_END:	case DKY_PAD_DOWN:	case DKY_PAD_PGDN:
	case DKY_PAD_INS:	case DKY_PAD_DEL:

	case DKY_PAD_SEPARATOR:
#endif /* HAVE_UNICODE_KEYB */
		result = TRUE;
		break;
	}
	return result;
}

#endif /* keysym_attributes_h */
