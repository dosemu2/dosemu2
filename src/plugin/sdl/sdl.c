/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#define SDL_C
#include "config.h"

#include <stdio.h>
#include <stdlib.h> /* for malloc & free */
#include <string.h> /* for memset */
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include "emu.h"
#include "timers.h"
#include "init.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#include "../../env/video/remap.h"
#if SDL_VERSION_ATLEAST(1,2,10) && !defined(SDL_VIDEO_DRIVER_X11)
#undef X_SUPPORT
#endif
#ifdef X_SUPPORT
#include "../X/screen.h"
#include "../X/X.h"
#endif
#include "vgaemu.h"
#include "vgatext.h"
#include "render.h"
#include "sdl.h"
#include "keyb_clients.h"
#include "dos2linux.h"
#include "utilities.h"
#include "speaker.h"

static int SDL_init(void);
static void SDL_close(void);
static int SDL_set_videomode(int mode_class, int text_width, int text_height);
static void SDL_update_cursor(void);
static int SDL_update_screen(void);
static void SDL_refresh_private_palette(DAC_entry *col, int num);
static void SDL_put_image(int x, int y, unsigned width, unsigned height);
static void SDL_change_mode(int *x_res, int *y_res);

static void SDL_resize_image(unsigned width, unsigned height);
static void SDL_handle_events(void);
static int SDL_set_text_mode(int tw, int th, int w ,int h);

/* interface to xmode.exe */
static int SDL_change_config(unsigned, void *);
static void toggle_grab(void);

