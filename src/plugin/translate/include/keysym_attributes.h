#ifndef keysym_attributes_h
#define keysym_attributes_h

#include "keyboard.h"

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
	case KEY_PAD_SLASH:	case KEY_PAD_AST:	case KEY_PAD_MINUS:
	case KEY_PAD_7:		case KEY_PAD_8:		case KEY_PAD_9:	
	case KEY_PAD_4:		case KEY_PAD_5:		case KEY_PAD_6:	case KEY_PAD_PLUS:
	case KEY_PAD_1:		case KEY_PAD_2:		case KEY_PAD_3:
	case KEY_PAD_0:		case KEY_PAD_DECIMAL:	case KEY_PAD_ENTER:

#ifdef HAVE_UNICODE_KEYB
	case KEY_PAD_HOME:	case KEY_PAD_UP:	case KEY_PAD_PGUP:
	case KEY_PAD_LEFT:	case KEY_PAD_CENTER:	case KEY_PAD_RIGHT:
	case KEY_PAD_END:	case KEY_PAD_DOWN:	case KEY_PAD_PGDN:
	case KEY_PAD_INS:	case KEY_PAD_DEL:	

	case KEY_PAD_SEPARATOR:
#endif /* HAVE_UNICODE_KEYB */
		result = TRUE;
		break;
	}
	return result;
}

#endif /* keysym_attributes_h */
