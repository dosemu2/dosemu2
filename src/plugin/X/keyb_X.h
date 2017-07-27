#ifndef KEYB_X_H
#define KEYB_X_H
struct modifier_info {
	int CapsLockMask;
	KeyCode CapsLockKeycode;
	int NumLockMask;
	KeyCode NumLockKeycode;
	int ScrollLockMask;
	KeyCode ScrollLockKeycode;
	int AltMask;
	int AltGrMask;
	int InsLockMask;
};
struct mapped_X_event {
	t_modifiers  modifiers;
	t_unicode key;
	Boolean make;
};
extern void map_X_event(Display *, XKeyEvent *, struct mapped_X_event *);
/* Globals shared with X.c */
#ifdef HAVE_XKB
extern int using_xkb;
#define USING_XKB (using_xkb)
t_unicode Xkb_lookup_key(Display *display, KeyCode keycode, unsigned int state);
int Xkb_get_group(Display *display, unsigned int *mods);
#else
#define USING_XKB 0
#endif

void X_keycode_process_key(Display *display, XKeyEvent *e);
void X_keycode_process_keys(XKeymapEvent *e);
void X_sync_shiftstate(Boolean make, KeyCode kc, unsigned int e_state);
int X11_DetectLayout (void);
void X_keycode_initialize(Display *display);
void keyb_X_init(Display *display);
KeyCode keynum_to_keycode(t_keynum keynum);
struct modifier_info X_get_modifier_info(void);

#endif /* KEYB_X_H */