struct video_system Video_SDL = 
{
  0,
  SDL_init,
  SDL_close,
  SDL_set_videomode,
  SDL_update_screen,
  SDL_update_cursor,
  SDL_change_config,
  SDL_handle_events
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

static int font_width, font_height;

static int w_x_res, w_y_res;			/* actual window size */
static int saved_w_x_res, saved_w_y_res;	/* saved normal window size */

/* For graphics mode */
static vga_emu_update_type veut;
static ColorSpaceDesc SDL_csd;
static SDL_Color vga_colors[256];

static struct {
  int num, max;
  SDL_Rect *rects;
} sdl_rects;

static int force_grab = 0;
int grab_active = 0;

#ifdef X_SUPPORT
#ifdef USE_DL_PLUGINS
#define X_load_text_font pX_load_text_font
static void (*X_load_text_font)(Display *display, int private_dpy,
				Window window, const char *p,  int *w, int *h);
#define X_handle_text_expose pX_handle_text_expose
static int (*X_handle_text_expose)(void);
#endif

#ifdef CONFIG_SELECTION
#ifdef USE_DL_PLUGINS
#define X_handle_selection pX_handle_selection
static void (*X_handle_selection)(Display *display, Window mainwindow,
				  XEvent *e);
#endif
#define CONFIG_SDL_SELECTION 1
#endif /* CONFIG_SELECTION */

static struct {
  Display *display;
  Window window;
  void (*lock_func)(void);
  void (*unlock_func)(void);
} x11;

static void init_x11_support(void)
{
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (SDL_GetWMInfo(&info) && info.subsystem == SDL_SYSWM_X11) {
#ifdef USE_DL_PLUGINS
    void (*X_speaker_on)(void *, unsigned, unsigned short);
    void (*X_speaker_off)(void *);
    void *handle = load_plugin("X");
    X_speaker_on = dlsym(handle, "X_speaker_on");
    X_speaker_off = dlsym(handle, "X_speaker_off");
    X_load_text_font = dlsym(handle, "X_load_text_font");
    X_handle_text_expose = dlsym(handle, "X_handle_text_expose");
#ifdef CONFIG_SDL_SELECTION
    X_handle_selection = dlsym(handle, "X_handle_selection");
#endif
#endif
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    x11.display = info.info.x11.display;
    x11.lock_func = info.info.x11.lock_func;
    x11.unlock_func = info.info.x11.unlock_func;
    register_speaker(x11.display, X_speaker_on, X_speaker_off);
  }
}

static void init_x11_window_font(void)
{
  /* called as soon as the window is active (first set video mode) */
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (SDL_GetWMInfo(&info) && info.subsystem == SDL_SYSWM_X11) {
    x11.window = info.info.x11.window;
    SDL_change_config(CHG_FONT, config.X_font);
  }
}
#endif /* X_SUPPORT */

int SDL_init(void)
{
  SDL_Event evt;
  char driver[8];
  int have_true_color;

  use_bitmap_font = 1;
  if (SDL_Init(SDL_INIT_VIDEO| SDL_INIT_NOPARACHUTE) < 0) {
    error("SDL: %s\n", SDL_GetError());
    leavedos(99);
  }
  SDL_EnableUNICODE(1);
  SDL_VideoDriverName(driver, 8);
  v_printf("SDL: Using driver: %s\n", driver);

  /* Collect some data */
  video_info = SDL_GetVideoInfo();
  if (video_info->wm_available)
    SDL_WM_SetCaption(config.X_title, config.X_icon_name);
  else
    config.X_fullscreen = 1;
  if (config.X_fullscreen)
    toggle_grab();
   
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

  if(vga_emu_init(remap_src_modes, &SDL_csd)) {
    error("SDL: SDL_init: VGAEmu init failed!\n");
    leavedos(99);
  }

#ifdef X_SUPPORT
  init_x11_support();
#endif

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
#ifdef X_SUPPORT
  if (x11.display && x11.window != None)
    X_load_text_font(x11.display, 1, x11.window, NULL, NULL, NULL);
#endif
  SDL_Quit();   
}

static void SDL_update(void)
{
  if (sdl_rects.num == 0) return;
  SDL_UpdateRects(surface, sdl_rects.num, sdl_rects.rects);
  sdl_rects.num = 0;
}

/*
 * Sync prev_screen & screen_adr.
 */
static void SDL_reset_redraw_text_screen(void)
{
  if(!is_mapped) return;
  reset_redraw_text_screen();
}

static void SDL_redraw_text_screen(void)
{
  if (!is_mapped) return;
#ifdef X_SUPPORT
  if (x11.display && !use_bitmap_font) {
    redraw_text_screen();
    return;
  }
#endif
  SDL_LockSurface(surface);
  redraw_text_screen();
  SDL_UnlockSurface(surface);
  SDL_update();
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
  }

  v_printf("X: X_setmode: %svideo_mode 0x%x (%s), size %d x %d (%d x %d pixel)\n",
    mode_class != -1 ? "" : "re-init ",
    (int) mode, vga.mode_class ? "GRAPH" : "TEXT",
    vga.text_width, vga.text_height, vga.width, vga.height
  );


  if(vga.mode_class == TEXT) {
    if (use_bitmap_font) {
      SDL_set_text_mode(vga.text_width, vga.text_height, vga.width, vga.height);
    } else {
      SDL_set_text_mode(vga.text_width, vga.text_height,
			vga.text_width * font_width,
			vga.text_height * font_height);
    }
    if (!grab_active) SDL_ShowCursor(SDL_ENABLE);
    if (is_mapped) SDL_reset_redraw_text_screen();
  } else {
    get_mode_parameters(&w_x_res, &w_y_res, SDL_image_mode, &veut);
    SDL_change_mode(&w_x_res, &w_y_res);
  }
  return 1;
}

void SDL_resize_image(unsigned width, unsigned height)
{
  v_printf("SDL: resize_image %d x %d\n", width, height);
  w_x_res = width;
  w_y_res = height;
  SDL_change_mode(&w_x_res, &w_y_res);
}

static void SDL_redraw_resize_image(unsigned width, unsigned height)
{
  SDL_resize_image(width, height);
  /* forget about those rectangles */
  sdl_rects.num = 0;
  dirty_all_video_pages();
  if (vga.mode_class == TEXT)
    vga.reconfig.mem = 1;
  SDL_update_screen();
}

