#ifndef DOSEMU_CHARSET_H
#define DOSEMU_CHARSET_H

struct char_set;
struct char_set *get_terminal_charset(struct char_set *set);
int is_terminal_charset(struct char_set *set);
int is_display_charset(struct char_set *set);

#endif /* DOSEMU_CHARSET_H */
