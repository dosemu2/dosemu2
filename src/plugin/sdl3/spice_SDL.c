/*
 *  Copyright (C) 2026 the dosemu2 project
 *
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
 * Purpose: a SPICE remote display, running alongside the local SDL window.
 *
 * It lives in the SDL plugin rather than beside it because everything the
 * plugin renders -- graphics, bitmap fonts, and TTF glyphs alike -- passes
 * through do_rend_rects() as a CPU-side SDL_Surface with a destination
 * rectangle.  Mirroring that one funnel gets the remote display the whole
 * of the plugin's rendering, TTF included, for one call per rectangle.  A
 * standalone plugin would have had to reimplement the text engine to reach
 * the same place.
 *
 * Pointer events are reported as MOUSE_SDL rather than as a source of
 * their own.  That is not a shortcut: int33_mouse_accepts() exists to keep
 * a real serial mouse apart from the GUI one, and a remote viewer is the
 * GUI one -- the same pointer on the same screen, arriving over a socket.
 * Claiming to be a second mouse would have had it filtered out whenever a
 * physical mouse is configured, which is the opposite of what is wanted.
 *
 * Input is deliberately not shared with the local window.  The two are
 * independent sources into one emulated keyboard, either of which may be
 * typing at any moment: the local window does not have to lose focus for
 * the remote one to be used.  What they do share is the guest's modifier
 * state, so each side resynchronises that to its own held modifiers before
 * injecting a key -- which is what SDL_sync_shiftstate() already does for
 * the local window, and what spice_sync_shiftstate() below does for this
 * one.  Locks are the guest's and flow the other way, out to clients.
 *
 * Author: Claude, for the dosemu2 project.
 */

#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#if defined(__APPLE__) || defined(__ANDROID__)
/* redefines sem_init() and friends, as the other players do */
#include "utilities.h"
#else
#include <semaphore.h>
#endif

#include <sdl3-spice.h>
#include <sdl3-spice-sdl.h>

#include "emu.h"
#include "init.h"
#include "keyboard/keyboard.h"
#include "keyboard/keyb_clients.h"
#include "mouse.h"
#include "sig.h"
#include "sound/sound.h"
#include "video.h"

#include "spice_SDL.h"

#define SPICE_DEFAULT_PORT 5930
/* The size the display channel is published at before the guest has drawn
 * anything.  The first frame replaces it, usually within a few ms. */
#define SPICE_INIT_WIDTH 720
#define SPICE_INIT_HEIGHT 400

static SDLSpice_Server *server;
static SDLSpice_Display *display;
static SDLSpice_Input *input;
static SDLSpice_Audio *audio;
static int spice_on;

static int cur_width, cur_height;
static SDL_PixelFormat cur_format;

/* ---------------------------------------------------------- event queue */

/*
 * SPICE callbacks arrive on the library's own thread, and nothing in
 * dosemu2's input path may be called from there.  They are queued here and
 * replayed by a callback on dosemu2's thread.
 *
 * add_thread_callback() is posted only when the queue was empty, so a burst
 * of scancodes costs one callback rather than one per byte.
 */
enum ev_type {
    EV_SCANCODE,
    EV_MOTION_ABS,
    EV_BUTTONS,
    EV_WHEEL,
};

struct ev {
    enum ev_type type;
    int a, b, c, d;
};

#define EV_MAX 512
static struct ev ev_ring[EV_MAX];
static unsigned ev_head, ev_tail;
static int ev_posted;
static pthread_mutex_t ev_mtx = PTHREAD_MUTEX_INITIALIZER;

static void spice_drain_events(void *arg);

static void ev_push(const struct ev *e)
{
    int post = 0;

    pthread_mutex_lock(&ev_mtx);
    if (ev_head - ev_tail >= EV_MAX) {
        pthread_mutex_unlock(&ev_mtx);
        error("spice: input queue overflow\n");
        return;
    }
    ev_ring[ev_head++ % EV_MAX] = *e;
    if (!ev_posted) {
        ev_posted = 1;
        post = 1;
    }
    pthread_mutex_unlock(&ev_mtx);

    if (post)
        add_thread_callback(spice_drain_events, NULL, "spice input");
}

/* ------------------------------------------------------ shiftstate resync */

/*
 * Bring the guest's modifier state into line with what the remote client is
 * holding, before injecting a key from it.
 *
 * Without this, a remote key typed while the local window holds Shift comes
 * out shifted, and vice versa.  set_shiftstate() does not merely record the
 * new state, it synthesizes the make and break codes to reach it, so the
 * guest sees a consistent keyboard either way.  The local side does exactly
 * this against SDL's modifier state; this is the mirror of it, and the two
 * together are what make one emulated keyboard safe to share.
 *
 * Only the four modifiers a client actually reports are ours to set.  The
 * locks belong to the guest -- we push those the other way, to clients'
 * LEDs -- so they are preserved untouched.
 */
