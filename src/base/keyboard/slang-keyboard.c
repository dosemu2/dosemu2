#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "timers.h"
#include "slang.h"

#ifndef SLANG_VERSION
# define SLANG_VERSION 1
#endif

/* unsigned char DOSemu_Slang_Escape_Character = 30; */

char *DOSemu_Keyboard_Keymap_Prompt = NULL;
int DOSemu_Terminal_Scroll = 0;
int DOSemu_Slang_Show_Help = 0;

extern void dos_slang_redraw (void);
extern void dos_slang_suspend (void);
extern void dos_slang_smart_set_mono (void);

/* The goal of the routines here is simple: to allow SHIFT, ALT, and CONTROL
 * keys to be used with a remote terminal.  The way this is accomplished is 
 * simple:  There are 4 keymaps: normal, shift, control, and alt.  The user 
 * must a key or keysequence that invokes on of these keymaps.  The default
 * keymap is the normal one.  When one of the other keymaps is invoked via the
 * special key defined for it, the new keymap is only in effect for the next
 * character at which point the normal one becomes active again.
 * 
 * The normal keymap is fairly robust.  It contains mappings for all the
 * ascii characters (0-26, 28-256).  The escape character itself (27) is 
 * included only in the sence that it is the prefix character for arrow keys,
 * function keys, etc...
 */

/* The keymaps are simply a mapping from a character sequence to scan/ascii 
 * codes.
 */

typedef struct
{
   unsigned char keystr[10];
   unsigned long scan_code;
}
Keymap_Scan_Type;

#define SHIFT_MASK			0x00010000
#define CTRL_MASK			0x00020000
#define ALT_MASK			0x00040000
#define STICKY_SHIFT_MASK		0x00100000
#define STICKY_CTRL_MASK		0x00200000
#define STICKY_ALT_MASK			0x00400000

#define ALT_KEY_SCAN_CODE		0x80000000
#define STICKY_ALT_KEY_SCAN_CODE	0x80000001
#define SHIFT_KEY_SCAN_CODE		0x80000002
#define STICKY_SHIFT_KEY_SCAN_CODE	0x80000003
#define CTRL_KEY_SCAN_CODE		0x80000004
#define STICKY_CTRL_KEY_SCAN_CODE	0x80000005
#define SCROLL_UP_SCAN_CODE		0x80000020
#define SCROLL_DOWN_SCAN_CODE		0x80000021
#define REDRAW_SCAN_CODE		0x80000022
#define SUSPEND_SCAN_CODE		0x80000023
#define HELP_SCAN_CODE			0x80000024
#define RESET_SCAN_CODE			0x80000025
#define SET_MONO_SCAN_CODE		0x80000026


