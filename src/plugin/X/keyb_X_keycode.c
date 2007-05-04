/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "config.h"
#include "keyboard.h"
#include "keyb_clients.h"
#include "emu.h"
#include "keyb_X.h"
#include "../kbd_unicode/keynum.h"
#include "translate.h"
#include "X.h"
#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>
#endif

struct X_keyb_config {
	char *X_keysym;
	char *Dosemu_keysym;
	char *next;
};
struct X_keyb_config *keyb_config = NULL;

#define MAX_X_KEYCODES 256
static t_keysym keycode_to_keynum[MAX_X_KEYCODES];

#if 0
const char *XStatusString(Status status)
{
	static const char *status_names[] =
	{
		"Success",
		"BadRequest",
		"BadValue",
		"BadWindow",
		"BadPixmap",
		"BadAtom",
		"BadCursor",
		"BadFont",
		"BadMatch",
		"BadDrawable",
		"BadAccess",
		"BadAlloc",
		"BadColor",
		"BadGC",
		"BadIDChoice",
		"BadName",
		"BadLength",
		"BadImplementation",
	};
	const char *result = "Unknown Status code";
	if (status <= sizeof(status_names)/sizeof(status_names[0])) {
		result = status_names[status];
	}
	return result;
};

static void X_print_atom(Display *display, Atom atom)
{
	char *str;
	if (atom != None) {
		str = XGetAtomName(display, atom);
		X_printf("%s", str);
		XFree(str);
	} else {
		X_printf("None");
	}
}
#define X_atom(atom) X_print_atom(display, atom)