int SDL_set_text_mode(int tw, int th, int w ,int h)
{
  if (use_bitmap_font)
    resize_text_mapper(SDL_image_mode);
  SDL_resize_image(w, h);
  SDL_set_mouse_text_cursor();
  /* that's all */ 
  return 0;
}

static void SDL_change_mode(int *x_res, int *y_res)
{
  Uint32 flags = SDL_HWPALETTE | SDL_HWSURFACE;
  saved_w_x_res = *x_res;
  saved_w_y_res = *y_res;
  if (!use_bitmap_font && vga.mode_class == TEXT) {
    if (config.X_fullscreen)
      flags |= SDL_FULLSCREEN;
  } else if (config.X_fullscreen) {
    SDL_Rect **modes;
    int i;

    modes=SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
    if (modes == (SDL_Rect **) 0) {
      modes=SDL_ListModes(NULL, SDL_FULLSCREEN);			 
    }
    if (modes != (SDL_Rect **) -1) {
      unsigned mw = 0;
      do {
	unsigned mh = 0;
	int factor;
	mw++;
	for (i = 0; modes[i] && modes[i]->w >= mw*vga.width; i++);
	if (i > 0) i--;
	do {
	  mh++;
	  while (modes[i]->h < mh*vga.height && i > 0) i--;
	  if (modes[i]) {
	    factor = modes[i]->h / vga.height;
	    *y_res = vga.height * factor;
	  }
	} while (modes[i]->h - *y_res > *y_res/2);
	/* while the border is too high */
	factor = modes[i]->w / vga.width;
	*x_res = vga.width * factor;
      } while (modes[i]->w - *x_res > *x_res/2);
      /* while the border is too wide */
      v_printf("SDL: using fullscreen mode: x=%d, y=%d\n",
	       modes[i]->w, modes[i]->h);
    }
    flags |= SDL_FULLSCREEN;
  } else {
    flags |= SDL_RESIZABLE;
  }
  v_printf("SDL: using mode %d %d %d\n", *x_res, *y_res, SDL_csd.bits);
#ifdef X_SUPPORT
  if (!x11.display) /* SDL may crash otherwise.. */
#endif
    SDL_ShowCursor(SDL_ENABLE);
  surface =  SDL_SetVideoMode(*x_res, *y_res, SDL_csd.bits, flags);
  SDL_ShowCursor(SDL_DISABLE);
  if (use_bitmap_font || vga.mode_class == GRAPH) {
    remap_obj.dst_resize(&remap_obj, *x_res, *y_res, surface->pitch);
    remap_obj.dst_image = surface->pixels;
    *remap_obj.dst_color_space = SDL_csd;
  }
#ifdef X_SUPPORT
  {
    static int first = 1;
    if (first == 1) {
      first = 0;
      init_x11_window_font();
    }
  }
#endif
}

void SDL_update_cursor(void)
{
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH) return;
   else if (is_mapped) {
#ifdef X_SUPPORT
     if (!use_bitmap_font) {
       update_cursor();
       return;
     }
#endif
     SDL_LockSurface(surface);
     update_cursor();
     SDL_UnlockSurface(surface);
     SDL_update();
   }
}

int SDL_update_screen(void)
{
  if(vga.reconfig.re_init) {
    vga.reconfig.re_init = 0;
    sdl_rects.num = 0;
    dirty_all_video_pages();
    dirty_all_vga_colors();
    SDL_set_videomode(-1, 0, 0);
  }
  if (is_mapped) {
    int ret;
#ifdef X_SUPPORT
    if (!use_bitmap_font && vga.mode_class == TEXT)
      return update_screen(&veut);
#endif
    SDL_LockSurface(surface);
    ret = update_screen(&veut);
    SDL_UnlockSurface(surface);
    SDL_update();
    return ret;
  }
  return 0;
}