static Keymap_Scan_Type Normal_Map [] =
{
   /* These are the F1 - F12 keys for terminals without them. */
     {"^@1", 0x3b00},		       /* F1 */
     {"^@2", 0x3c00},		       /* F2 */
     {"^@3", 0x3d00},		       /* F3 */
     {"^@4", 0x3e00},		       /* F4 */
     {"^@5", 0x3f00},		       /* F5 */
     {"^@6", 0x4000},		       /* F6 */
     {"^@7", 0x4100},		       /* F7 */
     {"^@8", 0x4200},		       /* F8 */
     {"^@9", 0x4300},		       /* F9 */
     {"^@0", 0x4400},		       /* F10 */
     {"^@-", 0x5700},		       /* F11 */
     {"^@=", 0x5800},		       /* F12 */
     {"\033\033",	0x011B},       /* ESC */
     /* Alt keys (high bit set) */
     {"\233",	0x0100 | ALT_MASK},    /* Alt Esc */
     {"\377",	0x0E00 | ALT_MASK},    /* Alt Backspace */
     {"\361",	0x1000 | ALT_MASK},    /* Alt Q */
     {"\367",	0x1100 | ALT_MASK},    /* Alt W */
     {"\345",	0x1200 | ALT_MASK},    /* Alt E */
     {"\362",	0x1300 | ALT_MASK},    /* Alt R */
     {"\364",	0x1400 | ALT_MASK},    /* Alt T */
     {"\371",	0x1500 | ALT_MASK},    /* Alt Y */
     {"\365",	0x1600 | ALT_MASK},    /* Alt U */
     {"\351",	0x1700 | ALT_MASK},    /* Alt I */
     {"\357",	0x1800 | ALT_MASK},    /* Alt O */
     {"\360",	0x1900 | ALT_MASK},    /* Alt P */
     {"\333",	0x1A00 | ALT_MASK},    /* Alt [ */
     {"\335",	0x1B00 | ALT_MASK},    /* Alt ] */
     {"\215",	0x1C00 | ALT_MASK},    /* Alt Enter */
     {"\341",	0x1E00 | ALT_MASK},    /* Alt A */ 
     {"\363",	0x1F00 | ALT_MASK},    /* Alt S */
   /*  {"\344", 0x2000}, */ /* Alt D *//* This is not (always) true */
     {"\346",	0x2100 | ALT_MASK},    /* Alt F */
     {"\347",	0x2200 | ALT_MASK},    /* Alt G */
     {"\350",	0x2300 | ALT_MASK},    /* Alt H */
     {"\352",	0x2400 | ALT_MASK},    /* Alt J */
     {"\353",	0x2500 | ALT_MASK},    /* Alt K */
     {"\354",	0x2600 | ALT_MASK},    /* Alt L */
     {"\273",	0x2700 | ALT_MASK},    /* Alt ; */
     {"\247",	0x2800 | ALT_MASK},    /* Alt ' */
     {"\340",	0x2900 | ALT_MASK},    /* Alt ` */
   /*  {"\334", 0x2B00}, *//* Alt \ *//* This is not (always) true */
     {"\372",	0x2c00 | ALT_MASK},    /* Alt Z */
     {"\370",	0x2d00 | ALT_MASK},    /* Alt X */
     {"\343",	0x2e00 | ALT_MASK},    /* Alt C */
   /*  {"\366", 0x2f00}, *//* Alt V *//* This is not (always) true */
     {"\342",	0x3000 | ALT_MASK},    /* Alt B */
     {"\356",	0x3100 | ALT_MASK},    /* Alt N */
     {"\355",	0x3200 | ALT_MASK},    /* Alt M */
     {"\254",	0x3300 | ALT_MASK},    /* Alt , */
     {"\256",	0x3400 | ALT_MASK},    /* Alt . */
     {"\257",	0x3500 | ALT_MASK},    /* Alt / */
     {"\261",	0x7800 | ALT_MASK},    /* Alt 1 */
     {"\262",	0x7900 | ALT_MASK},    /* Alt 2 */
     {"\263",	0x7A00 | ALT_MASK},    /* Alt 3 */
     {"\264",	0x7B00 | ALT_MASK},    /* Alt 4 */
     {"\265",	0x7C00 | ALT_MASK},    /* Alt 5 */
     {"\266",	0x7D00 | ALT_MASK},    /* Alt 6 */
     {"\267",	0x7E00 | ALT_MASK},    /* Alt 7 */
     {"\270",	0x7F00 | ALT_MASK},    /* Alt 8 */
     {"\271",	0x8000 | ALT_MASK},    /* Alt 9 */
     {"\260",	0x8100 | ALT_MASK},    /* Alt 0 */
     {"\255",	0x8200 | ALT_MASK},    /* Alt - */
     {"\275",	0x8300 | ALT_MASK},    /* Alt = */
     {"\211",	0xA500 | ALT_MASK},    /* Alt Tab */
/* Another form of alt keys */   
     {"\033q",	0x1000 | ALT_MASK},    /* Alt Q */
     {"\033w",	0x1100 | ALT_MASK},    /* Alt W */
     {"\033e",	0x1200 | ALT_MASK},    /* Alt E */
     {"\033r",	0x1300 | ALT_MASK},    /* Alt R */
     {"\033t",	0x1400 | ALT_MASK},    /* Alt T */
     {"\033y",	0x1500 | ALT_MASK},    /* Alt Y */
     {"\033u",	0x1600 | ALT_MASK},    /* Alt U */
     {"\033i",	0x1700 | ALT_MASK},    /* Alt I */
     {"\033o",	0x1800 | ALT_MASK},    /* Alt O */
     {"\033p",	0x1900 | ALT_MASK},    /* Alt P */
     {"\033\015",	0x1C00 | ALT_MASK},/* Alt Enter */
     {"\033a",	0x1e00 | ALT_MASK},    /* Alt A */
     {"\033s",	0x1f00 | ALT_MASK},    /* Alt S */
     {"\033d",	0x2000 | ALT_MASK},    /* Alt D */
     {"\033f",	0x2100 | ALT_MASK},    /* Alt F */
     {"\033g",	0x2200 | ALT_MASK},    /* Alt G */
     {"\033h",	0x2300 | ALT_MASK},    /* Alt H */
     {"\033j",	0x2400 | ALT_MASK},    /* Alt J */
     {"\033k",	0x2500 | ALT_MASK},    /* Alt K */
     {"\033l",	0x2600 | ALT_MASK},    /* Alt L */
     {"\033;",	0x2700 | ALT_MASK},    /* Alt ; */
     {"\033'",	0x2800 | ALT_MASK},    /* Alt ' */
     {"\033`",	0x2900 | ALT_MASK},    /* Alt ` */
     {"\033\\",	0x2B00 | ALT_MASK},    /* Alt \ */
     {"\033z",	0x2c00 | ALT_MASK},    /* Alt Z */
     {"\033x",	0x2d00 | ALT_MASK},    /* Alt X */
     {"\033c",	0x2e00 | ALT_MASK},    /* Alt C */
     {"\033v",	0x2f00 | ALT_MASK},    /* Alt V */
     {"\033b",	0x3000 | ALT_MASK},    /* Alt B */
     {"\033n",	0x3100 | ALT_MASK},    /* Alt N */
     {"\033m",	0x3200 | ALT_MASK},    /* Alt M */
     {"\033,",	0x3300 | ALT_MASK},    /* Alt , */
     {"\033.",	0x3400 | ALT_MASK},    /* Alt . */
     {"\033/",	0x3500 | ALT_MASK},    /* Alt / */
     {"\0331",	0x7800 | ALT_MASK},    /* Alt 1 */
     {"\0332",	0x7900 | ALT_MASK},    /* Alt 2 */
     {"\0333",	0x7a00 | ALT_MASK},    /* Alt 3 */
     {"\0334",	0x7b00 | ALT_MASK},    /* Alt 4 */
     {"\0335",	0x7c00 | ALT_MASK},    /* Alt 5 */
     {"\0336",	0x7d00 | ALT_MASK},    /* Alt 6 */
     {"\0337",	0x7e00 | ALT_MASK},    /* Alt 7 */
     {"\0338",	0x7f00 | ALT_MASK},    /* Alt 8 */
     {"\0339",	0x8000 | ALT_MASK},    /* Alt 9 */
     {"\0330",	0x8100 | ALT_MASK},    /* Alt 0 */
     {"\033-",	0x8200 | ALT_MASK},    /* Alt - */
     {"\033=",	0x8300 | ALT_MASK},    /* Alt = */
     {"\033\011",	0xA500 | ALT_MASK},/* Alt Tab */
/* Keypad keys */
   {"\033OM",	0x1C0D},	       /* Keypad Enter */
   {"\033OQ",	0x352F},	       /* Keypad / */
   {"\033OR",	0x372A},	       /* Keypad * */
   {"\033Ow",	0x4700},	       /* Keypad 7 */
   {"\033Ox",	0x4800},	       /* Keypad 8 */
   {"\033Oy",	0x4900},	       /* Keypad 9 */
   {"\033OS",	0x4A2D},	       /* Keypad - */
   {"\033Ot",	0x4B00},	       /* Keypad 4 */
   {"\033Ou",	0x4C00},	       /* Keypad 5 */
   {"\033Ov",	0x4D00},	       /* Keypad 6 */
   {"\033Ol",	0x4E2B},	       /* Keypad + */
   {"\033Oq",	0x4F00},	       /* Keypad 1 */
   {"\033Or",	0x5000},	       /* Keypad 2 */
   {"\033Os",	0x5100},	       /* Keypad 3 */
   {"\033Op",	0x5200},	       /* Keypad 0 */
   {"\033On",	0x5300},	       /* Keypad . */

/* Now here are the simulated keys using escape sequences */
   {"\033[2~",	0x52E0},	       /* Ins */
   {"\033[3~",	0x53E0},	       /* Del    Another keyscan is 0x007F */
   {"\033[1~",	0x47E0},	       /* Ho     Another keyscan is 0x5c00 */
   {"\033[H",	0x47E0},	       /* Ho */
   {"\033[4~",	0x4fE0},	       /* End    Another keyscan is 0x6100 */
   {"\033[K",	0x4fE0},	       /* End */
   {"\033[5~",	0x49E0},	       /* PgUp */
   {"\033[6~",	0x51E0},	       /* PgDn */
   {"\033[A",	0x48E0},	       /* Up */
   {"\033OA",	0x48E0},	       /* Up */
   {"\033[B",	0x50E0},	       /* Dn */
   {"\033OB",	0x50E0},	       /* Dn */
   {"\033[C",	0x4dE0},	       /* Ri */
   {"\033OC",	0x4dE0},	       /* Ri */
   {"\033[D",	0x4bE0},	       /* Le */
   {"\033OD",	0x4bE0},	       /* Le */
   {"\033[[A",	0x3b00},	       /* F1 */
   {"\033[[B",	0x3c00},	       /* F2 */
   {"\033[[C",	0x3d00},	       /* F3 */
   {"\033[[D",	0x3e00},	       /* F4 */
   {"\033[[E",	0x3f00},	       /* F5 */
   {"\033[11~",	0x3b00},	       /* F1 */
   {"\033[12~",	0x3c00},	       /* F2 */
   {"\033[13~",	0x3d00},	       /* F3 */
   {"\033[14~",	0x3e00},	       /* F4 */
   {"\033[15~",	0x3f00},	       /* F5 */
   {"\033[17~",	0x4000},	       /* F6 */
   {"\033[18~",	0x4100},	       /* F7 */
   {"\033[19~",	0x4200},	       /* F8 */
   {"\033[20~",	0x4300},	       /* F9 */
   {"\033[21~",	0x4400},	       /* F10 */
   {"\033[23~",	0x5400 | SHIFT_MASK},	       /* Shift F1  (F11 acts like Shift-F1) */
   {"\033[24~",	0x5500 | SHIFT_MASK},	       /* Shift F2  (F12 acts like Shift-F2) */
   {"\033[25~",	0x5600 | SHIFT_MASK},	       /* Shift F3 */
   {"\033[26~",	0x5700 | SHIFT_MASK},	       /* Shift F4 */
   {"\033[28~",	0x5800 | SHIFT_MASK},	       /* Shift F5 */
   {"\033[29~",	0x5900 | SHIFT_MASK},	       /* Shift F6 */
   {"\033[31~",	0x5A00 | SHIFT_MASK},	       /* Shift F7 */
   {"\033[32~",	0x5B00 | SHIFT_MASK},	       /* Shift F8 */
   {"\033[33~",	0x5C00 | SHIFT_MASK},	       /* Shift F9 */
   {"\033[34~",	0x5D00 | SHIFT_MASK},	       /* Shift F10 */

#if SLANG_VERSION > 9934
   {"^(k1)",	0x3b00},	       /* F1 */
   {"^(k2)",	0x3c00},	       /* F2 */
   {"^(k3)",	0x3d00},	       /* F3 */
   {"^(k4)",	0x3e00},	       /* F4 */
   {"^(k5)",	0x3f00},	       /* F5 */
   {"^(k6)",	0x4000},	       /* F6 */
   {"^(k7)",	0x4100},	       /* F7 */
   {"^(k8)",	0x4200},	       /* F8 */
   {"^(k9)",	0x4300},	       /* F9 */
   {"^(k;)",	0x4400},	       /* F10 */
   {"^(kI)",	0x52E0},	       /* Ins */
   {"^(kD)",	0x53E0},	       /* Del    Another keyscan is 0x007F */
   {"^(kh)",	0x47E0},	       /* Ho */
   {"^(@7)",	0x4fE0},	       /* End */
   {"^(kP)",	0x49E0},	       /* PgUp */
   {"^(kN)",	0x51E0},	       /* PgDn */
   {"^(ku)",	0x48E0},	       /* Up */
   {"^(kd)",	0x50E0},	       /* Dn */
   {"^(kr)",	0x4dE0},	       /* Ri */
   {"^(kl)",	0x4bE0},	       /* Le */
#endif

   {"",	0}
};