static void display_x_components(Display *display)
{
#if HAVE_XKB
	XkbComponentNamesRec names = {
		"*","*", "*", "*", "*", "*"
	};
	XkbComponentListPtr list;
	int max_inout;
	int i;
	
	max_inout = 1024;
	list = XkbListComponents(display, XkbUseCoreKbd, &names, &max_inout);
	X_printf("X: Components(%d)\n", max_inout);
	X_printf("X:  keymaps(%d)\n", list->num_keymaps);
	for(i = 0; i < list->num_keymaps; i++) {
		X_printf("X:   flag: %d name %s\n",
			list->keymaps[i].flags,
			list->keymaps[i].name);
	}
	X_printf("X:  keycodes(%d)\n", list->num_keycodes);
	for(i = 0; i < list->num_keycodes; i++) {
		X_printf("X:   flag: %d name %s\n",
			list->keycodes[i].flags,
			list->keycodes[i].name);
	}
	X_printf("X:  types(%d)\n", list->num_types);
	for(i = 0; i < list->num_types; i++) {
		X_printf("X:   flag: %d name %s\n",
			list->types[i].flags,
			list->types[i].name);
	}
	X_printf("X:  compat(%d)\n", list->num_compat);
	for(i = 0; i < list->num_compat; i++) {
		X_printf("X:   flag: %d name %s\n",
			list->compat[i].flags,
			list->compat[i].name);
	}
	X_printf("X:  symbols(%d)\n", list->num_symbols);
	for(i = 0; i < list->num_symbols; i++) {
		X_printf("X:   flag: %d name %s\n",
			list->symbols[i].flags,
			list->symbols[i].name);
	}
	X_printf("X:  geometry(%d)\n", list->num_geometry);
	for(i = 0; i < list->num_geometry; i++) {
		X_printf("X:   flag: %d name %s\n",
			list->geometry[i].flags,
			list->geometry[i].name);
	}
	
	XkbFreeComponentList(list);
#endif /* HAVE_XKB */
}
static void display_x_keyboard(Display *display)
{
#if HAVE_XKB
	int i;
	XkbDescPtr desc;
	XkbNamesPtr names;
	XkbGeometryPtr geom;
	Status status;
	if (!USING_XKB) {
		X_printf("X: Not using Xkb extension\n");
		return;
	}
#if 0
	desc = XkbAllocKeyboard();
#else
	desc = XkbGetKeyboard(display, XkbAllComponentsMask, XkbUseCoreKbd);
#endif
	if (!desc) {
		X_printf("X: No keyboard description!\n");
		return;
	}
	X_printf("X: keycodes min:%d max:%d\n", 
		desc->min_key_code, desc->max_key_code);

#if 0
	status = XkbGetNames(display, XkbAllNamesMask, desc);
	if (status != Success) {
		X_printf("XkbGetNames failed: %d %s\n", 
			status,
			XStatusString(status)
			);
	}
#endif

	
	names = desc->names;
	if (!names) {
		X_printf("X: No keyboard names!\n");
	} else {
		X_printf("X: Names\n");
		/* Print Keyboard Names */
		X_printf("X: keycodes: "); X_atom(names->keycodes); X_printf("\n");
		X_printf("X: geometry: "); X_atom(names->geometry); X_printf("\n");
		X_printf("X: symbols: ");  X_atom(names->symbols);  X_printf("\n");
		X_printf("X: types: ");    X_atom(names->types);    X_printf("\n");
		X_printf("X: compat: ");   X_atom(names->compat);   X_printf("\n");
		for(i = 0; i < XkbNumVirtualMods; i++) {
			X_printf("X: vmod(%i): ", i); 
			X_atom(names->vmods[i]); 
			X_printf("\n");
		}
		for(i = 0; i < XkbNumIndicators; i++) {
			X_printf("X: indicator(%i): ", i); 
			X_atom(names->indicators[i]); 
			X_printf("\n");
		}
		for(i = 0; i < XkbNumKbdGroups; i++) {
			X_printf("X: group(%i): ", i); 
			X_atom(names->groups[i]); 
			X_printf("\n");
		}
		X_printf("X: key_names (%d)\n", names->num_keys);
		for(i = desc->min_key_code; i <= desc->max_key_code; i++) {
			int j;
			X_printf("X: %d->%.*s\n", 
				i, sizeof(names->keys[i].name), names->keys[i].name);
		}
		X_printf("X: key_aliases (%d)\n", names->num_key_aliases);
		for(i = 0; i < names->num_key_aliases; i++) {
			X_printf("%s->%s\n", 
				names->key_aliases[i].alias,
				names->key_aliases[i].real);
		}
		X_printf("X: radio_groups (%d)\n", names->num_rg);
		for(i = 0; i < names->num_rg; i++) {
			X_printf("X: %i->", i); 
			X_atom(names->radio_groups[i]); 
			X_printf("\n");
		}
		X_printf("X: phys_symbols: "); X_atom(names->phys_symbols); X_printf("\n");
	}


	/* Geometry */
	geom = desc->geom;
	if (!geom) {
		X_printf("X: No keyboard geometry!\n");
	} else {
		/* Print Keyboard Geometry */
		X_printf("X: Geometry "); X_atom(geom->name); X_printf("\n");
		
		X_printf("X: key_aliases(%d)\n", geom->sz_key_aliases);
		for(i = 0; i < geom->sz_key_aliases; i++) {
			X_printf("%s->%s\n", 
				geom->key_aliases[i].alias,
				geom->key_aliases[i].real);
		}
		X_printf("X: sections(%d)\n", geom->sz_sections);
		for(i = 0; i < geom->sz_sections; i++) {
			int j;
			XkbSectionPtr section;
			section = geom->sections +i;
			X_printf("X: section "); X_atom(section->name); X_printf("\n");
			for(j = 0; j < section->num_rows; j++) {
				int k;
				XkbRowPtr row;
				row = section->rows +j;
				X_printf("X:  row(%d)\n", row->num_keys);
				for(k = 0; k < row->num_keys; k++) {
					XkbKeyPtr key;
					key = row->keys + k;
					X_printf("X:   key(%d): %.*s\n", 
						k, sizeof(key->name.name), key->name.name);

				}
			}
		}
	}
	
	XkbFreeKeyboard(desc, XkbAllNamesMask, TRUE);
#endif /* HAVE_XKB */
}

#endif