static void SDL_refresh_private_palette(DAC_entry *col, int num)
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
  int cols;
  RGBColor c;
  int k;
  unsigned bits, shift;   
  cols = 1 << vga.pixel_size;   
  if(cols > 256) cols = 256;
  for(k = 0; k < num; k++) {
    c.r = col[k].r; c.g = col[k].g; c.b = col[k].b;
    bits = vga.dac.bits;
    gamma_correct(&remap_obj, &c, &bits);
    if (bits < 8)  shift = 8 - bits; 
    else shift = 0;
    vga_colors[col[k].index].r = c.r << shift;	  
    vga_colors[col[k].index].g = c.g << shift;	  
    vga_colors[col[k].index].b = c.b << shift;	  
  }
  SDL_SetColors(surface, vga_colors, 0, cols);
}

/* this only pushes the rectangle on a stack; updating is done later */
void SDL_put_image(int x, int y, unsigned width, unsigned height)
{
  SDL_Rect *rect;

  if (sdl_rects.num >= sdl_rects.max) {
    sdl_rects.rects = realloc(sdl_rects.rects, (sdl_rects.max + 10) *
			      sizeof(*sdl_rects.rects));
    sdl_rects.max += 10;
  }
  rect = &sdl_rects.rects[sdl_rects.num];
  rect->x = x;
  rect->y = y;
  rect->w = width;
  rect->h = height;
  sdl_rects.num++;
}

static void toggle_grab(void)
{
  if(grab_active ^= 1) {
    v_printf("SDL: grab activated\n");
    if (!config.X_fullscreen)
      SDL_WM_GrabInput(SDL_GRAB_ON);
    config.mouse.use_absolute = 0;
    v_printf("SDL: mouse grab activated\n");
    SDL_ShowCursor(SDL_DISABLE);
    mouse_enable_native_cursor(1);
  }
  else {
    config.mouse.use_absolute = 1;
    v_printf("SDL: grab released\n");
    if (!config.X_fullscreen)
      SDL_WM_GrabInput(SDL_GRAB_OFF);
    if(vga.mode_class == TEXT)
      SDL_ShowCursor(SDL_ENABLE);
    mouse_enable_native_cursor(0);
  }
  SDL_change_config(CHG_TITLE, NULL);
}

static void toggle_fullscreen_mode(void)
{
  config.X_fullscreen = !config.X_fullscreen;
  if (config.X_fullscreen) {
    v_printf("SDL: entering fullscreen mode\n");
    if (!grab_active) {
      toggle_grab();
      force_grab = 1;
    }
    SDL_redraw_resize_image(w_x_res, w_y_res);
  } else {
    v_printf("SDL: entering windowed mode!\n");
    SDL_redraw_resize_image(saved_w_x_res, saved_w_y_res);
    if (force_grab && grab_active) {
      toggle_grab();
    }
    force_grab = 0;
  }
}

/*
 * This function provides an interface to reconfigure parts
 * of SDL and the VGA emulation during a DOSEMU session.
 * It is used by the xmode.exe program that comes with DOSEMU.
 */