static SLKeyMap_List_Type *The_Normal_KeyMap;

#if SLANG_VERSION > 9929
# define SLang_define_key1(s,f,t,m) SLkm_define_key((s), (FVOID_STAR)(f), (m))
#endif

static unsigned char Esc_Char;
int define_key (unsigned char *key, unsigned long scan, SLKeyMap_List_Type *m)
{
   unsigned char buf[16], k1;
   
   if (SLang_Error) return -1;
   if ((*key == '^') && (Esc_Char != '@'))
     {
	k1 = key[1];
	if (k1 == Esc_Char) return 0;  /* ^Esc_Char is not defined here */
	if (k1 == '@')
	  {
	     strcpy ((char *) buf, (char *) key);
	     buf[1] = Esc_Char;
	     key = buf;
	  }
     }
   SLang_define_key1 (key, (VOID *) scan, SLKEY_F_INTRINSIC, m);
   if (SLang_Error)
     {
	fprintf (stderr, "Bad key: %s\n", key);
	return -1;
     }
   return 0;
}

/* The entries with 0x0000 are not going to be used by any sane person. */
unsigned long Ctrl_Char_Scan_Codes[32] = 
{
   0x0300,			       /* ^@ */
   0x1E01,			       /* ^A */
   0x3002,			       /* ^B */
   0x2E03,			       /* ^C */
   0x2004,			       /* ^D */
   0x1205,			       /* ^E */
   0x2106,			       /* ^F */
   0x2207,			       /* ^G */
   0x2308,			       /* ^H */
   0x0F09,			       /* TAB */
   0x240A,			       /* ^J */
   0x250B,			       /* ^K */
   0x260C,			       /* ^L */
   0x0000,			       /* ^M --- RETURN-- Not used.*/
   0x310E,			       /* ^N */
   0x180F,			       /* ^O */
   0x2510,			       /* ^P */
   0x1011,			       /* ^Q */
   0x1312,			       /* ^R */
   0x1F13,			       /* ^S */
   0x1414,			       /* ^T */
   0x1615,			       /* ^U */
   0x2F16,			       /* ^V */
   0x1117,			       /* ^W */
   0x2D18,			       /* ^X */
   0x1519,			       /* ^Y */
   0x2C1A,			       /* ^Z */
   0x0000,			       /* ^[ --- ESC --- Not used. */
   0x2B1C,			       /* ^\\ */
   0x1B1D,			       /* ^] */
   0x071E,			       /* ^^ */
   0x0C1F			       /* ^_ */
};