#if HAVE_XKB
static int XkbFindKeycodeByName(XkbDescPtr xkb,char *name,Bool use_aliases)
{
	register int	i;

	if ((!xkb)||(!xkb->names)||(!xkb->names->keys))
		return 0;
	for (i=xkb->min_key_code;i<=xkb->max_key_code;i++) {
		if (strncmp(xkb->names->keys[i].name,name,XkbKeyNameLength)==0)
			return i;
	}
	if (!use_aliases)
		return 0;
	if (xkb->geom && xkb->geom->key_aliases) {
		XkbKeyAliasPtr	a;
		a= xkb->geom->key_aliases;
		for (i=0;i<xkb->geom->num_key_aliases;i++,a++) {
			if (strncmp(name,a->alias,XkbKeyNameLength)==0)
				return XkbFindKeycodeByName(xkb,a->real,False);
		}
	}
	if (xkb->names && xkb->names->key_aliases) {
		XkbKeyAliasPtr	a;
		a= xkb->names->key_aliases;
		for (i=0;i<xkb->names->num_key_aliases;i++,a++) {
			if (strncmp(name,a->alias,XkbKeyNameLength)==0)
				return XkbFindKeycodeByName(xkb,a->real,False);
		}
	}
	return 0;
}
#endif


/* List the X keycode to dosemu keynum mapping by X keycode name.
 * This list is all we need to handle general pc style keyboards,
 * and to work reasonably well on most others.
 *
 * This list doesn't handle all keycodes.  If this causes
 * a problem feel free to extend it.  The only rule is that items
 * earlier in the list (if found) will have precedence over items
 * later in the list.
 */