static int SDL_change_config(unsigned item, void *buf)
{
  int err = 0;

  v_printf("SDL: SDL_change_config: item = %d, buffer = %p\n", item, buf);

  switch(item) {

    case CHG_TITLE:
      /* low-level write */
      if (buf) {
	v_printf("SDL: SDL_change_config: win_name = %s\n", (char *) buf);
	SDL_WM_SetCaption(buf, config.X_icon_name);
	break;
      }
      /* high-level write (shows name of emulator + running app) */
      /* fallthrough */

    case CHG_TITLE_EMUNAME:
    case CHG_TITLE_APPNAME:
    case CHG_TITLE_SHOW_APPNAME:
    case CHG_WINSIZE:
    case CHG_BACKGROUND_PAUSE:
    case GET_TITLE_APPNAME:
      change_config (item, buf, grab_active, grab_active);
      break;

#ifdef X_SUPPORT
    case CHG_FONT: {
      if (!x11.display || x11.window == None)
	break;
      x11.lock_func();
      X_load_text_font(x11.display, 1, x11.window, buf,
		       &font_width, &font_height);
      x11.unlock_func();
      if (use_bitmap_font) {
        register_render_system(&Render_SDL);
        if(vga.mode_class == TEXT)
	  SDL_set_text_mode(vga.text_width, vga.text_height,
			    vga.width, vga.height);
      } else {
        if (w_x_res != vga.text_width * font_width ||
            w_y_res != vga.text_height * font_height) {
	  if(vga.mode_class == TEXT)
	    SDL_set_text_mode(vga.text_width, vga.text_height,
			      vga.text_width * font_width,
			      vga.text_height * font_height);
        }
      }
      if (!grab_active) SDL_ShowCursor(SDL_ENABLE);
      break;
    }
#endif

    case CHG_FULLSCREEN:
      v_printf("SDL: SDL_change_config: fullscreen %i\n", *((int *) buf));
      if (*((int *) buf) == !config.X_fullscreen)
	toggle_fullscreen_mode();
      break;

    default:
      err = 100;
  }

  return err;
}

#if CONFIG_SDL_SELECTION

static void SDL_handle_selection(XEvent *e)
{
  switch(e->type) {
  case SelectionClear:
  case SelectionNotify: 
  case SelectionRequest:
  case ButtonRelease:
    if (x11.display && x11.window != None) {
      x11.lock_func();
      X_handle_selection(x11.display, x11.window, e);
      x11.unlock_func();
    }
    break;
  default:
    break;
  }
}

#endif /* CONFIG_SDL_SELECTION */