static void define_key_from_keymap (unsigned char *map, unsigned long mask, int flag)
{
   unsigned char buf[3], ch;
   int i, imax;
   static unsigned char mapped_already[256];
   
   imax = 97;
   buf[1] = 0; buf[2] = 0;
   for (i = 0; i < imax; i++)
     {
	if (((ch = map[i]) == 0) || (ch == 27) 
	    || (ch == config.term_esc_char)) continue;
	
	if (!mapped_already [ch])
	  {
	     buf[0] = ch;
	     define_key (buf, mask | ((unsigned long) i << 8), The_Normal_KeyMap);
	  }
	mapped_already[ch] = 1;
	
	if (flag && (ch < '@'+ 32) && (ch > '@') && (ch != '[')
	    && (0 == mapped_already[ch - '@']))
	  {
	     mapped_already[ch - '@'] = 1;
	     buf[0] = '^'; buf[1] = ch; 
	     define_key (buf, CTRL_MASK | ((unsigned long) i << 8), The_Normal_KeyMap);
	     buf[1] = 0;
	  }
     }
}

	
int init_slang_keymaps (void)
{
   char *str;
   SLKeyMap_List_Type *m;
   Keymap_Scan_Type *k;
   unsigned char buf[5];
   unsigned long esc_scan;
   
   /* Do some sanity checking */
   if (config.term_esc_char >= 32) config.term_esc_char = 30;
   
   esc_scan = Ctrl_Char_Scan_Codes[config.term_esc_char];
   if (esc_scan == 0) 
     {
	config.term_esc_char = 30;
	esc_scan = Ctrl_Char_Scan_Codes [30];
     }

   esc_scan |= CTRL_MASK;
   
   Esc_Char = config.term_esc_char + '@';
   
   
   if (The_Normal_KeyMap != NULL) return 0;
   
   if (NULL == (The_Normal_KeyMap = SLang_create_keymap ("Normal", NULL)))
     return -1;

   k = Normal_Map; m = The_Normal_KeyMap;
  
   while ((str = k->keystr), (*str != 0))
     {
	define_key (str, k->scan_code, m);
	k++;
     }

   /* Extract as much information as possible from the configed keypaps */
   define_key_from_keymap (config.key_map, 0, 1);
   define_key_from_keymap (config.shift_map, SHIFT_MASK, 1);
   define_key_from_keymap (config.alt_map, ALT_MASK, 0);
  
   /* Now setup the shift modifier keys */
   define_key ("^@a", ALT_KEY_SCAN_CODE, m);
   define_key ("^@c", CTRL_KEY_SCAN_CODE, m);
   define_key ("^@s", SHIFT_KEY_SCAN_CODE, m);
	
   define_key ("^@A", STICKY_ALT_KEY_SCAN_CODE, m);
   define_key ("^@C", STICKY_CTRL_KEY_SCAN_CODE, m);
   define_key ("^@S", STICKY_SHIFT_KEY_SCAN_CODE, m);
	
   define_key ("^@?", HELP_SCAN_CODE, m);
   define_key ("^@h", HELP_SCAN_CODE, m);
	
   define_key ("^@^R", REDRAW_SCAN_CODE, m);
   define_key ("^@^L", REDRAW_SCAN_CODE, m);
   define_key ("^@^Z", SUSPEND_SCAN_CODE, m);
   define_key ("^@ ", RESET_SCAN_CODE, m);
   define_key ("^@B", SET_MONO_SCAN_CODE, m);
	
   define_key ("^@\033[A", SCROLL_UP_SCAN_CODE, m);
   define_key ("^@\033OA", SCROLL_UP_SCAN_CODE, m);
   define_key ("^@U", SCROLL_UP_SCAN_CODE, m);
   
   define_key ("^@\033[B", SCROLL_DOWN_SCAN_CODE, m);
   define_key ("^@\033OB", SCROLL_DOWN_SCAN_CODE, m);
   define_key ("^@D", SCROLL_DOWN_SCAN_CODE, m);
	
   if (SLang_Error) return -1;
   
   /* Now add one more for the esc character so that sending it twice
    * sends it.
    */
   buf[0] = '^'; buf[1] = Esc_Char; 
   buf[2] = '^'; buf[3] = Esc_Char; 
   buf[4] = 0;
   SLang_define_key1 (buf, (VOID *) esc_scan, SLKEY_F_INTRINSIC, The_Normal_KeyMap);
   if (SLang_Error) return -1;
   return 0;
}