static const struct {
	t_keynum keynum;
	char *keycode_name;
} keynum_from_keycode[] =
{
	/* shift keys */
	{ NUM_L_ALT	, "LALT"},
	{ NUM_R_ALT	, "RALT"},
	{ NUM_L_CTRL	, "LCTL"},
	{ NUM_R_CTRL	, "RCTL"},
	{ NUM_L_SHIFT	, "LFSH"},
	{ NUM_R_SHIFT	, "RTSH"},
	{ NUM_NUM	, "NMLK"},
	{ NUM_SCROLL	, "SCLK"},
	{ NUM_CAPS	, "CAPS"},

/* main block */

	{ NUM_SPACE      , "SPCE" },
	{ NUM_BKSP	 , "BKSP" },
	{ NUM_RETURN	 , "RTRN" },
	{ NUM_TAB	 , "TAB" },
	{ NUM_A		 , "AC01" },
	{ NUM_B		 , "AB05" },
	{ NUM_C		 , "AB03" },
	{ NUM_D		 , "AC03" },
	{ NUM_E		 , "AD03" },
	{ NUM_F		 , "AC04" },
	{ NUM_G		 , "AC05" },
	{ NUM_H		 , "AC06" },
	{ NUM_I		 , "AD08" },
	{ NUM_J		 , "AC07" },
	{ NUM_K		 , "AC08" },
	{ NUM_L		 , "AC09" },
	{ NUM_M		 , "AB07" },
	{ NUM_N		 , "AB06" },
	{ NUM_O		 , "AD09" },
	{ NUM_P		 , "AD10" },
	{ NUM_Q		 , "AD01" },
	{ NUM_R		 , "AD04" },
	{ NUM_S		 , "AC02" },
	{ NUM_T		 , "AD05" },
	{ NUM_U		 , "AD07" },
	{ NUM_V		 , "AB04" },
	{ NUM_W		 , "AD02" },
	{ NUM_X		 , "AB02" },
	{ NUM_Y		 , "AD06" },
	{ NUM_Z		 , "AB01" },
	{ NUM_1		 , "AE01" },
	{ NUM_2		 , "AE02" },
	{ NUM_3		 , "AE03" },
	{ NUM_4		 , "AE04" },
	{ NUM_5		 , "AE05" },
	{ NUM_6		 , "AE06" },
	{ NUM_7		 , "AE07" },
	{ NUM_8		 , "AE08" },
	{ NUM_9		 , "AE09" },
	{ NUM_0		 , "AE10" },
	{ NUM_DASH	 , "AE11" },
	{ NUM_EQUALS	 , "AE12" },
	{ NUM_LBRACK	 , "AD11" },
	{ NUM_RBRACK	 , "AD12" },
	{ NUM_SEMICOLON	 , "AC10" },
	{ NUM_APOSTROPHE , "AC11" },
	{ NUM_GRAVE	 , "TLDE" },
	{ NUM_BACKSLASH	 , "BKSL" },
	{ NUM_COMMA	 , "AB08" },
	{ NUM_PERIOD	 , "AB09" },
	{ NUM_SLASH	 , "AB10" },
	{ NUM_LESSGREATER, "LSGT" },/* not on US keyboards */

	/* keypad */

	{ NUM_PAD_0	 , "KP0"},
	{ NUM_PAD_1	 , "KP1"},
	{ NUM_PAD_2	 , "KP2"},
	{ NUM_PAD_3	 , "KP3"},
	{ NUM_PAD_4	 , "KP4"},
	{ NUM_PAD_5	 , "KP5"},
	{ NUM_PAD_6	 , "KP6"},
	{ NUM_PAD_7	 , "KP7"},
	{ NUM_PAD_8	 , "KP8"},
	{ NUM_PAD_9	 , "KP9"},
	{ NUM_PAD_DECIMAL, "KPDL"},
	{ NUM_PAD_SLASH	 , "KPDV"},
	{ NUM_PAD_AST	 , "KPMU"},
	{ NUM_PAD_MINUS	 , "KPSU"},
	{ NUM_PAD_PLUS	 , "KPAD"},
	{ NUM_PAD_ENTER	 , "KPEN"},

	/* function keys */

	{ NUM_ESC, "ESC"},
	{ NUM_F1 , "FK01"},
	{ NUM_F2 , "FK02"},
	{ NUM_F3 , "FK03"},
	{ NUM_F4 , "FK04"},
	{ NUM_F5 , "FK05"},
	{ NUM_F6 , "FK06"},
	{ NUM_F7 , "FK07"},
	{ NUM_F8 , "FK08"},
	{ NUM_F9 , "FK09"},
	{ NUM_F10, "FK10"},
	{ NUM_F11, "FK11"},
	{ NUM_F12, "FK12"},

	/* cursor block */

	{ NUM_INS  , "INS"},
	{ NUM_DEL  , "DELE"},
	{ NUM_HOME , "HOME"},
	{ NUM_END  , "END"},
	{ NUM_PGUP , "PGUP"},
	{ NUM_PGDN , "PGDN"},
	{ NUM_UP   , "UP"},
	{ NUM_DOWN , "DOWN"},
	{ NUM_LEFT , "LEFT"},
	{ NUM_RIGHT, "RGHT"},

	/* New keys on a Microsoft Windows keyboard */
	{ NUM_LWIN, "LWIN"},
	{ NUM_RWIN, "RWIN"},
	{ NUM_MENU, "MENU"},

	/* Dual scancode keys */
	{ NUM_PRTSCR_SYSRQ, "PRSC"},
	{ NUM_PAUSE_BREAK,  "PAUS"},

	/* I had to the following to my X keyboard map
	 * to actually get sysrq and break to work...
	 *
	 *  <EXEC> = 92;
	 *  <BRK>  = 114;
	 *  key <EXEC> {
	 *	 type= "PC_SYSRQ",
	 *	 symbols[Group1]= [           Print,         Execute ]
	 *  };
	 *  key <BRK> {
	 *	 type= "PC_BREAK",
	 *	 symbols[Group1]= [           Pause,           Break ]
	 *  };
	 * -- EB 26 March 2000
	 */


	/* Try to catch the other scancode */
	{ NUM_PRTSCR_SYSRQ, "EXEC"},
	{ NUM_PAUSE_BREAK,  "BRK"},
	{ NUM_PAUSE_BREAK,  "BREA"},
};

