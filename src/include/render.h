/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* definitions for rendering graphics modes -- the middle layer
   between the graphics frontends and the remapper */

struct render_system
{
  void (*refresh_rect)(int x, int y, unsigned width, unsigned height);
  struct bitmap_desc (*lock)(void);
  void (*unlock)(void);
};

int register_render_system(struct render_system *render_system);
enum { REMAP_DOSEMU, REMAP_PIXMAN };
int register_remapper(struct remap_calls *calls, int prio);
int remapper_init(int have_true_color, int have_shmap, int features,
	ColorSpaceDesc *csd);
void remapper_done(void);
void get_mode_parameters(int *x_res_p, int *y_res_p, int *wx_res, int *wy_res);
int update_screen(void);
void color_space_complete(ColorSpaceDesc *color_space);
void render_blit(int x, int y, int width, int height);
int render_is_updating(void);
void redraw_text_screen(void);
void render_gain_focus(void);
void render_lose_focus(void);
