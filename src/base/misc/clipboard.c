#include <string.h>
#include "dosemu_debug.h"
#include "translate/translate.h"
#include "clipboard.h"

struct clipboard_system *Clipboard;

char *clipboard_make_str_utf8(int type, const char *p, int size)
{
  char *q;

  if (type == CF_TEXT) {
    q = strndup(p, size);
  } else { // CF_OEMTEXT
    struct char_set_state state;
    int characters;
    t_unicode *str;

    init_charset_state(&state, trconfig.dos_charset);

    characters = character_count(&state, p, size);
    if (characters == -1) {
      v_printf("SDL_clipboard: Write invalid char count\n");
      return NULL;
    }

    str = malloc(sizeof(t_unicode) * (characters + 1));
    charset_to_unicode_string(&state, str, &p, size, characters + 1);
    cleanup_charset_state(&state);
    q = unicode_string_to_charset((wchar_t *)str, "utf8");
    free(str);
  }
  return q;
}

char *clipboard_make_str_dos(int type, const char *p, int size)
{
  char *q;

  if (type == CF_TEXT) {
    q = strndup(p, size);
  } else { // CF_OEMTEXT
    struct char_set_state state;
    struct char_set *utf8;
    int characters;
    t_unicode *str;
    int size = 256;

    utf8 = lookup_charset("utf8");
    init_charset_state(&state, utf8);

    characters = character_count(&state, p, size);
    if (characters == -1) {
      v_printf("SDL_clipboard: _clipboard_grab_data()  invalid char count\n");
      return NULL;
    }

    str = malloc(sizeof(t_unicode) * (characters + 1));
    charset_to_unicode_string(&state, str, &p, size, characters + 1);
    cleanup_charset_state(&state);
    q = unicode_string_to_charset((wchar_t *)str, trconfig.dos_charset->names[0]);
    free(str);
  }
  return q;
}

int register_clipboard_system(struct clipboard_system *cs)
{
  Clipboard = cs;
  return 1;
}
