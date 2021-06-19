/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: SDL video renderer
 *
 * Author: Stas Sergeev
 * Loosely based on SDL1 plugin by Emmanuel Jeandel and Bart Oldeman
 */

#include <stdio.h>
#include <stdlib.h>		/* for malloc & free */
#include <string.h>		/* for memset */
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#include <SDL.h>
#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
#include <SDL_ttf.h>
#include <fontconfig/fontconfig.h>
#endif

#include "emu.h"
#include "timers.h"
#include "init.h"
#include "sig.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#include "vgaemu.h"
#include "vgatext.h"
#include "render.h"
#include "keyb_SDL.h"
#include "keyboard/keyb_clients.h"
#include "dos2linux.h"
#include "utilities.h"
#include "sdl.h"

#define THREADED_REND 1

static int SDL_priv_init(void);
static int SDL_init(void);
static void SDL_close(void);
static int SDL_set_videomode(struct vid_mode_params vmp);
static int SDL_update_screen(void);
static void SDL_put_image(int x, int y, unsigned width, unsigned height);
static void SDL_change_mode(int x_res, int y_res, int w_x_res,
			    int w_y_res);
static void SDL_handle_events(void);
/* interface to xmode.exe */
static int SDL_change_config(unsigned, void *);
static void toggle_grab(int kbd);
static void window_grab(int on, int kbd);
static struct bitmap_desc lock_surface(void);
static void unlock_surface(void);
#if THREADED_REND
static void *render_thread(void *arg);
#endif

#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
static void setup_ttf_winsize(int xtarget, int ytarget);
static int probe_font(int idx);
#endif

#define MIN_X 100
#define MIN_Y 75

static struct video_system Video_SDL = {
  SDL_priv_init,
  SDL_init,
  NULL,
  NULL,
  SDL_close,
  SDL_set_videomode,
  SDL_update_screen,
  SDL_change_config,
  SDL_handle_events,
  "sdl"
};

static struct render_system Render_SDL = {
  .refresh_rect = SDL_put_image,
  .lock = lock_surface,
  .unlock = unlock_surface,
  .name = "sdl",
};

static SDL_Renderer *renderer;
static SDL_Surface *surface;
static SDL_Texture *texture_buf;
static SDL_Window *window;
static ColorSpaceDesc SDL_csd;
static Uint32 pixel_format;
#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
static TTF_Font *sdl_font;
static pthread_mutex_t sdl_font_mtx = PTHREAD_MUTEX_INITIALIZER;
struct font_desc {
  SDL_RWops *rw;
  int width;
  int height;
};
#define MAX_FONTS 5
static struct font_desc sdl_fdesc[MAX_FONTS];
static int num_fdescs;
static int cur_fdesc;
static int sdl_font_size;
static SDL_Color text_colors[16];
static struct text_system Text_SDL;
#endif
static int font_width, font_height;
static int win_width, win_height;
static int real_win_width, real_win_height;
static int m_x_res, m_y_res;
static int use_bitmap_font;
static pthread_mutex_t rects_mtx = PTHREAD_MUTEX_INITIALIZER;
static int sdl_rects_num;
static int tmp_rects_num;
static pthread_mutex_t rend_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tex_mtx = PTHREAD_MUTEX_INITIALIZER;
#if THREADED_REND
static pthread_t rend_thr;
static sem_t rend_sem;
#endif

static int force_grab = 0;
static int grab_active = 0;
static int kbd_grab_active = 0;
static int m_cursor_visible;
static int initialized;
static int pre_initialized = 0;
static int wait_kup;
static int copypaste;
static int current_mode_class;
static SDL_Keycode mgrab_key = SDLK_HOME;

#define CONFIG_SDL_SELECTION 1

static void SDL_done(void)
{
#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
  if (!use_bitmap_font) {
    int i;
    TTF_CloseFont(sdl_font);
    for (i = 0; i < num_fdescs; i++)
      SDL_RWclose(sdl_fdesc[i].rw);
    TTF_Quit();
  }
#endif
  SDL_Quit();
}

void SDL_pre_init(void)
{
  int err;

  if (pre_initialized)
    return;
  pre_initialized = 1;

  err = SDL_Init(0);
  if (err)
    return;

  register_exit_handler(SDL_done);
}

static int SDL_priv_init(void)
{
  /* The privs are needed for opening /dev/input/mice.
   * Unfortunately SDL does not support gpm.
   * Also, as a bonus, /dev/fb0 can be opened with privs. */
  PRIV_SAVE_AREA
  int ret;

  assert(pthread_equal(pthread_self(), dosemu_pthread_self));
  SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
  SDL_pre_init();
  enter_priv_on();
  ret = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  leave_priv_setting();
  if (ret < 0) {
    error("SDL init: %s\n", SDL_GetError());
    return -1;
  }
  c_printf("VID: initializing SDL plugin\n");
  return 0;
}

#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
static int sdl_load_font(const char *name)
{
  char *pth = NULL;
  FcPattern *pat, *match;
  FcResult result;
  char *foundname;
  int idx;

  pat = FcNameParse((const FcChar8*)name);
  if (!pat)
    return 0;
  FcConfigSubstitute(NULL, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);
  match = FcFontMatch(NULL, pat, &result);
  if (!match) {
    FcPatternDestroy(pat);
    return 0;
  }
  FcPatternGetString(match, FC_FAMILY, 0, (FcChar8 **)&foundname);
  FcPatternGetString(match, FC_FILE, 0, (FcChar8 **)&pth);
  FcPatternGetInteger(match, FC_INDEX, 0, &idx);

  // Fontconfig guesses if not an exact match, which might be what we want
  if (strncasecmp(name, foundname, strlen(foundname)) != 0) {
    v_printf("SDL: not accepting substitute font '%s'\n", foundname);
    FcPatternDestroy(match);
    FcPatternDestroy(pat);
    return 0;
  }
  v_printf("SDL: using font '%s(%d)'\n", pth, idx);
  v_printf("SDL: searched for '%s'\n", name);
  v_printf("SDL: and found '%s'\n", foundname);

  assert(num_fdescs < MAX_FONTS);
  sdl_fdesc[num_fdescs].rw = SDL_RWFromFile(pth, "r");
  FcPatternDestroy(match);
  FcPatternDestroy(pat);
  if (!sdl_fdesc[num_fdescs].rw || !probe_font(num_fdescs)) {
    error("SDL_RWFromFile: %s\n", SDL_GetError());
    return 0;
  }
  cur_fdesc = num_fdescs++;

  return 1;
}
#endif

