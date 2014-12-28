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
};

int register_render_system(struct render_system *render_system);
enum { REMAP_DOSEMU, REMAP_PIXMAN };
int register_remapper(struct remap_calls *calls, int prio);
int remapper_init(unsigned *image_mode,
		  int have_true_color, int have_shmap, ColorSpaceDesc *csd);
void remapper_done(void);
void get_mode_parameters(int *wx_res, int *wy_res);
int update_screen(void);
void render_init(uint8_t *img, int width, int height, int scan_len);
void render_resize(uint8_t *img, int width, int height, int scan_len);
void color_space_complete(ColorSpaceDesc *color_space);
void render_blit(int x, int y, int width, int height);