static void SDL_handle_events(void)
{
   static int busy = 0;
   SDL_Event event;

   if (busy) return;
   busy = 1;
   while (SDL_PollEvent(&event)) {
     switch (event.type) {   
     case SDL_ACTIVEEVENT: {
       if (event.active.state  == SDL_APPACTIVE) {
	 if (event.active.gain == 1) {
	   v_printf("SDL: Expose Event\n");
	   is_mapped = TRUE;
	   if (vga.mode_class == TEXT) {
	     if (!exposure) {
	       SDL_redraw_text_screen();
	       exposure = 1;
	     }
	   } else {
	     /* TODO */
	   }
	 } else {
	   /* TODO */
	 }			  
       } else if (event.active.state == SDL_APPINPUTFOCUS) {
	 if (event.active.gain == 1) {
	   v_printf("SDL: focus in\n");
	   if (vga.mode_class == TEXT) text_gain_focus();
	   if (config.X_background_pause && !dosemu_user_froze) unfreeze_dosemu ();
	 } else {
	   v_printf("SDL: focus out\n");
	   if (vga.mode_class == TEXT) text_lose_focus();
	   if (config.X_background_pause && !dosemu_user_froze) freeze_dosemu ();
	 }
       } else if (event.active.state == SDL_APPMOUSEFOCUS) {
	 if (event.active.gain == 1) {
	   v_printf("SDL: mouse focus in\n");
	 } else {
	   v_printf("SDL: mouse focus out\n");
	 }
       } else {
	 v_printf("SDL: other activeevent\n");
       }
       break;
     }
     case SDL_VIDEORESIZE:
       v_printf("SDL: videoresize event\n");
       SDL_redraw_resize_image(event.resize.w, event.resize.h);
       break;
     case SDL_VIDEOEXPOSE:
       v_printf("SDL: videoexpose event\n");
       break;
     case SDL_KEYDOWN:
       {
	 SDL_keysym keysym = event.key.keysym;
	 if ((keysym.mod & KMOD_CTRL) && (keysym.mod & KMOD_ALT)) {
	   if (keysym.sym == SDLK_HOME || keysym.sym == SDLK_k) {
	     force_grab = 0;
	     toggle_grab();
	     break;
	   } else if (keysym.sym == SDLK_f) {
	     toggle_fullscreen_mode();
	     break;
	   }
	 }
       }
#if CONFIG_SDL_SELECTION
       clear_if_in_selection();
#endif
       SDL_process_key(event.key);
       break;
     case SDL_KEYUP:
#if CONFIG_SDL_SELECTION
       clear_if_in_selection();
#endif
       SDL_process_key(event.key);
       break;
     case SDL_MOUSEBUTTONDOWN:
       {
	 int buttons = SDL_GetMouseState(NULL, NULL);
#if CONFIG_SDL_SELECTION
	 if (x11.display && vga.mode_class == TEXT && !grab_active) {
	   if (event.button.button == SDL_BUTTON_LEFT)
	     start_selection(x_to_col(event.button.x, w_x_res),
			     y_to_row(event.button.y, w_y_res));
	   else if (event.button.button == SDL_BUTTON_RIGHT)
	     start_extend_selection(x_to_col(event.button.x, w_x_res),
				    y_to_row(event.button.y, w_y_res));
	 }
#endif /* CONFIG_SDL_SELECTION */
	 SDL_set_mouse_move(event.button.x, event.button.y, w_x_res, w_y_res);
	 mouse_move_buttons(buttons & SDL_BUTTON(1), buttons & SDL_BUTTON(2), buttons & SDL_BUTTON(3));
	 break;
       }
     case SDL_MOUSEBUTTONUP:
       {
	 int buttons = SDL_GetMouseState(NULL, NULL);
	 SDL_set_mouse_move(event.button.x, event.button.y, w_x_res, w_y_res);
#if CONFIG_SDL_SELECTION
	 if (x11.display && vga.mode_class == TEXT) {
	   XEvent e;
	   e.type = ButtonRelease;
	   e.xbutton.button = 0;
	   if (event.button.button == SDL_BUTTON_LEFT)
	     e.xbutton.button = Button1;
	   else if (event.button.button == SDL_BUTTON_MIDDLE)
	     e.xbutton.button = Button2;
	   else if (event.button.button == SDL_BUTTON_RIGHT)
	     e.xbutton.button = Button3;
	   e.xbutton.time = CurrentTime;
	   SDL_handle_selection(&e);
	 }
#endif /* CONFIG_SDL_SELECTION */
	 mouse_move_buttons(buttons & SDL_BUTTON(1), buttons & SDL_BUTTON(2), buttons & SDL_BUTTON(3));
	 break;
       }

     case SDL_MOUSEMOTION:
#if CONFIG_SDL_SELECTION
       if (x11.display && x11.window != None)
	 extend_selection(x_to_col(event.button.x, w_x_res),
			  y_to_row(event.button.y, w_y_res));
#endif /* CONFIG_SDL_SELECTION */
       SDL_set_mouse_move(event.button.x, event.button.y, w_x_res, w_y_res);
       break;
     case SDL_QUIT:
       leavedos(0);
       break;
#if CONFIG_SDL_SELECTION
     case SDL_SYSWMEVENT:
       if (x11.display)
	 SDL_handle_selection(&event.syswm.msg->event.xevent);
       break;
#endif /* CONFIG_SDL_SELECTION */
     default:
       v_printf("PAS ENCORE TRAITE\n");
       /* TODO */
       break;
     }	
   }
#ifdef X_SUPPORT
   if (!use_bitmap_font && X_handle_text_expose())
     /* need to check separately because SDL_VIDEOEXPOSE is eaten by SDL */
     SDL_redraw_text_screen();
#endif
   busy = 0;
   do_mouse_irq();
}

CONSTRUCTOR(static void init(void))
{
   if (Video)
     return;
   config.X = 1;
   Video = &Video_SDL;
   register_keyboard_client(&Keyboard_SDL);
   register_mouse_client(&Mouse_SDL);
}