static Boolean setup_keycode_to_keynum_mapping(Display *display)
{
#if HAVE_XKB
	int i;
	XkbDescPtr desc;
	desc = XkbGetKeyboard(display, XkbAllComponentsMask,
		XkbUseCoreKbd);
	if (!desc) {
		X_printf("X: No keyboard Description!\n");
		return FALSE;
	}
	for(i = 0; 
		i < sizeof(keynum_from_keycode)/sizeof(keynum_from_keycode[0]); 
		i++) {
		KeyCode xcode;
		xcode = XkbFindKeycodeByName(desc,
			keynum_from_keycode[i].keycode_name, TRUE);
		X_printf("X: looking for %s\n", keynum_from_keycode[i].keycode_name);
		if (xcode && (keycode_to_keynum[xcode] == NUM_VOID)) {
			keycode_to_keynum[xcode] = keynum_from_keycode[i].keynum;
			X_printf("X: mapping %s(%02x) -> %02x\n",
				keynum_from_keycode[i].keycode_name,
				xcode, 
				keynum_from_keycode[i].keynum);
		}
	}
	XkbFreeKeyboard(desc, XkbAllComponentsMask, TRUE);
	return TRUE;
#else /* !HAVE_XKB */
	return FALSE;
#endif /* HAVE_XKB */
}

static void setup_keycode_to_keynum(void *p, t_unicode dosemu_keysym, 
	const unsigned char *str, size_t str_len)
{
	KeySym xkey;
	Display *display = p;
	KeyCode xcode;
	t_keynum keynum;
	xkey = *((KeySym *)str);
	keynum = keysym_to_keynum(dosemu_keysym);
	xcode = XKeysymToKeycode(display, xkey);
	// Only allow detection of unshifted keys, to prevent things
	// like mapping the key used for "Pilcrow Sign (0xB6)", which
	// is (AltGr)-'r' for a german keyboard to 't', because the
	// pilcrow sign is Ctrl-T in CP437/CP850
	if (xcode && keynum != NUM_VOID &&
	    (XKeycodeToKeysym(display,xcode,0) == xkey)) {
		keycode_to_keynum[xcode] = keynum;
	}
}

static int X_keycode_initialized = 0;
static void X_keycode_initialize(Display *display)
{
	int i;
	Boolean worked;
	X_keycode_initialized = 1;


	for(i = 0; i < MAX_X_KEYCODES; i++) {
		keycode_to_keynum[i] = NUM_VOID;
	}
#if 0
	display_x_keyboard(display);
	display_x_components(display);
#endif
	worked = FALSE;
	worked = setup_keycode_to_keynum_mapping(display);
	if (!worked) {
		foreach_character_mapping(
			lookup_charset("X_keysym"),
			display, setup_keycode_to_keynum);
	}
#if 1
	for(i = 0; i < MAX_X_KEYCODES; i++) {
		t_keynum keynum = keycode_to_keynum[i];
		if (keynum != NUM_VOID) {
			k_printf("mapping keycode:%d  -> keynum: 0x%02x\n",
				 i, keynum);
		}
	}
#endif
}

static void put_keycode(int make, int keycode, t_keysym sym)
{
	t_keysym keynum;
	keynum = keycode_to_keynum[keycode];
	if (keynum == NUM_VOID) 
		return;
	move_keynum(make, keynum, sym);
}


void X_keycode_process_keys(XKeymapEvent *e)
{
	int i;
	char *key_vector;
	int length;
	if (!X_keycode_initialized) {
		X_keycode_initialize(display);
	}
	key_vector = e->key_vector;
	length = sizeof(e->key_vector);
	for(i = 0; i < length; i++) {
		int j;
		int c;
		c = key_vector[i];
		/* for each bit */
		for(j = 0; j < 8; j++) {
			int keycode;
			int pressed;
			keycode = i*8 + j;
			pressed = c & (1 << j);
			put_keycode(pressed, keycode, KEY_VOID);
		}
	}
}

void X_keycode_process_key(XKeyEvent *e)
{
	struct mapped_X_event event;
	Boolean make;
	if (!X_keycode_initialized) {
		X_keycode_initialize(display);
	}
#if 1
	k_printf("KBD:Xev: keycode = %d type = %d\n",
		 e->keycode, e->type);
#endif
	make = e->type == KeyPress;
	X_sync_shiftstate(make, e->keycode, e->state);
	map_X_event(display, e, &event);
	put_keycode(make, e->keycode, event.key);
	return;
}