#define SPICE_SYNCED_MASK \
    (MODIFIER_SHIFT | MODIFIER_CTRL | MODIFIER_ALT | MODIFIER_ALTGR)

static void spice_sync_shiftstate(void)
{
    uint32_t held = SDLSpice_GetHeldModifiers(input);
    t_modifiers want = 0;

    if (held & SDLSPICE_MOD_SHIFT)
        want |= MODIFIER_SHIFT;
    if (held & SDLSPICE_MOD_CTRL)
        want |= MODIFIER_CTRL;
    if (held & SDLSPICE_MOD_ALT)
        want |= MODIFIER_ALT;
    if (held & SDLSPICE_MOD_ALTGR)
        want |= MODIFIER_ALTGR;

    set_shiftstate((get_shiftstate() & ~SPICE_SYNCED_MASK) | want);
}

/*
 * True for a scancode that is itself a modifier, in which case resyncing
 * would fight the key being injected: the held mask already accounts for
 * it, so set_shiftstate() would synthesize the very make or break that is
 * about to arrive.  The local side skips the same set of keys.
 */
static int is_modifier_code(uint8_t code, int e0)
{
    switch (code & 0x7f) {
    case 0x2a:                  /* left shift  (e0 2a is the fake shift) */
    case 0x36:                  /* right shift */
        return !e0;
    case 0x1d:                  /* ctrl, either */
    case 0x38:                  /* alt, and e0 38 for AltGr */
        return 1;
    case 0x3a:                  /* caps lock */
    case 0x45:                  /* num lock */
    case 0x46:                  /* scroll lock */
        return 1;
    case 0x52:                  /* insert */
        return 1;
    }
    return 0;
}

/* ----------------------------------------------------- dosemu2-side replay */

static void replay_scancode(int code)
{
    /* Framing has to be tracked here too: whether the byte is a modifier
     * depends on the prefix that preceded it. */
    static int pending_e0;
    static int pending_e1;
    uint8_t c = code;
    int e0;

    if (pending_e1 > 0) {
        pending_e1--;
        put_rawkey(c);
        return;
    }
    if (c == 0xe1) {
        pending_e1 = 5;
        put_rawkey(c);
        return;
    }
    if (c == 0xe0) {
        pending_e0 = 1;
        put_rawkey(c);
        return;
    }

    e0 = pending_e0;
    pending_e0 = 0;

    if (!is_modifier_code(c, e0))
        spice_sync_shiftstate();

    put_rawkey(c);
}

static void spice_drain_events(void *arg)
{
    for (;;) {
        struct ev e;

        pthread_mutex_lock(&ev_mtx);
        if (ev_head == ev_tail) {
            ev_posted = 0;
            pthread_mutex_unlock(&ev_mtx);
            return;
        }
        e = ev_ring[ev_tail++ % EV_MAX];
        pthread_mutex_unlock(&ev_mtx);

        switch (e.type) {
        case EV_SCANCODE:
            replay_scancode(e.a);
            break;
        case EV_MOTION_ABS:
            mouse_move_absolute(e.a, e.b, e.c, e.d, 1, MOUSE_SDL);
            break;
        case EV_BUTTONS:
            mouse_move_buttons(!!(e.a & SDLSPICE_BUTTON_LEFT),
                               !!(e.a & SDLSPICE_BUTTON_MIDDLE),
                               !!(e.a & SDLSPICE_BUTTON_RIGHT), MOUSE_SDL);
            break;
        case EV_WHEEL:
            mouse_move_wheel(-e.a, MOUSE_SDL);
            break;
        }
    }
}

/* ------------------------------------------------- library-thread callbacks */

static void cb_scancode(void *opaque, uint8_t code)
{
    struct ev e = { .type = EV_SCANCODE, .a = code };

    ev_push(&e);
}

static void cb_motion_abs(void *opaque, int x, int y, int w, int h,
                          uint32_t buttons)
{
    struct ev e = { .type = EV_MOTION_ABS, .a = x, .b = y, .c = w, .d = h };

    ev_push(&e);
}

static void cb_buttons(void *opaque, uint32_t buttons)
{
    struct ev e = { .type = EV_BUTTONS, .a = buttons };

    ev_push(&e);
}

/*
 * How much of the screen the client could draw out of its own cache rather
 * than be sent again.  Logged when a viewer leaves and when the plugin
 * shuts down, because it is the number that says whether a remote session
 * is usable or a slideshow.
 */