static int SDL_text_init(void)
{
#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
  int err;
  char *p, *p1;

  err = TTF_Init();
  if (err) {
    error("TTF_Init: %s\n", TTF_GetError());
    return 0;
  }

  // Lookup font path with fontconfig
  if (!FcInit()) {
    error("FcInit: returned false\n");
    goto tidy_ttf;
  }

  p = config.sdl_fonts;
  while ((p1 = strsep(&p, ","))) {
    while (*p1 == ' ')
      p1++;
    if (!sdl_load_font(p1)) {
      error("SDL: failed to load font \"%s\"\n", p1);
      goto tidy_ttf;
    }
  }

  register_text_system(&Text_SDL);
  /* set initial font size to match VGA font */
  font_width = 9;
  font_height = 16;
  return 1;

tidy_ttf:
  TTF_Quit();
  return 0;

#else
  v_printf("SDL: TTF support not compiled in\n");
  return 0;
#endif
}

static int SDL_init(void)
{
  Uint32 flags = SDL_WINDOW_HIDDEN;
  Uint32 rflags = SDL_RENDERER_TARGETTEXTURE;
  int bpp, features;
  Uint32 rm, gm, bm, am;
  int rc;

  assert(pthread_equal(pthread_self(), dosemu_pthread_self));

  if (!config.sdl_hwrend)
    rflags |= SDL_RENDERER_SOFTWARE;
#ifdef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR /* only available since SDL 2.0.8 */
  SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif

  rc = 0;
  if (config.sdl_fonts && config.sdl_fonts[0] && !config.vga_fonts)
    rc = SDL_text_init();
  use_bitmap_font = !rc;

  /* hints are set before renderer is created */
  if (config.X_lin_filt || config.X_bilin_filt) {
    v_printf("SDL: enabling scaling filter\n");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  }
  if (!config.sdl_hwrend)
      SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
#if SDL_VERSION_ATLEAST(2,0,10)
  SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
#endif
  flags |= SDL_WINDOW_RESIZABLE;
#if 0
  /* some SDL bug prevents resizing if the window was created with this
   * flag. And leaving full-screen mode doesn't help.
   * It remains unresizable. */
  if (config.X_fullscreen)
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
#if 0
  /* it is better to create window and renderer at once. They have
   * internal cyclic dependencies, so if you create renderer after
   * creating window, SDL will destroy and re-create the window. */
  int err = SDL_CreateWindowAndRenderer(0, 0, flags, &window, &renderer);
  if (err || !window || !renderer) {
    error("SDL window failed: %s\n", SDL_GetError());
    goto err;
  }
  SDL_SetWindowTitle(window, config.X_title);
#else
  window = SDL_CreateWindow(config.X_title, SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED, 0, 0, flags);
  if (!window) {
    error("SDL window failed: %s\n", SDL_GetError());
    goto err;
  }
  renderer = SDL_CreateRenderer(window, -1, rflags);
  if (!renderer) {
    error("SDL renderer failed: %s\n", SDL_GetError());
    goto err;
  }
#endif

  SDL_SetWindowMinimumSize(window, MIN_X, MIN_Y);

  if (config.X_fullscreen) {
    window_grab(1, 1);
    force_grab = 1;
  }

  pixel_format = SDL_GetWindowPixelFormat(window);
  if (pixel_format == SDL_PIXELFORMAT_UNKNOWN) {
    error("SDL: unable to get pixel format\n");
    pixel_format = SDL_PIXELFORMAT_RGB888;
  }
  SDL_PixelFormatEnumToMasks(pixel_format, &bpp, &rm, &gm, &bm, &am);
  SDL_csd.bits = bpp;
  SDL_csd.r_mask = rm;
  SDL_csd.g_mask = gm;
  SDL_csd.b_mask = bm;
  color_space_complete(&SDL_csd);
  features = 0;
  if (use_bitmap_font)
    features |= RFF_BITMAP_FONT;
  register_render_system(&Render_SDL);
  if (remapper_init(1, 1, features, &SDL_csd)) {
    error("SDL: SDL_init: VGAEmu init failed!\n");
    config.exitearly = 1;
    return -1;
  }

  if (config.X_mgrab_key && config.X_mgrab_key[0])
    mgrab_key = SDL_GetKeyFromName(config.X_mgrab_key);

#if THREADED_REND
  sem_init(&rend_sem, 0, 0);
  pthread_create(&rend_thr, NULL, render_thread, NULL);
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(__GLIBC__)
  pthread_setname_np(rend_thr, "dosemu: sdl_r");
#endif
#endif

  c_printf("VID: SDL plugin initialization completed\n");

  return 0;

err:
  SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  return -1;
}