/* Global variables this module uses:
 *   int kbcount : 
 *       number of characters in the keyboard buffer to be processed.
 *   unsigned char kbbuf[KBBUF_SIZE] : 
 *       buffer where characters are waiting to be processed
 *   unsigned char *kbp : 
 *       pointer into kbbuf where next char to be processed is.
 */


static int read_some_keys (void)
{
   int cc;

   if (kbcount == 0) kbp = kbbuf;
   else if (kbp > &kbbuf[(KBBUF_SIZE * 3) / 5]) 
     {
	memmove(kbbuf, kbp, kbcount);
	kbp = kbbuf;
     }
   cc = read(kbd_fd, &kbp[kbcount], KBBUF_SIZE - kbcount - 1);
   k_printf("KBD: cc found %d characters (Xlate)\n", cc);
   if (cc > 0) kbcount += cc;
   return cc;
}


/* This function is a callback to read the key.  It returns 0 if one is not
 * ready.
 */

static int KeyNot_Ready;	       /* a flag */
static int Keystr_Len;
static int getkey_callback (void)
{
   if (kbcount == Keystr_Len) read_some_keys ();
   if (kbcount == Keystr_Len)
     {
	KeyNot_Ready = 1;
	return 0;
     }
   return (int) *(kbp + Keystr_Len++);
}

