#include "slang.h"

/* These need to be made user definable */
static char *Alt_Shiftkey = "^@A";
static char *Ctrl_Shiftkey = "^@C";
static char *Shift_Shiftkey = "^@S";

char *DOSemu_Keyboard_Keymap_Prompt = NULL;

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
   unsigned char keystr[8];
   unsigned int scan_code;
}
Keymap_Scan_Type;

#define ALT_KEY_SCAN_CODE 0x10000
#define SHIFT_KEY_SCAN_CODE 0x10001
#define CTRL_KEY_SCAN_CODE 0x10002


static Keymap_Scan_Type Normal_Map [] =
{
   {"\033\033",	0x011B},	       /* ESC */
   {"1",	0x0231},   	       /* 1 */
   {"!",	0x0221},	       /* ! */
   {"2",	0x0332},	       /* 2 */
   {"@",	0x0340},	       /* @ */
   {"^@",	0x0300},	       /* ^@ */
   {"3",	0x0433},	       /* 3 */
   {"#",	0x0423},	       /* # */
   {"4",	0x0534},	       /* 4 */
   {"$",	0x0524},	       /* $ */
   {"5",	0x0635},	       /* 5 */
   {"%",	0x0625},	       /* % */
   {"6",	0x0736},	       /* 6 */
   {"^",	0x075E},	       /* ^ */
   {"^^",	0x071E},	       /* ^^ */
   {"7",	0x0837},	       /* 7 */
   {"&",	0x0826},	       /* & */
   {"8",	0x0938},	       /* 8 */
   {"*",	0x092A},	       /* Shift-8 */
   {"9",	0x0A39},	       /* 9 */
   {"(",	0x0A28},	       /* ( */
   {"0",	0x0B30},	       /* 0 */
   {")",	0x0B29},	       /* ) */
   {"-",	0x0C2D},	       /* - */
   {"_",	0x0C5F},	       /* _ */
   {"^_",	0x0C1F},	       /* ^_ */
   {"=",	0x0D3D},	       /* = */
   {"+",	0x0D2B},	       /* + */
   {"^?",	0x0E08},	       /* Delete mapped to backspace. */
   {"^I",	0x0F09},   	       /* TAB */
   {"q",	0x1071},	       /* q */
   {"Q",	0x1051},	       /* Q */
   {"^Q",	0x1011},	       /* ^Q */
   {"w",	0x1177},	       /* w */
   {"W",	0x1157},	       /* W */
   {"^W",	0x1117},	       /* ^W */
   {"e",	0x1265},	       /* e */
   {"E",	0x1245},	       /* E */
   {"^E",	0x1205},	       /* ^E */
   {"r",	0x1372},	       /* r */
   {"R",	0x1352},	       /* R */
   {"^R",	0x1312},	       /* ^R */
   {"t",	0x1474},	       /* t */
   {"T",	0x1454},	       /* T */
   {"^T",	0x1414},	       /* ^T */
   {"y",	0x1579},	       /* y */
   {"Y",	0x1559},	       /* Y */
   {"^Y",	0x1519},	       /* ^Y */
   {"u",	0x1675},	       /* u */
   {"U",	0x1655},	       /* U */
   {"^U",	0x1615},	       /* ^U */
   {"i",	0x1769},	       /* i */
   {"I",	0x1749},	       /* I */
   {"o",	0x186F},	       /* o */
   {"O",	0x184F},	       /* O */
   {"^O",	0x180F},	       /* ^O */
   {"p",	0x1970},	       /* p */
   {"P",	0x1950},	       /* P */
   {"^P",	0x2510},	       /* ^P */
   {"[",	0x1A5B},	       /* [ */
   {"{",	0x1A7B},	       /* { */
   {"]",	0x1B5D},	       /* ] */
   {"}",	0x1B7D},	       /* } */
   {"^]",	0x1B1D},	       /* ^] */
   {"^M",	0x1C0D},	       /* ENTER */
   {"a",	0x1E61},	       /* a */
   {"A",	0x1E41},	       /* A */
   {"^A",	0x1E01},	       /* ^A */
   {"s",	0x1F73},	       /* s */
   {"S",	0x1F53},	       /* S */
   {"^S",	0x1F13},	       /* ^S */
   {"d",	0x2064},	       /* d */
   {"D",	0x2044},	       /* D */
   {"^D",	0x2004},	       /* ^D */
   {"f",	0x2166},	       /* f */
   {"F",	0x2146},	       /* F */
   {"^F",	0x2106},	       /* ^F */
   {"g",	0x2267},	       /* g */
   {"G",	0x2247},	       /* G */
   {"^G",	0x2207},	       /* ^G */
   {"h",	0x2368},	       /* h */
   {"H",	0x2348},	       /* H */
   {"^H",	0x2308},	       /* ^H */
   {"j",	0x246A},	       /* j */
   {"J",	0x244A},	       /* J */
   {"^J",	0x240A},	       /* ^J */
   {"k",	0x256B},	       /* k */
   {"K",	0x254B},	       /* K */
   {"^K",	0x250B},	       /* ^K */
   {"l",	0x266C},	       /* l */
   {"L",	0x264C},	       /* L */
   {"^L",	0x260C},	       /* ^L */
   {";",	0x273B},	       /* ; */
   {":",	0x273A},	       /* : */
   {"'",	0x2827},	       /* ' */
   {"\"",	0x2822},	       /* " */
   {"`",	0x2960},	       /* ` */
   {"~",	0x297E},	       /* ~ */
   {"\\",	0x2B5C},	       /* \\ */
   {"|",	0x2B7C},	       /* | */
   {"^\\",	0x2B1C},	       /* ^\\ */
   {"z",	0x2C7A},	       /* z */
   {"Z",	0x2C5A},	       /* Z */
   {"^Z",	0x2C1A},	       /* ^Z */
   {"x",	0x2D78},	       /* x */
   {"X",	0x2D58},	       /* X */
   {"^X",	0x2D18},	       /* ^X */
   {"c",	0x2E63},	       /* c */
   {"C",	0x2E43},	       /* C */
   {"^C",	0x2E03},	       /* ^C */
   {"v",	0x2F76},	       /* v */
   {"V",	0x2F56},	       /* V */
   {"^V",	0x2F16},	       /* ^V */
   {"b",	0x3062},	       /* b */
   {"B",	0x3042},	       /* B */
   {"^B",	0x3002},	       /* ^B */
   {"n",	0x316E},	       /* n */
   {"N",	0x314E},	       /* N */
   {"^N",	0x310E},	       /* ^N */
   {"m",	0x326D},	       /* m */
   {"M",	0x324D},	       /* M */
   {",",	0x332C},	       /* , */
   {"<",	0x333C},	       /* < */
   {".",	0x342E},	       /* . */
   {">",	0x343E},	       /* > */
   {"/",	0x352F},	       /* / */
   {"?",	0x353F},	       /* ? */
   {" ",	0x3920},	       /* SPACE */
   
/* Alt keys (high bit set) */
   {"\374",	0x0081},	       /* Umlaut ue */
   {"\344",	0x0084},	       /* Umlaut ae */
   {"\304",	0x008E},	       /* Umlaut Ae */
   {"\366",	0x0094},	       /* Umlaut oe */
   {"\326",	0x0099},	       /* Umlaut Oe */
   {"\334",	0x009A},	       /* Umlaut Ue */
   {"\337",	0x00E1},	       /* Umlaut sz */
   {"\233",	0x0100},	       /* Alt Esc */
   {"\377",	0x0E00},	       /* Alt Backspace */
   {"\361",	0x1000},	       /* Alt Q */
   {"\367",	0x1100},	       /* Alt W */
   {"\345",	0x1200},	       /* Alt E */
   {"\362",	0x1300},	       /* Alt R */
   {"\364",	0x1400},	       /* Alt T */
   {"\371",	0x1500},	       /* Alt Y */
   {"\365",	0x1600},	       /* Alt U */
   {"\351",	0x1700},	       /* Alt I */
   {"\357",	0x1800},	       /* Alt O */
   {"\360",	0x1900},	       /* Alt P */
   {"\333",	0x1A00},	       /* Alt [ */
   {"\335",	0x1B00},	       /* Alt ] */
   {"\215",	0x1C00},	       /* Alt Enter */
   {"\341",	0x1E00},	       /* Alt A */ 
   {"\363",	0x1F00},	       /* Alt S */
   /*  {"\344", 0x2000}, */ /* Alt D *//* This is not (always) true */
   {"\346",	0x2100},	       /* Alt F */
   {"\347",	0x2200},	       /* Alt G */
   {"\350",	0x2300},	       /* Alt H */
   {"\352",	0x2400},	       /* Alt J */
   {"\353",	0x2500},	       /* Alt K */
   {"\354",	0x2600},	       /* Alt L */
   {"\273",	0x2700},	       /* Alt ; */
   {"\247",	0x2800},	       /* Alt ' */
   {"\340",	0x2900},	       /* Alt ` */
   /*  {"\334", 0x2B00}, *//* Alt \ *//* This is not (always) true */
   {"\372",	0x2c00},	       /* Alt Z */
   {"\370",	0x2d00},	       /* Alt X */
   {"\343",	0x2e00},	       /* Alt C */
   /*  {"\366", 0x2f00}, *//* Alt V *//* This is not (always) true */
   {"\342",	0x3000},	       /* Alt B */
   {"\356",	0x3100},	       /* Alt N */
   {"\355",	0x3200},	       /* Alt M */
   {"\254",	0x3300},	       /* Alt , */
   {"\256",	0x3400},	       /* Alt . */
   {"\257",	0x3500},	       /* Alt / */
   {"\261",	0x7800},	       /* Alt 1 */
   {"\262",	0x7900},	       /* Alt 2 */
   {"\263",	0x7A00},	       /* Alt 3 */
   {"\264",	0x7B00},	       /* Alt 4 */
   {"\265",	0x7C00},	       /* Alt 5 */
   {"\266",	0x7D00},	       /* Alt 6 */
   {"\267",	0x7E00},	       /* Alt 7 */
   {"\270",	0x7F00},	       /* Alt 8 */
   {"\271",	0x8000},	       /* Alt 9 */
   {"\260",	0x8100},	       /* Alt 0 */
   {"\255",	0x8200},	       /* Alt - */
   {"\275",	0x8300},	       /* Alt = */
   {"\211",	0xA500},	       /* Alt Tab */
/* Another form of alt keys */   
   {"\033q",	0x1000},	       /* Alt Q */
   {"\033w",	0x1100},	       /* Alt W */
   {"\033e",	0x1200},	       /* Alt E */
   {"\033r",	0x1300},	       /* Alt R */
   {"\033t",	0x1400},	       /* Alt T */
   {"\033y",	0x1500},	       /* Alt Y */
   {"\033u",	0x1600},	       /* Alt U */
   {"\033i",	0x1700},	       /* Alt I */
   {"\033o",	0x1800},	       /* Alt O */
   {"\033p",	0x1900},	       /* Alt P */
   {"\033\015",	0x1C00},	       /* Alt Enter */
   {"\033a",	0x1e00},	       /* Alt A */
   {"\033s",	0x1f00},	       /* Alt S */
   {"\033d",	0x2000},	       /* Alt D */
   {"\033f",	0x2100},	       /* Alt F */
   {"\033g",	0x2200},	       /* Alt G */
   {"\033h",	0x2300},	       /* Alt H */
   {"\033j",	0x2400},	       /* Alt J */
   {"\033k",	0x2500},	       /* Alt K */
   {"\033l",	0x2600},	       /* Alt L */
   {"\033;",	0x2700},	       /* Alt ; */
   {"\033'",	0x2800},	       /* Alt ' */
   {"\033`",	0x2900},	       /* Alt ` */
   {"\033\\",	0x2B00},	       /* Alt \ */
   {"\033z",	0x2c00},	       /* Alt Z */
   {"\033x",	0x2d00},	       /* Alt X */
   {"\033c",	0x2e00},	       /* Alt C */
   {"\033v",	0x2f00},	       /* Alt V */
   {"\033b",	0x3000},	       /* Alt B */
   {"\033n",	0x3100},	       /* Alt N */
   {"\033m",	0x3200},	       /* Alt M */
   {"\033,",	0x3300},	       /* Alt , */
   {"\033.",	0x3400},	       /* Alt . */
   {"\033/",	0x3500},	       /* Alt / */
   {"\0331",	0x7800},	       /* Alt 1 */
   {"\0332",	0x7900},	       /* Alt 2 */
   {"\0333",	0x7a00},	       /* Alt 3 */
   {"\0334",	0x7b00},	       /* Alt 4 */
   {"\0335",	0x7c00},	       /* Alt 5 */
   {"\0336",	0x7d00},	       /* Alt 6 */
   {"\0337",	0x7e00},	       /* Alt 7 */
   {"\0338",	0x7f00},	       /* Alt 8 */
   {"\0339",	0x8000},	       /* Alt 9 */
   {"\0330",	0x8100},	       /* Alt 0 */
   {"\033-",	0x8200},	       /* Alt - */
   {"\033=",	0x8300},	       /* Alt = */
   {"\033\011",	0xA500},	       /* Alt Tab */
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
   {"\033[2~",	0x5200},	       /* Ins */
   {"\033[3~",	0x5300},	       /* Del    Another keyscan is 0x007F */
   {"\033[1~",	0x4700},	       /* Ho     Another keyscan is 0x5c00 */
   {"\033[H",	0x4700},	       /* Ho */
   {"\033[4~",	0x4f00},	       /* End    Another keyscan is 0x6100 */
   {"\033[K",	0x4f00},	       /* End */
   {"\033[5~",	0x4900},	       /* PgUp */
   {"\033[6~",	0x5100},	       /* PgDn */
   {"\033[A",	0x4800},	       /* Up */
   {"\033OA",	0x4800},	       /* Up */
   {"\033[B",	0x5000},	       /* Dn */
   {"\033OB",	0x5000},	       /* Dn */
   {"\033[C",	0x4d00},	       /* Ri */
   {"\033OC",	0x4d00},	       /* Ri */
   {"\033[D",	0x4b00},	       /* Le */
   {"\033OD",	0x4b00},	       /* Le */
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
   {"\033[23~",	0x5400},	       /* Shift F1  (F11 acts like Shift-F1) */
   {"\033[24~",	0x5500},	       /* Shift F2  (F12 acts like Shift-F2) */
   {"\033[25~",	0x5600},	       /* Shift F3 */
   {"\033[26~",	0x5700},	       /* Shift F4 */
   {"\033[28~",	0x5800},	       /* Shift F5 */
   {"\033[29~",	0x5900},	       /* Shift F6 */
   {"\033[31~",	0x5A00},	       /* Shift F7 */
   {"\033[32~",	0x5B00},	       /* Shift F8 */
   {"\033[33~",	0x5C00},	       /* Shift F9 */
   {"\033[34~",	0x5D00},	       /* Shift F10 */
   {"",	0}
};

