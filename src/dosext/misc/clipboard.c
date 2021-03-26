#include "clipboard.h"


struct clipboard_system *Clipboard;


int register_clipboard_system(struct clipboard_system *cs)
{
  Clipboard = cs;
  return 1;
}

