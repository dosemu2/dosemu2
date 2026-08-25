#ifndef MT32REMAP_H
#define MT32REMAP_H

typedef struct _mt32 mt32_t;

mt32_t *mt32remap_init(void);
void mt32remap_done(mt32_t *mt);
int mt32remap_channel_assigned(const mt32_t *m, int ch);
int mt32remap_noteon(mt32_t *mt, int ch, int key, int vel,
        void (*write_cb)(unsigned char *data, int len));
void mt32remap_program(mt32_t *mt, int ch, int prog);
void mt32remap_sysex(mt32_t *mt, const unsigned char *p, int len);

#endif