static Keymap_Scan_Type Alt_Map [] =
{
   {"1",	0x7800},	       /* Alt 1 */
   {"2",	0x7900},	       /* Alt 2 */
   {"3",	0x7a00},	       /* Alt 3 */
   {"4",	0x7b00},	       /* Alt 4 */
   {"5",	0x7c00},	       /* Alt 5 */
   {"6",	0x7d00},	       /* Alt 6 */
   {"7",	0x7e00},	       /* Alt 7 */
   {"8",	0x7f00},	       /* Alt 8 */
   {"9",	0x8000},	       /* Alt 9 */
   {"0",	0x8100},	       /* Alt 0 */
   {"-",	0x8200},	       /* Alt - */
   {"=",	0x8300},	       /* Alt = */
   {"Q",	0x1000},	       /* Alt Q */
   {"W",	0x1100},	       /* Alt W */
   {"E",	0x1200},	       /* Alt E */
   {"R",	0x1300},	       /* Alt R */
   {"T",	0x1400},	       /* Alt T */
   {"Y",	0x1500},	       /* Alt Y */
   {"U",	0x1600},	       /* Alt U */
   {"I",	0x1700},	       /* Alt I */
   {"O",	0x1800},	       /* Alt O */
   {"P",	0x1900},	       /* Alt P */
   {"[",	0x1A00},	       /* Alt [ */
   {"]",	0x1B00},	       /* Alt ] */
   {"A",	0x1e00},	       /* Alt A */
   {"S",	0x1f00},	       /* Alt S */
   {"D",	0x2000},	       /* Alt D */
   {"F",	0x2100},	       /* Alt F */
   {"G",	0x2200},	       /* Alt G */
   {"H",	0x2300},	       /* Alt H */
   {"J",	0x2400},	       /* Alt J */
   {"K",	0x2500},	       /* Alt K */
   {"L",	0x2600},	       /* Alt L */
   {";",	0x2700},	       /* Alt ; */
   {"'",	0x2800},	       /* Alt ' */
   {"`",	0x2900},	       /* Alt ` */
   {"\\",	0x2B00},	       /* Alt \ */  
   {"Z",	0x2c00},	       /* Alt Z */
   {"X",	0x2d00},	       /* Alt X */
   {"C",	0x2e00},	       /* Alt C */
   {"V",	0x2f00},	       /* Alt V */
   {"B",	0x3000},	       /* Alt B */
   {"N",	0x3100},	       /* Alt N */
   {"M",	0x3200},	       /* Alt M */
   {",",	0x3300},	       /* Alt , */
   {".",	0x3400},	       /* Alt . */
   {"/",	0x3500},	       /* Alt / */
/* F1-F10 */
   {"\033[[A",	0x6800},	       /* F1 */
   {"\033[[B",	0x6900},	       /* F2 */
   {"\033[[C",	0x6A00},	       /* F3 */
   {"\033[[D",	0x6B00},	       /* F4 */
   {"\033[[E",	0x6C00},	       /* F5 */
   {"\033[11~",	0x6800},	       /* F1 */
   {"\033[12~",	0x6900},	       /* F2 */
   {"\033[13~",	0x6A00},	       /* F3 */
   {"\033[14~",	0x6B00},	       /* F4 */
   {"\033[15~",	0x6C00},	       /* F5 */
   {"\033[17~",	0x6D00},	       /* F6 */
   {"\033[18~",	0x6E00},	       /* F7 */
   {"\033[19~",	0x6F00},	       /* F8 */
   {"\033[20~",	0x7000},	       /* F9 */
   {"\033[21~",	0x7100},	       /* F10 */
   {"",	0}
};