void SDL_close(void)
{
#if THREADED_REND
  pthread_cancel(rend_thr);
  pthread_join(rend_thr, NULL);
#endif
  remapper_done();
  vga_emu_done();
  /* destroy texture before renderer, or crash */
  SDL_DestroyTexture(texture_buf);
  SDL_DestroyRenderer(renderer);
  SDL_FreeSurface(surface);
  SDL_DestroyWindow(window);
  SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

static void do_redraw(void)
{
  pthread_mutex_lock(&rend_mtx);
  SDL_RenderPresent(renderer);
  pthread_mutex_unlock(&rend_mtx);
}

static void do_redraw_full(void)
{
  pthread_mutex_lock(&rend_mtx);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);
  pthread_mutex_lock(&tex_mtx);
  SDL_RenderCopy(renderer, texture_buf, NULL, NULL);
  pthread_mutex_unlock(&tex_mtx);
  SDL_RenderPresent(renderer);
  pthread_mutex_unlock(&rend_mtx);
}

static void SDL_update(void)
{
  int i;
  v_printf("SDL_update\n");

  pthread_mutex_lock(&rects_mtx);
  i = sdl_rects_num;
  sdl_rects_num = 0;
  pthread_mutex_unlock(&rects_mtx);
  if (i > 0)
    do_redraw();
}

static void redraw_text(void)
{
  pthread_mutex_lock(&rend_mtx);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);
  pthread_mutex_unlock(&rend_mtx);
  redraw_text_screen();
}

static void SDL_redraw(void)
{
  if (current_mode_class == TEXT) {
    assert(!use_bitmap_font);
    redraw_text();
    return;
  }

  do_redraw_full();
}

static struct bitmap_desc lock_surface(void)
{
  int err;

  assert(surface);
  err = SDL_LockSurface(surface);
  assert(!err);
  return BMP(surface->pixels, win_width, win_height, surface->pitch);
}

#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)

static TTF_Font *do_open_font(int idx, int psize, int *w, int *h)
{
  TTF_Font *f;
  int minx, maxx, miny, maxy;

  SDL_RWseek(sdl_fdesc[idx].rw, 0, RW_SEEK_SET);

  f = TTF_OpenFontRW(sdl_fdesc[idx].rw, 0, psize);
  if (!f) {
    error("TTF_OpenFontRW: %s\n", TTF_GetError());
    return NULL;
  }

  if (!TTF_FontFaceIsFixedWidth(f)) {
    TTF_CloseFont(f);
    error("TTF_FontFaceIsFixedWidth: Font is not fixed width\n");
    return NULL;
  }

  // get metrics
  *h = TTF_FontLineSkip(f);
  TTF_GlyphMetrics(f, 'W', &minx, &maxx, &miny, &maxy, w);
  return f;
}

static int probe_font(int idx)
{
  TTF_Font *f = do_open_font(idx, 80, &sdl_fdesc[idx].width,
      &sdl_fdesc[idx].height);
  if (!f)
    return 0;
  TTF_CloseFont(f);
  return 1;
}

static int open_font(int psize)
{
  assert(sdl_font == NULL);
  sdl_font = do_open_font(cur_fdesc, psize, &font_width, &font_height);
  if (!sdl_font)
    return 0;
  sdl_font_size = psize;
  return 1;
}

static void close_font(void)
{
  TTF_CloseFont(sdl_font);
  sdl_font = NULL;
}

static int find_best_font(int xtarget, int ytarget, int cols, int rows)
{
  int i;
  int idx = -1;
  float delta = 0;
  float asp = xtarget / (float)ytarget;

  for (i = 0; i < num_fdescs; i++) {
    float fasp = sdl_fdesc[i].width * cols /
        (float)(sdl_fdesc[i].height * rows);
    float d0 = fabsf(fasp - asp);
    if (idx == -1 || d0 < delta) {
      idx = i;
      delta = d0;
    }
  }
  return idx;
}

static int _setup_ttf_winsize(int xtarget, int ytarget)
{
  int xnow, ynow;
  int cols, rows;
  int i, idx;
  int ret = 0;

  v_printf("SDL: setup_ttf_winsize called with xtarget %d, ytarget %d\n", xtarget, ytarget);

  cols = vga.text_width;
  rows = vga.text_height;
  idx = find_best_font(xtarget, ytarget, cols, rows);
  if (idx == -1)
    return 0;

  pthread_mutex_lock(&sdl_font_mtx);
  if (idx != cur_fdesc) {
    close_font();
    cur_fdesc = idx;
  }

  if (!sdl_font) {  // In initialisation
    if (!open_font(80)) {
      v_printf("SDL: open_font(80) failed\n");
      goto done;
    }
  }

  xnow = cols * font_width;
  ynow = rows * font_height;

  // increase if necessary
  while (xnow < xtarget && ynow < ytarget) {
    i = sdl_font_size + 1;
    v_printf("SDL: resizing larger, increasing a point size(%d)\n", i);
    v_printf("     xtarget = %d, xnow = %d, ytarget = %d, ynow = %d\n", xtarget, xnow, ytarget, ynow);

    close_font();
    if (!open_font(i)) {
      v_printf("SDL: open_font(%d) failed\n", i);
      goto done;
    }
    xnow = cols * font_width;
    ynow = rows * font_height;
  }

  // reduce to fit if necessary
  while ((xnow > xtarget || ynow > ytarget) && sdl_font_size > 1) {
    i = sdl_font_size - 1;
    v_printf("SDL: reducing a point size(%d)\n", i);
    v_printf("     xtarget = %d, xnow = %d, ytarget = %d, ynow = %d\n", xtarget, xnow, ytarget, ynow);

    close_font();
    if (!open_font(i)) {
      v_printf("SDL: open_font(%d) failed\n", i);
      goto done;
    }
    xnow = cols * font_width;
    ynow = rows * font_height;
  }

  if (xnow <= xtarget && ynow <= ytarget) {
    v_printf("SDL: point size %d fits xtarget = %d, xnow = %d, ytarget = %d, ynow = %d\n",
               i, xtarget, xnow, ytarget, ynow);
    ret = 1;
  } else
    v_printf("SDL: reduced pointsize to zero xtarget = %d, xnow = %d, ytarget = %d, ynow = %d\n",
           xtarget, xnow, ytarget, ynow);

done:
  pthread_mutex_unlock(&sdl_font_mtx);
  return ret;
}

