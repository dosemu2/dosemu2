#include <config.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <X11/X.h>
#define XK_MISCELLANY
#define XK_XKB_KEYS
#define XK_3270
#define XK_LATIN1
#define XK_LATIN2
#define XK_LATIN3
#define XK_LATIN4
#define XK_LATIN9
#define XK_KATAKANA
#define XK_ARABIC
#define XK_CYRILLIC
#define XK_GREEK
#define XK_TECHNICAL
#define XK_SPECIAL
#define XK_PUBLISHING
#define XK_APL
#define XK_HEBREW
#define XK_THAI
#define XK_KOREAN
#define XK_ARMENIAN
#define XK_GEORGIAN
#define XK_CURRENCY
#include <X11/keysymdef.h>
#include "unicode_symbols.h"
#include "translate.h"
#include "keyboard.h"

struct xkey_to_dosemu_key {
	KeySym xkey;
	t_keysym dosemu_key;
};

#ifndef KEY_VOID
#define KEY_VOID U_VOID
#endif /* KEY_VOID */

#ifndef HAVE_UNICODE_KEYB
#define KEY_PAD_HOME       KEY_VOID
#define KEY_PAD_UP         KEY_VOID
#define KEY_PAD_PGUP   	   KEY_VOID
#define KEY_PAD_LEFT   	   KEY_VOID
#define KEY_PAD_CENTER 	   KEY_VOID
#define KEY_PAD_RIGHT  	   KEY_VOID
#define KEY_PAD_END    	   KEY_VOID
#define KEY_PAD_DOWN   	   KEY_VOID
#define KEY_PAD_PGDN   	   KEY_VOID
#define KEY_PAD_INS    	   KEY_VOID
#define KEY_PAD_DEL        KEY_VOID
#define KEY_PAD_SEPARATOR  KEY_VOID
#define KEY_PAD_SPACE      KEY_VOID
#define KEY_PAD_F1         KEY_VOID
#define KEY_PAD_F2         KEY_VOID
#define KEY_PAD_F3         KEY_VOID
#define KEY_PAD_F4         KEY_VOID
#define KEY_PAD_EQUAL      KEY_VOID
#define KEY_PAD_TAB        KEY_VOID

#define KEY_F13            KEY_VOID
#define KEY_F14            KEY_VOID
#define KEY_F15            KEY_VOID
#define KEY_F16            KEY_VOID
#define KEY_F17            KEY_VOID
#define KEY_F18            KEY_VOID
#define KEY_F19            KEY_VOID
#define KEY_F20            KEY_VOID
#define KEY_F21            KEY_VOID
#define KEY_F22            KEY_VOID
#define KEY_F23            KEY_VOID
#define KEY_F24            KEY_VOID
#define KEY_F25            KEY_VOID
#define KEY_F26            KEY_VOID
#define KEY_F27            KEY_VOID
#define KEY_F28            KEY_VOID
#define KEY_F29            KEY_VOID
#define KEY_F30            KEY_VOID
#define KEY_F31            KEY_VOID
#define KEY_F32            KEY_VOID
#define KEY_F33            KEY_VOID
#define KEY_F34            KEY_VOID
#define KEY_F35            KEY_VOID

#define KEY_L_META         KEY_VOID
#define KEY_R_META         KEY_VOID
#define KEY_L_SUPER        KEY_VOID
#define KEY_R_SUPER        KEY_VOID
#define KEY_L_HYPER        KEY_VOID
#define KEY_R_HYPER        KEY_VOID

#define KEY_LEFT_TAB       KEY_VOID
#define KEY_SHIFT_LOCK     KEY_VOID
#define KEY_MULTI_KEY      KEY_VOID 
#define KEY_MODE_SWITCH    KEY_VOID 

#define KEY_DOSEMU_CLEAR   KEY_VOID
#define KEY_DOSEMU_BEGIN   KEY_VOID
#define KEY_DOSEMU_SELECT  KEY_VOID
#define KEY_DOSEMU_EXECUTE KEY_VOID
#define KEY_DOSEMU_UNDO    KEY_VOID
#define KEY_DOSEMU_REDO    KEY_VOID
#define KEY_DOSEMU_FIND    KEY_VOID
#define KEY_DOSEMU_CANCEL  KEY_VOID
#define KEY_DOSEMU_HELP    KEY_VOID
#endif /* !HAVE_UNICODE_KEYB */

#ifndef XK_BackSpace
#define XK_BackSpace XK_VoidSymbol
#endif