static Keymap_Scan_Type Ctrl_Map [] =
{
   {"2",	0x0300},	       /* ^@ */
   {"6",	0x071E},	       /* ^^ */
   {"-",	0x0C1F},	       /* ^_ */
   {"H",	0x0E08},	       /* Backspace */
   {"^?",	0x0E7F},	       /* Ctrl-Backspace (delete) */
   {"^H",	0x0E7F},	       /* Ctrl-Backspace (delete) */
   {"/",	0x0E7F},	       /* Ctrl-? (delete) */
   {"Q",	0x1011},	       /* ^Q */
   {"W",	0x1117},	       /* ^W */
   {"E",	0x1205},	       /* ^E */
   {"R",	0x1312},	       /* ^R */
   {"T",	0x1414},	       /* ^T */
   {"Y",	0x1519},	       /* ^Y */
   {"U",	0x1615},	       /* ^U */
   {"I",	0x1709},   	       /* ^I */
   {"O",	0x180F},	       /* ^O */
   {"P",	0x2510},	       /* ^P */
   {"[",	0x1A1B},	       /* ^[ (esc) */
   {"]",	0x1B1D},	       /* ^] */
   {"A",	0x1E01},	       /* ^A */
   {"S",	0x1F13},	       /* ^S */
   {"D",	0x2004},	       /* ^D */
   {"F",	0x2106},	       /* ^F */
   {"G",	0x2207},	       /* ^G */
   {"J",	0x240A},	       /* ^J */
   {"K",	0x250B},	       /* ^K */
   {"L",	0x260C},	       /* ^L */
   {"\\",	0x2B1C},	       /* ^\\ */
   {"Z",	0x2C1A},	       /* ^Z */
   {"X",	0x2D18},	       /* ^X */
   {"C",	0x2E03},	       /* ^C */
   {"V",	0x2F16},	       /* ^V */
   {"B",	0x3002},	       /* ^B */
   {"N",	0x310E},	       /* ^N */
   {"M",	0x320D},	       /* ^M */
   {" ",	0x0300},	       /* SPACE --> ^@ */
   
/* Now here are the simulated keys using escape sequences */
   {"\033[4~",	0x7500},	       /* End */
   {"\033[K",	0x7500},	       /* End */
   {"\033[5~",	0x8400},	       /* PgUp */
   {"\033[6~",	0x7600},	       /* PgDn */
   {"\033[C",	0x7400},	       /* Ri */
   {"\033OC",	0x7400},	       /* Ri */
   {"\033[D",	0x7300},	       /* Le */
   {"\033OD",	0x7300},	       /* Le */
   {"\033[[A",	0x5E00},	       /* F1 */
   {"\033[[B",	0x5F00},	       /* F2 */
   {"\033[[C",	0x6000},	       /* F3 */
   {"\033[[D",	0x6100},	       /* F4 */
   {"\033[[E",	0x6200},	       /* F5 */
   {"\033[11~",	0x5E00},	       /* F1 */
   {"\033[12~",	0x5F00},	       /* F2 */
   {"\033[13~",	0x6000},	       /* F3 */
   {"\033[14~",	0x6100},	       /* F4 */
   {"\033[15~",	0x6200},	       /* F5 */
   {"\033[17~",	0x6300},	       /* F6 */
   {"\033[18~",	0x6400},	       /* F7 */
   {"\033[19~",	0x6500},	       /* F8 */
   {"\033[20~",	0x6600},	       /* F9 */
   {"\033[21~",	0x6700},	       /* F10 */
   {"",	0}
};


