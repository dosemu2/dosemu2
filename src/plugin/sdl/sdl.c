/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#define SDL_C
#include "config.h"

#include <stdio.h>
#include <stdlib.h> /* for malloc & free */
#include <string.h> /* for memset */
#include <unistd.h>
#include <fcntl.h>
#include <SDL/SDL.h>

#include "emu.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#include "../../env/video/remap.h"
#include "vgaemu.h"
#include "vgatext.h"
#include "render.h"
#include "sdl.h"

static int SDL_init(void);
static void SDL_close(void);
static int SDL_set_videomode(int mode_class, int text_width, int text_height);
static void SDL_update_cursor(void);
static int SDL_update_screen(void);
static void SDL_draw_string(int x, int y, unsigned char *text, int len, Bit8u attr);
static void SDL_draw_cursor(int x, int y, Bit8u attr, int , int , Boolean);
static void SDL_update(void);
static void SDL_set_text_palette(DAC_entry);
static void SDL_refresh_private_palette(void);
static void SDL_put_image(int x, int y, unsigned width, unsigned height);
static void SDL_change_mode(int *x_res, int *y_res);

static void SDL_draw_line(int x, int y, int len) {}
static void SDL_resize_text_screen(void);
static void SDL_resize_image(unsigned width, unsigned height);
static void SDL_handle_events(void);
static int SDL_set_text_mode(int tw, int th, int w ,int h);

struct video_system Video_SDL = 
{
  0,
  SDL_init,
  SDL_close,
  SDL_set_videomode,
  SDL_update_screen,
  SDL_update_cursor,
  NULL,
  SDL_handle_events
};

struct text_system Text_SDL =
{
   SDL_update,
   SDL_draw_string, 
   SDL_draw_line, 
   SDL_draw_cursor,
   SDL_set_text_palette,
   SDL_resize_text_screen,
};

struct render_system Render_SDL =
{
   SDL_refresh_private_palette,
   SDL_put_image,
};

static const SDL_VideoInfo *video_info;
static int remap_src_modes = 0;
static SDL_Surface* surface = NULL;
static int SDL_image_mode;

static Boolean is_mapped = FALSE;
static int exposure = 0;

static int font_width = 8, font_shift = 1;

static int w_x_res, w_y_res;

/* For graphics mode */
static vga_emu_update_type veut;
static ColorSpaceDesc SDL_csd;
static SDL_Color vga_colors[256];

static DAC_entry text_colors[16];

void SDL_update(void)
{
  /* do nothing for now -- but see comment in SDL_put_image */
}

int SDL_init(void)
{
  SDL_Event evt;
  char driver[8];
  int have_true_color;

  use_bitmap_font = 1;
  co = 80; li = 25;
  set_video_bios_size();		/* make it stick */
  if (SDL_Init(SDL_INIT_VIDEO| SDL_INIT_NOPARACHUTE) < 0) {
    error("SDL: %s\n", SDL_GetError());
    leavedos(99);
  }
  SDL_EnableUNICODE(1);
  SDL_VideoDriverName(driver, 8);
  v_printf("SDL: Using driver: %s\n", driver);

  /* Collect some data */
  video_info = SDL_GetVideoInfo();
  if (video_info->wm_available) {
    SDL_WM_SetCaption(config.X_title, config.X_icon_name);
    if (config.X_fullscreen)
      SDL_WM_GrabInput(SDL_GRAB_ON);
  } else {
    config.X_fullscreen = 1;
  }
   
  SDL_csd.bits = video_info->vfmt->BitsPerPixel;
  SDL_csd.bytes = (video_info->vfmt->BitsPerPixel + 7) >> 3;
  SDL_csd.r_mask = video_info->vfmt->Rmask;
  SDL_csd.g_mask = video_info->vfmt->Gmask;
  SDL_csd.b_mask = video_info->vfmt->Bmask;
  color_space_complete(&SDL_csd);

  set_remap_debug_msg(stderr);
  have_true_color = (video_info->vfmt->palette == NULL);
  remap_src_modes = remapper_init(&SDL_image_mode, SDL_csd.bits, have_true_color, 0);

  if(have_true_color)
    Render_SDL.refresh_private_palette = NULL;
  register_render_system(&Render_SDL);
  register_text_system(&Text_SDL);

  if(vga_emu_init(remap_src_modes, &SDL_csd)) {
    error("SDL: SDL_init: VGAEmu init failed!\n");
    leavedos(99);
  }

  SDL_set_videomode(TEXT, co, li); 
   
  /* SDL_APPACTIVE event does not occur when an application window is first
   * created.
   * So we push that event into the queue */
 
  evt.type = SDL_ACTIVEEVENT;
  evt.active.type = SDL_ACTIVEEVENT;
  evt.active.gain = 1;
  evt.active.state = SDL_APPACTIVE;
  SDL_PushEvent(&evt);
 
  /* Same for focus event */
  evt.type = SDL_ACTIVEEVENT;
  evt.active.type = SDL_ACTIVEEVENT;
  evt.active.gain = 1;
  evt.active.state = SDL_APPINPUTFOCUS;
  SDL_PushEvent(&evt);

  /* enable repeat here (in the keyboard code it's too early) */
  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

  return 0;
}

