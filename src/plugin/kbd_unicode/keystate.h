#ifndef keystate_h
#define keystate_h

#include "keyboard.h"
#include "keyb_server.h"
#include "keynum.h"

struct key_pressed_state {
	unsigned char keys[32];
};
typedef Bit32u keystring;

struct press_state {
	unsigned char	character;
	t_keynum	key;
	t_keysym	deadsym;
	t_modifiers	shiftstate;
};

struct character_translate_rules {
	struct press_state keys[NUM_KEYSYMS];
};


struct scancode_translate_rules {
	t_keysym plain[NUM_KEY_NUMS];
	t_keysym shift[NUM_KEY_NUMS];
	t_keysym ctrl[NUM_KEY_NUMS];
	t_keysym alt[NUM_KEY_NUMS]; /* unnecessary? */
	t_keysym altgr[NUM_KEY_NUMS];
	t_keysym shift_altgr[NUM_KEY_NUMS];
	t_keysym ctrl_alt[NUM_KEY_NUMS];
};

struct raw_key_state {
	Bit32u rawprefix;
};

struct keyboard_rules {
	struct character_translate_rules charset;
	struct scancode_translate_rules map;
};

struct keyboard_state {
	struct keyboard_rules *rules;
	struct key_pressed_state keys_pressed;
	t_shiftstate shiftstate;
	int alt_num_buffer;		/* used for alt-keypad entry mode */ 
	t_keysym accent;		/* buffer for dead-key accents */
	struct raw_key_state raw_state;	/* used in putrawkey() */
};


extern t_keynum compute_functional_keynum(Boolean make, t_keynum keynum,
					  struct key_pressed_state *keystate);
extern t_keynum compute_keynum(Boolean *make_ret, 
			       t_rawkeycode code, struct raw_key_state *state);
extern Bit16u translate_key(Boolean make, t_keynum key, 
			    struct keyboard_state *state);

extern struct keyboard_state dos_keyboard_state;
extern struct keyboard_state input_keyboard_state;
extern Boolean handle_dosemu_keys(Boolean make, t_keysym key);
extern t_modifiers get_modifiers_r(struct keyboard_state *state);

#endif /* keystate_h */