static int sltermio_input_pending (void)
{
   struct timeval scr_tv;
   hitimer_t t_start;
   fd_set fds;
   long t_dif;

#if 0
#define	THE_TIMEOUT 750000L
#else
#define THE_TIMEOUT 250000L
#endif
   FD_ZERO(&fds);
   FD_SET(kbd_fd, &fds);
   scr_tv.tv_sec = 0L;
   scr_tv.tv_usec = THE_TIMEOUT;
   
   t_start = GETusTIME(0);
   errno = 0;
   while ((int) select(kbd_fd + 1, &fds, NULL, NULL, &scr_tv) < (int) 1) 
     {
	t_dif = (long)(GETusTIME(0) - t_start);
	
	if ((t_dif >= THE_TIMEOUT) || (errno != EINTR))
	  return 0;
	errno = 0;
	scr_tv.tv_sec = 0L;
	scr_tv.tv_usec = THE_TIMEOUT - t_dif;
     }
   return 1;
}



#define SHIFT_SCAN	0x2A
#define CTRL_SCAN	0x1D
#define ALT_SCAN	0x38

/* If the sticky bits are set, then the scan code or the modifier key has 
 * already been taken care of.  In this case, the unsticky bit should be 
 * ignored.
 */
static unsigned long Shift_Flags;