static Keymap_Scan_Type Shift_Map [] =
{
   {"1",	0x0221},	       /* ! */
   {"2",	0x0340},	       /* @ */
   {"3",	0x0423},	       /* # */
   {"4",	0x0524},	       /* $ */
   {"5",	0x0625},	       /* % */
   {"6",	0x075E},	       /* ^ */
   {"7",	0x0826},	       /* & */
   {"8",	0x092A},	       /* Shift-8 */
   {"9",	0x0A28},	       /* ( */
   {"0",	0x0B29},	       /* ) */
   {"-",	0x0C5F},	       /* _ */
   {"=",	0x0D2B},	       /* + */
   {"^I",	0x0F00},   	       /* BACK TAB */
   {"q",	0x1051},	       /* Q */
   {"w",	0x1157},	       /* W */
   {"e",	0x1245},	       /* E */
   {"r",	0x1352},	       /* R */
   {"t",	0x1454},	       /* T */
   {"y",	0x1559},	       /* Y */
   {"u",	0x1655},	       /* U */
   {"i",	0x1749},	       /* I */
   {"o",	0x184F},	       /* O */
   {"p",	0x1950},	       /* P */
   {"[",	0x1A7B},	       /* { */
   {"]",	0x1B7D},	       /* } */
   {"a",	0x1E41},	       /* A */
   {"s",	0x1F53},	       /* S */
   {"d",	0x2044},	       /* D */
   {"f",	0x2146},	       /* F */
   {"g",	0x2247},	       /* G */
   {"h",	0x2348},	       /* H */
   {"j",	0x244A},	       /* J */
   {"k",	0x254B},	       /* K */
   {"l",	0x264C},	       /* L */
   {";",	0x273A},	       /* : */
   {"'",	0x2822},	       /* " */
   {"`",	0x297E},	       /* ~ */
   {"\\",	0x2B7C},	       /* | */
   {"z",	0x2C5A},	       /* Z */
   {"x",	0x2D58},	       /* X */
   {"c",	0x2E43},	       /* C */
   {"v",	0x2F56},	       /* V */
   {"b",	0x3042},	       /* B */
   {"n",	0x314E},	       /* N */
   {"m",	0x324D},	       /* M */
   {"<",	0x333C},	       /* < */
   {".",	0x343E},	       /* > */
   {"/",	0x353F},	       /* ? */
   
/* Now here are the simulated keys using escape sequences */
   {"\033[[A",	0x5400},	       /* F1 */
   {"\033[[B",	0x5500},	       /* F2 */
   {"\033[[C",	0x5600},	       /* F3 */
   {"\033[[D",	0x5700},	       /* F4 */
   {"\033[[E",	0x5800},	       /* F5 */
   {"\033[11~",	0x5400},	       /* F1 */
   {"\033[12~",	0x5500},	       /* F2 */
   {"\033[13~",	0x5600},	       /* F3 */
   {"\033[14~",	0x5700},	       /* F4 */
   {"\033[15~",	0x5800},	       /* F5 */
   {"\033[17~",	0x5900},	       /* F6 */
   {"\033[18~",	0x5A00},	       /* F7 */
   {"\033[19~",	0x5B00},	       /* F8 */
   {"\033[20~",	0x5C00},	       /* F9 */
   {"\033[21~",	0x5D00},	       /* F10 */
   {"",	0}
};

