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
	t_modifiers	shiftstate_mask;
	int 		map;
};

struct character_translate_rules {
	struct press_state keys[NUM_KEYSYMS];
};

struct translate_rule {
	t_keysym rule_map[NUM_KEY_NUMS];
	t_modifiers modifiers;
};

struct translate_rules_struct {
	struct translate_rule plain;
	struct translate_rule shift;
	struct translate_rule ctrl;
	struct translate_rule alt; /* unnecessary? */
	struct translate_rule altgr;
	struct translate_rule shift_altgr;
	struct translate_rule ctrl_alt;
};
#define NUM_RULES (sizeof(struct translate_rules_struct) / \
    sizeof(struct translate_rule))

struct scancode_translate_rules {
	int keyboard;
	union {
		struct translate_rules_struct rule_structs;
		struct translate_rule rule_arr[NUM_RULES];
	} trans_rules;
};

struct raw_key_state {
	Bit32u rawprefix;
};

#define MAPS_MAX 4
struct keyboard_rules {
	struct character_translate_rules charset;
	struct scancode_translate_rules maps[MAPS_MAX];
	int activemap;
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
extern t_modifiers get_modifiers_r(t_shiftstate shiftstate);

#endif /* keystate_h */