static void slang_add_scancode (unsigned long lscan)
{
   unsigned long flags = 0;

   /* fprintf (stderr, "Scancode: %X\n", lscan); */
   if ((lscan & SHIFT_MASK)
       && ((lscan & STICKY_SHIFT_MASK) == 0))
     {
	flags |= SHIFT_MASK;
	add_scancode_to_queue (SHIFT_SCAN);
     }
   
   if ((lscan & CTRL_MASK)
       && ((lscan & STICKY_CTRL_MASK) == 0))
     {
	flags |= CTRL_MASK;
	add_scancode_to_queue (CTRL_SCAN);
     }
   
   if ((lscan & ALT_MASK)
       && ((lscan & STICKY_ALT_MASK) == 0))
     {
	flags |= ALT_MASK;
	add_scancode_to_queue (ALT_SCAN);
     }

   if ((lscan & 0xFF) == 0xE0) add_scancode_to_queue (0xE0);
   add_scancode_to_queue ((lscan & 0xFF00) >> 8);
   add_scancode_to_queue (((lscan | 0x8000) & 0xFF00) >> 8);

   if (flags & SHIFT_MASK) 
     {
	add_scancode_to_queue (SHIFT_SCAN | 0x80);
	Shift_Flags &= ~SHIFT_MASK;
     }
   if (flags & CTRL_MASK) 
     {
	add_scancode_to_queue (CTRL_SCAN | 0x80);
	Shift_Flags &= ~CTRL_MASK;
     }
   if (flags & ALT_MASK) 
     {
	add_scancode_to_queue (ALT_SCAN | 0x80);
	Shift_Flags &= ~ALT_MASK;
     }
}