void SDL_close(void)
{
  remapper_done();
  vga_emu_done();
  SDL_Quit();   
}

static void SDL_resize_text_screen(void)
{
  font_width = vga.char_width;
  font_height = vga.char_height;
  w_x_res = (vga.width <= 320) ? (2 * vga.width) : vga.width;
  w_y_res = (vga.height <= 240) ? (2 * vga.height) : vga.height;
  redraw_text_screen();
}

/* NOTE : Like X.c, the actual mode is taken via video_mode */
int SDL_set_videomode(int mode_class, int text_width, int text_height)
{
  int mode = video_mode;

  if(mode_class != -1) {
    if(!vga_emu_setmode(mode, text_width, text_height)) {
      v_printf("vga_emu_setmode(%d, %d, %d) failed\n",
	       mode, text_width, text_height);
      return 0;
    }
    font_width = vga.char_width;
    font_height = vga.char_height;
  }

  v_printf("X: X_setmode: %svideo_mode 0x%x (%s), size %d x %d (%d x %d pixel)\n",
    mode_class != -1 ? "" : "re-init ",
    (int) mode, vga.mode_class ? "GRAPH" : "TEXT",
    vga.text_width, vga.text_height, vga.width, vga.height
  );


  if (surface != NULL) {
    SDL_FreeSurface(surface);
  }
   
  if(vga.mode_class == TEXT) {
    SDL_set_text_mode(vga.text_width, vga.text_height, vga.width, vga.height);
    SDL_ShowCursor(SDL_ENABLE);
    if (is_mapped) reset_redraw_text_screen();
  } else {
    get_mode_parameters(&w_x_res, &w_y_res, SDL_image_mode, &veut);
    SDL_change_mode(&w_x_res, &w_y_res);
  }
  return 1;
}

void SDL_set_text_palette(DAC_entry col)
{
  SDL_Color color[1];
  int shift;
  if (vga.dac.bits < 8)  shift = 8 - vga.dac.bits; 
  else shift = 0;

  color[0].r = col.r << shift;
  color[0].g = col.g << shift;
  color[0].b = col.b << shift;   
  text_colors[col.index] = col;
  SDL_SetColors(surface, color, col.index, 1);   
}

void SDL_resize_image(unsigned width, unsigned height)
{
  X_printf("X: resize_ximage %d x %d\n", width, height);
  w_x_res = width;
  w_y_res = height;
  SDL_change_mode(&w_x_res, &w_y_res);
}

int SDL_set_text_mode(int tw, int th, int w ,int h)
{
  /* Try to find the best font */
  /* We only have fonts by 8 x y pixels, so we just have to choose y */
  font_width = vga.char_width;
  font_shift = (w / tw ) - vga.char_width;
  co = tw;
  li = th;
  /* re adjust h */
  h = vga.char_height * th;
  resize_text_mapper(SDL_image_mode);
  SDL_resize_image(w, h);
  SDL_set_mouse_text_cursor();
  /* that's all */ 
  return 0;
}

static void SDL_change_mode(int *x_res, int *y_res)
{
  Uint32 flags = SDL_HWPALETTE | SDL_HWSURFACE;
  if (config.X_fullscreen) {
    SDL_Rect **modes;
    int i;

    modes=SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
    if (modes == (SDL_Rect **) 0) {
      modes=SDL_ListModes(NULL, SDL_FULLSCREEN);			 
    }
    if (modes != (SDL_Rect **) -1) {
      for (i = 0; modes[i] && modes[i]->w >= vga.width; i++);
      if (i > 0) i--;
      while (modes[i]->h < vga.height && i > 0) i--;
      if (modes[i]) {
	int factor;
	factor = modes[i]->w / vga.width;
	if (*y_res * factor > modes[i]->h) {
	  factor = modes[i]->h / vga.height;
	}
	*x_res = vga.width * factor;
	*y_res = vga.height * factor;
      }
    }
    flags |= SDL_FULLSCREEN;
  } else {
    flags |= SDL_RESIZABLE;
  }
  v_printf("SDL: using mode %d %d %d\n", *x_res, *y_res, SDL_csd.bits);
  surface =  SDL_SetVideoMode(*x_res, *y_res, SDL_csd.bits, flags);
  SDL_ShowCursor(SDL_DISABLE);
  remap_obj.dst_resize(&remap_obj, *x_res, *y_res, surface->pitch);
  remap_obj.dst_image = surface->pixels;
  *remap_obj.dst_color_space = SDL_csd;
}

static void SDL_draw_cursor(int x, int y, Bit8u attr, int start, int end, Boolean focus)
{
  RectArea ra;

  SDL_LockSurface(surface);
  ra = draw_bitmap_cursor(x, y, attr, start, end, focus);
  SDL_UnlockSurface(surface);
  if (ra.width)
    SDL_UpdateRect(surface, ra.x, ra.y, ra.width, ra.height);
}

   
void SDL_update_cursor(void)
{
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH) return;
   else if (is_mapped) update_cursor();
}