static void setup_ttf_winsize(int xtarget, int ytarget)
{
  int rc = _setup_ttf_winsize(xtarget, ytarget);
  if (!rc)
    error("SDL: failed to set font for %i:%i\n", xtarget, ytarget);

  if (texture_buf)
    SDL_DestroyTexture(texture_buf);
  texture_buf = SDL_CreateTexture(renderer,
        pixel_format,
        SDL_TEXTUREACCESS_TARGET,
        xtarget, ytarget);
  if (!texture_buf) {
    error("SDL target texture failed: %s\n", SDL_GetError());
    leavedos(99);
  }
}
#endif

static void do_rend(void)
{
  pthread_mutex_lock(&rend_mtx);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);
  pthread_mutex_lock(&tex_mtx);
  SDL_RenderCopy(renderer, texture_buf, NULL, NULL);
  pthread_mutex_lock(&rects_mtx);
  sdl_rects_num = tmp_rects_num;
  tmp_rects_num = 0;
  pthread_mutex_unlock(&rects_mtx);
  pthread_mutex_unlock(&tex_mtx);
  pthread_mutex_unlock(&rend_mtx);
}

static void unlock_surface(void)
{
  assert(surface);
  SDL_UnlockSurface(surface);

  if (!tmp_rects_num) {
#if 1
    v_printf("ERROR: update with zero rects count\n");
#else
    error("update with zero rects count\n");
#endif
    return;
  }

#if THREADED_REND
  sem_post(&rend_sem);
#else
  do_rend();
#endif
}

#if THREADED_REND
static void *render_thread(void *arg)
{
  while (1) {
    sem_wait(&rend_sem);
    render_mode_lock();
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    do_rend();
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    render_mode_unlock();
  }
  return NULL;
}
#endif

int SDL_set_videomode(struct vid_mode_params vmp)
{
  v_printf
      ("SDL: set_videomode: 0x%x (%s), size %d x %d (%d x %d pixel)\n",
       video_mode, vmp.mode_class ? "GRAPH" : "TEXT",
       vmp.text_width, vmp.text_height, vmp.x_res, vmp.y_res);
  if (win_width == vmp.x_res && win_height == vmp.y_res) {
    v_printf("SDL: same mode, not changing\n");
    return 1;
  }
  if (vmp.mode_class == TEXT)
    SDL_change_mode(0, 0, vmp.text_width * font_width,
		      vmp.text_height * font_height);
  else
    SDL_change_mode(vmp.x_res, vmp.y_res, vmp.w_x_res, vmp.w_y_res);

  current_mode_class = vmp.mode_class;
  return 1;
}

static void sync_mouse_coords(void)
{
  int m_x, m_y;

  SDL_GetMouseState(&m_x, &m_y);
  mouse_move_absolute(m_x, m_y, m_x_res, m_y_res, m_cursor_visible, MOUSE_SDL);
}

static void update_mouse_coords(void)
{
  if (grab_active)
    return;
  sync_mouse_coords();
}

static void SDL_change_mode(int x_res, int y_res, int w_x_res, int w_y_res)
{
  Uint32 flags;
  int is_text;

  assert(pthread_equal(pthread_self(), dosemu_pthread_self));
  v_printf("SDL: using mode %dx%d %dx%d %d\n", x_res, y_res, w_x_res,
	   w_y_res, SDL_csd.bits);
  if (surface)
    SDL_FreeSurface(surface);
  if (texture_buf)
    SDL_DestroyTexture(texture_buf);
  if (x_res > 0 && y_res > 0) {
    texture_buf = SDL_CreateTexture(renderer,
        pixel_format,
        SDL_TEXTUREACCESS_STREAMING,
        x_res, y_res);
    if (!texture_buf) {
      error("SDL target texture failed: %s\n", SDL_GetError());
      leavedos(99);
    }
    surface = SDL_CreateRGBSurface(0, x_res, y_res, SDL_csd.bits,
            SDL_csd.r_mask, SDL_csd.g_mask, SDL_csd.b_mask, 0);
    if (!surface) {
      error("SDL surface failed: %s\n", SDL_GetError());
      leavedos(99);
    }
    render_enable(&Render_SDL);
#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
    Text_SDL.flags |= TEXTF_DISABLED;
#endif
    is_text = 0;
  } else {
    surface = NULL;
    texture_buf = NULL;
    render_disable(&Render_SDL);
#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
    Text_SDL.flags &= ~TEXTF_DISABLED;
#endif
    is_text = 1;
  }

  pthread_mutex_lock(&rend_mtx);
  flags = SDL_GetWindowFlags(window);
  if (!(flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN_DESKTOP))) {
    int nw_x_res, nw_y_res;
    SDL_SetWindowSize(window, w_x_res, w_y_res);
    /* work around SDL bug:
     * https://bugzilla.libsdl.org/show_bug.cgi?id=5341
     */
    SDL_GetWindowSize(window, &nw_x_res, &nw_y_res);
    if (nw_x_res != w_x_res) {
      error("X res changed: %i -> %i\n", w_x_res, nw_x_res);
      w_x_res = nw_x_res;
    }
    if (nw_y_res != w_y_res) {
      error("Y res changed: %i -> %i\n", w_y_res, nw_y_res);
      w_y_res = nw_y_res;
    }
    /* set window size again to avoid crash, huh? */
    SDL_SetWindowSize(window, w_x_res, w_y_res);
  }
  if (config.X_fixed_aspect) {
    if (!is_text)
      SDL_RenderSetLogicalSize(renderer, w_x_res, w_y_res);
    else
      SDL_RenderSetLogicalSize(renderer, 0, 0);
  }
  if (!initialized) {
    initialized = 1;
    if (config.X_fullscreen) {
      SDL_DisplayMode dm;
      SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
      /* SDL_GetWindowSize() cannot be used before resize event,
       * so query mode instead. */
      SDL_GetDesktopDisplayMode(0, &dm);
      w_x_res = dm.w;
      w_y_res = dm.h;
    }
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    if (config.X_fullscreen)
      render_gain_focus();
  }
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);
  pthread_mutex_unlock(&rend_mtx);