SLKeyMap_List_Type *The_Normal_KeyMap;
SLKeyMap_List_Type *The_Alt_KeyMap;
SLKeyMap_List_Type *The_Ctrl_KeyMap;
SLKeyMap_List_Type *The_Shift_KeyMap;


static int init_slang_keymaps (void)
{
   char *str;
   Keymap_Scan_Type *k;
   
   if (The_Normal_KeyMap != NULL) return 0;
   
   if ((NULL == (The_Normal_KeyMap = SLang_create_keymap ("Normal", NULL)))
       || (NULL == (The_Alt_KeyMap = SLang_create_keymap ("Alt", NULL)))
       || (NULL == (The_Ctrl_KeyMap = SLang_create_keymap ("Ctrl", NULL)))
       || (NULL == (The_Shift_KeyMap = SLang_create_keymap ("Shift", NULL))))
     return -1;

   k = Normal_Map;
   while ((str = k->keystr), (*str != 0))
     {
	SLang_define_key1 (str, (VOID *) k->scan_code, SLKEY_F_INTRINSIC, The_Normal_KeyMap);
	k++;
     }
   
   k = Alt_Map;
   while ((str = k->keystr), (*str != 0))
     {
	SLang_define_key1 (str, (VOID *) k->scan_code, SLKEY_F_INTRINSIC, The_Alt_KeyMap);
	k++;
     }
   k = Ctrl_Map;
   while ((str = k->keystr), (*str != 0))
     {
	SLang_define_key1 (str, (VOID *) k->scan_code, SLKEY_F_INTRINSIC, The_Ctrl_KeyMap);
	k++;
     }
   k = Shift_Map;
   while ((str = k->keystr), (*str != 0))
     {
	SLang_define_key1 (str, (VOID *) k->scan_code, SLKEY_F_INTRINSIC, The_Shift_KeyMap);
	k++;
     }

   /* Now setup the shift modifier keys in each of the keymaps.  */
   SLang_define_key1 (Alt_Shiftkey, (VOID *) ALT_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Normal_KeyMap);
   SLang_define_key1 (Alt_Shiftkey, (VOID *) ALT_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Alt_KeyMap);
   SLang_define_key1 (Alt_Shiftkey, (VOID *) ALT_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Ctrl_KeyMap);
   SLang_define_key1 (Alt_Shiftkey, (VOID *) ALT_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Shift_KeyMap);
   SLang_define_key1 (Ctrl_Shiftkey, (VOID *) CTRL_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Normal_KeyMap);
   SLang_define_key1 (Ctrl_Shiftkey, (VOID *) CTRL_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Alt_KeyMap);
   SLang_define_key1 (Ctrl_Shiftkey, (VOID *) CTRL_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Ctrl_KeyMap);
   SLang_define_key1 (Ctrl_Shiftkey, (VOID *) CTRL_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Shift_KeyMap);
   SLang_define_key1 (Shift_Shiftkey, (VOID *) SHIFT_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Normal_KeyMap);
   SLang_define_key1 (Shift_Shiftkey, (VOID *) SHIFT_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Alt_KeyMap);
   SLang_define_key1 (Shift_Shiftkey, (VOID *) SHIFT_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Ctrl_KeyMap);
   SLang_define_key1 (Shift_Shiftkey, (VOID *) SHIFT_KEY_SCAN_CODE, SLKEY_F_INTRINSIC, The_Shift_KeyMap);

   if (SLang_Error) return -1;
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
   struct timeval scr_tv, t_start, t_end;
   fd_set fds;
   long t_dif;

#define THE_TIMEOUT 750000L
   FD_ZERO(&fds);
   FD_SET(kbd_fd, &fds);
   scr_tv.tv_sec = 0L;
   scr_tv.tv_usec = THE_TIMEOUT;
   
   gettimeofday(&t_start, NULL);
   errno = 0;
   while ((int) select(kbd_fd + 1, &fds, NULL, NULL, &scr_tv) < (int) 1) 
     {
	gettimeofday(&t_end, NULL);
	t_dif = ((t_end.tv_sec * 1000000 + t_end.tv_usec) -
		 (t_start.tv_sec * 1000000 + t_start.tv_usec));
	
	if ((t_dif >= THE_TIMEOUT) || (errno != EINTR))
	  return 0;
	errno = 0;
	scr_tv.tv_sec = 0L;
	scr_tv.tv_usec = THE_TIMEOUT - t_dif;
     }
   return 1;
}