static void log_cache_stats(void)
{
    unsigned long hits, misses;

    SDLSpice_GetCacheStats(display, &hits, &misses, NULL, NULL);
    if (hits + misses)
        v_printf("SPICE: image cache %lu hits, %lu misses (%lu%%)\n",
                 hits, misses, 100 * hits / (hits + misses));
}

static void cb_client(void *opaque, SDLSpice_ClientEvent ev, uint32_t id)
{
    v_printf("SPICE: client %u %s\n", id,
             ev == SDLSPICE_CLIENT_CONNECTED ? "connected" : "disconnected");
    if (ev == SDLSPICE_CLIENT_DISCONNECTED && !SDLSpice_NumClients(server))
        log_cache_stats();
}

static void cb_wheel(void *opaque, int dz, uint32_t buttons)
{
    struct ev e = { .type = EV_WHEEL, .a = dz };

    ev_push(&e);
}

static void cb_log(void *opaque, SDLSpice_LogLevel level, const char *msg)
{
    switch (level) {
    case SDLSPICE_LOG_ERROR:
        error("spice: %s\n", msg);
        break;
    case SDLSPICE_LOG_WARN:
        warn("spice: %s\n", msg);
        break;
    default:
        v_printf("spice: %s\n", msg);
        break;
    }
}

/* ------------------------------------------------------------------ sound */

#define spicesnd_name "spice"
#define spicesnd_longname "Sound Output: SPICE"

static struct player_params snd_params;
static pthread_t snd_thr;
static int snd_started;
static pthread_mutex_t snd_mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t snd_start_sem;
static sem_t snd_stop_sem;

static void *spicesnd_write(void *arg)
{
    /* Frames, not bytes: stereo s16 is what the channel is. */
#define SND_FRAMES 2048
    static int16_t buf[SND_FRAMES * 2];

    while (1) {
        sem_wait(&snd_start_sem);
        while (1) {
            uint32_t size;
            int l_started;

            pthread_mutex_lock(&snd_mtx);
            l_started = snd_started;
            pthread_mutex_unlock(&snd_mtx);
            if (!l_started)
                break;

            size = pcm_data_get(buf, sizeof(buf), &snd_params);
            if (!size) {
                usleep(10000);
                continue;
            }
            SDLSpice_QueueAudio(audio, buf, size / (2 * sizeof(int16_t)));
        }
        sem_post(&snd_stop_sem);
    }
    return NULL;
}

static int spicesnd_cfg(void *arg)
{
    if (!spice_on)
        return PCM_CF_DISABLED;
    return pcm_parse_cfg(config.sound_driver, spicesnd_name);
}

static int spicesnd_open(void *arg)
{
    int rate = SDLSpice_GetAudioRate(audio);

    if (rate <= 0)
        return 0;

    /* Whatever the clients negotiated: the pcm layer resamples to it far
     * more cheaply than the wire would. */
    snd_params.rate = rate;
    snd_params.format = PCM_FORMAT_S16_LE;
    snd_params.channels = 2;

    pcm_setup_hpf(&snd_params);
    sem_init(&snd_start_sem, 0, 0);
    sem_init(&snd_stop_sem, 0, 0);
    pthread_create(&snd_thr, NULL, spicesnd_write, NULL);
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(__GLIBC__)
    pthread_setname_np(snd_thr, "dosemu: spice snd");
#endif
    return 1;
}

static void spicesnd_close(void *arg)
{
    pthread_cancel(snd_thr);
    pthread_join(snd_thr, NULL);
    sem_destroy(&snd_start_sem);
    sem_destroy(&snd_stop_sem);
}

static void spicesnd_start(void *arg)
{
    SDLSpice_StartAudio(audio);
    pthread_mutex_lock(&snd_mtx);
    snd_started = 1;
    sem_post(&snd_start_sem);
    pthread_mutex_unlock(&snd_mtx);
}

static void spicesnd_stop(void *arg)
{
    pthread_mutex_lock(&snd_mtx);
    if (!snd_started) {
        pthread_mutex_unlock(&snd_mtx);
        return;
    }
    snd_started = 0;
    pthread_mutex_unlock(&snd_mtx);
    sem_wait(&snd_stop_sem);
    SDLSpice_StopAudio(audio);
}

static const struct pcm_player spice_player = {
    .name = spicesnd_name,
    .longname = spicesnd_longname,
    .get_cfg = spicesnd_cfg,
    .open = spicesnd_open,
    .close = spicesnd_close,
    .start = spicesnd_start,
    .stop = spicesnd_stop,
    .id = PCM_ID_P,
};

/* ------------------------------------------------------------ public API */

int spice_sdl_on(void)
{
    return spice_on;
}

static const char *nonempty(const char *s)
{
    return (s && s[0]) ? s : NULL;
}