#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
  if (is_text)
    setup_ttf_winsize(w_x_res, w_y_res);
#endif
  m_x_res = w_x_res;
  m_y_res = w_y_res;
  real_win_width = w_x_res;
  real_win_height = w_y_res;
  win_width = x_res;
  win_height = y_res;

  /* forget about those rectangles */
  pthread_mutex_lock(&rects_mtx);
  sdl_rects_num = 0;
  pthread_mutex_unlock(&rects_mtx);

  update_mouse_coords();
}

static int SDL_update_screen(void)
{
  if (render_is_updating())
    return 0;
  SDL_update();
  return 0;
}

static void SDL_put_image(int x, int y, unsigned width, unsigned height)
{
  const SDL_Rect rect = { .x = x, .y = y, .w = width, .h = height };
  int offs = x * SDL_csd.bits / 8 + y * surface->pitch;

  /* even though we do not do anything with renderer, there is
   * an emplicit dependency between texture and renderer, see
   * https://bugzilla.libsdl.org/show_bug.cgi?id=5421
   */
  pthread_mutex_lock(&rend_mtx);
  pthread_mutex_lock(&tex_mtx);
  SDL_UpdateTexture(texture_buf, &rect, surface->pixels + offs,
      surface->pitch);
  tmp_rects_num++;
  pthread_mutex_unlock(&tex_mtx);
  pthread_mutex_unlock(&rend_mtx);
}