void do_slang_getkeys (void)
{
   SLang_Key_Type *key;
   unsigned long scan;
   
   if (-1 == read_some_keys ()) return;
   
   /* Now process the keys that are buffered up */
   while (kbcount)
     {
	Keystr_Len = 0;
	KeyNot_Ready = 0;
	key = SLang_do_key (The_Normal_KeyMap, getkey_callback);
	
	SLang_Error = 0;

	if (KeyNot_Ready)
	  {
	     if ((Keystr_Len == 1) && (*kbp == 27))
	       {
		  /* We have an esc character.  If nothing else is available 
		   * to be read after a brief period, assume user really
		   * wants esc.
		   */
		  if (sltermio_input_pending ()) return;
		  
		  slang_add_scancode (0x011b | Shift_Flags);
		  key = NULL;
		  /* drop on through to the return for the undefined key below. */
	       }
	     else break; /* try again next time */
	  }
		
	kbcount -= Keystr_Len;	       /* update count */
	kbp += Keystr_Len;
	
	if (key == NULL) 
	  {
	     /* undefined key --- return */
	     DOSemu_Slang_Show_Help = 0;
	     kbcount = 0;
	     break;
	  }
	
	if (DOSemu_Slang_Show_Help)
	  {
	     DOSemu_Slang_Show_Help = 0;
	     continue;
	  }
	     
#if SLANG_VERSION < 9930
        scan = (unsigned long) key->f;
#else
	scan = (unsigned long) key->f.f;
#endif
	
	if (scan < ALT_KEY_SCAN_CODE)
	  {
  	     slang_add_scancode (scan | Shift_Flags);
	     continue;
	  }
	
	switch (scan)
	  {
	   case STICKY_ALT_KEY_SCAN_CODE:
	   case ALT_KEY_SCAN_CODE:
	     if (Shift_Flags & STICKY_ALT_MASK)
	       {
		  add_scancode_to_queue (ALT_SCAN | 0x80);
		  Shift_Flags &= ~STICKY_ALT_MASK;
		  break;
	       }
	     
	     if (scan == STICKY_ALT_KEY_SCAN_CODE)
	       {
		  Shift_Flags |= STICKY_ALT_MASK;
		  add_scancode_to_queue (ALT_SCAN);
	       }
	     else Shift_Flags |= ALT_MASK;
	     break;
	     
	     
	   case STICKY_SHIFT_KEY_SCAN_CODE:
	   case SHIFT_KEY_SCAN_CODE:
	     if (Shift_Flags & STICKY_SHIFT_MASK)
	       {
		  add_scancode_to_queue (SHIFT_SCAN | 0x80);
		  Shift_Flags &= ~STICKY_SHIFT_MASK;
		  break;
	       }
	     
	     if (scan == STICKY_SHIFT_KEY_SCAN_CODE)
	       {
		  Shift_Flags |= STICKY_SHIFT_MASK;
		  add_scancode_to_queue (SHIFT_SCAN);
	       }
	     else Shift_Flags |= SHIFT_MASK;
	     break;
	     
	     
	   case STICKY_CTRL_KEY_SCAN_CODE:
	   case CTRL_KEY_SCAN_CODE:
	     if (Shift_Flags & STICKY_CTRL_MASK)
	       {
		  add_scancode_to_queue (CTRL_SCAN | 0x80);
		  Shift_Flags &= ~STICKY_CTRL_MASK;
		  break;
	       }
	     
	     if (scan == STICKY_CTRL_KEY_SCAN_CODE)
	       {
		  Shift_Flags |= STICKY_CTRL_MASK;
		  add_scancode_to_queue (CTRL_SCAN);
	       }
	     else Shift_Flags |= CTRL_MASK;
	     break;

	     
	     
	   case SCROLL_DOWN_SCAN_CODE:
	     DOSemu_Terminal_Scroll = 1;
	     break;
	     
	   case SCROLL_UP_SCAN_CODE:
	     DOSemu_Terminal_Scroll = -1;
	     break;
	     
	   case REDRAW_SCAN_CODE:
	     dos_slang_redraw ();
	     break;
	     
	   case SUSPEND_SCAN_CODE:
	     dos_slang_suspend ();
	     break;
	     
	   case HELP_SCAN_CODE:
	     DOSemu_Slang_Show_Help = 1;
	     break;
	     
	   case RESET_SCAN_CODE:
	     DOSemu_Slang_Show_Help = 0;
	     DOSemu_Terminal_Scroll = 0;
	     
	     if (Shift_Flags & STICKY_CTRL_MASK)
	       {
		  add_scancode_to_queue (CTRL_SCAN | 0x80);
	       }
	     if (Shift_Flags & STICKY_SHIFT_MASK)
	       {
		  add_scancode_to_queue (SHIFT_SCAN | 0x80);
	       }
	     if (Shift_Flags & STICKY_ALT_MASK)
	       {
		  add_scancode_to_queue (ALT_SCAN | 0x80);
	       }
	     
	     Shift_Flags = 0;
		  
	     break;

	   case SET_MONO_SCAN_CODE:
	     dos_slang_smart_set_mono ();
	     break;
	  }
     }

   
   if (Shift_Flags & (ALT_MASK | STICKY_ALT_MASK))
     {
	if (Shift_Flags & (SHIFT_MASK | STICKY_SHIFT_MASK))
	  {
	     if (Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK))
	       {
		  DOSemu_Keyboard_Keymap_Prompt = "[Alt-Ctrl-Shift]";
	       }
	     else DOSemu_Keyboard_Keymap_Prompt = "[Alt-Shift]";
	  }
	else if (Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK))
	  DOSemu_Keyboard_Keymap_Prompt = "[Alt-Ctrl]";
	else DOSemu_Keyboard_Keymap_Prompt = "[Alt]";
     }
   else if (Shift_Flags & (SHIFT_MASK | STICKY_SHIFT_MASK))
     {
	if (Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK))
	  {
	     DOSemu_Keyboard_Keymap_Prompt = "[Ctrl-Shift]";
	  }
	else DOSemu_Keyboard_Keymap_Prompt = "[Shift]";
     }
   else if (Shift_Flags & (CTRL_MASK | STICKY_CTRL_MASK))
     DOSemu_Keyboard_Keymap_Prompt = "[Ctrl]";
   else DOSemu_Keyboard_Keymap_Prompt = NULL;
}