int spice_sdl_init(void)
{
    SDLSpice_Config cfg = SDLSPICE_CONFIG_INIT;
    SDLSpice_InputCallbacks cbs;

    if (!config.spice)
        return 0;

    SDLSpice_SetLogCallback(cb_log, NULL);

    /* The config parser gives an unset string variable as "", not NULL,
     * and the library reads "" as an address rather than as "unset". */
    cfg.port = config.spice_port ?: SPICE_DEFAULT_PORT;
    cfg.addr = nonempty(config.spice_addr);
    cfg.password = nonempty(config.spice_password);
    cfg.name = "dosemu2";

    server = SDLSpice_CreateServer(&cfg);
    if (!server) {
        error("spice: cannot start the server\n");
        return -1;
    }

    SDLSpice_SetClientCallback(server, cb_client, NULL);

    memset(&cbs, 0, sizeof(cbs));
    cbs.scancode = cb_scancode;
    cbs.motion_abs = cb_motion_abs;
    cbs.buttons = cb_buttons;
    cbs.wheel = cb_wheel;
    input = SDLSpice_CreateInput(server, &cbs);
    if (!input) {
        error("spice: cannot publish the input channels\n");
        SDLSpice_DestroyServer(server);
        server = NULL;
        return -1;
    }

    audio = SDLSpice_CreateAudio(server, 0);
    if (!audio)
        warn("spice: no audio channel, continuing without sound\n");

    spice_on = 1;

    /* Publish the display channel before anyone can link.  Spice sends the
     * channel list once, when a client links, and never mentions a channel
     * that appeared afterwards, so a viewer started alongside dosemu2 --
     * which is the normal way to start one -- would get a session with no
     * display in it and stay blank until it reconnected. */
    spice_sdl_set_size(SPICE_INIT_WIDTH, SPICE_INIT_HEIGHT,
                       SDL_PIXELFORMAT_XRGB8888);

    c_printf("SPICE: listening on %s:%d\n", cfg.addr ?: "*", cfg.port);
    return 1;
}

void spice_sdl_done(void)
{
    if (!spice_on)
        return;
    spice_on = 0;

    log_cache_stats();

    SDLSpice_DestroyAudio(audio);
    audio = NULL;
    SDLSpice_DestroyInput(input);
    input = NULL;
    SDLSpice_DestroyDisplay(display);
    display = NULL;
    SDLSpice_DestroyServer(server);
    server = NULL;
}

void spice_sdl_set_size(int width, int height, SDL_PixelFormat format)
{
    SDLSpice_Format fmt;

    if (!spice_on || width <= 0 || height <= 0)
        return;

    if (width == cur_width && height == cur_height && format == cur_format)
        return;

    fmt = SDLSpice_FormatFromSDL(format);
    if (fmt == SDLSPICE_FMT_INVALID) {
        error("spice: cannot present SDL pixel format %#x\n", format);
        return;
    }

    if (!display) {
        display = SDLSpice_CreateDisplay(server, width, height, fmt);
        if (!display) {
            error("spice: cannot publish the display channel\n");
            return;
        }
    } else if (SDLSpice_ResizeDisplay(display, width, height, fmt) != 0) {
        error("spice: cannot resize to %dx%d\n", width, height);
        return;
    }

    cur_width = width;
    cur_height = height;
    cur_format = format;
}

void spice_sdl_blit(int x, int y, SDL_Surface *s)
{
    if (!spice_on || !display)
        return;
    SDLSpice_BlitSurface(display, x, y, s);
}

void spice_sdl_flush(void)
{
    if (!spice_on || !display)
        return;
    SDLSpice_FlushDisplay(display);
}

void spice_sdl_clear(void)
{
    SDLSpice_Pixels px;
    int row;

    if (!spice_on || !display)
        return;
    if (SDLSpice_LockDisplay(display, &px) != 0)
        return;

    for (row = 0; row < px.height; row++)
        memset((uint8_t *)px.pixels + (size_t)row * px.pitch, 0,
               (size_t)px.width * SDL_BYTESPERPIXEL(cur_format));
    SDLSpice_MarkDirty(display, 0, 0, px.width, px.height);
    SDLSpice_UnlockDisplay(display);
}

void spice_sdl_update_leds(void)
{
    t_modifiers m;
    uint32_t leds = 0;

    if (!spice_on || !input)
        return;

    m = get_shiftstate();
    if (m & MODIFIER_SCR)
        leds |= SDLSPICE_LED_SCROLL;
    if (m & MODIFIER_NUM)
        leds |= SDLSPICE_LED_NUM;
    if (m & MODIFIER_CAPS)
        leds |= SDLSPICE_LED_CAPS;
    SDLSpice_SetLeds(input, leds);
}

CONSTRUCTOR(static void spicesnd_register(void))
{
    snd_params.handle = pcm_register_player(&spice_player, NULL);
}
