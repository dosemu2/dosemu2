/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* definitions for rendering graphics modes -- the middle layer
   between the graphics frontends and the remapper */

struct render_system
{
  /* set the private palette */
  void (*refresh_private_palette)(void);
  void (*put_image)(int x, int y, unsigned width, unsigned height);
};

extern struct RemapObjectStruct remap_obj;
extern int remap_features;

int register_render_system(struct render_system *render_system);

int remapper_init(unsigned *image_mode, unsigned bits_per_pixel,
		  int have_true_color, int have_shmap);
void remapper_done(void);

int update_screen(vga_emu_update_type *veut, int is_mapped);