static struct xkey_to_dosemu_key keysym_map[] = {
{ XK_VoidSymbol, KEY_VOID },

#ifdef XK_MISCELLANY
#ifdef XK_BackSpace
/*
 * TTY Functions, cleverly chosen to map to ascii, for convenience of
 * programming, but could have been arbitrary (at the cost of lookup
 * tables in client code.
 */

{ XK_BackSpace,		KEY_BKSP },
{ XK_Tab,		KEY_TAB },
{ XK_Linefeed,		U_LINE_FEED },
{ XK_Clear,		KEY_DOSEMU_CLEAR },
{ XK_Return,		KEY_RETURN },
{ XK_Pause,		KEY_PAUSE },
{ XK_Scroll_Lock,	KEY_SCROLL },
{ XK_Sys_Req,		KEY_SYSRQ },
{ XK_Escape,		KEY_ESC },
{ XK_Delete,		KEY_DEL },

/* International & multi-key character composition */

{ XK_Multi_key,		KEY_MULTI_KEY },

/* Japanese keyboard support */

{ XK_Kanji,		KEY_VOID },
{ XK_Muhenkan,		KEY_VOID },
{ XK_Henkan_Mode,	KEY_VOID },
{ XK_Henkan,		KEY_VOID },
{ XK_Romaji,		KEY_VOID },
{ XK_Hiragana,		KEY_VOID },
{ XK_Katakana,		KEY_VOID },
{ XK_Hiragana_Katakana,	KEY_VOID },
{ XK_Zenkaku,		KEY_VOID },
{ XK_Hankaku,		KEY_VOID },
{ XK_Zenkaku_Hankaku,	KEY_VOID },
{ XK_Touroku,		KEY_VOID },
{ XK_Massyo,		KEY_VOID },
{ XK_Kana_Lock,		KEY_VOID },
{ XK_Kana_Shift,	KEY_VOID },
{ XK_Eisu_Shift,	KEY_VOID },
{ XK_Eisu_toggle,	KEY_VOID },

/* 0xFF31 thru 0xFF3F are under XK_KOREAN */

/* Cursor control & motion */

{ XK_Home,		KEY_HOME },
{ XK_Left,		KEY_LEFT },
{ XK_Up,		KEY_UP },
{ XK_Right,		KEY_RIGHT },
{ XK_Down,		KEY_DOWN },
{ XK_Prior,		KEY_PGUP },
{ XK_Page_Up,		KEY_PGUP },
{ XK_Next,		KEY_PGDN },
{ XK_Page_Down,		KEY_PGDN },
{ XK_End,		KEY_END },
{ XK_Begin,		KEY_DOSEMU_BEGIN },


/* Misc Functions */

{ XK_Select,		KEY_DOSEMU_SELECT },
{ XK_Print,		KEY_PRTSCR },
#if 0
{ XK_Execute,		KEY_DOSEMU_EXECUTE },
#else
{ XK_Execute,		KEY_SYSRQ },
#endif
{ XK_Insert,		KEY_INS },
{ XK_Undo,		KEY_DOSEMU_UNDO },
{ XK_Redo,		KEY_DOSEMU_REDO },
{ XK_Menu,		KEY_DOSEMU_UNDO },
{ XK_Find,		KEY_DOSEMU_FIND },
{ XK_Cancel,		KEY_DOSEMU_CANCEL },
{ XK_Help,		KEY_DOSEMU_HELP },
{ XK_Break,		KEY_BREAK },
{ XK_Mode_switch,	KEY_MODE_SWITCH },
{ XK_script_switch,     KEY_MODE_SWITCH },
{ XK_Num_Lock,		KEY_NUM },

/* Keypad Functions, keypad numbers cleverly chosen to map to ascii */

{ XK_KP_Space,		KEY_PAD_SPACE },
{ XK_KP_Tab,		KEY_PAD_TAB },
{ XK_KP_Enter,		KEY_PAD_ENTER },
{ XK_KP_F1,		KEY_PAD_F1 },
{ XK_KP_F2,		KEY_PAD_F2 },
{ XK_KP_F3,		KEY_PAD_F3 },
{ XK_KP_F4,		KEY_PAD_F4 },
{ XK_KP_Home,		KEY_PAD_HOME },
{ XK_KP_Left,		KEY_PAD_LEFT },
{ XK_KP_Up,		KEY_PAD_UP },
{ XK_KP_Right,		KEY_PAD_RIGHT },
{ XK_KP_Down,		KEY_PAD_DOWN },
{ XK_KP_Prior,		KEY_PAD_PGUP },
{ XK_KP_Page_Up,	KEY_PAD_PGUP },
{ XK_KP_Next,		KEY_PAD_PGDN },
{ XK_KP_Page_Down,	KEY_PAD_PGDN },
{ XK_KP_End,		KEY_PAD_END },
{ XK_KP_Begin,		KEY_PAD_CENTER },
{ XK_KP_Insert,		KEY_PAD_INS },
{ XK_KP_Delete,		KEY_PAD_DEL },
{ XK_KP_Equal,		KEY_PAD_EQUAL },
{ XK_KP_Multiply,	KEY_PAD_AST },
{ XK_KP_Add,		KEY_PAD_PLUS },
{ XK_KP_Separator,	KEY_PAD_SEPARATOR },
{ XK_KP_Subtract,	KEY_PAD_MINUS },
{ XK_KP_Decimal,	KEY_PAD_DECIMAL },
{ XK_KP_Divide,		KEY_PAD_SLASH },

{ XK_KP_0,		KEY_PAD_0 },
{ XK_KP_1,		KEY_PAD_1 },
{ XK_KP_2,		KEY_PAD_2 },
{ XK_KP_3,		KEY_PAD_3 },
{ XK_KP_4,		KEY_PAD_4 },
{ XK_KP_5,		KEY_PAD_5 },
{ XK_KP_6,		KEY_PAD_6 },
{ XK_KP_7,		KEY_PAD_7 },
{ XK_KP_8,		KEY_PAD_8 },
{ XK_KP_9,		KEY_PAD_9 },

/*
 * Auxilliary Functions; note the duplicate definitions for left and right
 * function keys;  Sun keyboards and a few other manufactures have such
 * function key groups on the left and/or right sides of the keyboard.
 * We've not found a keyboard with more than 35 function keys total.
 */

{ XK_F1,		KEY_F1 },
{ XK_F2,		KEY_F2 },
{ XK_F3,		KEY_F3 },
{ XK_F4,		KEY_F4 },
{ XK_F5,		KEY_F5 },
{ XK_F6,		KEY_F6 },
{ XK_F7,		KEY_F7 },
{ XK_F8,		KEY_F8 },
{ XK_F9,		KEY_F9 },
{ XK_F10,		KEY_F10 },
{ XK_F11,		KEY_F11 },
{ XK_L1,		KEY_F11 },
{ XK_F12,		KEY_F12 },
{ XK_L2,		KEY_F12 },
{ XK_F13,		KEY_F13 },
{ XK_L3,		KEY_F13 },
{ XK_F14,		KEY_F14 },
{ XK_L4,		KEY_F14 },
{ XK_F15,		KEY_F15 },
{ XK_L5,		KEY_F15 },
{ XK_F16,		KEY_F16 },
{ XK_L6,		KEY_F16 },
{ XK_F17,		KEY_F17 },
{ XK_L7,		KEY_F17 },
{ XK_F18,		KEY_F18 },
{ XK_L8,		KEY_F18 },
{ XK_F19,		KEY_F19 },
{ XK_L9,		KEY_F19 },
{ XK_F20,		KEY_F20 },
{ XK_L10,		KEY_F20 },
{ XK_F21,		KEY_F21 },
{ XK_R1,		KEY_F21 },
{ XK_F22,		KEY_F22 },
{ XK_R2,		KEY_F22 },
{ XK_F23,		KEY_F23 },
{ XK_R3,		KEY_F23 },
{ XK_F24,		KEY_F24 },
{ XK_R4,		KEY_F24 },
{ XK_F25,		KEY_F25 },
{ XK_R5,		KEY_F25 },
{ XK_F26,		KEY_F26 },
{ XK_R6,		KEY_F26 },
{ XK_F27,		KEY_F27 },
{ XK_R7,		KEY_F27 },
{ XK_F28,		KEY_F28 },
{ XK_R8,		KEY_F28 },
{ XK_F29,		KEY_F29 },
{ XK_R9,		KEY_F29 },
{ XK_F30,		KEY_F30 },
{ XK_R10,		KEY_F30 },
{ XK_F31,		KEY_F31 },
{ XK_R11,		KEY_F31 },
{ XK_F32,		KEY_F32 },
{ XK_R12,		KEY_F32 },
{ XK_F33,		KEY_F33 },
{ XK_R13,		KEY_F33 },
{ XK_F34,		KEY_F34 },
{ XK_R14,		KEY_F34 },
{ XK_F35,		KEY_F35 },
{ XK_R15,		KEY_F35 },

/* Modifiers */

{ XK_Shift_L,		KEY_L_SHIFT },
{ XK_Shift_R,		KEY_R_SHIFT },
{ XK_Control_L,		KEY_L_CTRL },
{ XK_Control_R,		KEY_R_CTRL },
{ XK_Caps_Lock,		KEY_CAPS },
{ XK_Shift_Lock,	KEY_SHIFT_LOCK },
 			
{ XK_Meta_L,		KEY_L_META },
{ XK_Meta_R,		KEY_R_META },
{ XK_Alt_L,		KEY_L_ALT },
{ XK_Alt_R,		KEY_R_ALT },
{ XK_Super_L,		KEY_L_SUPER },
{ XK_Super_R,		KEY_R_SUPER },
{ XK_Hyper_L,		KEY_L_HYPER },
{ XK_Hyper_R,		KEY_R_HYPER },
#endif /* XK_BackSpace */
#endif /* XK_MISCELLANY */

/*
 * ISO 9995 Function and Modifier Keys
 * Byte 3 = 0xFE
 */

#ifdef XK_XKB_KEYS
#ifdef XK_ISO_Lock
#if 0
#define	XK_ISO_Lock					0xFE01
#define	XK_ISO_Level2_Latch				0xFE02
#endif
{ XK_ISO_Level3_Shift,					KEY_R_ALT },
#if 0
#define	XK_ISO_Level3_Latch				0xFE04
#define	XK_ISO_Level3_Lock				0xFE05
#define	XK_ISO_Group_Shift		KEY_MODE_SWITCH }0xFF7E	/* Alias for mode_switch */
#define	XK_ISO_Group_Latch				0xFE06
#define	XK_ISO_Group_Lock				0xFE07
#define	XK_ISO_Next_Group				0xFE08
#define	XK_ISO_Next_Group_Lock				0xFE09
#define	XK_ISO_Prev_Group				0xFE0A
#define	XK_ISO_Prev_Group_Lock				0xFE0B
#define	XK_ISO_First_Group				0xFE0C
#define	XK_ISO_First_Group_Lock				0xFE0D
#define	XK_ISO_Last_Group				0xFE0E
#define	XK_ISO_Last_Group_Lock				0xFE0F
#endif

{ XK_ISO_Left_Tab,					KEY_LEFT_TAB },
#if 0
#define	XK_ISO_Move_Line_Up				0xFE21
#define	XK_ISO_Move_Line_Down				0xFE22
#define	XK_ISO_Partial_Line_Up				0xFE23
#define	XK_ISO_Partial_Line_Down			0xFE24
#define	XK_ISO_Partial_Space_Left			0xFE25
#define	XK_ISO_Partial_Space_Right			0xFE26
#define	XK_ISO_Set_Margin_Left				0xFE27
#define	XK_ISO_Set_Margin_Right				0xFE28
#define	XK_ISO_Release_Margin_Left			0xFE29
#define	XK_ISO_Release_Margin_Right			0xFE2A
#define	XK_ISO_Release_Both_Margins			0xFE2B
#define	XK_ISO_Fast_Cursor_Left				0xFE2C
#define	XK_ISO_Fast_Cursor_Right			0xFE2D
#define	XK_ISO_Fast_Cursor_Up				0xFE2E
#define	XK_ISO_Fast_Cursor_Down				0xFE2F
#define	XK_ISO_Continuous_Underline			0xFE30
#define	XK_ISO_Discontinuous_Underline			0xFE31
#define	XK_ISO_Emphasize				0xFE32
#define	XK_ISO_Center_Object				0xFE33
#define	XK_ISO_Enter					0xFE34
#endif

{ XK_dead_grave,		U_COMBINING_GRAVE_ACCENT },
{ XK_dead_acute,		U_COMBINING_ACUTE_ACCENT },
{ XK_dead_circumflex,		U_COMBINING_CIRCUMFLEX_ACCENT },
{ XK_dead_tilde,		U_COMBINING_TILDE },
{ XK_dead_macron,		U_COMBINING_MACRON },
{ XK_dead_breve,		U_COMBINING_BREVE },
{ XK_dead_abovedot,		U_COMBINING_DOT_ABOVE },
{ XK_dead_diaeresis,		U_COMBINING_DIAERESIS },
{ XK_dead_abovering,		U_COMBINING_RING_ABOVE },
{ XK_dead_doubleacute,		U_COMBINING_DOUBLE_ACUTE_ACCENT },
{ XK_dead_caron,		U_COMBINING_CARON },
{ XK_dead_cedilla,		U_COMBINING_CEDILLA },
{ XK_dead_ogonek,		U_COMBINING_OGONEK },
{ XK_dead_iota,			KEY_VOID },
{ XK_dead_voiced_sound,		U_COMBINING_KATAKANA_HIRAGANA_VOICED_SOUND_MARK },
{ XK_dead_semivoiced_sound,	U_COMBINING_KATAKANA_HIRAGANA_SEMI_VOICED_SOUND_MARK },

#if 0
#define	XK_First_Virtual_Screen				0xFED0
#define	XK_Prev_Virtual_Screen				0xFED1
#define	XK_Next_Virtual_Screen				0xFED2
#define	XK_Last_Virtual_Screen				0xFED4
#define	XK_Terminate_Server				0xFED5

#define	XK_Pointer_Left					0xFEE0
#define	XK_Pointer_Right				0xFEE1
#define	XK_Pointer_Up					0xFEE2
#define	XK_Pointer_Down					0xFEE3
#define	XK_Pointer_UpLeft				0xFEE4
#define	XK_Pointer_UpRight				0xFEE5
#define	XK_Pointer_DownLeft				0xFEE6
#define	XK_Pointer_DownRight				0xFEE7
#define	XK_Pointer_Button_Dflt				0xFEE8
#define	XK_Pointer_Button1				0xFEE9
#define	XK_Pointer_Button2				0xFEEA
#define	XK_Pointer_Button3				0xFEEB
#define	XK_Pointer_Button4				0xFEEC
#define	XK_Pointer_Button5				0xFEED
#define	XK_Pointer_DblClick_Dflt			0xFEEE
#define	XK_Pointer_DblClick1				0xFEEF
#define	XK_Pointer_DblClick2				0xFEF0
#define	XK_Pointer_DblClick3				0xFEF1
#define	XK_Pointer_DblClick4				0xFEF2
#define	XK_Pointer_DblClick5				0xFEF3
#define	XK_Pointer_Drag_Dflt				0xFEF4
#define	XK_Pointer_Drag1				0xFEF5
#define	XK_Pointer_Drag2				0xFEF6
#define	XK_Pointer_Drag3				0xFEF7
#define	XK_Pointer_Drag4				0xFEF8

#define	XK_Pointer_EnableKeys				0xFEF9
#define	XK_Pointer_Accelerate				0xFEFA
#define	XK_Pointer_DfltBtnNext				0xFEFB
#define	XK_Pointer_DfltBtnPrev				0xFEFC
#endif
#endif /* XK_ISO_Lock */
#endif

/*
 * 3270 Terminal Keys
 * Byte 3 = 0xFD
 */

#ifdef XK_3270
#ifdef XK_3270_Duplicate
#if 0
#define XK_3270_Duplicate      0xFD01
#define XK_3270_FieldMark      0xFD02
#define XK_3270_Right2         0xFD03
#define XK_3270_Left2          0xFD04
#define XK_3270_BackTab        0xFD05
#define XK_3270_EraseEOF       0xFD06
#define XK_3270_EraseInput     0xFD07
#define XK_3270_Reset          0xFD08
#define XK_3270_Quit           0xFD09
#define XK_3270_PA1            0xFD0A
#define XK_3270_PA2            0xFD0B
#define XK_3270_PA3            0xFD0C
#define XK_3270_Test           0xFD0D
#define XK_3270_Attn           0xFD0E
#define XK_3270_CursorBlink    0xFD0F
#define XK_3270_AltCursor      0xFD10
#define XK_3270_KeyClick       0xFD11
#define XK_3270_Jump           0xFD12
#define XK_3270_Ident          0xFD13
#define XK_3270_Rule           0xFD14
#define XK_3270_Copy           0xFD15
#define XK_3270_Play           0xFD16
#define XK_3270_Setup          0xFD17
#define XK_3270_Record         0xFD18
#define XK_3270_ChangeScreen   0xFD19
#define XK_3270_DeleteWord     0xFD1A
#define XK_3270_ExSelect       0xFD1B
#define XK_3270_CursorSelect   0xFD1C
#define XK_3270_PrintScreen    0xFD1D
#define XK_3270_Enter          0xFD1E
#endif
#endif /* XK_3270_Duplicate */
#endif

/*
 *  Latin 1
 *  Byte 3 = 0
 */
#ifdef XK_LATIN1
#ifdef XK_space
{ XK_space,               U_SPACE },
{ XK_exclam,              U_EXCLAMATION_MARK },
{ XK_quotedbl,            U_QUOTATION_MARK },
{ XK_numbersign,          U_NUMBER_SIGN },
{ XK_dollar,              U_DOLLAR_SIGN },
{ XK_percent,             U_PERCENT_SIGN },
{ XK_ampersand,           U_AMPERSAND },
{ XK_apostrophe,          U_APOSTROPHE },
{ XK_quoteright,          U_APOSTROPHE },	   /* deprecated */
{ XK_parenleft,           U_LEFT_PARENTHESIS },
{ XK_parenright,          U_RIGHT_PARENTHESIS },
{ XK_asterisk,            U_ASTERISK },
{ XK_plus,                U_PLUS_SIGN },
{ XK_comma,               U_COMMA },
{ XK_minus,               U_HYPHEN_MINUS },
{ XK_period,              U_PERIOD },
{ XK_slash,               U_SLASH },
{ XK_0,                   U_DIGIT_ZERO },
{ XK_1,                   U_DIGIT_ONE },
{ XK_2,                   U_DIGIT_TWO },
{ XK_3,                   U_DIGIT_THREE },
{ XK_4,                   U_DIGIT_FOUR },
{ XK_5,                   U_DIGIT_FIVE },
{ XK_6,                   U_DIGIT_SIX },
{ XK_7,                   U_DIGIT_SEVEN },
{ XK_8,                   U_DIGIT_EIGHT },
{ XK_9,                   U_DIGIT_NINE },
{ XK_colon,               U_COLON },
{ XK_semicolon,           U_SEMICOLON },
{ XK_less,                U_LESS_THAN_SIGN },
{ XK_equal,               U_EQUALS_SIGN },
{ XK_greater,             U_GREATER_THAN_SIGN },
{ XK_question,            U_QUESTION_MARK },
{ XK_at,                  U_COMMERCIAL_AT },
{ XK_A,                   U_LATIN_CAPITAL_LETTER_A },
{ XK_B,                   U_LATIN_CAPITAL_LETTER_B },
{ XK_C,                   U_LATIN_CAPITAL_LETTER_C },
{ XK_D,                   U_LATIN_CAPITAL_LETTER_D },
{ XK_E,                   U_LATIN_CAPITAL_LETTER_E },
{ XK_F,			  U_LATIN_CAPITAL_LETTER_F },
{ XK_G,			  U_LATIN_CAPITAL_LETTER_G },
{ XK_H,			  U_LATIN_CAPITAL_LETTER_H },
{ XK_I,			  U_LATIN_CAPITAL_LETTER_I },
{ XK_J,			  U_LATIN_CAPITAL_LETTER_J },
{ XK_K,			  U_LATIN_CAPITAL_LETTER_K },
{ XK_L,			  U_LATIN_CAPITAL_LETTER_L },
{ XK_M,			  U_LATIN_CAPITAL_LETTER_M },
{ XK_N,			  U_LATIN_CAPITAL_LETTER_N },
{ XK_O,			  U_LATIN_CAPITAL_LETTER_O },
{ XK_P,			  U_LATIN_CAPITAL_LETTER_P },
{ XK_Q,			  U_LATIN_CAPITAL_LETTER_Q },
{ XK_R,			  U_LATIN_CAPITAL_LETTER_R },
{ XK_S,			  U_LATIN_CAPITAL_LETTER_S },
{ XK_T,			  U_LATIN_CAPITAL_LETTER_T },
{ XK_U,			  U_LATIN_CAPITAL_LETTER_U },
{ XK_V,			  U_LATIN_CAPITAL_LETTER_V },
{ XK_W,			  U_LATIN_CAPITAL_LETTER_W },
{ XK_X,			  U_LATIN_CAPITAL_LETTER_X },
{ XK_Y,			  U_LATIN_CAPITAL_LETTER_Y },
{ XK_Z,			  U_LATIN_CAPITAL_LETTER_Z },
{ XK_bracketleft,         U_LEFT_SQUARE_BRACKET },
{ XK_backslash,           U_BACKSLASH },
{ XK_bracketright,        U_RIGHT_SQUARE_BRACKET },
{ XK_asciicircum,         U_CIRCUMFLEX_ACCENT },
{ XK_underscore,          U_LOW_LINE },
{ XK_grave,               U_GRAVE_ACCENT },
{ XK_quoteleft,           U_GRAVE_ACCENT },	   /* deprecated */
{ XK_a,			  U_LATIN_SMALL_LETTER_A },
{ XK_b,			  U_LATIN_SMALL_LETTER_B },
{ XK_c,			  U_LATIN_SMALL_LETTER_C },
{ XK_d,			  U_LATIN_SMALL_LETTER_D },
{ XK_e,			  U_LATIN_SMALL_LETTER_E },
{ XK_f,			  U_LATIN_SMALL_LETTER_F },
{ XK_g,			  U_LATIN_SMALL_LETTER_G },
{ XK_h,			  U_LATIN_SMALL_LETTER_H },
{ XK_i,			  U_LATIN_SMALL_LETTER_I },
{ XK_j,			  U_LATIN_SMALL_LETTER_J },
{ XK_k,			  U_LATIN_SMALL_LETTER_K },
{ XK_l,			  U_LATIN_SMALL_LETTER_L },
{ XK_m,			  U_LATIN_SMALL_LETTER_M },
{ XK_n,			  U_LATIN_SMALL_LETTER_N },
{ XK_o,			  U_LATIN_SMALL_LETTER_O },
{ XK_p,			  U_LATIN_SMALL_LETTER_P },
{ XK_q,			  U_LATIN_SMALL_LETTER_Q },
{ XK_r,			  U_LATIN_SMALL_LETTER_R },
{ XK_s,			  U_LATIN_SMALL_LETTER_S },
{ XK_t,			  U_LATIN_SMALL_LETTER_T },
{ XK_u,			  U_LATIN_SMALL_LETTER_U },
{ XK_v,			  U_LATIN_SMALL_LETTER_V },
{ XK_w,			  U_LATIN_SMALL_LETTER_W },
{ XK_x,			  U_LATIN_SMALL_LETTER_X },
{ XK_y,			  U_LATIN_SMALL_LETTER_Y },
{ XK_z,			  U_LATIN_SMALL_LETTER_Z },
{ XK_braceleft,           U_LEFT_CURLY_BRACKET },
{ XK_bar,                 U_VERTICAL_LINE },
{ XK_braceright,          U_RIGHT_CURLY_BRACKET },
{ XK_asciitilde,          U_TILDE },
			  
{ XK_nobreakspace,        U_NO_BREAK_SPACE },
{ XK_exclamdown,          U_INVERTED_EXCLAMATION_MARK },
{ XK_cent,        	  U_CENT_SIGN },
{ XK_sterling,            U_POUND_SIGN },
{ XK_currency,            U_CURRENCY_SIGN },
{ XK_yen,                 U_YEN_SIGN },
{ XK_brokenbar,           U_BROKEN_BAR },
{ XK_section,             U_SECTION_SIGN },
{ XK_diaeresis,           U_DIAERESIS },
{ XK_copyright,           U_COPYRIGHT_SIGN },
{ XK_ordfeminine,         U_FEMININE_ORDINAL_INDICATOR },
{ XK_guillemotleft,       U_LEFT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK },   /* left angle quotation mark */
{ XK_notsign,             U_NOT_SIGN },
{ XK_hyphen,              U_SOFT_HYPHEN },
{ XK_registered,          U_REGISTERED_SIGN },
{ XK_macron,              U_MACRON },
{ XK_degree,              U_DEGREE_SIGN },
{ XK_plusminus,           U_PLUS_MINUS_SIGN },
{ XK_twosuperior,         U_SUPERSCRIPT_TWO },
{ XK_threesuperior,       U_SUPERSCRIPT_THREE },
{ XK_acute,               U_ACUTE_ACCENT },
{ XK_mu,                  U_MICRO_SIGN },
{ XK_paragraph,           U_PILCROW_SIGN },
{ XK_periodcentered,      U_MIDDLE_DOT },
{ XK_cedilla,             U_CEDILLA },
{ XK_onesuperior,         U_SUPERSCRIPT_ONE },
{ XK_masculine,           U_MASCULINE_ORDINAL_INDICATOR },
{ XK_guillemotright,      U_RIGHT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK },   /* right angle quotation mark */
{ XK_onequarter,          U_VULGAR_FRACTION_ONE_QUARTER },
{ XK_onehalf,             U_VULGAR_FRACTION_ONE_HALF },
{ XK_threequarters,       U_VULGAR_FRACTION_THREE_QUARTERS },
{ XK_questiondown,        U_INVERTED_QUESTION_MARK },
{ XK_Agrave,              U_LATIN_CAPITAL_LETTER_A_WITH_GRAVE },
{ XK_Aacute,              U_LATIN_CAPITAL_LETTER_A_WITH_ACUTE },
{ XK_Acircumflex,         U_LATIN_CAPITAL_LETTER_A_WITH_CIRCUMFLEX },
{ XK_Atilde,              U_LATIN_CAPITAL_LETTER_A_WITH_TILDE },
{ XK_Adiaeresis,          U_LATIN_CAPITAL_LETTER_A_WITH_DIAERESIS },
{ XK_Aring,               U_LATIN_CAPITAL_LETTER_A_WITH_RING_ABOVE },
{ XK_AE,                  U_LATIN_CAPITAL_LETTER_AE },
{ XK_Ccedilla,            U_LATIN_CAPITAL_LETTER_C_WITH_CEDILLA },
{ XK_Egrave,              U_LATIN_CAPITAL_LETTER_E_WITH_GRAVE },
{ XK_Eacute,              U_LATIN_CAPITAL_LETTER_E_WITH_ACUTE },
{ XK_Ecircumflex,         U_LATIN_CAPITAL_LETTER_E_WITH_CIRCUMFLEX },
{ XK_Ediaeresis,          U_LATIN_CAPITAL_LETTER_E_WITH_DIAERESIS },
{ XK_Igrave,              U_LATIN_CAPITAL_LETTER_I_WITH_GRAVE },
{ XK_Iacute,              U_LATIN_CAPITAL_LETTER_I_WITH_ACUTE },
{ XK_Icircumflex,         U_LATIN_CAPITAL_LETTER_I_WITH_CIRCUMFLEX },
{ XK_Idiaeresis,          U_LATIN_CAPITAL_LETTER_I_WITH_DIAERESIS },
{ XK_ETH,                 U_LATIN_CAPITAL_LETTER_ETH },
{ XK_Eth,                 U_LATIN_CAPITAL_LETTER_ETH },   /* deprecated */
{ XK_Ntilde,              U_LATIN_CAPITAL_LETTER_N_WITH_TILDE },
{ XK_Ograve,              U_LATIN_CAPITAL_LETTER_O_WITH_GRAVE },
{ XK_Oacute,              U_LATIN_CAPITAL_LETTER_O_WITH_ACUTE },
{ XK_Ocircumflex,         U_LATIN_CAPITAL_LETTER_O_WITH_CIRCUMFLEX },
{ XK_Otilde,              U_LATIN_CAPITAL_LETTER_O_WITH_TILDE },
{ XK_Odiaeresis,          U_LATIN_CAPITAL_LETTER_O_WITH_DIAERESIS },
{ XK_multiply,            U_MULTIPLICATION_SIGN },
{ XK_Ooblique,            U_LATIN_CAPITAL_LETTER_O_WITH_STROKE },
{ XK_Ugrave,              U_LATIN_CAPITAL_LETTER_U_WITH_GRAVE },
{ XK_Uacute,              U_LATIN_CAPITAL_LETTER_U_WITH_ACUTE },
{ XK_Ucircumflex,         U_LATIN_CAPITAL_LETTER_U_WITH_CIRCUMFLEX },
{ XK_Udiaeresis,          U_LATIN_CAPITAL_LETTER_U_WITH_DIAERESIS },
{ XK_Yacute,              U_LATIN_CAPITAL_LETTER_Y_WITH_ACUTE },
{ XK_THORN,               U_LATIN_CAPITAL_LETTER_THORN },
{ XK_Thorn,               U_LATIN_CAPITAL_LETTER_THORN },   /* deprecated */
{ XK_ssharp,              U_LATIN_SMALL_LETTER_SHARP_S },
{ XK_agrave,              U_LATIN_SMALL_LETTER_A_WITH_GRAVE },
{ XK_aacute,              U_LATIN_SMALL_LETTER_A_WITH_ACUTE },
{ XK_acircumflex,         U_LATIN_SMALL_LETTER_A_WITH_CIRCUMFLEX },
{ XK_atilde,              U_LATIN_SMALL_LETTER_A_WITH_TILDE },
{ XK_adiaeresis,          U_LATIN_SMALL_LETTER_A_WITH_DIAERESIS },
{ XK_aring,               U_LATIN_SMALL_LETTER_A_WITH_RING_ABOVE },
{ XK_ae,                  U_LATIN_SMALL_LETTER_AE },
{ XK_ccedilla,            U_LATIN_SMALL_LETTER_C_WITH_CEDILLA },
{ XK_egrave,              U_LATIN_SMALL_LETTER_E_WITH_GRAVE },
{ XK_eacute,              U_LATIN_SMALL_LETTER_E_WITH_ACUTE },
{ XK_ecircumflex,         U_LATIN_SMALL_LETTER_E_WITH_CIRCUMFLEX },
{ XK_ediaeresis,          U_LATIN_SMALL_LETTER_E_WITH_DIAERESIS },
{ XK_igrave,              U_LATIN_SMALL_LETTER_I_WITH_GRAVE },
{ XK_iacute,              U_LATIN_SMALL_LETTER_I_WITH_ACUTE },
{ XK_icircumflex,         U_LATIN_SMALL_LETTER_I_WITH_CIRCUMFLEX },
{ XK_idiaeresis,          U_LATIN_SMALL_LETTER_I_WITH_DIAERESIS },
{ XK_eth,                 U_LATIN_SMALL_LETTER_ETH },
{ XK_ntilde,              U_LATIN_SMALL_LETTER_N_WITH_TILDE },
{ XK_ograve,              U_LATIN_SMALL_LETTER_O_WITH_GRAVE },
{ XK_oacute,              U_LATIN_SMALL_LETTER_O_WITH_ACUTE },
{ XK_ocircumflex,         U_LATIN_SMALL_LETTER_O_WITH_CIRCUMFLEX },
{ XK_otilde,              U_LATIN_SMALL_LETTER_O_WITH_TILDE },
{ XK_odiaeresis,          U_LATIN_SMALL_LETTER_O_WITH_DIAERESIS },
{ XK_division,            U_DIVISION_SIGN },
{ XK_oslash,              U_LATIN_SMALL_LETTER_O_WITH_STROKE },
{ XK_ugrave,              U_LATIN_SMALL_LETTER_U_WITH_GRAVE },
{ XK_uacute,              U_LATIN_SMALL_LETTER_U_WITH_ACUTE },
{ XK_ucircumflex,         U_LATIN_SMALL_LETTER_U_WITH_CIRCUMFLEX },
{ XK_udiaeresis,          U_LATIN_SMALL_LETTER_U_WITH_DIAERESIS },
{ XK_yacute,              U_LATIN_SMALL_LETTER_Y_WITH_ACUTE },
{ XK_thorn,               U_LATIN_SMALL_LETTER_THORN },
{ XK_ydiaeresis,          U_LATIN_SMALL_LETTER_Y_WITH_DIAERESIS },
#endif /* XK_Space */
#endif /* XK_LATIN1 */

/*
 *   Latin 2
 *   Byte 3 = 1
 */

#ifdef XK_LATIN2
#ifdef XK_Aogonek
{ XK_Aogonek,		U_LATIN_CAPITAL_LETTER_A_WITH_OGONEK },
{ XK_breve,		U_BREVE },
{ XK_Lstroke,		U_LATIN_CAPITAL_LETTER_L_WITH_STROKE },
{ XK_Lcaron,		U_LATIN_CAPITAL_LETTER_L_WITH_CARON },
{ XK_Sacute,		U_LATIN_CAPITAL_LETTER_S_WITH_ACUTE },
{ XK_Scaron,		U_LATIN_CAPITAL_LETTER_S_WITH_CARON },
{ XK_Scedilla,		U_LATIN_CAPITAL_LETTER_S_WITH_CEDILLA },
{ XK_Tcaron,		U_LATIN_CAPITAL_LETTER_T_WITH_CARON },
{ XK_Zacute,		U_LATIN_CAPITAL_LETTER_Z_WITH_ACUTE },
{ XK_Zcaron,		U_LATIN_CAPITAL_LETTER_Z_WITH_CARON },
{ XK_Zabovedot,		U_LATIN_CAPITAL_LETTER_Z_WITH_DOT_ABOVE },
{ XK_aogonek,		U_LATIN_SMALL_LETTER_A_WITH_OGONEK },
{ XK_ogonek,		U_OGONEK },
{ XK_lstroke,		U_LATIN_SMALL_LETTER_L_WITH_STROKE },
{ XK_lcaron,		U_LATIN_SMALL_LETTER_L_WITH_CARON },
{ XK_sacute,		U_LATIN_SMALL_LETTER_S_WITH_ACUTE },
{ XK_caron,		U_CARON },
{ XK_scaron,		U_LATIN_SMALL_LETTER_S_WITH_CARON },
{ XK_scedilla,		U_LATIN_SMALL_LETTER_S_WITH_CEDILLA },
{ XK_tcaron,		U_LATIN_SMALL_LETTER_T_WITH_CARON },
{ XK_zacute,		U_LATIN_SMALL_LETTER_Z_WITH_ACUTE },
{ XK_doubleacute,	U_DOUBLE_ACUTE_ACCENT },
{ XK_zcaron,		U_LATIN_SMALL_LETTER_Z_WITH_CARON },
{ XK_zabovedot,		U_LATIN_SMALL_LETTER_Z_WITH_DOT_ABOVE },
{ XK_Racute,		U_LATIN_CAPITAL_LETTER_R_WITH_ACUTE },
{ XK_Abreve,		U_LATIN_CAPITAL_LETTER_A_WITH_BREVE },
{ XK_Lacute,		U_LATIN_CAPITAL_LETTER_L_WITH_ACUTE },
{ XK_Cacute,		U_LATIN_CAPITAL_LETTER_C_WITH_ACUTE },
{ XK_Ccaron,		U_LATIN_CAPITAL_LETTER_C_WITH_CARON },
{ XK_Eogonek,		U_LATIN_CAPITAL_LETTER_E_WITH_OGONEK },
{ XK_Ecaron,		U_LATIN_CAPITAL_LETTER_E_WITH_CARON },
{ XK_Dcaron,		U_LATIN_CAPITAL_LETTER_D_WITH_CARON },
{ XK_Dstroke,		U_LATIN_CAPITAL_LETTER_D_WITH_STROKE },
{ XK_Nacute,		U_LATIN_CAPITAL_LETTER_N_WITH_ACUTE },
{ XK_Ncaron,		U_LATIN_CAPITAL_LETTER_N_WITH_CARON },
{ XK_Odoubleacute,	U_LATIN_CAPITAL_LETTER_O_WITH_DOUBLE_ACUTE },
{ XK_Rcaron,		U_LATIN_CAPITAL_LETTER_R_WITH_CARON },
{ XK_Uring,		U_LATIN_CAPITAL_LETTER_U_WITH_RING_ABOVE },
{ XK_Udoubleacute,	U_LATIN_CAPITAL_LETTER_U_WITH_DOUBLE_ACUTE },
{ XK_Tcedilla,		U_LATIN_CAPITAL_LETTER_T_WITH_CEDILLA },
{ XK_racute,		U_LATIN_SMALL_LETTER_R_WITH_ACUTE },
{ XK_abreve,		U_LATIN_SMALL_LETTER_A_WITH_BREVE },
{ XK_lacute,		U_LATIN_SMALL_LETTER_L_WITH_ACUTE },
{ XK_cacute,		U_LATIN_SMALL_LETTER_C_WITH_ACUTE },
{ XK_ccaron,		U_LATIN_SMALL_LETTER_C_WITH_CARON },
{ XK_eogonek,		U_LATIN_SMALL_LETTER_E_WITH_OGONEK },
{ XK_ecaron,		U_LATIN_SMALL_LETTER_E_WITH_CARON },
{ XK_dcaron,		U_LATIN_SMALL_LETTER_D_WITH_CARON },
{ XK_dstroke,		U_LATIN_SMALL_LETTER_D_WITH_STROKE },
{ XK_nacute,		U_LATIN_SMALL_LETTER_N_WITH_ACUTE },
{ XK_ncaron,		U_LATIN_SMALL_LETTER_N_WITH_CARON },
{ XK_odoubleacute,	U_LATIN_SMALL_LETTER_O_WITH_DOUBLE_ACUTE },
{ XK_udoubleacute,	U_LATIN_SMALL_LETTER_U_WITH_DOUBLE_ACUTE },
{ XK_rcaron,		U_LATIN_SMALL_LETTER_R_WITH_CARON },
{ XK_uring,		U_LATIN_SMALL_LETTER_U_WITH_RING_ABOVE },
{ XK_tcedilla,		U_LATIN_SMALL_LETTER_T_WITH_CEDILLA },
{ XK_abovedot,		U_DOT_ABOVE },
#endif /* XK_Aogonek */
#endif /* XK_LATIN2 */

/*
 *   Latin 3
 *   Byte 3 = 2
 */

#ifdef XK_LATIN3
#ifdef XK_Hstroke
{ XK_Hstroke,             U_LATIN_CAPITAL_LETTER_H_WITH_STROKE },
{ XK_Hcircumflex,         U_LATIN_CAPITAL_LETTER_H_WITH_CIRCUMFLEX },
{ XK_Iabovedot,           U_LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE },
{ XK_Gbreve,              U_LATIN_CAPITAL_LETTER_G_WITH_BREVE },
{ XK_Jcircumflex,         U_LATIN_CAPITAL_LETTER_J_WITH_CIRCUMFLEX },
{ XK_hstroke,             U_LATIN_SMALL_LETTER_H_WITH_STROKE },
{ XK_hcircumflex,         U_LATIN_SMALL_LETTER_H_WITH_CIRCUMFLEX },
{ XK_idotless,            U_LATIN_SMALL_LETTER_DOTLESS_I },
{ XK_gbreve,              U_LATIN_SMALL_LETTER_G_WITH_BREVE },
{ XK_jcircumflex,         U_LATIN_SMALL_LETTER_J_WITH_CIRCUMFLEX },
{ XK_Cabovedot,           U_LATIN_CAPITAL_LETTER_C_WITH_DOT_ABOVE },
{ XK_Ccircumflex,         U_LATIN_CAPITAL_LETTER_C_WITH_CIRCUMFLEX },
{ XK_Gabovedot,           U_LATIN_CAPITAL_LETTER_G_WITH_DOT_ABOVE },
{ XK_Gcircumflex,         U_LATIN_CAPITAL_LETTER_G_WITH_CIRCUMFLEX },
{ XK_Ubreve,              U_LATIN_CAPITAL_LETTER_U_WITH_BREVE },
{ XK_Scircumflex,         U_LATIN_CAPITAL_LETTER_S_WITH_CIRCUMFLEX },
{ XK_cabovedot,           U_LATIN_SMALL_LETTER_C_WITH_DOT_ABOVE },
{ XK_ccircumflex,         U_LATIN_SMALL_LETTER_C_WITH_CIRCUMFLEX },
{ XK_gabovedot,           U_LATIN_SMALL_LETTER_G_WITH_DOT_ABOVE },
{ XK_gcircumflex,         U_LATIN_SMALL_LETTER_G_WITH_CIRCUMFLEX },
{ XK_ubreve,              U_LATIN_SMALL_LETTER_U_WITH_BREVE },
{ XK_scircumflex,         U_LATIN_SMALL_LETTER_S_WITH_CIRCUMFLEX },
#endif /* XK_Hstroke */
#endif /* XK_LATIN3 */


/*
 *   Latin 4
 *   Byte 3 = 3
 */

#ifdef XK_LATIN4
#ifdef XK_kra
{ XK_kra,                 U_LATIN_SMALL_LETTER_KRA },
{ XK_kappa,               U_LATIN_SMALL_LETTER_KRA },	/* deprecated */
{ XK_Rcedilla,            U_LATIN_CAPITAL_LETTER_R_WITH_CEDILLA },
{ XK_Itilde,              U_LATIN_CAPITAL_LETTER_I_WITH_TILDE },
{ XK_Lcedilla,            U_LATIN_CAPITAL_LETTER_L_WITH_CEDILLA },
{ XK_Emacron,             U_LATIN_CAPITAL_LETTER_E_WITH_MACRON },
{ XK_Gcedilla,            U_LATIN_CAPITAL_LETTER_G_WITH_CEDILLA },
{ XK_Tslash,              U_LATIN_CAPITAL_LETTER_T_WITH_STROKE },
{ XK_rcedilla,            U_LATIN_SMALL_LETTER_R_WITH_CEDILLA },
{ XK_itilde,              U_LATIN_SMALL_LETTER_I_WITH_TILDE },
{ XK_lcedilla,            U_LATIN_SMALL_LETTER_L_WITH_CEDILLA },
{ XK_emacron,             U_LATIN_SMALL_LETTER_E_WITH_MACRON },
{ XK_gcedilla,            U_LATIN_SMALL_LETTER_G_WITH_CEDILLA },
{ XK_tslash,              U_LATIN_SMALL_LETTER_T_WITH_STROKE },
{ XK_ENG,                 U_LATIN_CAPITAL_LETTER_ENG },
{ XK_eng,                 U_LATIN_SMALL_LETTER_ENG },
{ XK_Amacron,             U_LATIN_CAPITAL_LETTER_A_WITH_MACRON },
{ XK_Iogonek,             U_LATIN_CAPITAL_LETTER_I_WITH_OGONEK },
{ XK_Eabovedot,           U_LATIN_CAPITAL_LETTER_E_WITH_DOT_ABOVE },
{ XK_Imacron,             U_LATIN_CAPITAL_LETTER_I_WITH_MACRON },
{ XK_Ncedilla,            U_LATIN_CAPITAL_LETTER_N_WITH_CEDILLA },
{ XK_Omacron,             U_LATIN_CAPITAL_LETTER_O_WITH_MACRON },
{ XK_Kcedilla,            U_LATIN_CAPITAL_LETTER_K_WITH_CEDILLA },
{ XK_Uogonek,             U_LATIN_CAPITAL_LETTER_U_WITH_OGONEK },
{ XK_Utilde,              U_LATIN_CAPITAL_LETTER_U_WITH_TILDE },
{ XK_Umacron,             U_LATIN_CAPITAL_LETTER_U_WITH_MACRON },
{ XK_amacron,             U_LATIN_SMALL_LETTER_A_WITH_MACRON },
{ XK_iogonek,             U_LATIN_SMALL_LETTER_I_WITH_OGONEK },
{ XK_eabovedot,           U_LATIN_SMALL_LETTER_E_WITH_DOT_ABOVE },
{ XK_imacron,             U_LATIN_SMALL_LETTER_I_WITH_MACRON },
{ XK_ncedilla,            U_LATIN_SMALL_LETTER_N_WITH_CEDILLA },
{ XK_omacron,             U_LATIN_SMALL_LETTER_O_WITH_MACRON },
{ XK_kcedilla,            U_LATIN_SMALL_LETTER_K_WITH_CEDILLA },
{ XK_uogonek,             U_LATIN_SMALL_LETTER_U_WITH_OGONEK },
{ XK_utilde,              U_LATIN_SMALL_LETTER_U_WITH_TILDE },
{ XK_umacron,             U_LATIN_SMALL_LETTER_U_WITH_TILDE },
#endif /* XK_kra */
#endif /* XK_LATIN4 */

/*
 * Latin-9 (a.k.a. Latin-0)
 * Byte 3 = 19
 */

#ifdef XK_LATIN9
#ifdef XK_OE
{ XK_OE,		U_LATIN_CAPITAL_LIGATURE_OE },
{ XK_oe,		U_LATIN_SMALL_LIGATURE_OE },
{ XK_Ydiaeresis,        U_LATIN_CAPITAL_LETTER_Y_WITH_DIAERESIS },
#endif /* XK_OE */
#endif /* XK_LATIN9 */

/*
 * Katakana
 * Byte 3 = 4
 */

#ifdef XK_KATAKANA
#ifdef XK_overline
{ XK_overline,				 U_OVERLINE },
{ XK_kana_fullstop,                      U_HALFWIDTH_IDEOGRAPHIC_FULL_STOP },
{ XK_kana_openingbracket,                U_HALFWIDTH_LEFT_CORNER_BRACKET },
{ XK_kana_closingbracket,                U_HALFWIDTH_RIGHT_CORNER_BRACKET },
{ XK_kana_comma,                         U_HALFWIDTH_IDEOGRAPHIC_COMMA },
{ XK_kana_conjunctive,                   U_HALFWIDTH_KATAKANA_MIDDLE_DOT },
{ XK_kana_middledot,                     U_HALFWIDTH_KATAKANA_MIDDLE_DOT }, /* deprecated */
{ XK_kana_WO,                            U_HALFWIDTH_KATAKANA_LETTER_WO },
{ XK_kana_a,				 U_HALFWIDTH_KATAKANA_LETTER_SMALL_A },
{ XK_kana_i,				 U_HALFWIDTH_KATAKANA_LETTER_SMALL_I },
{ XK_kana_u,				 U_HALFWIDTH_KATAKANA_LETTER_SMALL_U },
{ XK_kana_e,				 U_HALFWIDTH_KATAKANA_LETTER_SMALL_E },
{ XK_kana_o,				 U_HALFWIDTH_KATAKANA_LETTER_SMALL_O },
{ XK_kana_ya,                            U_HALFWIDTH_KATAKANA_LETTER_SMALL_YA },
{ XK_kana_yu,                            U_HALFWIDTH_KATAKANA_LETTER_SMALL_YU },
{ XK_kana_yo,                            U_HALFWIDTH_KATAKANA_LETTER_SMALL_YO },
{ XK_kana_tsu,                           U_HALFWIDTH_KATAKANA_LETTER_SMALL_TU },
{ XK_kana_tu,                            U_HALFWIDTH_KATAKANA_LETTER_SMALL_TU },  /* deprecated */
{ XK_prolongedsound,                     U_HALFWIDTH_KATAKANA_HIRAGANA_PROLONGED_SOUND_MARK },
{ XK_kana_A,                             U_HALFWIDTH_KATAKANA_LETTER_A },
{ XK_kana_I,                             U_HALFWIDTH_KATAKANA_LETTER_I },
{ XK_kana_U,                             U_HALFWIDTH_KATAKANA_LETTER_U },
{ XK_kana_E,                             U_HALFWIDTH_KATAKANA_LETTER_E },
{ XK_kana_O,                             U_HALFWIDTH_KATAKANA_LETTER_O },
{ XK_kana_KA,				 U_HALFWIDTH_KATAKANA_LETTER_KA },
{ XK_kana_KI,				 U_HALFWIDTH_KATAKANA_LETTER_KI },
{ XK_kana_KU,				 U_HALFWIDTH_KATAKANA_LETTER_KU },
{ XK_kana_KE,				 U_HALFWIDTH_KATAKANA_LETTER_KE },
{ XK_kana_KO,				 U_HALFWIDTH_KATAKANA_LETTER_KO },
{ XK_kana_SA,				 U_HALFWIDTH_KATAKANA_LETTER_SA },
{ XK_kana_SHI,                           U_HALFWIDTH_KATAKANA_LETTER_SI },
{ XK_kana_SU,				 U_HALFWIDTH_KATAKANA_LETTER_SU },
{ XK_kana_SE,				 U_HALFWIDTH_KATAKANA_LETTER_SE },
{ XK_kana_SO,				 U_HALFWIDTH_KATAKANA_LETTER_SO },
{ XK_kana_TA,				 U_HALFWIDTH_KATAKANA_LETTER_TA },
{ XK_kana_CHI,                           U_HALFWIDTH_KATAKANA_LETTER_TI },
{ XK_kana_TI,                            U_HALFWIDTH_KATAKANA_LETTER_TI },  /* deprecated */
{ XK_kana_TSU,                           U_HALFWIDTH_KATAKANA_LETTER_TU },
{ XK_kana_TU,				 U_HALFWIDTH_KATAKANA_LETTER_TU },  /* deprecated */
{ XK_kana_TE,				 U_HALFWIDTH_KATAKANA_LETTER_TE },
{ XK_kana_TO,				 U_HALFWIDTH_KATAKANA_LETTER_TO },
{ XK_kana_NA,				 U_HALFWIDTH_KATAKANA_LETTER_NA },
{ XK_kana_NI,				 U_HALFWIDTH_KATAKANA_LETTER_NI },
{ XK_kana_NU,				 U_HALFWIDTH_KATAKANA_LETTER_NU },
{ XK_kana_NE,				 U_HALFWIDTH_KATAKANA_LETTER_NE },
{ XK_kana_NO,				 U_HALFWIDTH_KATAKANA_LETTER_NO },
{ XK_kana_HA,				 U_HALFWIDTH_KATAKANA_LETTER_HA },
{ XK_kana_HI,				 U_HALFWIDTH_KATAKANA_LETTER_HI },
{ XK_kana_FU,				 U_HALFWIDTH_KATAKANA_LETTER_HU },
{ XK_kana_HU,				 U_HALFWIDTH_KATAKANA_LETTER_HU },  /* deprecated */
{ XK_kana_HE,				 U_HALFWIDTH_KATAKANA_LETTER_HE },
{ XK_kana_HO,				 U_HALFWIDTH_KATAKANA_LETTER_HO },
{ XK_kana_MA,				 U_HALFWIDTH_KATAKANA_LETTER_MA },
{ XK_kana_MI,				 U_HALFWIDTH_KATAKANA_LETTER_MI },
{ XK_kana_MU,				 U_HALFWIDTH_KATAKANA_LETTER_MU },
{ XK_kana_ME,				 U_HALFWIDTH_KATAKANA_LETTER_ME },
{ XK_kana_MO,				 U_HALFWIDTH_KATAKANA_LETTER_MO },
{ XK_kana_YA,				 U_HALFWIDTH_KATAKANA_LETTER_YA },
{ XK_kana_YU,				 U_HALFWIDTH_KATAKANA_LETTER_YU },
{ XK_kana_YO,				 U_HALFWIDTH_KATAKANA_LETTER_YO },
{ XK_kana_RA,				 U_HALFWIDTH_KATAKANA_LETTER_RA },
{ XK_kana_RI,				 U_HALFWIDTH_KATAKANA_LETTER_RI },
{ XK_kana_RU,				 U_HALFWIDTH_KATAKANA_LETTER_RU },
{ XK_kana_RE,				 U_HALFWIDTH_KATAKANA_LETTER_RE },
{ XK_kana_RO,				 U_HALFWIDTH_KATAKANA_LETTER_RO },
{ XK_kana_WA,				 U_HALFWIDTH_KATAKANA_LETTER_WA },
{ XK_kana_N,                             U_HALFWIDTH_KATAKANA_LETTER_N },
{ XK_voicedsound,                        U_HALFWIDTH_KATAKANA_VOICED_SOUND_MARK },
{ XK_semivoicedsound,                    U_HALFWIDTH_KATAKANA_SEMI_VOICED_SOUND_MARK },
{ XK_kana_switch,          		 KEY_MODE_SWITCH },  /* Alias for mode_switch */
#endif /* XK_overline */
#endif /* XK_KATAKANA */

/*
 *  Arabic
 *  Byte 3 = 5
 */

#ifdef XK_ARABIC
#ifdef XK_Arabic_comma
{ XK_Arabic_comma,                                U_ARABIC_COMMA },
{ XK_Arabic_semicolon,                            U_ARABIC_SEMICOLON },
{ XK_Arabic_question_mark,                        U_ARABIC_QUESTION_MARK },
{ XK_Arabic_hamza,                                U_ARABIC_LETTER_HAMZA },
{ XK_Arabic_maddaonalef,                          U_ARABIC_LETTER_ALEF_WITH_MADDA_ABOVE },
{ XK_Arabic_hamzaonalef,                          U_ARABIC_LETTER_ALEF_WITH_HAMZA_ABOVE },
{ XK_Arabic_hamzaonwaw,                           U_ARABIC_LETTER_WAW_WITH_HAMZA_ABOVE },
{ XK_Arabic_hamzaunderalef,                       U_ARABIC_LETTER_ALEF_WITH_HAMZA_BELOW },
{ XK_Arabic_hamzaonyeh,                           U_ARABIC_LETTER_ALEF_WITH_HAMZA_ABOVE },
{ XK_Arabic_alef,                                 U_ARABIC_LETTER_ALEF },
{ XK_Arabic_beh,                                  U_ARABIC_LETTER_BEH },
{ XK_Arabic_tehmarbuta,                           U_ARABIC_LETTER_TEH_MARBUTA },
{ XK_Arabic_teh,                                  U_ARABIC_LETTER_TEH },
{ XK_Arabic_theh,                                 U_ARABIC_LETTER_THEH },
{ XK_Arabic_jeem,                                 U_ARABIC_LETTER_JEEM },
{ XK_Arabic_hah,                                  U_ARABIC_LETTER_HAH },
{ XK_Arabic_khah,                                 U_ARABIC_LETTER_KHAH },
{ XK_Arabic_dal,                                  U_ARABIC_LETTER_DAL },
{ XK_Arabic_thal,                                 U_ARABIC_LETTER_THAL },
{ XK_Arabic_ra,                                   U_ARABIC_LETTER_REH },
{ XK_Arabic_zain,                                 U_ARABIC_LETTER_ZAIN },
{ XK_Arabic_seen,                                 U_ARABIC_LETTER_SEEN },
{ XK_Arabic_sheen,                                U_ARABIC_LETTER_SHEEN },
{ XK_Arabic_sad,                                  U_ARABIC_LETTER_SAD },
{ XK_Arabic_dad,                                  U_ARABIC_LETTER_DAD },
{ XK_Arabic_tah,                                  U_ARABIC_LETTER_TAH },
{ XK_Arabic_zah,                                  U_ARABIC_LETTER_ZAH },
{ XK_Arabic_ain,                                  U_ARABIC_LETTER_AIN },
{ XK_Arabic_ghain,                                U_ARABIC_LETTER_GHAIN },
{ XK_Arabic_tatweel,                              U_ARABIC_TATWEEL },
{ XK_Arabic_feh,				  U_ARABIC_LETTER_FEH },
{ XK_Arabic_qaf,				  U_ARABIC_LETTER_QAF },
{ XK_Arabic_kaf,				  U_ARABIC_LETTER_KAF },
{ XK_Arabic_lam,				  U_ARABIC_LETTER_LAM },
{ XK_Arabic_meem,                                 U_ARABIC_LETTER_MEEM },
{ XK_Arabic_noon,                                 U_ARABIC_LETTER_NOON },
{ XK_Arabic_ha,                                   U_ARABIC_LETTER_HEH },
{ XK_Arabic_heh,                                  U_ARABIC_LETTER_HEH },  /* deprecated */
{ XK_Arabic_waw,                                  U_ARABIC_LETTER_WAW },
{ XK_Arabic_alefmaksura,                          U_ARABIC_LETTER_ALEF_MAKSURA },
{ XK_Arabic_yeh,                                  U_ARABIC_LETTER_YEH },
{ XK_Arabic_fathatan,                             U_ARABIC_FATHATAN },
{ XK_Arabic_dammatan,                             U_ARABIC_DAMMATAN },
{ XK_Arabic_kasratan,                             U_ARABIC_KASRATAN },
{ XK_Arabic_fatha,                                U_ARABIC_FATHA },
{ XK_Arabic_damma,                                U_ARABIC_DAMMA },
{ XK_Arabic_kasra,                                U_ARABIC_KASRA },
{ XK_Arabic_shadda,                               U_ARABIC_SHADDA },
{ XK_Arabic_sukun,                                U_ARABIC_SUKUN },
{ XK_Arabic_switch,        			  KEY_MODE_SWITCH },  /* Alias for mode_switch */
#endif /* XK_Arabic_comma */
#endif /* XK_ARABIC */

/*
 * Cyrillic
 * Byte 3 = 6
 */
#ifdef XK_CYRILLIC
#ifdef XK_Serbian_dje
{ XK_Serbian_dje,              U_CYRILLIC_SMALL_LETTER_DJE },
{ XK_Macedonia_gje,            U_CYRILLIC_SMALL_LETTER_GJE },
{ XK_Cyrillic_io,              U_CYRILLIC_SMALL_LETTER_IO },
{ XK_Ukrainian_ie,             U_CYRILLIC_SMALL_LETTER_UKRAINIAN_IE },
{ XK_Ukranian_je,              U_CYRILLIC_SMALL_LETTER_UKRAINIAN_IE },  /* deprecated */
{ XK_Macedonia_dse,            U_CYRILLIC_SMALL_LETTER_DZE },
{ XK_Ukrainian_i,              U_CYRILLIC_SMALL_LETTER_BYELORUSSIAN_UKRAINIAN_I },
{ XK_Ukranian_i,               U_CYRILLIC_SMALL_LETTER_BYELORUSSIAN_UKRAINIAN_I },
{ XK_Ukrainian_yi,             U_CYRILLIC_SMALL_LETTER_YI },
{ XK_Ukranian_yi,              U_CYRILLIC_SMALL_LETTER_YI },  /* deprecated */
{ XK_Cyrillic_je,              U_CYRILLIC_SMALL_LETTER_JE },
{ XK_Serbian_je,               U_CYRILLIC_SMALL_LETTER_JE }, /* deprecated */
{ XK_Cyrillic_lje,             U_CYRILLIC_SMALL_LETTER_LJE },
{ XK_Serbian_lje,              U_CYRILLIC_SMALL_LETTER_LJE },  /* deprecated */
{ XK_Cyrillic_nje,             U_CYRILLIC_SMALL_LETTER_NJE },
{ XK_Serbian_nje,              U_CYRILLIC_SMALL_LETTER_NJE },  /* deprecated */
{ XK_Serbian_tshe,             U_CYRILLIC_SMALL_LETTER_TSHE },
{ XK_Macedonia_kje,            U_CYRILLIC_SMALL_LETTER_KJE },
{ XK_Byelorussian_shortu,      U_CYRILLIC_SMALL_LETTER_SHORT_U },
{ XK_Cyrillic_dzhe,            U_CYRILLIC_SMALL_LETTER_DZHE },
{ XK_Serbian_dze,              U_CYRILLIC_SMALL_LETTER_DZHE },  /* deprecated */
{ XK_numerosign,               U_NUMERO_SIGN },
{ XK_Serbian_DJE,              U_CYRILLIC_CAPITAL_LETTER_DJE },
{ XK_Macedonia_GJE,            U_CYRILLIC_CAPITAL_LETTER_GJE },
{ XK_Cyrillic_IO,              U_CYRILLIC_CAPITAL_LETTER_IO },
{ XK_Ukrainian_IE,             U_CYRILLIC_CAPITAL_LETTER_UKRAINIAN_IE },
{ XK_Ukranian_JE,              U_CYRILLIC_CAPITAL_LETTER_UKRAINIAN_IE },  /* deprecated */
{ XK_Macedonia_DSE,            U_CYRILLIC_CAPITAL_LETTER_DZE },
{ XK_Ukrainian_I,              U_CYRILLIC_CAPITAL_LETTER_BYELORUSSIAN_UKRAINIAN_I },
{ XK_Ukranian_I,               U_CYRILLIC_CAPITAL_LETTER_BYELORUSSIAN_UKRAINIAN_I }, /* deprecated */
{ XK_Ukrainian_YI,             U_CYRILLIC_CAPITAL_LETTER_YI },
{ XK_Ukranian_YI,              U_CYRILLIC_CAPITAL_LETTER_YI },  /* deprecated */
{ XK_Cyrillic_JE,              U_CYRILLIC_CAPITAL_LETTER_JE },
{ XK_Serbian_JE,               U_CYRILLIC_CAPITAL_LETTER_JE }, /* deprecated */
{ XK_Cyrillic_LJE,             U_CYRILLIC_CAPITAL_LETTER_LJE },
{ XK_Serbian_LJE,              U_CYRILLIC_CAPITAL_LETTER_LJE },  /* deprecated */
{ XK_Cyrillic_NJE,             U_CYRILLIC_CAPITAL_LETTER_NJE },
{ XK_Serbian_NJE,              U_CYRILLIC_CAPITAL_LETTER_NJE },  /* deprecated */
{ XK_Serbian_TSHE,             U_CYRILLIC_CAPITAL_LETTER_TSHE },
{ XK_Macedonia_KJE,            U_CYRILLIC_CAPITAL_LETTER_KJE },
{ XK_Byelorussian_SHORTU,      U_CYRILLIC_CAPITAL_LETTER_SHORT_U },
{ XK_Cyrillic_DZHE,            U_CYRILLIC_CAPITAL_LETTER_DZHE },
{ XK_Serbian_DZE,              U_CYRILLIC_CAPITAL_LETTER_DZHE },  /* deprecated */
{ XK_Cyrillic_yu,              U_CYRILLIC_SMALL_LETTER_YU },
{ XK_Cyrillic_a,               U_CYRILLIC_SMALL_LETTER_A },
{ XK_Cyrillic_be,              U_CYRILLIC_SMALL_LETTER_BE },
{ XK_Cyrillic_tse,             U_CYRILLIC_SMALL_LETTER_TSE },
{ XK_Cyrillic_de,              U_CYRILLIC_SMALL_LETTER_DE },
{ XK_Cyrillic_ie,              U_CYRILLIC_SMALL_LETTER_IE },
{ XK_Cyrillic_ef,              U_CYRILLIC_SMALL_LETTER_EF },
{ XK_Cyrillic_ghe,             U_CYRILLIC_SMALL_LETTER_GHE },
{ XK_Cyrillic_ha,              U_CYRILLIC_SMALL_LETTER_HA },
{ XK_Cyrillic_i,               U_CYRILLIC_SMALL_LETTER_I },
{ XK_Cyrillic_shorti,          U_CYRILLIC_SMALL_LETTER_SHORT_I },
{ XK_Cyrillic_ka,              U_CYRILLIC_SMALL_LETTER_KA },
{ XK_Cyrillic_el,	       U_CYRILLIC_SMALL_LETTER_EL },
{ XK_Cyrillic_em,	       U_CYRILLIC_SMALL_LETTER_EM },
{ XK_Cyrillic_en,	       U_CYRILLIC_SMALL_LETTER_EN },
{ XK_Cyrillic_o,	       U_CYRILLIC_SMALL_LETTER_O },
{ XK_Cyrillic_pe,	       U_CYRILLIC_SMALL_LETTER_PE },
{ XK_Cyrillic_ya,	       U_CYRILLIC_SMALL_LETTER_YA },
{ XK_Cyrillic_er,	       U_CYRILLIC_SMALL_LETTER_ER },
{ XK_Cyrillic_es,	       U_CYRILLIC_SMALL_LETTER_ES },
{ XK_Cyrillic_te,	       U_CYRILLIC_SMALL_LETTER_TE },
{ XK_Cyrillic_u, 	       U_CYRILLIC_SMALL_LETTER_U },
{ XK_Cyrillic_zhe,             U_CYRILLIC_SMALL_LETTER_ZHE },
{ XK_Cyrillic_ve,              U_CYRILLIC_SMALL_LETTER_VE },
{ XK_Cyrillic_softsign,        U_CYRILLIC_SMALL_LETTER_SOFT_SIGN },
{ XK_Cyrillic_yeru,            U_CYRILLIC_SMALL_LETTER_YERU },
{ XK_Cyrillic_ze,              U_CYRILLIC_SMALL_LETTER_ZE },
{ XK_Cyrillic_sha,             U_CYRILLIC_SMALL_LETTER_SHA },
{ XK_Cyrillic_e,               U_CYRILLIC_SMALL_LETTER_E },
{ XK_Cyrillic_shcha,           U_CYRILLIC_SMALL_LETTER_SHCHA },
{ XK_Cyrillic_che,             U_CYRILLIC_SMALL_LETTER_CHE },
{ XK_Cyrillic_hardsign,        U_CYRILLIC_SMALL_LETTER_HARD_SIGN },
{ XK_Cyrillic_YU,              U_CYRILLIC_CAPITAL_LETTER_YU },
{ XK_Cyrillic_A,               U_CYRILLIC_CAPITAL_LETTER_A },
{ XK_Cyrillic_BE,              U_CYRILLIC_CAPITAL_LETTER_BE },
{ XK_Cyrillic_TSE,             U_CYRILLIC_CAPITAL_LETTER_TSE },
{ XK_Cyrillic_DE,              U_CYRILLIC_CAPITAL_LETTER_DE },
{ XK_Cyrillic_IE,              U_CYRILLIC_CAPITAL_LETTER_IE },
{ XK_Cyrillic_EF,              U_CYRILLIC_CAPITAL_LETTER_EF },
{ XK_Cyrillic_GHE,             U_CYRILLIC_CAPITAL_LETTER_GHE },
{ XK_Cyrillic_HA,              U_CYRILLIC_CAPITAL_LETTER_HA },
{ XK_Cyrillic_I,               U_CYRILLIC_CAPITAL_LETTER_I },
{ XK_Cyrillic_SHORTI,          U_CYRILLIC_CAPITAL_LETTER_SHORT_I },
{ XK_Cyrillic_KA,              U_CYRILLIC_CAPITAL_LETTER_KA },
{ XK_Cyrillic_EL,              U_CYRILLIC_CAPITAL_LETTER_EL },
{ XK_Cyrillic_EM,              U_CYRILLIC_CAPITAL_LETTER_EM },
{ XK_Cyrillic_EN,              U_CYRILLIC_CAPITAL_LETTER_EN },
{ XK_Cyrillic_O,               U_CYRILLIC_CAPITAL_LETTER_O },
{ XK_Cyrillic_PE,              U_CYRILLIC_CAPITAL_LETTER_PE },
{ XK_Cyrillic_YA,              U_CYRILLIC_CAPITAL_LETTER_YA },
{ XK_Cyrillic_ER,              U_CYRILLIC_CAPITAL_LETTER_ER },
{ XK_Cyrillic_ES,              U_CYRILLIC_CAPITAL_LETTER_ES },
{ XK_Cyrillic_TE,              U_CYRILLIC_CAPITAL_LETTER_TE },
{ XK_Cyrillic_U,               U_CYRILLIC_CAPITAL_LETTER_U },
{ XK_Cyrillic_ZHE,             U_CYRILLIC_CAPITAL_LETTER_ZHE },
{ XK_Cyrillic_VE,              U_CYRILLIC_CAPITAL_LETTER_VE },
{ XK_Cyrillic_SOFTSIGN,        U_CYRILLIC_CAPITAL_LETTER_SOFT_SIGN },
{ XK_Cyrillic_YERU,            U_CYRILLIC_CAPITAL_LETTER_YERU },
{ XK_Cyrillic_ZE,              U_CYRILLIC_CAPITAL_LETTER_ZE },
{ XK_Cyrillic_SHA,             U_CYRILLIC_CAPITAL_LETTER_SHA },
{ XK_Cyrillic_E,               U_CYRILLIC_CAPITAL_LETTER_E },
{ XK_Cyrillic_SHCHA,           U_CYRILLIC_CAPITAL_LETTER_SHCHA },
{ XK_Cyrillic_CHE,             U_CYRILLIC_CAPITAL_LETTER_CHE },
{ XK_Cyrillic_HARDSIGN,        U_CYRILLIC_CAPITAL_LETTER_HARD_SIGN },
#endif /* XK_Serbian_dje */
#endif /* XK_CYRILLIC */

/*
 * Greek
 * Byte 3 = 7
 */

#ifdef XK_GREEK
#ifdef XK_Greek_ALPHAaccent
{ XK_Greek_ALPHAaccent,                U_GREEK_CAPITAL_LETTER_ALPHA_WITH_TONOS },
{ XK_Greek_EPSILONaccent,              U_GREEK_CAPITAL_LETTER_EPSILON_WITH_TONOS },
{ XK_Greek_ETAaccent,                  U_GREEK_CAPITAL_LETTER_ETA_WITH_TONOS },
{ XK_Greek_IOTAaccent,                 U_GREEK_CAPITAL_LETTER_IOTA_WITH_TONOS },
{ XK_Greek_IOTAdiaeresis,              U_GREEK_CAPITAL_LETTER_IOTA_WITH_DIALYTIKA },
{ XK_Greek_OMICRONaccent,              U_GREEK_CAPITAL_LETTER_OMICRON_WITH_TONOS },
{ XK_Greek_UPSILONaccent,              U_GREEK_CAPITAL_LETTER_UPSILON_WITH_TONOS },
{ XK_Greek_UPSILONdieresis,            U_GREEK_CAPITAL_LETTER_UPSILON_WITH_DIALYTIKA },
{ XK_Greek_OMEGAaccent,                U_GREEK_CAPITAL_LETTER_OMEGA_WITH_TONOS },                 
{ XK_Greek_accentdieresis,             U_GREEK_DIALYTIKA_TONOS },
{ XK_Greek_horizbar,                   U_HORIZONTAL_BAR },
{ XK_Greek_alphaaccent,                U_GREEK_SMALL_LETTER_ALPHA_WITH_TONOS },                 
{ XK_Greek_epsilonaccent,              U_GREEK_SMALL_LETTER_EPSILON_WITH_TONOS },               
{ XK_Greek_etaaccent,                  U_GREEK_SMALL_LETTER_ETA_WITH_TONOS },                   
{ XK_Greek_iotaaccent,                 U_GREEK_SMALL_LETTER_IOTA_WITH_TONOS },                 
{ XK_Greek_iotadieresis,               U_GREEK_SMALL_LETTER_IOTA_WITH_DIALYTIKA },
{ XK_Greek_iotaaccentdieresis,         U_GREEK_SMALL_LETTER_IOTA_WITH_DIALYTIKA_AND_TONOS },   
{ XK_Greek_omicronaccent,              U_GREEK_SMALL_LETTER_OMICRON_WITH_TONOS },               
{ XK_Greek_upsilonaccent,              U_GREEK_SMALL_LETTER_UPSILON_WITH_TONOS },               
{ XK_Greek_upsilondieresis,            U_GREEK_SMALL_LETTER_UPSILON_WITH_DIALYTIKA },           
{ XK_Greek_upsilonaccentdieresis,      U_GREEK_SMALL_LETTER_UPSILON_WITH_DIALYTIKA_AND_TONOS },
{ XK_Greek_omegaaccent,                U_GREEK_SMALL_LETTER_OMEGA_WITH_TONOS },                 
{ XK_Greek_ALPHA,                      U_GREEK_CAPITAL_LETTER_ALPHA },
{ XK_Greek_BETA,                       U_GREEK_CAPITAL_LETTER_BETA },
{ XK_Greek_GAMMA,                      U_GREEK_CAPITAL_LETTER_GAMMA },
{ XK_Greek_DELTA,                      U_GREEK_CAPITAL_LETTER_DELTA },
{ XK_Greek_EPSILON,                    U_GREEK_CAPITAL_LETTER_EPSILON },
{ XK_Greek_ZETA,                       U_GREEK_CAPITAL_LETTER_ZETA },
{ XK_Greek_ETA,                        U_GREEK_CAPITAL_LETTER_ETA },
{ XK_Greek_THETA,                      U_GREEK_CAPITAL_LETTER_THETA },
{ XK_Greek_IOTA,                       U_GREEK_CAPITAL_LETTER_IOTA },
{ XK_Greek_KAPPA,                      U_GREEK_CAPITAL_LETTER_KAPPA },
{ XK_Greek_LAMDA,                      U_GREEK_CAPITAL_LETTER_LAMDA },
{ XK_Greek_LAMBDA,                     U_GREEK_CAPITAL_LETTER_LAMDA },
{ XK_Greek_MU,                         U_GREEK_CAPITAL_LETTER_MU },
{ XK_Greek_NU,                         U_GREEK_CAPITAL_LETTER_NU },
{ XK_Greek_XI,                         U_GREEK_CAPITAL_LETTER_XI },
{ XK_Greek_OMICRON,                    U_GREEK_CAPITAL_LETTER_OMICRON },
{ XK_Greek_PI,                         U_GREEK_CAPITAL_LETTER_PI },
{ XK_Greek_RHO,                        U_GREEK_CAPITAL_LETTER_RHO },
{ XK_Greek_SIGMA,                      U_GREEK_CAPITAL_LETTER_SIGMA },
{ XK_Greek_TAU,                        U_GREEK_CAPITAL_LETTER_TAU },
{ XK_Greek_UPSILON,                    U_GREEK_CAPITAL_LETTER_UPSILON },
{ XK_Greek_PHI,                        U_GREEK_CAPITAL_LETTER_PHI },
{ XK_Greek_CHI,                        U_GREEK_CAPITAL_LETTER_CHI },
{ XK_Greek_PSI,                        U_GREEK_CAPITAL_LETTER_PSI },
{ XK_Greek_OMEGA,                      U_GREEK_CAPITAL_LETTER_OMEGA },
{ XK_Greek_alpha,                      U_GREEK_SMALL_LETTER_ALPHA },
{ XK_Greek_beta,                       U_GREEK_SMALL_LETTER_BETA },
{ XK_Greek_gamma,                      U_GREEK_SMALL_LETTER_GAMMA },
{ XK_Greek_delta,                      U_GREEK_SMALL_LETTER_DELTA },
{ XK_Greek_epsilon,                    U_GREEK_SMALL_LETTER_EPSILON },
{ XK_Greek_zeta,                       U_GREEK_SMALL_LETTER_ZETA },
{ XK_Greek_eta,                        U_GREEK_SMALL_LETTER_ETA },
{ XK_Greek_theta,                      U_GREEK_SMALL_LETTER_THETA },
{ XK_Greek_iota,                       U_GREEK_SMALL_LETTER_IOTA },
{ XK_Greek_kappa,                      U_GREEK_SMALL_LETTER_KAPPA },
{ XK_Greek_lamda,                      U_GREEK_SMALL_LETTER_LAMDA },
{ XK_Greek_lambda,                     U_GREEK_SMALL_LETTER_LAMDA },
{ XK_Greek_mu,                         U_GREEK_SMALL_LETTER_MU },
{ XK_Greek_nu,                         U_GREEK_SMALL_LETTER_NU },
{ XK_Greek_xi,                         U_GREEK_SMALL_LETTER_XI },
{ XK_Greek_omicron,                    U_GREEK_SMALL_LETTER_OMICRON },
{ XK_Greek_pi,                         U_GREEK_SMALL_LETTER_PI },
{ XK_Greek_rho,                        U_GREEK_SMALL_LETTER_RHO },
{ XK_Greek_sigma,                      U_GREEK_SMALL_LETTER_SIGMA },
{ XK_Greek_finalsmallsigma,            U_GREEK_SMALL_LETTER_FINAL_SIGMA },
{ XK_Greek_tau,                        U_GREEK_SMALL_LETTER_TAU },
{ XK_Greek_upsilon,                    U_GREEK_SMALL_LETTER_UPSILON },
{ XK_Greek_phi,                        U_GREEK_SMALL_LETTER_PHI },
{ XK_Greek_chi,                        U_GREEK_SMALL_LETTER_CHI },
{ XK_Greek_psi,                        U_GREEK_SMALL_LETTER_PSI },
{ XK_Greek_omega,                      U_GREEK_SMALL_LETTER_OMEGA },
{ XK_Greek_switch,         	       KEY_MODE_SWITCH }, /* Alias for mode_switch */
#endif /* XK_Greek_ALPHAaccent */
#endif /* XK_GREEK */

/*
 * Technical
 * Byte 3 = 8
 */

#ifdef XK_TECHNICAL
#ifdef XK_leftradical
#if 0
#define XK_leftradical                                 0x8a1
#define XK_topleftradical                              0x8a2
#define XK_horizconnector                              0x8a3
#define XK_topintegral                                 0x8a4
#define XK_botintegral                                 0x8a5
#define XK_vertconnector                               0x8a6
#define XK_topleftsqbracket                            0x8a7
#define XK_botleftsqbracket                            0x8a8
#define XK_toprightsqbracket                           0x8a9
#define XK_botrightsqbracket                           0x8aa
#define XK_topleftparens                               0x8ab
#define XK_botleftparens                               0x8ac
#define XK_toprightparens                              0x8ad
#define XK_botrightparens                              0x8ae
#define XK_leftmiddlecurlybrace                        0x8af
#define XK_rightmiddlecurlybrace                       0x8b0
#define XK_topleftsummation                            0x8b1
#define XK_botleftsummation                            0x8b2
#define XK_topvertsummationconnector                   0x8b3
#define XK_botvertsummationconnector                   0x8b4
#define XK_toprightsummation                           0x8b5
#define XK_botrightsummation                           0x8b6
#define XK_rightmiddlesummation                        0x8b7
#define XK_lessthanequal                               0x8bc
#define XK_notequal                                    0x8bd
#define XK_greaterthanequal                            0x8be
#define XK_integral                                    0x8bf
#define XK_therefore                                   0x8c0
#define XK_variation                                   0x8c1
#define XK_infinity                                    0x8c2
#define XK_nabla                                       0x8c5
#define XK_approximate                                 0x8c8
#define XK_similarequal                                0x8c9
#define XK_ifonlyif                                    0x8cd
#define XK_implies                                     0x8ce
#define XK_identical                                   0x8cf
#define XK_radical                                     0x8d6
#define XK_includedin                                  0x8da
#define XK_includes                                    0x8db
#define XK_intersection                                0x8dc
#define XK_union                                       0x8dd
#define XK_logicaland                                  0x8de
#define XK_logicalor                                   0x8df
#define XK_partialderivative                           0x8ef
#define XK_function                                    0x8f6
#define XK_leftarrow                                   0x8fb
#define XK_uparrow                                     0x8fc
#define XK_rightarrow                                  0x8fd
#define XK_downarrow                                   0x8fe
#endif
#endif /* XK_leftradical */
#endif /* XK_TECHNICAL */


/*
 *  Special
 *  Byte 3 = 9
 */

#ifdef XK_SPECIAL
#ifdef XK_blank
#if 0
#define XK_blank                                       0x9df
#define XK_soliddiamond                                0x9e0
#define XK_checkerboard                                0x9e1
#define XK_ht                                          0x9e2
#define XK_ff                                          0x9e3
#define XK_cr                                          0x9e4
#define XK_lf                                          0x9e5
#define XK_nl                                          0x9e8
#define XK_vt                                          0x9e9
#define XK_lowrightcorner                              0x9ea
#define XK_uprightcorner                               0x9eb
#define XK_upleftcorner                                0x9ec
#define XK_lowleftcorner                               0x9ed
#define XK_crossinglines                               0x9ee
#define XK_horizlinescan1                              0x9ef
#define XK_horizlinescan3                              0x9f0
#define XK_horizlinescan5                              0x9f1
#define XK_horizlinescan7                              0x9f2
#define XK_horizlinescan9                              0x9f3
#define XK_leftt                                       0x9f4
#define XK_rightt                                      0x9f5
#define XK_bott                                        0x9f6
#define XK_topt                                        0x9f7
#define XK_vertbar                                     0x9f8
#endif
#endif /* XK_blank */
#endif /* XK_SPECIAL */

/*
 *  Publishing
 *  Byte 3 = a
 */

#ifdef XK_PUBLISHING
#ifdef XK_emspace
#if 0
#define XK_emspace                                     0xaa1
#define XK_enspace                                     0xaa2
#define XK_em3space                                    0xaa3
#define XK_em4space                                    0xaa4
#define XK_digitspace                                  0xaa5
#define XK_punctspace                                  0xaa6
#define XK_thinspace                                   0xaa7
#define XK_hairspace                                   0xaa8
#define XK_emdash                                      0xaa9
#define XK_endash                                      0xaaa
#define XK_signifblank                                 0xaac
#define XK_ellipsis                                    0xaae
#define XK_doubbaselinedot                             0xaaf
#define XK_onethird                                    0xab0
#define XK_twothirds                                   0xab1
#define XK_onefifth                                    0xab2
#define XK_twofifths                                   0xab3
#define XK_threefifths                                 0xab4
#define XK_fourfifths                                  0xab5
#define XK_onesixth                                    0xab6
#define XK_fivesixths                                  0xab7
#define XK_careof                                      0xab8
#define XK_figdash                                     0xabb
#define XK_leftanglebracket                            0xabc
#define XK_decimalpoint                                0xabd
#define XK_rightanglebracket                           0xabe
#define XK_marker                                      0xabf
#define XK_oneeighth                                   0xac3
#define XK_threeeighths                                0xac4
#define XK_fiveeighths                                 0xac5
#define XK_seveneighths                                0xac6
#define XK_trademark                                   0xac9
#define XK_signaturemark                               0xaca
#define XK_trademarkincircle                           0xacb
#define XK_leftopentriangle                            0xacc
#define XK_rightopentriangle                           0xacd
#define XK_emopencircle                                0xace
#define XK_emopenrectangle                             0xacf
#define XK_leftsinglequotemark                         0xad0
#define XK_rightsinglequotemark                        0xad1
#define XK_leftdoublequotemark                         0xad2
#define XK_rightdoublequotemark                        0xad3
#define XK_prescription                                0xad4
#define XK_minutes                                     0xad6
#define XK_seconds                                     0xad7
#define XK_latincross                                  0xad9
#define XK_hexagram                                    0xada
#define XK_filledrectbullet                            0xadb
#define XK_filledlefttribullet                         0xadc
#define XK_filledrighttribullet                        0xadd
#define XK_emfilledcircle                              0xade
#define XK_emfilledrect                                0xadf
#define XK_enopencircbullet                            0xae0
#define XK_enopensquarebullet                          0xae1
#define XK_openrectbullet                              0xae2
#define XK_opentribulletup                             0xae3
#define XK_opentribulletdown                           0xae4
#define XK_openstar                                    0xae5
#define XK_enfilledcircbullet                          0xae6
#define XK_enfilledsqbullet                            0xae7
#define XK_filledtribulletup                           0xae8
#define XK_filledtribulletdown                         0xae9
#define XK_leftpointer                                 0xaea
#define XK_rightpointer                                0xaeb
#define XK_club                                        0xaec
#define XK_diamond                                     0xaed
#define XK_heart                                       0xaee
#define XK_maltesecross                                0xaf0
#define XK_dagger                                      0xaf1
#define XK_doubledagger                                0xaf2
#define XK_checkmark                                   0xaf3
#define XK_ballotcross                                 0xaf4
#define XK_musicalsharp                                0xaf5
#define XK_musicalflat                                 0xaf6
#define XK_malesymbol                                  0xaf7
#define XK_femalesymbol                                0xaf8
#define XK_telephone                                   0xaf9
#define XK_telephonerecorder                           0xafa
#define XK_phonographcopyright                         0xafb
#define XK_caret                                       0xafc
#define XK_singlelowquotemark                          0xafd
#define XK_doublelowquotemark                          0xafe
#define XK_cursor                                      0xaff
#endif
#endif /* XK_emspace */
#endif /* XK_PUBLISHING */

/*
 *  APL
 *  Byte 3 = b
 */

#ifdef XK_APL
#ifdef XK_leftcaret
#if 0
#define XK_leftcaret                                   0xba3
#define XK_rightcaret                                  0xba6
#define XK_downcaret                                   0xba8
#define XK_upcaret                                     0xba9
#define XK_overbar                                     0xbc0
#define XK_downtack                                    0xbc2
#define XK_upshoe                                      0xbc3
#define XK_downstile                                   0xbc4
#define XK_underbar                                    0xbc6
#define XK_jot                                         0xbca
#define XK_quad                                        0xbcc
#define XK_uptack                                      0xbce
#define XK_circle                                      0xbcf
#define XK_upstile                                     0xbd3
#define XK_downshoe                                    0xbd6
#define XK_rightshoe                                   0xbd8
#define XK_leftshoe                                    0xbda
#define XK_lefttack                                    0xbdc
#define XK_righttack                                   0xbfc
#endif
#endif /* XK_leftcaret */
#endif /* XK_APL */

/*
 * Hebrew
 * Byte 3 = c
 */

#ifdef XK_HEBREW
#ifdef XK_hebrew_doublelowline
{ XK_hebrew_doublelowline,            U_DOUBLE_LOW_LINE },
{ XK_hebrew_aleph,                    U_HEBREW_LETTER_ALEF },
{ XK_hebrew_bet,                      U_HEBREW_LETTER_BET },
{ XK_hebrew_beth,                     U_HEBREW_LETTER_BET }, /* deprecated */
{ XK_hebrew_gimel,                    U_HEBREW_LETTER_GIMEL },
{ XK_hebrew_gimmel,                   U_HEBREW_LETTER_GIMEL }, /* deprecated */
{ XK_hebrew_dalet,                    U_HEBREW_LETTER_DALET },
{ XK_hebrew_daleth,                   U_HEBREW_LETTER_DALET }, /* deprecated */
{ XK_hebrew_he,                       U_HEBREW_LETTER_HE },
{ XK_hebrew_waw,                      U_HEBREW_LETTER_VAV },
{ XK_hebrew_zain,                     U_HEBREW_LETTER_ZAYIN },
{ XK_hebrew_zayin,                    U_HEBREW_LETTER_ZAYIN }, /* deprecated */
{ XK_hebrew_chet,                     U_HEBREW_LETTER_HET },
{ XK_hebrew_het,                      U_HEBREW_LETTER_HET },
{ XK_hebrew_tet,                      U_HEBREW_LETTER_TET },
{ XK_hebrew_teth,                     U_HEBREW_LETTER_TET },  /* deprecated */
{ XK_hebrew_yod,                      U_HEBREW_LETTER_YOD },
{ XK_hebrew_finalkaph,                U_HEBREW_LETTER_FINAL_KAF },
{ XK_hebrew_kaph,                     U_HEBREW_LETTER_KAF },
{ XK_hebrew_lamed,                    U_HEBREW_LETTER_LAMED },
{ XK_hebrew_finalmem,                 U_HEBREW_LETTER_FINAL_MEM },
{ XK_hebrew_mem,                      U_HEBREW_LETTER_MEM },
{ XK_hebrew_finalnun,                 U_HEBREW_LETTER_FINAL_NUN },
{ XK_hebrew_nun,                      U_HEBREW_LETTER_NUN },
{ XK_hebrew_samech,                   U_HEBREW_LETTER_SAMEKH },
{ XK_hebrew_samekh,                   U_HEBREW_LETTER_SAMEKH }, /* deprecated */
{ XK_hebrew_ayin,                     U_HEBREW_LETTER_AYIN },
{ XK_hebrew_finalpe,                  U_HEBREW_LETTER_FINAL_PE },
{ XK_hebrew_pe,                       U_HEBREW_LETTER_PE },
{ XK_hebrew_finalzade,                U_HEBREW_LETTER_FINAL_TSADI },
{ XK_hebrew_finalzadi,                U_HEBREW_LETTER_FINAL_TSADI }, /* deprecated */
{ XK_hebrew_zade,                     U_HEBREW_LETTER_TSADI },
{ XK_hebrew_zadi,                     U_HEBREW_LETTER_TSADI }, /* deprecated */
{ XK_hebrew_qoph,                     U_HEBREW_LETTER_QOF },
{ XK_hebrew_kuf,                      U_HEBREW_LETTER_QOF }, /* deprecated */
{ XK_hebrew_resh,                     U_HEBREW_LETTER_RESH },
{ XK_hebrew_shin,                     U_HEBREW_LETTER_SHIN },
{ XK_hebrew_taw,                      U_HEBREW_LETTER_TAV },
{ XK_hebrew_taf,                      U_HEBREW_LETTER_TAV },  /* deprecated */
{ XK_Hebrew_switch,        	      KEY_MODE_SWITCH },  /* Alias for mode_switch */
#endif /* XK_hebrew_doublelowline */
#endif /* XK_HEBREW */

/*
 * Thai
 * Byte 3 = d
 */

#ifdef XK_THAI
#ifdef XK_Thai_kokai
{ XK_Thai_kokai,			U_THAI_CHARACTER_KO_KAI },
{ XK_Thai_khokhai,			U_THAI_CHARACTER_KHO_KHAI },
{ XK_Thai_khokhuat,			U_THAI_CHARACTER_KHO_KHUAT },
{ XK_Thai_khokhwai,			U_THAI_CHARACTER_KHO_KHWAI },
{ XK_Thai_khokhon,			U_THAI_CHARACTER_KHO_KHON },
{ XK_Thai_khorakhang,			U_THAI_CHARACTER_KHO_RAKHANG },
{ XK_Thai_ngongu,			U_THAI_CHARACTER_NGO_NGU },
{ XK_Thai_chochan,			U_THAI_CHARACTER_CHO_CHAN },
{ XK_Thai_choching,			U_THAI_CHARACTER_CHO_CHING },
{ XK_Thai_chochang,			U_THAI_CHARACTER_CHO_CHANG },
{ XK_Thai_soso,				U_THAI_CHARACTER_SO_SO },
{ XK_Thai_chochoe,			U_THAI_CHARACTER_CHO_CHOE },
{ XK_Thai_yoying,			U_THAI_CHARACTER_YO_YING },
{ XK_Thai_dochada,			U_THAI_CHARACTER_DO_CHADA },
{ XK_Thai_topatak,			U_THAI_CHARACTER_TO_PATAK },
{ XK_Thai_thothan,			U_THAI_CHARACTER_THO_THAN },
{ XK_Thai_thonangmontho,		U_THAI_CHARACTER_THO_NANGMONTHO },
{ XK_Thai_thophuthao,			U_THAI_CHARACTER_THO_PHUTHAO },
{ XK_Thai_nonen,			U_THAI_CHARACTER_NO_NEN },
{ XK_Thai_dodek,			U_THAI_CHARACTER_DO_DEK },
{ XK_Thai_totao,			U_THAI_CHARACTER_TO_TAO },
{ XK_Thai_thothung,			U_THAI_CHARACTER_THO_THUNG },
{ XK_Thai_thothahan,			U_THAI_CHARACTER_THO_THAHAN },
{ XK_Thai_thothong,	 		U_THAI_CHARACTER_THO_THONG },
{ XK_Thai_nonu,				U_THAI_CHARACTER_NO_NU },
{ XK_Thai_bobaimai,			U_THAI_CHARACTER_BO_BAIMAI },
{ XK_Thai_popla,			U_THAI_CHARACTER_PO_PLA },
{ XK_Thai_phophung,			U_THAI_CHARACTER_PHO_PHUNG },
{ XK_Thai_fofa,				U_THAI_CHARACTER_FO_FA },
{ XK_Thai_phophan,			U_THAI_CHARACTER_PHO_PHAN },
{ XK_Thai_fofan,			U_THAI_CHARACTER_FO_FAN },
{ XK_Thai_phosamphao,			U_THAI_CHARACTER_PHO_PHAN },
{ XK_Thai_moma,				U_THAI_CHARACTER_MO_MA },
{ XK_Thai_yoyak,			U_THAI_CHARACTER_YO_YAK },
{ XK_Thai_rorua,			U_THAI_CHARACTER_RO_RUA },
{ XK_Thai_ru,				U_THAI_CHARACTER_RU },
{ XK_Thai_loling,			U_THAI_CHARACTER_LO_LING },
{ XK_Thai_lu,				U_THAI_CHARACTER_LU },
{ XK_Thai_wowaen,			U_THAI_CHARACTER_WO_WAEN },
{ XK_Thai_sosala,			U_THAI_CHARACTER_SO_SALA },
{ XK_Thai_sorusi,			U_THAI_CHARACTER_SO_RUSI },
{ XK_Thai_sosua,			U_THAI_CHARACTER_SO_SUA },
{ XK_Thai_hohip,			U_THAI_CHARACTER_HO_HIP },
{ XK_Thai_lochula,			U_THAI_CHARACTER_LO_CHULA },
{ XK_Thai_oang,				U_THAI_CHARACTER_O_ANG },
{ XK_Thai_honokhuk,			U_THAI_CHARACTER_HO_NOKHUK },
{ XK_Thai_paiyannoi,			U_THAI_CHARACTER_PAIYANNOI },
{ XK_Thai_saraa,			U_THAI_CHARACTER_SARA_A },
{ XK_Thai_maihanakat,			U_THAI_CHARACTER_MAI_HAN_AKAT },
{ XK_Thai_saraaa,			U_THAI_CHARACTER_SARA_AA },
{ XK_Thai_saraam,			U_THAI_CHARACTER_SARA_AM },
{ XK_Thai_sarai,			U_THAI_CHARACTER_SARA_I },
{ XK_Thai_saraii,			U_THAI_CHARACTER_SARA_II },
{ XK_Thai_saraue,			U_THAI_CHARACTER_SARA_UE },
{ XK_Thai_sarauee,			U_THAI_CHARACTER_SARA_UEE },
{ XK_Thai_sarau,			U_THAI_CHARACTER_SARA_U },
{ XK_Thai_sarauu,			U_THAI_CHARACTER_SARA_UU },
{ XK_Thai_phinthu,			U_THAI_CHARACTER_PHINTHU },
{ XK_Thai_maihanakat_maitho,   		U_REPLACEMENT_CHARACTER },
{ XK_Thai_baht,				U_THAI_CURRENCY_SYMBOL_BAHT },
{ XK_Thai_sarae,			U_THAI_CHARACTER_SARA_E },
{ XK_Thai_saraae,			U_THAI_CHARACTER_SARA_AE },
{ XK_Thai_sarao,			U_THAI_CHARACTER_SARA_O },
{ XK_Thai_saraaimaimuan,		U_THAI_CHARACTER_SARA_AI_MAIMUAN },
{ XK_Thai_saraaimaimalai,		U_THAI_CHARACTER_SARA_AI_MAIMALAI },
{ XK_Thai_lakkhangyao,			U_THAI_CHARACTER_LAKKHANGYAO },
{ XK_Thai_maiyamok,			U_THAI_CHARACTER_MAIYAMOK },
{ XK_Thai_maitaikhu,			U_THAI_CHARACTER_MAITAIKHU },
{ XK_Thai_maiek,			U_THAI_CHARACTER_MAI_EK },
{ XK_Thai_maitho,			U_THAI_CHARACTER_MAI_THO },
{ XK_Thai_maitri,			U_THAI_CHARACTER_MAI_TRI },
{ XK_Thai_maichattawa,			U_THAI_CHARACTER_MAI_CHATTAWA },
{ XK_Thai_thanthakhat,			U_THAI_CHARACTER_THANTHAKHAT },
{ XK_Thai_nikhahit,			U_THAI_CHARACTER_NIKHAHIT },
{ XK_Thai_leksun,			U_THAI_DIGIT_ZERO },
{ XK_Thai_leknung,			U_THAI_DIGIT_ONE },
{ XK_Thai_leksong,			U_THAI_DIGIT_TWO },
{ XK_Thai_leksam,			U_THAI_DIGIT_THREE },
{ XK_Thai_leksi,			U_THAI_DIGIT_FOUR },
{ XK_Thai_lekha,			U_THAI_DIGIT_FIVE },
{ XK_Thai_lekhok,			U_THAI_DIGIT_SIX },
{ XK_Thai_lekchet,			U_THAI_DIGIT_SEVEN },
{ XK_Thai_lekpaet,			U_THAI_DIGIT_EIGHT },
{ XK_Thai_lekkao,			U_THAI_DIGIT_NINE },
#endif /* XK_Thai_kokai */
#endif /* XK_THAI */

/*
 *   Korean
 *   Byte 3 = e
 */

#ifdef XK_KOREAN
#ifdef XK_Hangul
#if 0
#define XK_Hangul		0xff31    /* Hangul start/stop(toggle) */
#define XK_Hangul_Start		0xff32    /* Hangul start */
#define XK_Hangul_End		0xff33    /* Hangul end, English start */
#define XK_Hangul_Hanja		0xff34    /* Start Hangul->Hanja Conversion */
#define XK_Hangul_Jamo		0xff35    /* Hangul Jamo mode */
#define XK_Hangul_Romaja	0xff36    /* Hangul Romaja mode */
#define XK_Hangul_Codeinput	0xff37    /* Hangul code input mode */
#define XK_Hangul_Jeonja	0xff38    /* Jeonja mode */
#define XK_Hangul_Banja		0xff39    /* Banja mode */
#define XK_Hangul_PreHanja	0xff3a    /* Pre Hanja conversion */
#define XK_Hangul_PostHanja	0xff3b    /* Post Hanja conversion */
#define XK_Hangul_SingleCandidate	0xff3c    /* Single candidate */
#define XK_Hangul_MultipleCandidate	0xff3d    /* Multiple candidate */
#define XK_Hangul_PreviousCandidate	0xff3e    /* Previous candidate */
#define XK_Hangul_Special	0xff3f    /* Special symbols */
{ XK_Hangul_switch,	KEY_MODE_SWITCH },   /* Alias for mode_switch */
#endif

/* Hangul Consonant Characters */
{ XK_Hangul_Kiyeog,		U_HANGUL_LETTER_KIYEOK },
{ XK_Hangul_SsangKiyeog,	U_HANGUL_LETTER_SSANGKIYEOK },
{ XK_Hangul_KiyeogSios,		U_HANGUL_LETTER_KIYEOK_SIOS },
{ XK_Hangul_Nieun,		U_HANGUL_LETTER_NIEUN },
{ XK_Hangul_NieunJieuj,		U_HANGUL_LETTER_NIEUN_CIEUC },
{ XK_Hangul_NieunHieuh,		U_HANGUL_LETTER_NIEUN_HIEUH },
{ XK_Hangul_Dikeud,		U_HANGUL_LETTER_TIKEUT },
{ XK_Hangul_SsangDikeud,	U_HANGUL_LETTER_SSANGTIKEUT },
{ XK_Hangul_Rieul,		U_HANGUL_LETTER_RIEUL },
{ XK_Hangul_RieulKiyeog,	U_HANGUL_LETTER_RIEUL_KIYEOK },
{ XK_Hangul_RieulMieum,		U_HANGUL_LETTER_RIEUL_MIEUM },
{ XK_Hangul_RieulPieub,		U_HANGUL_LETTER_RIEUL_PIEUP },
{ XK_Hangul_RieulSios,		U_HANGUL_LETTER_RIEUL_SIOS },
{ XK_Hangul_RieulTieut,		U_HANGUL_LETTER_RIEUL_THIEUTH },
{ XK_Hangul_RieulPhieuf,	U_HANGUL_LETTER_RIEUL_PHIEUPH },
{ XK_Hangul_RieulHieuh,		U_HANGUL_LETTER_RIEUL_HIEUH },
{ XK_Hangul_Mieum,		U_HANGUL_LETTER_MIEUM },
{ XK_Hangul_Pieub,		U_HANGUL_LETTER_PIEUP },
{ XK_Hangul_SsangPieub,		U_HANGUL_LETTER_SSANGPIEUP },
{ XK_Hangul_PieubSios,		U_HANGUL_LETTER_PIEUP_SIOS },
{ XK_Hangul_Sios,		U_HANGUL_LETTER_SIOS },
{ XK_Hangul_SsangSios,		U_HANGUL_LETTER_SSANGSIOS },
{ XK_Hangul_Ieung,		U_HANGUL_LETTER_IEUNG },
{ XK_Hangul_Jieuj,		U_HANGUL_LETTER_CIEUC },
{ XK_Hangul_SsangJieuj,		U_HANGUL_LETTER_SSANGCIEUC },
{ XK_Hangul_Cieuc,		U_HANGUL_LETTER_CHIEUCH },
{ XK_Hangul_Khieuq,		U_HANGUL_LETTER_KHIEUKH },
{ XK_Hangul_Tieut,		U_HANGUL_LETTER_THIEUTH },
{ XK_Hangul_Phieuf,		U_HANGUL_LETTER_PHIEUPH },
{ XK_Hangul_Hieuh,		U_HANGUL_LETTER_HIEUH },

/* Hangul Vowel Characters */
{ XK_Hangul_A,			U_HANGUL_LETTER_A },
{ XK_Hangul_AE,			U_HANGUL_LETTER_AE },
{ XK_Hangul_YA,			U_HANGUL_LETTER_YA },
{ XK_Hangul_YAE,		U_HANGUL_LETTER_YAE },
{ XK_Hangul_EO,			U_HANGUL_LETTER_EO },
{ XK_Hangul_E,			U_HANGUL_LETTER_E },
{ XK_Hangul_YEO,		U_HANGUL_LETTER_YEO },
{ XK_Hangul_YE,			U_HANGUL_LETTER_YE },
{ XK_Hangul_O,			U_HANGUL_LETTER_O },
{ XK_Hangul_WA,			U_HANGUL_LETTER_WA },
{ XK_Hangul_WAE,		U_HANGUL_LETTER_WAE },
{ XK_Hangul_OE,			U_HANGUL_LETTER_OE },
{ XK_Hangul_YO,			U_HANGUL_LETTER_YO },
{ XK_Hangul_U,			U_HANGUL_LETTER_U },
{ XK_Hangul_WEO,		U_HANGUL_LETTER_WEO },
{ XK_Hangul_WE,			U_HANGUL_LETTER_WE },
{ XK_Hangul_WI,			U_HANGUL_LETTER_WI },
{ XK_Hangul_YU,			U_HANGUL_LETTER_YU },
{ XK_Hangul_EU,			U_HANGUL_LETTER_EU },
{ XK_Hangul_YI,			U_HANGUL_LETTER_YI },
{ XK_Hangul_I,			U_HANGUL_LETTER_I },

/* Hangul syllable-final (JongSeong) Characters */
{ XK_Hangul_J_Kiyeog,		U_HANGUL_JONGSEONG_KIYEOK },
{ XK_Hangul_J_SsangKiyeog,	U_HANGUL_JONGSEONG_SSANGKIYEOK },
{ XK_Hangul_J_KiyeogSios,	U_HANGUL_JONGSEONG_KIYEOK_SIOS },
{ XK_Hangul_J_Nieun,		U_HANGUL_JONGSEONG_NIEUN },
{ XK_Hangul_J_NieunJieuj,	U_HANGUL_JONGSEONG_NIEUN_CIEUC },
{ XK_Hangul_J_NieunHieuh,	U_HANGUL_JONGSEONG_NIEUN_HIEUH },
{ XK_Hangul_J_Dikeud,		U_HANGUL_JONGSEONG_TIKEUT },
{ XK_Hangul_J_Rieul,		U_HANGUL_JONGSEONG_RIEUL },
{ XK_Hangul_J_RieulKiyeog,	U_HANGUL_JONGSEONG_RIEUL_KIYEOK },
{ XK_Hangul_J_RieulMieum,	U_HANGUL_JONGSEONG_RIEUL_MIEUM },
{ XK_Hangul_J_RieulPieub,	U_HANGUL_JONGSEONG_RIEUL_PIEUP },
{ XK_Hangul_J_RieulSios,	U_HANGUL_JONGSEONG_RIEUL_SIOS },
{ XK_Hangul_J_RieulTieut,	U_HANGUL_JONGSEONG_RIEUL_THIEUTH },
{ XK_Hangul_J_RieulPhieuf,	U_HANGUL_JONGSEONG_RIEUL_PHIEUPH },
{ XK_Hangul_J_RieulHieuh,	U_HANGUL_JONGSEONG_RIEUL_HIEUH },
{ XK_Hangul_J_Mieum,		U_HANGUL_JONGSEONG_MIEUM },
{ XK_Hangul_J_Pieub,		U_HANGUL_JONGSEONG_PIEUP },
{ XK_Hangul_J_PieubSios,	U_HANGUL_JONGSEONG_PIEUP_SIOS },
{ XK_Hangul_J_Sios,		U_HANGUL_JONGSEONG_SIOS },
{ XK_Hangul_J_SsangSios,	U_HANGUL_JONGSEONG_SSANGSIOS },
{ XK_Hangul_J_Ieung,		U_HANGUL_JONGSEONG_IEUNG },
{ XK_Hangul_J_Jieuj,		U_HANGUL_JONGSEONG_CIEUC },
{ XK_Hangul_J_Cieuc,		U_HANGUL_JONGSEONG_CHIEUCH },
{ XK_Hangul_J_Khieuq,		U_HANGUL_JONGSEONG_KHIEUKH },
{ XK_Hangul_J_Tieut,		U_HANGUL_JONGSEONG_THIEUTH },
{ XK_Hangul_J_Phieuf,		U_HANGUL_JONGSEONG_PHIEUPH },
{ XK_Hangul_J_Hieuh,		U_HANGUL_JONGSEONG_HIEUH },

/* Ancient Hangul Consonant Characters */
{ XK_Hangul_RieulYeorinHieuh,		U_HANGUL_LETTER_RIEUL_YEORINHIEUH },
{ XK_Hangul_SunkyeongeumMieum,		U_HANGUL_LETTER_KAPYEOUNMIEUM },
{ XK_Hangul_SunkyeongeumPieub,		U_HANGUL_LETTER_KAPYEOUNPIEUP },
{ XK_Hangul_PanSios,			U_HANGUL_LETTER_PANSIOS },
{ XK_Hangul_KkogjiDalrinIeung,		U_REPLACEMENT_CHARACTER },
{ XK_Hangul_SunkyeongeumPhieuf,		U_HANGUL_LETTER_KAPYEOUNPHIEUPH },
{ XK_Hangul_YeorinHieuh,		U_HANGUL_LETTER_YEORINHIEUH },

/* Ancient Hangul Vowel Characters */
{ XK_Hangul_AraeA,			U_HANGUL_LETTER_ARAEA },
{ XK_Hangul_AraeAE,			U_HANGUL_LETTER_ARAEAE },

/* Ancient Hangul syllable-final (JongSeong) Characters */
{ XK_Hangul_J_PanSios,			U_HANGUL_JONGSEONG_PANSIOS },
{ XK_Hangul_J_KkogjiDalrinIeung,	U_REPLACEMENT_CHARACTER },
{ XK_Hangul_J_YeorinHieuh,		U_HANGUL_JONGSEONG_YEORINHIEUH },

/* Korean currency symbol */
{ XK_Korean_Won,			U_WON_SIGN },
#endif /* XK_Hangul */
#endif /* XK_KOREAN */


/*
 *   Armenian
 *   Byte 3 = 0x14
 */

#ifdef XK_ARMENIAN
#ifdef XK_Armenian_eternity
{ XK_Armenian_eternity,	        U_REPLACEMENT_CHARACTER },
{ XK_Armenian_section_sign,	U_SECTION_SIGN },
{ XK_Armenian_full_stop,	U_ARMENIAN_FULL_STOP },
{ XK_Armenian_verjaket,		U_REPLACEMENT_CHARACTER },
{ XK_Armenian_parenright,	U_RIGHT_PARENTHESIS },
{ XK_Armenian_parenleft,	U_LEFT_PARENTHESIS },
{ XK_Armenian_guillemotright,	U_RIGHT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK },
{ XK_Armenian_guillemotleft,	U_LEFT_POINTING_DOUBLE_ANGLE_QUOTATION_MARK },
{ XK_Armenian_em_dash,		U_EM_DASH },
{ XK_Armenian_dot,		U_MIDDLE_DOT },
{ XK_Armenian_mijaket,		U_REPLACEMENT_CHARACTER },
{ XK_Armenian_separation_mark,	U_REPLACEMENT_CHARACTER },
{ XK_Armenian_but,		U_REPLACEMENT_CHARACTER },
{ XK_Armenian_comma,		U_ARMENIAN_COMMA },
{ XK_Armenian_en_dash,		U_EN_DASH },
{ XK_Armenian_hyphen,		U_HYPHEN },
{ XK_Armenian_yentamna,		U_REPLACEMENT_CHARACTER },
{ XK_Armenian_ellipsis,		U_HORIZONTAL_ELLIPSIS },
{ XK_Armenian_exclam,		U_ARMENIAN_EXCLAMATION_MARK },
{ XK_Armenian_amanak,		U_REPLACEMENT_CHARACTER },
{ XK_Armenian_accent,		U_CIRCUMFLEX_ACCENT },
{ XK_Armenian_shesht,		U_REPLACEMENT_CHARACTER },
{ XK_Armenian_question,		U_ARMENIAN_QUESTION_MARK },
{ XK_Armenian_paruyk,		U_REPLACEMENT_CHARACTER },
{ XK_Armenian_AYB,		U_ARMENIAN_CAPITAL_LETTER_AYB    },
{ XK_Armenian_ayb,		U_ARMENIAN_SMALL_LETTER_AYB      },
{ XK_Armenian_BEN,		U_ARMENIAN_CAPITAL_LETTER_BEN    },
{ XK_Armenian_ben,		U_ARMENIAN_SMALL_LETTER_BEN    },
{ XK_Armenian_GIM,		U_ARMENIAN_CAPITAL_LETTER_GIM    },
{ XK_Armenian_gim,		U_ARMENIAN_SMALL_LETTER_GIM    },
{ XK_Armenian_DA,		U_ARMENIAN_CAPITAL_LETTER_DA     },
{ XK_Armenian_da,		U_ARMENIAN_SMALL_LETTER_DA     },
{ XK_Armenian_YECH,		U_ARMENIAN_CAPITAL_LETTER_ECH    },
{ XK_Armenian_yech,		U_ARMENIAN_SMALL_LETTER_ECH    },
{ XK_Armenian_ZA,		U_ARMENIAN_CAPITAL_LETTER_ZA     },
{ XK_Armenian_za,		U_ARMENIAN_SMALL_LETTER_ZA     },
{ XK_Armenian_E,		U_ARMENIAN_CAPITAL_LETTER_EH     },
{ XK_Armenian_e,		U_ARMENIAN_SMALL_LETTER_EH     },
{ XK_Armenian_AT,		U_ARMENIAN_CAPITAL_LETTER_ET     },
{ XK_Armenian_at,		U_ARMENIAN_SMALL_LETTER_ET     },
{ XK_Armenian_TO,		U_ARMENIAN_CAPITAL_LETTER_TO     },
{ XK_Armenian_to,		U_ARMENIAN_SMALL_LETTER_TO     },
{ XK_Armenian_ZHE,		U_ARMENIAN_CAPITAL_LETTER_ZHE    },
{ XK_Armenian_zhe,		U_ARMENIAN_SMALL_LETTER_ZHE    },
{ XK_Armenian_INI,		U_ARMENIAN_CAPITAL_LETTER_INI    },
{ XK_Armenian_ini,		U_ARMENIAN_SMALL_LETTER_INI },
{ XK_Armenian_LYUN,		U_ARMENIAN_CAPITAL_LETTER_LIWN   },
{ XK_Armenian_lyun,		U_ARMENIAN_SMALL_LETTER_LIWN },
{ XK_Armenian_KHE,		U_ARMENIAN_CAPITAL_LETTER_XEH    },
{ XK_Armenian_khe,		U_ARMENIAN_SMALL_LETTER_XEH },
{ XK_Armenian_TSA,		U_ARMENIAN_CAPITAL_LETTER_CA     },
{ XK_Armenian_tsa,		U_ARMENIAN_SMALL_LETTER_CA },
{ XK_Armenian_KEN,		U_ARMENIAN_CAPITAL_LETTER_KEN    },
{ XK_Armenian_ken,		U_ARMENIAN_SMALL_LETTER_KEN },
{ XK_Armenian_HO,		U_ARMENIAN_CAPITAL_LETTER_HO     },
{ XK_Armenian_ho,		U_ARMENIAN_SMALL_LETTER_HO },
{ XK_Armenian_DZA,		U_ARMENIAN_CAPITAL_LETTER_JA     },
{ XK_Armenian_dza,		U_ARMENIAN_SMALL_LETTER_JA },
{ XK_Armenian_GHAT,		U_ARMENIAN_CAPITAL_LETTER_GHAD   },
{ XK_Armenian_ghat,		U_ARMENIAN_SMALL_LETTER_GHAD },
{ XK_Armenian_TCHE,		U_ARMENIAN_CAPITAL_LETTER_CHEH   },
{ XK_Armenian_tche,		U_ARMENIAN_SMALL_LETTER_CHEH },
{ XK_Armenian_MEN,		U_ARMENIAN_CAPITAL_LETTER_MEN    },
{ XK_Armenian_men,		U_ARMENIAN_SMALL_LETTER_MEN },
{ XK_Armenian_HI,		U_ARMENIAN_CAPITAL_LETTER_YI     },
{ XK_Armenian_hi,		U_ARMENIAN_SMALL_LETTER_YI },
{ XK_Armenian_NU,		U_ARMENIAN_CAPITAL_LETTER_NOW    },
{ XK_Armenian_nu,		U_ARMENIAN_SMALL_LETTER_NOW },
{ XK_Armenian_SHA,		U_ARMENIAN_CAPITAL_LETTER_SHA    },
{ XK_Armenian_sha,		U_ARMENIAN_SMALL_LETTER_SHA },
{ XK_Armenian_VO,		U_ARMENIAN_CAPITAL_LETTER_VO     },
{ XK_Armenian_vo,		U_ARMENIAN_SMALL_LETTER_VO },
{ XK_Armenian_CHA,		U_ARMENIAN_CAPITAL_LETTER_CHA    },
{ XK_Armenian_cha,		U_ARMENIAN_SMALL_LETTER_CHA },
{ XK_Armenian_PE,		U_ARMENIAN_CAPITAL_LETTER_PEH    },
{ XK_Armenian_pe,		U_ARMENIAN_SMALL_LETTER_PEH },
{ XK_Armenian_JE,		U_ARMENIAN_CAPITAL_LETTER_JHEH   },
{ XK_Armenian_je,		U_ARMENIAN_SMALL_LETTER_JHEH },
{ XK_Armenian_RA,		U_ARMENIAN_CAPITAL_LETTER_RA     },
{ XK_Armenian_ra,		U_ARMENIAN_SMALL_LETTER_RA },
{ XK_Armenian_SE,		U_ARMENIAN_CAPITAL_LETTER_SEH    },
{ XK_Armenian_se,		U_ARMENIAN_SMALL_LETTER_SEH },
{ XK_Armenian_VEV,		U_ARMENIAN_CAPITAL_LETTER_VEW    },
{ XK_Armenian_vev,		U_ARMENIAN_SMALL_LETTER_VEW },
{ XK_Armenian_TYUN,		U_ARMENIAN_CAPITAL_LETTER_TIWN   },
{ XK_Armenian_tyun,		U_ARMENIAN_SMALL_LETTER_TIWN },
{ XK_Armenian_RE,		U_ARMENIAN_CAPITAL_LETTER_REH    },
{ XK_Armenian_re,		U_ARMENIAN_SMALL_LETTER_REH },
{ XK_Armenian_TSO,		U_ARMENIAN_CAPITAL_LETTER_CO     },
{ XK_Armenian_tso,		U_ARMENIAN_SMALL_LETTER_CO },
{ XK_Armenian_VYUN,		U_ARMENIAN_CAPITAL_LETTER_YIWN   },
{ XK_Armenian_vyun,		U_ARMENIAN_SMALL_LETTER_YIWN },
{ XK_Armenian_PYUR,		U_ARMENIAN_CAPITAL_LETTER_PIWR   },
{ XK_Armenian_pyur,		U_ARMENIAN_SMALL_LETTER_PIWR },
{ XK_Armenian_KE,		U_ARMENIAN_CAPITAL_LETTER_KEH    },
{ XK_Armenian_ke,		U_ARMENIAN_SMALL_LETTER_KEH },
{ XK_Armenian_O,		U_ARMENIAN_CAPITAL_LETTER_OH     },
{ XK_Armenian_o,		U_ARMENIAN_SMALL_LETTER_OH },
{ XK_Armenian_FE,		U_ARMENIAN_CAPITAL_LETTER_FEH    },
{ XK_Armenian_fe,		U_ARMENIAN_SMALL_LETTER_FEH },
{ XK_Armenian_apostrophe,	U_ARMENIAN_APOSTROPHE },
{ XK_Armenian_ligature_ew,	U_REPLACEMENT_CHARACTER },
#endif /* XK_Armenian_eternity */
#endif /* XK_ARMENIAN */

/*
 *   Georgian
 *   Byte 3 = 0x15
 */

#ifdef XK_GEORGIAN
#ifdef XK_Georgian_an
{ XK_Georgian_an,	U_GEORGIAN_LETTER_AN     },
{ XK_Georgian_ban,	U_GEORGIAN_LETTER_BAN    },
{ XK_Georgian_gan,	U_GEORGIAN_LETTER_GAN    },
{ XK_Georgian_don,	U_GEORGIAN_LETTER_DON    },
{ XK_Georgian_en,	U_GEORGIAN_LETTER_EN     },
{ XK_Georgian_vin,	U_GEORGIAN_LETTER_VIN    },
{ XK_Georgian_zen,	U_GEORGIAN_LETTER_ZEN    },
{ XK_Georgian_tan,	U_GEORGIAN_LETTER_TAN    },
{ XK_Georgian_in,	U_GEORGIAN_LETTER_IN     },
{ XK_Georgian_kan,	U_GEORGIAN_LETTER_KAN    },
{ XK_Georgian_las,	U_GEORGIAN_LETTER_LAS    },
{ XK_Georgian_man,	U_GEORGIAN_LETTER_MAN    },
{ XK_Georgian_nar,	U_GEORGIAN_LETTER_NAR    },
{ XK_Georgian_on,	U_GEORGIAN_LETTER_ON     },
{ XK_Georgian_par,	U_GEORGIAN_LETTER_PAR    },
{ XK_Georgian_zhar,	U_GEORGIAN_LETTER_ZHAR   },
{ XK_Georgian_rae,	U_GEORGIAN_LETTER_RAE    },
{ XK_Georgian_san,	U_GEORGIAN_LETTER_SAN    },
{ XK_Georgian_tar,	U_GEORGIAN_LETTER_TAR    },
{ XK_Georgian_un,	U_GEORGIAN_LETTER_UN     },
{ XK_Georgian_phar,	U_GEORGIAN_LETTER_PHAR   },
{ XK_Georgian_khar,	U_GEORGIAN_LETTER_KHAR   },
{ XK_Georgian_ghan,	U_GEORGIAN_LETTER_GHAN   },
{ XK_Georgian_qar,	U_GEORGIAN_LETTER_QAR    },
{ XK_Georgian_shin,	U_GEORGIAN_LETTER_SHIN   },
{ XK_Georgian_chin,	U_GEORGIAN_LETTER_CHIN   },
{ XK_Georgian_can,	U_GEORGIAN_LETTER_CAN    },
{ XK_Georgian_jil,	U_GEORGIAN_LETTER_JIL    },
{ XK_Georgian_cil,	U_GEORGIAN_LETTER_CIL    },
{ XK_Georgian_char,	U_GEORGIAN_LETTER_CHAR   },
{ XK_Georgian_xan,	U_GEORGIAN_LETTER_XAN    },
{ XK_Georgian_jhan,	U_GEORGIAN_LETTER_JHAN   },
{ XK_Georgian_hae,	U_GEORGIAN_LETTER_HAE    },
{ XK_Georgian_he,	U_GEORGIAN_LETTER_HE     },
{ XK_Georgian_hie,	U_GEORGIAN_LETTER_HIE    },
{ XK_Georgian_we,	U_GEORGIAN_LETTER_WE     },
{ XK_Georgian_har,	U_GEORGIAN_LETTER_HAR    },
{ XK_Georgian_hoe,	U_GEORGIAN_LETTER_HOE    },
{ XK_Georgian_fi,	U_GEORGIAN_LETTER_FI     },
#endif /* XK_Georgian_an */
#endif /* XK_GEORGIAN */

#ifdef XK_CURRENCY
#ifdef XK_EcuSign
{ XK_EcuSign,		U_EURO_CURRENCY_SIGN },
{ XK_ColonSign,		U_COLON_SIGN },
{ XK_CruzeiroSign,	U_CRUZEIRO_SIGN },
{ XK_FFrancSign,	U_FRENCH_FRANC_SIGN },
{ XK_LiraSign,		U_LIRA_SIGN },
{ XK_MillSign,		U_MILL_SIGN },
{ XK_NairaSign,		U_NAIRA_SIGN },
{ XK_PesetaSign,	U_PESETA_SIGN },
{ XK_RupeeSign,		U_RUPEE_SIGN },
{ XK_WonSign,		U_WON_SIGN },
{ XK_NewSheqelSign,	U_NEW_SHEQEL_SIGN },
{ XK_DongSign,		U_DONG_SIGN },
{ XK_EuroSign,		U_EURO_SIGN },
#endif /* XK_EcuSign */
#endif

};

