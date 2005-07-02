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
extern struct modifier_info X_mi;
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
#else
#define USING_XKB 0
#endif

void X_keycode_process_key(XKeyEvent *e);
void X_keycode_process_keys(XKeymapEvent *e);
void X_sync_shiftstate(Boolean make, KeyCode kc, unsigned int e_state);
int X11_DetectLayout (void);

#endif /* KEYB_X_H */
