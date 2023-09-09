#include <stdint.h>

/* Text format. Each line ends with a carriage return/linefeed (CR-LF)
 * combination. A null character signals the end of the data. Use this
 * format for ANSI text. */
#define CF_TEXT 1

/* Text format containing characters in the OEM character set. Each line
 * ends with a carriage return/linefeed (CR-LF) combination. A null
 * character signals the end of the data. */
#define CF_OEMTEXT 7

/* Unicode text format. Each line ends with a carriage return/linefeed
 * (CR-LF) combination. A null character signals the end of the data. */
#define CF_UNICODETEXT 13

struct clipboard_system
{
   int (*clear)(void);
   int (*write)(int type, const char *p, int size);
   int (*getsize)(int type);
   int (*getdata)(int type, char *p, int size);
   const char *name;
};

extern struct clipboard_system *Clipboard;

int register_clipboard_system(struct clipboard_system *cs);