static const int keysym_map_size = sizeof(keysym_map)/sizeof(keysym_map[0]);

static int keysym_map_compare(const void *left, const void *right)
{
	const struct xkey_to_dosemu_key *lmap, *rmap;
	lmap = left;
	rmap = right;
	return lmap->xkey - rmap->xkey;
}

static size_t X_keysym_to_unicode(struct char_set_state *state,
	struct char_set *set, int offset,
	t_unicode *symbol,
	const unsigned char *str, size_t len)
{
	static int initialized = 0;
	struct xkey_to_dosemu_key match[1], *result;

	if (!initialized) {
		qsort(keysym_map, keysym_map_size, sizeof(match), 
			keysym_map_compare);
		initialized = 1;
	}

	*symbol = match->dosemu_key = U_VOID;
	if (len < sizeof(match->xkey)) {
		goto bad_length;
	}
	else {
		match->xkey = *((KeySym *)str);
	}
	result = bsearch(match, keysym_map, keysym_map_size, sizeof(match), 
		keysym_map_compare);
	if (!result) {
		result = match;
	}
	*symbol = result->dosemu_key;
	return sizeof(result->xkey);
 bad_length:
	errno = EINVAL;
	return -1;
}

static size_t unicode_to_X_keysym(struct char_set_state *state,
	struct char_set *set, int offset,
	t_unicode dosemu_key,
	unsigned char *out_str, size_t out_len)
{
	int i;
	KeySym result;
	result = XK_VoidSymbol;
	for(i = 0; i < keysym_map_size; i++) {
		if (keysym_map[i].dosemu_key == dosemu_key) {
			result = keysym_map[i].xkey;
			break;
		}
	}
	if (out_len < sizeof(result)) {
		goto too_little_space;
	}
	memcpy(out_str, &keysym_map[i].xkey, sizeof(result));
	return sizeof(result);
 too_little_space:
	errno = E2BIG;
	return -1;
}

static void for_each_keysym_mapping(struct char_set *set, 
	int offset,
	void *callback_data, foreach_callback_t callback)
{
	int i;
	for(i = 0; i < keysym_map_size; i++) {
		callback(callback_data, 
			keysym_map[i].dosemu_key,
			(const char *)&keysym_map[i].xkey, 
			sizeof(keysym_map[i].xkey));
	}
}

struct char_set_operations X_keysym_operations = {
	.unicode_to_charset = &unicode_to_X_keysym,
	.charset_to_unicode = &X_keysym_to_unicode,
	.foreach = &for_each_keysym_mapping,
};

struct char_set X_keysym = {
	.names = { "X_keysym", 0 },
	.ops = &X_keysym_operations,
};

CONSTRUCTOR(static void init(void))
{
	register_charset(&X_keysym);
}
