/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* definitions for updating text modes */

#include "translate.h"
#include "render.h"
#define CONFIG_SELECTION 1

/********************************************/

#define ATTR_FG(attrib) (attrib & 0x0F & vga.attr.data[0x12])
#define ATTR_BG(attrib) (attrib >> 4)

extern Boolean have_focus;

struct text_system
{
   /* function to draw a string in text mode using attribute attr */
   void (*Draw_string)(void *opaque, int x, int y , unsigned char *s, int len, Bit8u attr);
   void (*Draw_line)(void *opaque, int x, int y , int len);
   void (*Draw_cursor)(void *opaque, int x, int y, Bit8u attr, int first, int last, Boolean focus);
   void (*SetPalette) (void *opaque, DAC_entry *color, int index);
   int  (*lock)(void *opaque);
   void (*unlock)(void *opaque);
   void *opaque;
};

struct RemapObjectStruct;
struct RectArea;

int register_text_system(struct text_system *text_system);
struct bitmap_desc draw_bitmap_cursor(int x, int y, Bit8u attr, int start,
    int end, Boolean focus);
struct bitmap_desc draw_bitmap_line(int x, int y, int len);
void blink_cursor(void);
void reset_redraw_text_screen(void);
void dirty_text_screen(void);
int text_is_dirty(void);
void init_text_mapper(int image_mode, int features, ColorSpaceDesc *csd);
void done_text_mapper(void);
struct bitmap_desc convert_bitmap_string(int x, int y, unsigned char *text,
      int len, Bit8u attr);
void update_text_screen(void);
void text_gain_focus(void);
void text_lose_focus(void);
struct bitmap_desc get_text_canvas(void);
int text_lock(void);
void text_unlock(void);

#ifdef CONFIG_SELECTION
/* for selections */
t_unicode* end_selection(void);
void clear_if_in_selection(void);
void start_selection(int col, int row);
void start_extend_selection(int col, int row);
void clear_selection_data(void);
void extend_selection(int col, int row);

int x_to_col(int x, int w_x_res);
int y_to_row(int y, int w_y_res);

#endif