int SDL_update_screen(void)
{
  return update_screen(&veut, is_mapped);
}

static void SDL_refresh_private_palette(void)
{  
 /* SDL cannot change colors 1, 5, 7... in a single step, cause
  * the SetColors command can only change consecutive colors.
  * 
  * So there were three choices : 
  * i. Do a SetColor for each color that changed
  * ii. Do a SetColor for the whole palette. This need the current palette
  * to be saved in memory
  * iii. Group all changed consecutive colors and do a SetColor for each group
  * The 2nd solution is faster (5/10 times) than the first, and is implemented
  * here. The 3rd solution is perhaps better but i haven't really tried
  * to implement it */
  DAC_entry col[256];
  int cols;
  RGBColor c;
  int i, j, k;
  unsigned bits, shift;   
  cols = 1 << vga.pixel_size;   
  if(cols > 256) cols = 256;
  j = changed_vga_colors(col);
  for(i = k = 0; k < j; k++) {
    c.r = col[k].r; c.g = col[k].g; c.b = col[k].b;
    bits = vga.dac.bits;
    gamma_correct(&remap_obj, &c, &bits);
    if (bits < 8)  shift = 8 - bits; 
    else shift = 0;
    vga_colors[col[k].index].r = c.r << shift;	  
    vga_colors[col[k].index].g = c.g << shift;	  
    vga_colors[col[k].index].b = c.b << shift;	  
  }
  if (j) SDL_SetColors(surface, vga_colors, 0, cols);
}

void SDL_put_image(int x, int y, unsigned width, unsigned height)
{
  /* faster would be to push the rectangle and do the real work
     using SDL_UpdateRects in SDL_Update */
  SDL_UpdateRect(surface, x, y, width, height);
}

void SDL_draw_string(int x, int y, unsigned char *text, int len, Bit8u attr)
{
  RectArea ra;
   v_printf(
    "SDL_draw_string: %d chars at (%d, %d), attr = 0x%02x\n",
    len, x, y, (unsigned) attr
	);
  SDL_LockSurface(surface);
  ra = convert_bitmap_string(x, y, text, len, attr);
  SDL_UnlockSurface(surface);
   v_printf(
    "SDL_draw_string: %d chars at (%d, %d), attr = 0x%02x\n",
    ra.width, ra.x, ra.y, ra.height
	);
  if (ra.width)
    SDL_UpdateRect(surface, ra.x, ra.y, ra.width, ra.height);
}

static void SDL_handle_events(void)
{
   static int busy = 0;
   SDL_Event event;

   if (busy) return;
   busy = 1;
   while (SDL_PollEvent(&event)) {
     switch (event.type) {   
     case SDL_ACTIVEEVENT:
       if (event.active.state  == SDL_APPACTIVE) {
	 if (event.active.gain == 1) {
	   v_printf("Expose Event\n");
	   is_mapped = TRUE;
	   if (vga.mode_class == TEXT) {
	     if (!exposure) {
	       redraw_text_screen();
	       exposure = 1;
	     }
	   } else {
	     /* TODO */
	   }
	 } else {
	   /* TODO */
	 }			  
       } else if (event.active.state == SDL_APPINPUTFOCUS || SDL_APPMOUSEFOCUS) {
	 if (event.active.gain == 1) {
	   v_printf("SDL: focus in\n");
	   if (vga.mode_class == TEXT) text_gain_focus();
	 } else {
	   v_printf("SDL: focus out\n");
	   if (vga.mode_class == TEXT) text_lose_focus();
	 }
       }
       break;
     case SDL_VIDEORESIZE:
       SDL_resize_image(event.resize.w, event.resize.h);
       dirty_all_video_pages();
       if (vga.mode_class == TEXT)
	 vga.reconfig.mem = 1;
       SDL_update_screen();
       break;
     case SDL_KEYUP:
     case SDL_KEYDOWN:
       SDL_process_key(event.key);
       break;
     case SDL_MOUSEBUTTONDOWN:
       {
	 int buttons = SDL_GetMouseState(NULL, NULL);
	 SDL_set_mouse_move(event.button.x, event.button.y, w_x_res, w_y_res);
	 mouse_move_buttons(buttons & SDL_BUTTON(1), buttons & SDL_BUTTON(2), buttons & SDL_BUTTON(3));
	 break;
       }
     case SDL_MOUSEBUTTONUP:
       {
	 int buttons = SDL_GetMouseState(NULL, NULL);
	 SDL_set_mouse_move(event.button.x, event.button.y, w_x_res, w_y_res);
	 mouse_move_buttons(buttons & SDL_BUTTON(1), buttons & SDL_BUTTON(2), buttons & SDL_BUTTON(3));
	 break;
       }

     case SDL_MOUSEMOTION:
       SDL_set_mouse_move(event.button.x, event.button.y, w_x_res, w_y_res);
       break;
     default:
       v_printf("PAS ENCORE TRAITE\n");
       /* TODO */
       break;
     }	
   }
   busy = 0;
   do_mouse_irq();
}