static void window_grab(int on, int kbd)
{
  if (on) {
    if (kbd) {
      SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
      v_printf("SDL: keyboard grab activated\n");
    } else {
      SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
    }
    SDL_SetWindowGrab(window, SDL_TRUE);
    v_printf("SDL: mouse grab activated\n");
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    mouse_enable_native_cursor(1, MOUSE_SDL);
    kbd_grab_active = kbd;
  } else {
    v_printf("SDL: grab released\n");
    SDL_SetWindowGrab(window, SDL_FALSE);
    if (m_cursor_visible)
      SDL_ShowCursor(SDL_ENABLE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    mouse_enable_native_cursor(0, MOUSE_SDL);
    kbd_grab_active = 0;
    sync_mouse_coords();
  }
  grab_active = on;
  /* update title with grab info */
  SDL_change_config(CHG_TITLE, NULL);
}

static void toggle_grab(int kbd)
{
  window_grab(grab_active ^ 1, kbd);
}

static void toggle_fullscreen_mode(void)
{
  config.X_fullscreen = !config.X_fullscreen;
  if (config.X_fullscreen) {
    v_printf("SDL: entering fullscreen mode\n");
    if (!grab_active) {
      window_grab(1, 1);
      force_grab = 1;
    }
    pthread_mutex_lock(&rend_mtx);
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    pthread_mutex_unlock(&rend_mtx);
  } else {
    v_printf("SDL: entering windowed mode!\n");
    pthread_mutex_lock(&rend_mtx);
    SDL_SetWindowFullscreen(window, 0);
    pthread_mutex_unlock(&rend_mtx);
    if (force_grab && grab_active) {
      window_grab(0, 0);
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

  switch (item) {

  case CHG_TITLE:
    /* low-level write */
    if (buf) {
      char *sw;
      const char *charset;
/*	size_t iconlen = strlen(config.X_icon_name) + 1;
	wchar_t iconw[iconlen];
	if (mbstowcs(iconw, config.X_icon_name, iconlen) == -1)
	  iconlen = 1;
	iconw[iconlen-1] = 0;*/
      charset = "utf8";
      sw = unicode_string_to_charset(buf, charset);
//      si = unicode_string_to_charset(iconw, charset);
      v_printf("SDL: SDL_change_config: win_name = %s\n", sw);
      SDL_SetWindowTitle(window, sw);
      free(sw);
//      free(si);
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
    change_config(item, buf, grab_active, kbd_grab_active);
    break;

  case CHG_FONT: {
#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
    char *p;
    while ((p = strchr(buf, '_')))
      *p = ' ';
    if (!sdl_load_font(buf)) {
      error("xmode: font %s not found\n", (char *)buf);
      break;
    }
    close_font();
    setup_ttf_winsize(real_win_width, real_win_height);
    redraw_text();
#else
    v_printf("SDL: CHG_FONT not implemented\n");
#endif
    break;
  }

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
static char *get_selection_string(t_unicode sel_text[], const char *charset)
{
	struct char_set_state paste_state;
	struct char_set *paste_charset;
	t_unicode *u = sel_text;
	char *s, *p;
	size_t sel_space = 0;

	while (sel_text[sel_space])
		sel_space++;
	paste_charset = lookup_charset(charset);
	sel_space *= MB_LEN_MAX;
	p = s = malloc(sel_space);
	init_charset_state(&paste_state, paste_charset);

	while (*u) {
		size_t result = unicode_to_charset(&paste_state, *u++,
						   (unsigned char *)p, sel_space);
		if (result == -1) {
			warn("save_selection unfinished2\n");
			break;
		}
		p += result;
		sel_space -= result;
	}
	*p = '\0';
	cleanup_charset_state(&paste_state);
	return s;
}

static int shift_pressed(void)
{
  const Uint8 *state = SDL_GetKeyboardState(NULL);
  return (state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT]);
}

static int window_has_focus(void)
{
  uint32_t flags = SDL_GetWindowFlags(window);
  return (flags & SDL_WINDOW_INPUT_FOCUS);
}
#endif				/* CONFIG_SDL_SELECTION */

static void SDL_handle_events(void)
{
  SDL_Event event;

  assert(pthread_equal(pthread_self(), dosemu_pthread_self));
  if (render_is_updating())
    return;
  /* events may resize renderer, so lock */
  pthread_mutex_lock(&rend_mtx);
  SDL_PumpEvents();
  pthread_mutex_unlock(&rend_mtx);
  /* SDL_PeepEvents() is thread-safe, SDL_PollEvent() - not */
  while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT,
	SDL_LASTEVENT) > 0) {
    switch (event.type) {

    case SDL_WINDOWEVENT:
      switch (event.window.event) {
      case SDL_WINDOWEVENT_FOCUS_GAINED:
	v_printf("SDL: focus in\n");
	render_gain_focus();
	if (config.X_background_pause && !dosemu_user_froze)
	  unfreeze_dosemu();
	break;
      case SDL_WINDOWEVENT_FOCUS_LOST:
	v_printf("SDL: focus out\n");
	render_lose_focus();
	if (config.X_background_pause && !dosemu_user_froze)
	  freeze_dosemu();
	break;

      case SDL_WINDOWEVENT_SIZE_CHANGED:
        v_printf("SDL: window size changed to %dx%d\n", event.window.data1, event.window.data2);
        break;

      case SDL_WINDOWEVENT_RESIZED:
        v_printf("SDL: window resized %dx%d\n", event.window.data1, event.window.data2);
        real_win_width = event.window.data1;
        real_win_height = event.window.data2;
#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
        if (current_mode_class == TEXT)
          setup_ttf_winsize(event.window.data1, event.window.data2);
#endif

	/* very strange things happen: if renderer size was explicitly
	 * set, SDL reports mouse coords relative to that. Otherwise
	 * it reports mouse coords relative to the window. */
	SDL_RenderGetLogicalSize(renderer, &m_x_res, &m_y_res);
	if (!m_x_res || !m_y_res) {
	  m_x_res = event.window.data1;
	  m_y_res = event.window.data2;
	}
	update_mouse_coords();
	SDL_redraw();
	break;

      case SDL_WINDOWEVENT_EXPOSED:
	SDL_redraw();
	break;
      case SDL_WINDOWEVENT_ENTER:
        /* ignore fake enter events */
        if (config.X_fullscreen)
          break;
        mouse_drag_to_corner(m_x_res, m_y_res, MOUSE_SDL);
        break;
      }
      break;

    case SDL_TEXTINPUT:
      {
	SDL_Event key_event;
	int rc;

	k_printf("SDL: TEXTINPUT event before KEYDOWN\n");
	do
	  rc = SDL_PeepEvents(&key_event, 1, SDL_GETEVENT, SDL_KEYDOWN,
		SDL_KEYDOWN);
	while (rc == 1 && event.text.timestamp != key_event.key.timestamp);
	if (rc != 1) {
	  error("SDL: missing key event\n");
	  break;
	}
	SDL_process_key_text(key_event.key, event.text);
      }
      break;

    case SDL_KEYDOWN:
      {
	SDL_Event text_event;
	int rc;
	SDL_Keysym keysym = event.key.keysym;

	if (wait_kup)
	  break;
	if ((keysym.mod & KMOD_CTRL) && (keysym.mod & KMOD_ALT)) {
	  if (keysym.sym == mgrab_key || keysym.sym == SDLK_k) {
	    force_grab = 0;
	    toggle_grab(keysym.sym == SDLK_k);
	    break;
	  } else if (keysym.sym == SDLK_f) {
	    toggle_fullscreen_mode();
	    /* some versions of SDL re-send the keydown events after the
	     * full-screen switch. We need to filter them out to prevent
	     * the infinite switching loop. */
	    wait_kup = 1;
	    break;
	  }
	}
	if (vga.mode_class == TEXT &&
	    (keysym.sym == SDLK_LSHIFT || keysym.sym == SDLK_RSHIFT)) {
	  copypaste = 1;
	  /* enable cursor for copy/paste */
	  if (!m_cursor_visible)
	    SDL_ShowCursor(SDL_ENABLE);
	}
#if CONFIG_SDL_SELECTION
	clear_if_in_selection();
#endif
	do
	  rc = SDL_PeepEvents(&text_event, 1, SDL_GETEVENT, SDL_TEXTINPUT,
		SDL_TEXTINPUT);
	while (rc == 1 && event.key.timestamp != text_event.text.timestamp);
	if (rc == 1) {
	    SDL_Event key_event;
	    int rc2 = SDL_PeepEvents(&key_event, 1, SDL_PEEKEVENT, SDL_KEYDOWN,
		    SDL_KEYDOWN);
	    if (rc2 == 1 && event.key.timestamp == key_event.key.timestamp) {
		error("SDL: duplicate keypress events\n");
		/* at this point we can't trust text_event */
		rc = 0;
	    }
	}
	if (rc == 1)
	    SDL_process_key_text(event.key, text_event.text);
	else
	    SDL_process_key_press(event.key);
      }
      break;

    case SDL_KEYUP: {
      SDL_Keysym keysym = event.key.keysym;
      wait_kup = 0;
      if (copypaste && (keysym.sym == SDLK_LSHIFT ||
              keysym.sym == SDLK_RSHIFT)) {
        copypaste = 0;
        if (!m_cursor_visible)
	    SDL_ShowCursor(SDL_DISABLE);
      }
	SDL_process_key_release(event.key);
      break;
    }

    case SDL_MOUSEBUTTONDOWN:
      {
	int buttons = SDL_GetMouseState(NULL, NULL);
#if CONFIG_SDL_SELECTION
	if (window_has_focus() && !shift_pressed()) {
	  clear_selection_data();
	} else if (vga.mode_class == TEXT && !grab_active) {
	  if (event.button.button == SDL_BUTTON_LEFT)
	    start_selection(x_to_col(event.button.x, m_x_res),
			    y_to_row(event.button.y, m_y_res));
	  else if (event.button.button == SDL_BUTTON_RIGHT)
	    start_extend_selection(x_to_col(event.button.x, m_x_res),
				   y_to_row(event.button.y, m_y_res));
	  else if (event.button.button == SDL_BUTTON_MIDDLE) {
	    char *paste = SDL_GetClipboardText();
	    if (paste) {
	      set_shiftstate(0);
	      paste_text(paste, strlen(paste), "utf8");
	    }
	  }
	  break;
	}
#endif				/* CONFIG_SDL_SELECTION */
	mouse_move_buttons(!!(buttons & SDL_BUTTON(1)),
			   !!(buttons & SDL_BUTTON(2)),
			   !!(buttons & SDL_BUTTON(3)),
			   MOUSE_SDL);
	break;
      }
    case SDL_MOUSEBUTTONUP:
      {
	int buttons = SDL_GetMouseState(NULL, NULL);
#if CONFIG_SDL_SELECTION
	if (vga.mode_class == TEXT && !grab_active) {
	    t_unicode *sel = end_selection();
	    if (sel) {
		char *send_text = get_selection_string(sel, "utf8");
		SDL_SetClipboardText(send_text);
		free(send_text);
	    }
	}
#endif				/* CONFIG_SDL_SELECTION */
	mouse_move_buttons(!!(buttons & SDL_BUTTON(1)),
			   !!(buttons & SDL_BUTTON(2)),
			   !!(buttons & SDL_BUTTON(3)),
			   MOUSE_SDL);
	break;
      }

    case SDL_MOUSEMOTION:
#if CONFIG_SDL_SELECTION
      extend_selection(x_to_col(event.motion.x, m_x_res),
			 y_to_row(event.motion.y, m_y_res));
#endif				/* CONFIG_SDL_SELECTION */
      if (grab_active)
	mouse_move_relative(event.motion.xrel, event.motion.yrel,
			    m_x_res, m_y_res, MOUSE_SDL);
      else
	mouse_move_absolute(event.motion.x, event.motion.y, m_x_res,
			    m_y_res, m_cursor_visible, MOUSE_SDL);
      break;
    case SDL_MOUSEWHEEL:
      mouse_move_wheel(-event.wheel.y, MOUSE_SDL);
      break;
    case SDL_QUIT:
      leavedos(0);
      break;
    default:
      v_printf("PAS ENCORE TRAITE %x\n", event.type);
      /* TODO */
      break;
    }
  }
}

static int SDL_mouse_init(void)
{
  mouse_t *mice = &config.mouse;
  if (Video != &Video_SDL)
    return FALSE;

  mice->type = MOUSE_SDL;
  mouse_enable_native_cursor(config.X_fullscreen, MOUSE_SDL);
  /* we have the X cursor, but if we start fullscreen, grab by default */
  m_printf("MOUSE: SDL Mouse being set\n");
  return TRUE;
}

static void SDL_show_mouse_cursor(int yes)
{
  m_cursor_visible = yes;
  SDL_ShowCursor((yes && !grab_active) ? SDL_ENABLE : SDL_DISABLE);
}

struct mouse_client Mouse_SDL = {
  "SDL",			/* name */
  SDL_mouse_init,		/* init */
  NULL,				/* close */
  SDL_show_mouse_cursor		/* show_cursor */
};

static void sdl_scrub(void)
{
  /* allow -S -t for SDL audio and terminal video */
  if (config.sdl && config.term) {
    config.sdl = 0;
    config.X = 0;
    Video = NULL;
  }
}

#if defined(HAVE_SDL2_TTF) && defined(HAVE_FONTCONFIG)
static void SDL_draw_string(void *opaque, int x, int y, unsigned char *text, int len, Bit8u attr)
{
  char *s;
  struct char_set_state state;
  int characters;
  t_unicode *str;
  SDL_Rect rect;

  v_printf("SDL_draw_string\n");

  init_charset_state(&state, trconfig.video_mem_charset);
  characters = character_count(&state, (char *)text, len);
  if (characters == -1) {
    v_printf("SDL: invalid char count\n");
    return;
  }
  str = malloc(sizeof(t_unicode) * (characters + 1));

  charset_to_unicode_string(&state, str, (const char **)&text, len, characters + 1);
  cleanup_charset_state(&state);

  s = unicode_string_to_charset((wchar_t *)str, "utf8");
  free(str);

  render_mode_lock();
  pthread_mutex_lock(&sdl_font_mtx);
  if (!sdl_font) {
    pthread_mutex_unlock(&sdl_font_mtx);
    render_mode_unlock();
    free(s);
    error("SDL: sdl_font is null\n");
    return;
  }

  SDL_Surface *srf = TTF_RenderUTF8_Shaded(sdl_font, s,
                                           text_colors[ATTR_FG(attr)],
                                           text_colors[ATTR_BG(attr)]);
  rect.x = font_width * x;  /* font_width/height needs to be under font mtx */
  rect.y = font_height * y; /* height plus spacing to next line */
  rect.w = min(srf->w, font_width * len);
  rect.h = min(srf->h, font_height);
  pthread_mutex_unlock(&sdl_font_mtx);
  free(s);

  pthread_mutex_lock(&rend_mtx);
  SDL_Texture *txt = SDL_CreateTextureFromSurface(renderer, srf);
  pthread_mutex_lock(&tex_mtx);
  SDL_SetRenderTarget(renderer, texture_buf);
  SDL_RenderCopy(renderer, txt, NULL, &rect);
  SDL_SetRenderTarget(renderer, NULL);
  pthread_mutex_unlock(&tex_mtx);
  pthread_mutex_unlock(&rend_mtx);
  render_mode_unlock();

  SDL_DestroyTexture(txt);
  SDL_FreeSurface(srf);

  pthread_mutex_lock(&rects_mtx);
  tmp_rects_num++;
  pthread_mutex_unlock(&rects_mtx);

#if THREADED_REND
  sem_post(&rend_sem);
#else
  do_rend();
#endif
}

/*
 * Draw a horizontal line (for text modes)
 * The attribute is the VGA color/mono text attribute.
 */
static void SDL_draw_line(void *opaque, int x, int y, float ul, int len)
{
  v_printf("SDL_draw_line x(%d) y(%d) len(%d)\n", x, y, len);

  pthread_mutex_lock(&rend_mtx);
  pthread_mutex_lock(&tex_mtx);
  SDL_SetRenderTarget(renderer, texture_buf);
  SDL_RenderDrawLine(renderer,
      font_width * x,
      font_height * y + (font_height - 1) * ul,
      font_width * (x + len) - 1,
      font_height * y + (font_height - 1) * ul
  );
  SDL_SetRenderTarget(renderer, NULL);
  pthread_mutex_unlock(&tex_mtx);
  pthread_mutex_unlock(&rend_mtx);

  pthread_mutex_lock(&rects_mtx);
  tmp_rects_num++;
  pthread_mutex_unlock(&rects_mtx);

#if THREADED_REND
  sem_post(&rend_sem);
#else
  do_rend();
#endif
}

/*
 * Draw the cursor (nothing in graphics modes, normal if we have focus,
 * rectangle otherwise).
 */
static void SDL_draw_text_cursor(void *opaque, int x, int y, Bit8u attr,
                               int start, int end, Boolean focus)
{
  SDL_Rect rect;

  if (current_mode_class == GRAPH)
    return;

  if (!focus) {
    rect.x = font_width * x;
    rect.y = font_height * y;
    rect.w = font_width;
    rect.h = font_height - 1;

    pthread_mutex_lock(&rend_mtx);
    pthread_mutex_lock(&tex_mtx);
    SDL_SetRenderTarget(renderer, texture_buf);
    SDL_SetRenderDrawColor(renderer,
                           text_colors[ATTR_FG(attr)].r,
                           text_colors[ATTR_FG(attr)].g,
                           text_colors[ATTR_FG(attr)].b,
                           text_colors[ATTR_FG(attr)].a);
    SDL_RenderDrawRect(renderer, &rect);
    SDL_SetRenderTarget(renderer, NULL);
    pthread_mutex_unlock(&tex_mtx);
    pthread_mutex_unlock(&rend_mtx);

  } else {
    int cstart, cend;

    cstart = ((start + 1) * font_height) / vga.char_height - 1;
    if (cstart == -1)
      cstart = 0;
    cend = ((end + 1) * font_height) / vga.char_height - 1;
    if (cend == -1)
      cend = 0;

    rect.x = font_width * x;
    rect.y = font_height * y + cstart;
    rect.w = font_width;
    rect.h = cend - cstart + 1;

    pthread_mutex_lock(&rend_mtx);
    pthread_mutex_lock(&tex_mtx);
    SDL_SetRenderTarget(renderer, texture_buf);
    SDL_SetRenderDrawColor(renderer,
                           text_colors[ATTR_FG(attr)].r,
                           text_colors[ATTR_FG(attr)].g,
                           text_colors[ATTR_FG(attr)].b,
                           text_colors[ATTR_FG(attr)].a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderTarget(renderer, NULL);
    pthread_mutex_unlock(&tex_mtx);
    pthread_mutex_unlock(&rend_mtx);
  }

  pthread_mutex_lock(&rects_mtx);
  tmp_rects_num++;
  pthread_mutex_unlock(&rects_mtx);

#if THREADED_REND
  sem_post(&rend_sem);
#else
  do_rend();
#endif
}

/*
 * Update the active SDL colormap for text modes DAC entry col.
 */
static void SDL_set_text_palette(void *opaque, DAC_entry *col, int i)
{
  int shift = 8 - vga.dac.bits;

  v_printf("SDL_set_text_palette %d: shift %d, rgb(%x, %x, %x)\n", i, shift, col->r << shift, col->g << shift, col->b << shift);

  text_colors[i].r = col->r << shift;
  text_colors[i].g = col->g << shift;
  text_colors[i].b = col->b << shift;
  text_colors[i].a = 0;
}

static void SDL_text_lock(void *opaque)
{
}

static void SDL_text_unlock(void *opaque)
{
}


static struct text_system Text_SDL =
{
  SDL_draw_string,
  SDL_draw_line,
  SDL_draw_text_cursor,
  SDL_set_text_palette,
  SDL_text_lock,
  SDL_text_unlock,
  NULL,
  "sdl",
};
#endif

CONSTRUCTOR(static void init(void))
{
  register_video_client(&Video_SDL);
  register_keyboard_client(&Keyboard_SDL);
  register_mouse_client(&Mouse_SDL);
  register_config_scrub(sdl_scrub);
}