void do_slang_getkeys (void)
{
   SLang_Key_Type *key;
   static SLKeyMap_List_Type *map;
   unsigned int scan;
   
   if (map == NULL) map = The_Normal_KeyMap;
   
   if (-1 == read_some_keys ()) return;
   
   /* Now process the keys that are buffered up */
   while (kbcount)
     {
	Keystr_Len = 0;
	KeyNot_Ready = 0;
	key = SLang_do_key (map, getkey_callback);
	
	SLang_Error = 0;

	if (KeyNot_Ready)
	  {
	     if ((Keystr_Len == 1) && (*kbp == 27) && (map == The_Normal_KeyMap))
	       {
		  /* We have an esc character.  If nothing else is available 
		   * to be read after a brief period, assume user really
		   * wants esc.
		   */
		  if (sltermio_input_pending ()) return;      
		  DOS_setscan ((unsigned short) 0x011B);
		  key = NULL;
		  /* drop on through to the return for the undefined key below. */
	       }
	     else return; /* try again next time */
	  }
	
	DOSemu_Keyboard_Keymap_Prompt = NULL;
	
	kbcount -= Keystr_Len;	       /* update count */
	kbp += Keystr_Len;
	
	if (key == NULL) 
	  {
	     /* undefined key --- return */
	     map = The_Normal_KeyMap;
	     kbcount = 0;
	     return;
	  }
	
	scan = (unsigned int) key->f;
	
	if (scan < ALT_KEY_SCAN_CODE)
	  {
	     map = The_Normal_KeyMap;
	     DOS_setscan ((unsigned short) scan);
	     continue;
	  }
	
	if (scan == ALT_KEY_SCAN_CODE)
	  {
	     if (map == The_Alt_KeyMap) map = The_Normal_KeyMap;
	     else 
	       {
		  map = The_Alt_KeyMap;
		  DOSemu_Keyboard_Keymap_Prompt = "Alt-";
	       }
	  }
	
	else if (scan == SHIFT_KEY_SCAN_CODE)
	  {
	     if (map == The_Shift_KeyMap) map = The_Normal_KeyMap;
	     else 
	       {
		  map = The_Shift_KeyMap;
		  DOSemu_Keyboard_Keymap_Prompt = "Shift-";
	       }
	  }
	
	else if (scan == CTRL_KEY_SCAN_CODE)
	  {
	     if (map == The_Ctrl_KeyMap) map = The_Normal_KeyMap;
	     else 
	       {
		  map = The_Ctrl_KeyMap;
		  DOSemu_Keyboard_Keymap_Prompt = "Ctrl-";
	       }
	  }
     }
}
