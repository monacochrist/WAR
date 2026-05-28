//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/war_main.c
//-----------------------------------------------------------------------------

#include "../vendor/wayland/generated/linux-dmabuf-v1-protocol.c"
#include "../vendor/wayland/generated/xdg-shell-protocol.c"

#include "../key/key.h"
#include "../vendor/libsodium-1.0.21/include/sodium.h"
#include "../vendor/wayland/generated/linux-dmabuf-v1-client-protocol.h"
#include "../vendor/wayland/generated/xdg-shell-client-protocol.h"
#include "h/war_build_keymap_functions.h"
#include "h/war_color.h"
#include "h/war_command.h"
#include "h/war_config.h"
#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_functions.h"
#include "h/war_keymap.h"
#include "h/war_keymap_functions.h"
#include "h/war_main.h"
#include "h/war_pool.h"
#include "h/war_vulkan.h"
#include "h/war_wayland.h"

#include <errno.h>
#include <fcntl.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lualib.h>
#include <pipewire-0.3/pipewire/context.h>
#include <pipewire-0.3/pipewire/core.h>
#include <pipewire-0.3/pipewire/pipewire.h>
#include <pipewire-0.3/pipewire/stream.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <spa-0.2/spa/param/audio/format-utils.h>
#include <spa-0.2/spa/param/audio/format.h>
#include <spa-0.2/spa/param/audio/raw.h>
#include <spa-0.2/spa/param/latency-utils.h>
#include <spa-0.2/spa/pod/builder.h>
#include <spa-0.2/spa/pod/pod.h>
#include <spa-0.2/spa/utils/hook.h>
#include <spa-0.2/spa/utils/list.h>
#include <spa-0.2/spa/utils/result.h>
#include <spa-0.2/spa/utils/string.h>
#include <stdint.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

//---------------------------------------------------------------------------
// WAYLAND LISTENERS
//---------------------------------------------------------------------------

#define WASSERT(x)                                                             \
    do {                                                                       \
        if (!(x)) {                                                            \
            call_king_terry("assert: %s", #x);                                 \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

static void war_wayland_registry_global(void* data,
                                        struct wl_registry* registry,
                                        uint32_t name,
                                        const char* interface,
                                        uint32_t version) {
    war_wayland_context* ctx_wayland = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0)
        ctx_wayland->compositor = wl_registry_bind(registry,
                                                   name,
                                                   &wl_compositor_interface,
                                                   version < 6 ? version : 6);
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
        ctx_wayland->xdg_wm_base = wl_registry_bind(
            registry, name, &xdg_wm_base_interface, version < 6 ? version : 6);
    else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0)
        ctx_wayland->dmabuf = wl_registry_bind(registry,
                                               name,
                                               &zwp_linux_dmabuf_v1_interface,
                                               version < 4 ? version : 4);
    else if (strcmp(interface, wl_seat_interface.name) == 0)
        ctx_wayland->seat = wl_registry_bind(
            registry, name, &wl_seat_interface, version < 8 ? version : 8);
    else if (strcmp(interface, wl_output_interface.name) == 0)
        ctx_wayland->output = wl_registry_bind(
            registry, name, &wl_output_interface, version < 4 ? version : 4);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        ctx_wayland->shm =
            wl_registry_bind(registry, name, &wl_shm_interface, 1);
}
static void war_wayland_registry_global_remove(void* data,
                                               struct wl_registry* registry,
                                               uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}
static const struct wl_registry_listener war_wayland_registry_listener = {
    .global = war_wayland_registry_global,
    .global_remove = war_wayland_registry_global_remove,
};
static void war_xdg_surface_configure(void* data,
                                      struct xdg_surface* xdg_surface,
                                      uint32_t serial) {
    war_wayland_context* ctx_wayland = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    ctx_wayland->configured = 1;
}
static const struct xdg_surface_listener war_xdg_surface_listener = {
    .configure = war_xdg_surface_configure,
};
static void war_xdg_toplevel_configure(void* data,
                                       struct xdg_toplevel* toplevel,
                                       int32_t width,
                                       int32_t height,
                                       struct wl_array* states) {
    war_wayland_context* ctx_wayland = data;
    (void)toplevel;
    (void)states;
    if (width > 0) ctx_wayland->width = (uint32_t)width;
    if (height > 0) ctx_wayland->height = (uint32_t)height;
    if (ctx_wayland->env && ctx_wayland->env->ctx_cursor) {
        double cw = ctx_wayland->env->ctx_cursor->cell_width;
        double ch = ctx_wayland->env->ctx_cursor->cell_height;
        if (cw > 0 && ch > 0) {
            ctx_wayland->num_cols =
                (uint32_t)((double)width / cw) - ctx_wayland->gutter_cols;
            ctx_wayland->num_rows =
                (uint32_t)((double)height / ch) - ctx_wayland->gutter_rows;
        }
    }
}
static void war_xdg_toplevel_configure_bounds(void* data,
                                              struct xdg_toplevel* toplevel,
                                              int32_t width,
                                              int32_t height) {
    (void)data;
    (void)toplevel;
    (void)width;
    (void)height;
}
static void war_xdg_toplevel_wm_capabilities(void* data,
                                             struct xdg_toplevel* toplevel,
                                             struct wl_array* capabilities) {
    (void)data;
    (void)toplevel;
    (void)capabilities;
}
static void war_xdg_toplevel_close(void* data, struct xdg_toplevel* toplevel) {
    war_wayland_context* ctx_wayland = data;
    (void)toplevel;
    ctx_wayland->running = 0;
}
static const struct xdg_toplevel_listener war_xdg_toplevel_listener = {
    .configure = war_xdg_toplevel_configure,
    .configure_bounds = war_xdg_toplevel_configure_bounds,
    .wm_capabilities = war_xdg_toplevel_wm_capabilities,
    .close = war_xdg_toplevel_close,
};
static void
war_frame_done(void* data, struct wl_callback* callback, uint32_t time);
static const struct wl_callback_listener war_frame_listener = {
    .done = war_frame_done,
};
static void
war_frame_done(void* data, struct wl_callback* callback, uint32_t time) {
    war_wayland_context* ctx_wayland = data;
    wl_callback_destroy(callback);
    // advance playback bar
    war_env* env = ctx_wayland->env;
    if (env->play_bar_playing) {
        if (env->play_bar_last_frame_ms != 0) {
            uint32_t delta_ms = time - env->play_bar_last_frame_ms;
            env->play_bar_position_seconds += (double)delta_ms / 1000.0;
        }
        env->play_bar_last_frame_ms = time;
        double bpm = env->atomics->bpm;
        if (bpm <= 0.0) bpm = 100.0;
        double seconds_per_cell = 60.0 / bpm;
        double current_cell_pos =
            (double)ctx_wayland->gutter_cols +
            env->play_bar_position_seconds / seconds_per_cell;
        // scan notes between previous and current playhead position
        if (env->ctx_note && env->play_bar_last_frame_ms != 0) {
            uint32_t ncount = env->ctx_note->instance_count;
            for (uint32_t i = 0; i < ncount; i++) {
                double ns = env->ctx_note->instance[i].pos[0];
                if (ns >= env->play_bar_prev_cell_pos &&
                    ns < current_cell_pos) {
                    uint32_t pitch =
                        (uint32_t)env->ctx_note->instance[i].pos[1];
                    if (pitch > 127) pitch = 127;
                    for (uint32_t l = 1; l <= 9; l++) {
                        uint32_t idx =
                            pitch * WAR_CAPTURE_SLOT_LAYERS + (l - 1);
                        war_capture_slot* slot = &env->capture_slots[idx];
                        if (slot->samples && slot->count > 0) {
                            env->play_bar_preview_note = pitch;
                            env->play_bar_preview_layer = l;
                            env->play_bar_preview_read_pos = 0;
                            env->play_bar_preview_active = 1;
                            break;
                        }
                    }
                }
            }
        }
        env->play_bar_prev_cell_pos = current_cell_pos;
        env->ctx_line->instance[0].pos[0] = (float)current_cell_pos;
    }
    if (ctx_wayland->rendering) {
        war_render_frame(ctx_wayland, ctx_wayland->vk);
    }
    ctx_wayland->frame_callback = wl_surface_frame(ctx_wayland->surface);
    wl_callback_add_listener(
        ctx_wayland->frame_callback, &war_frame_listener, ctx_wayland);
    wl_surface_attach(ctx_wayland->surface, ctx_wayland->buffer, 0, 0);
    wl_surface_damage_buffer(ctx_wayland->surface,
                             0,
                             0,
                             (int32_t)ctx_wayland->width,
                             (int32_t)ctx_wayland->height);
    wl_surface_commit(ctx_wayland->surface);
}

static void war_keyboard_keymap(void* data,
                                struct wl_keyboard* keyboard,
                                uint32_t format,
                                int fd,
                                uint32_t size);
static void war_keyboard_enter(void* data,
                               struct wl_keyboard* keyboard,
                               uint32_t serial,
                               struct wl_surface* surface,
                               struct wl_array* keys);
static void war_keyboard_leave(void* data,
                               struct wl_keyboard* keyboard,
                               uint32_t serial,
                               struct wl_surface* surface);
static void war_keyboard_key(void* data,
                             struct wl_keyboard* keyboard,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t key,
                             uint32_t state);
static void war_keyboard_modifiers(void* data,
                                   struct wl_keyboard* keyboard,
                                   uint32_t serial,
                                   uint32_t mods_depressed,
                                   uint32_t mods_latched,
                                   uint32_t mods_grp,
                                   uint32_t mods_locked);
static void war_keyboard_repeat_info(void* data,
                                     struct wl_keyboard* keyboard,
                                     int32_t rate,
                                     int32_t delay);
static const struct wl_keyboard_listener war_keyboard_listener = {
    .keymap = war_keyboard_keymap,
    .enter = war_keyboard_enter,
    .leave = war_keyboard_leave,
    .key = war_keyboard_key,
    .modifiers = war_keyboard_modifiers,
    .repeat_info = war_keyboard_repeat_info,
};
static void war_keyboard_keymap(void* data,
                                struct wl_keyboard* keyboard,
                                uint32_t format,
                                int fd,
                                uint32_t size) {
    war_wayland_context* ctx_wayland = data;
    (void)keyboard;
    (void)format;
    char* map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    WASSERT(map_str != MAP_FAILED);
    ctx_wayland->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    ctx_wayland->xkb_keymap =
        xkb_keymap_new_from_string(ctx_wayland->xkb_ctx,
                                   map_str,
                                   XKB_KEYMAP_FORMAT_TEXT_V1,
                                   XKB_KEYMAP_COMPILE_NO_FLAGS);
    ctx_wayland->xkb_state = xkb_state_new(ctx_wayland->xkb_keymap);
    munmap(map_str, size);
    close(fd);
}
static void war_keyboard_enter(void* data,
                               struct wl_keyboard* keyboard,
                               uint32_t serial,
                               struct wl_surface* surface,
                               struct wl_array* keys) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
    (void)keys;
}
static void war_keyboard_leave(void* data,
                               struct wl_keyboard* keyboard,
                               uint32_t serial,
                               struct wl_surface* surface) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
}
static void war_keyboard_key(void* data,
                             struct wl_keyboard* keyboard,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t key,
                             uint32_t state) {
    war_wayland_context* ctx_wayland = data;
    (void)keyboard;
    (void)serial;
    (void)time;
    if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (key == ctx_wayland->repeat_key) {
            ctx_wayland->repeat_active = 0;
            struct itimerspec off = {0};
            timerfd_settime(ctx_wayland->repeat_timer_fd, 0, &off, NULL);
        }
        return;
    }
    xkb_keysym_t raw_sym =
        xkb_state_key_get_one_sym(ctx_wayland->xkb_state, key + 8);
    uint32_t keysym = war_normalize_keysym(raw_sym);
    if (keysym == XKB_KEY_NoSymbol) return;

    war_env* env = ctx_wayland->env;
    war_cursor_context* cur = env->ctx_cursor;
    war_keymap_context* keymap = env->ctx_keymap;
    war_config_context* config = env->ctx_config;

    uint32_t mode = WAR_MODE_ID_ROLL;
    // check timeout for pending prefix state (500ms)
    if (ctx_wayland->keymap_state &&
        time - ctx_wayland->keymap_state_time > 500)
        ctx_wayland->keymap_state = 0;
    uint32_t mod = 0;
    {
        xkb_mod_index_t mi;
        mi = xkb_keymap_mod_get_index(ctx_wayland->xkb_keymap,
                                      XKB_MOD_NAME_SHIFT);
        if (mi != XKB_MOD_INVALID &&
            xkb_state_mod_index_is_active(
                ctx_wayland->xkb_state, mi, XKB_STATE_MODS_DEPRESSED))
            mod |= MOD_SHIFT;
        mi = xkb_keymap_mod_get_index(ctx_wayland->xkb_keymap,
                                      XKB_MOD_NAME_CTRL);
        if (mi != XKB_MOD_INVALID &&
            xkb_state_mod_index_is_active(
                ctx_wayland->xkb_state, mi, XKB_STATE_MODS_DEPRESSED))
            mod |= MOD_CTRL;
        mi =
            xkb_keymap_mod_get_index(ctx_wayland->xkb_keymap, XKB_MOD_NAME_ALT);
        if (mi != XKB_MOD_INVALID &&
            xkb_state_mod_index_is_active(
                ctx_wayland->xkb_state, mi, XKB_STATE_MODS_DEPRESSED))
            mod |= MOD_ALT;
        mi = xkb_keymap_mod_get_index(ctx_wayland->xkb_keymap,
                                      XKB_MOD_NAME_LOGO);
        if (mi != XKB_MOD_INVALID &&
            xkb_state_mod_index_is_active(
                ctx_wayland->xkb_state, mi, XKB_STATE_MODS_DEPRESSED))
            mod |= MOD_LOGO;
    }

    if (ctx_wayland->repeat_rate > 0) {
        ctx_wayland->repeat_key = key;
        ctx_wayland->repeat_sym = keysym;
        ctx_wayland->repeat_mod = mod;
        ctx_wayland->repeat_active = 1;
        int32_t d = ctx_wayland->repeat_delay;
        int32_t r = ctx_wayland->repeat_rate;
        struct itimerspec its = {
            .it_value = {d / 1000, (long)(d % 1000) * 1000000L},
            .it_interval = {0, r > 0 ? 1000000000L / r : 0},
        };
        timerfd_settime(ctx_wayland->repeat_timer_fd, 0, &its, NULL);
    }

    int is_digit = (mod == 0 && keysym >= XKB_KEY_0 && keysym <= XKB_KEY_9);
    if (is_digit) {
        cur->prefix = cur->prefix * 10 + (uint32_t)(keysym - XKB_KEY_0);
    }

    // command mode handling
    if (env->cmd_active) {
        if (raw_sym == XKB_KEY_Escape) {
            env->cmd_active = 0;
            env->cmd_len = 0;
            cur->prefix = 0;
            return;
        }
        if (raw_sym == XKB_KEY_Return || raw_sym == XKB_KEY_KP_Enter) {
            if (env->cmd_len == 2 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'w')
                fprintf(stderr, "hello world\n");
            env->cmd_active = 0;
            env->cmd_len = 0;
            cur->prefix = 0;
            return;
        }
        if (raw_sym == XKB_KEY_BackSpace) {
            if (env->cmd_len > 1) env->cmd_len--;
            cur->prefix = 0;
            return;
        }
        // printable ASCII via utf8 from raw sym
        char utf8[8] = {0};
        int n = xkb_keysym_to_utf8(raw_sym, utf8, sizeof(utf8));
        if (n > 1 && utf8[0] >= 32 && utf8[0] <= 126) {
            if (env->cmd_len < 255)
                env->cmd_buf[env->cmd_len++] = utf8[0];
        }
        cur->prefix = 0;
        return;
    }

    // enter command mode on ':' (check raw sym before normalizer maps it to ';')
    if (raw_sym == XKB_KEY_colon) {
        env->cmd_active = 1;
        env->cmd_len = 1;
        env->cmd_buf[0] = ':';
        cur->prefix = 0;
        // suppress repeat — command mode handles text separately
        ctx_wayland->repeat_active = 0;
        struct itimerspec off = {0};
        timerfd_settime(ctx_wayland->repeat_timer_fd, 0, &off, NULL);
        return;
    }

    uint64_t next = 0;
    // try from stored prefix state first (for multi-key sequences like gg)
    if (ctx_wayland->keymap_state) {
        size_t prefix_idx =
            mode * (size_t)config->KEYMAP_STATE_CAPACITY *
                config->KEYMAP_KEYSYM_CAPACITY * config->KEYMAP_MOD_CAPACITY +
            ctx_wayland->keymap_state * (size_t)config->KEYMAP_KEYSYM_CAPACITY *
                config->KEYMAP_MOD_CAPACITY +
            (size_t)keysym * config->KEYMAP_MOD_CAPACITY + mod;
        next = keymap->next_state[prefix_idx];
    }
    // if no transition from prefix state, fall back to root
    size_t trans_idx;
    if (!next) {
        ctx_wayland->keymap_state = 0;
        trans_idx = mode * (size_t)config->KEYMAP_STATE_CAPACITY *
                        config->KEYMAP_KEYSYM_CAPACITY *
                        config->KEYMAP_MOD_CAPACITY +
                    0 * (size_t)config->KEYMAP_KEYSYM_CAPACITY *
                        config->KEYMAP_MOD_CAPACITY +
                    (size_t)keysym * config->KEYMAP_MOD_CAPACITY + mod;
        next = keymap->next_state[trans_idx];
    }
    if (!next) {
        if (!is_digit) cur->prefix = 0;
        return;
    }

    size_t func_count_idx = mode * (size_t)config->KEYMAP_STATE_CAPACITY + next;
    uint8_t count = keymap->function_count[func_count_idx];
    if (!count) {
        // prefix state — store for next keypress
        // flag is on the SOURCE state (the state the transition leaves from)
        uint64_t src_state =
            ctx_wayland->keymap_state ? ctx_wayland->keymap_state : 0;
        size_t flag_idx =
            mode * (size_t)config->KEYMAP_STATE_CAPACITY + src_state;
        if (keymap->flags[flag_idx] & WAR_KEYMAP_PREFIX) {
            ctx_wayland->keymap_state = next;
            ctx_wayland->keymap_state_time = time;
        }
        if (!is_digit) cur->prefix = 0;
        return;
    }

    uint32_t repeat;
    if (is_digit) {
        repeat = 1;
    } else {
        repeat = cur->prefix;
        if (repeat == 0) repeat = 1;
    }
    size_t flag_idx = mode * (size_t)config->KEYMAP_STATE_CAPACITY + next;
    if (keymap->flags[flag_idx] & WAR_KEYMAP_UNIQUE_PREFIX) repeat = 1;
    size_t func_base = mode * (size_t)config->KEYMAP_STATE_CAPACITY *
                           config->KEYMAP_FUNCTION_CAPACITY +
                       next * (size_t)config->KEYMAP_FUNCTION_CAPACITY;
    for (uint32_t r = 0; r < repeat; r++) {
        for (uint8_t i = 0; i < count; i++) {
            void (*fn)(war_env*) = keymap->function[func_base + i];
            if (fn) fn(env);
        }
    }
    ctx_wayland->keymap_state = 0;
    if (!is_digit) cur->prefix = 0;
}
static void war_keyboard_modifiers(void* data,
                                   struct wl_keyboard* keyboard,
                                   uint32_t serial,
                                   uint32_t mods_depressed,
                                   uint32_t mods_latched,
                                   uint32_t mods_grp,
                                   uint32_t mods_locked) {
    war_wayland_context* ctx_wayland = data;
    (void)keyboard;
    (void)serial;
    xkb_state_update_mask(ctx_wayland->xkb_state,
                          mods_depressed,
                          mods_latched,
                          mods_grp,
                          0,
                          0,
                          mods_locked);
}
static void war_keyboard_repeat_info(void* data,
                                     struct wl_keyboard* keyboard,
                                     int32_t rate,
                                     int32_t delay) {
    war_wayland_context* ctx_wayland = data;
    (void)keyboard;
    (void)rate;
    (void)delay;
    // client-side repeat uses hardcoded defaults in bootstrap
}

//---------------------------------------------------------------------------
// PIPEWIRE AUDIO THREAD
//---------------------------------------------------------------------------
// Capture process callback: audio thread reads captured mic data,
// writes it into pc_capture ring buffer (audio→main direction).
static int capture_cb_count = 0;
static void on_pw_capture_process(void* userdata) {
    war_env* env = (war_env*)userdata;
    struct pw_buffer* b;
    if (!(b = pw_stream_dequeue_buffer(env->ctx_pw->capture_stream))) return;
    struct spa_buffer* spa = b->buffer;
    void* src = spa->datas[0].data;
    uint32_t n_bytes = spa->datas[0].chunk->size;
    if (src && n_bytes > 0) {
        if (++capture_cb_count <= 5)
            call_king_terry("CAPTURE: %u bytes from mic", n_bytes);
        war_pc_to_wr(env->pc_capture, 0, n_bytes, src);
        env->atomics->capture_frames++;
    }
    pw_stream_queue_buffer(env->ctx_pw->capture_stream, b);
}

static int loopback_cb_count = 0;
static void on_pw_loopback_process(void* userdata) {
    war_env* env = (war_env*)userdata;
    struct pw_buffer* b;
    if (!(b = pw_stream_dequeue_buffer(env->ctx_pw->loopback_capture_stream)))
        return;
    struct spa_buffer* spa = b->buffer;
    void* src = spa->datas[0].data;
    uint32_t n_bytes = spa->datas[0].chunk->size;
    if (src && n_bytes > 0 && env->atomics->capture_loopback) {
        if (++loopback_cb_count <= 5)
            call_king_terry("LOOPBACK: %u bytes from sink", n_bytes);
        war_pc_to_wr(env->pc_loopback, 0, n_bytes, src);
        env->atomics->loopback_frames++;
    }
    pw_stream_queue_buffer(env->ctx_pw->loopback_capture_stream, b);
}

static void on_pw_play_process(void* userdata) {
    war_env* env = (war_env*)userdata;
    struct pw_buffer* b;
    if (!(b = pw_stream_dequeue_buffer(env->ctx_pw->play_stream))) return;
    struct spa_buffer* spa = b->buffer;
    void* dst = spa->datas[0].data;
    uint32_t max = spa->datas[0].maxsize;
    uint32_t written = 0;
    uint32_t hdr, sz;
    while (written < max &&
           war_pc_from_wr(env->pc_play, &hdr, &sz, (uint8_t*)dst + written) &&
           sz <= max - written) {
        written += sz;
    }
    if (written > 0) {
        spa->datas[0].chunk->size = written;
        spa->datas[0].chunk->stride = 8;
    } else {
        memset(dst, 0, max);
        spa->datas[0].chunk->size = max;
        spa->datas[0].chunk->stride = 8;
    }
    pw_stream_queue_buffer(env->ctx_pw->play_stream, b);
}

// Dedicated audio thread: runs Pipewire main loop at SCHED_FIFO.
// Communicates with main thread only via lock-free ring buffers + atomics.
void* war_pipewire(void* args) {
    war_env* env = (war_env*)args;
    // attempt real-time scheduling
    struct sched_param sp = {.sched_priority = 80};
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);

    pw_init(NULL, NULL);

    struct pw_main_loop* loop = pw_main_loop_new(NULL);
    env->ctx_pw->main_loop = loop; // so main thread can pw_main_loop_quit

    struct pw_context* ctx =
        pw_context_new(pw_main_loop_get_loop(loop), NULL, 0);
    struct pw_core* core = pw_context_connect(ctx, NULL, 0);
    WASSERT(core);

    // spa_hook listeners AND event vtables must live for entire thread lifetime
    struct spa_hook capture_listener = {0};
    struct spa_hook play_listener = {0};
    struct spa_hook loopback_listener = {0};
    struct pw_stream_events capture_events = {0};
    struct pw_stream_events play_events = {0};
    struct pw_stream_events loopback_events = {0};

    // --- capture stream (mic in) ---
    {
        struct pw_properties* props =
            pw_properties_new("media.name", "war-capture", NULL);
        env->ctx_pw->capture_stream = pw_stream_new(core, "war-capture", props);
        struct spa_audio_info_raw capture_info = {
            .format = SPA_AUDIO_FORMAT_F32,
            .rate = 48000,
            .channels = 1,
        };
        uint8_t buf[1024];
        struct spa_pod_builder bld = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
        const struct spa_pod* params[1];
        params[0] = spa_format_audio_raw_build(
            &bld, SPA_PARAM_EnumFormat, &capture_info);
        capture_events.version = PW_VERSION_STREAM_EVENTS;
        capture_events.process = on_pw_capture_process;
        pw_stream_add_listener(env->ctx_pw->capture_stream,
                               &capture_listener,
                               &capture_events,
                               env);
        WASSERT(pw_stream_connect(env->ctx_pw->capture_stream,
                                  PW_DIRECTION_INPUT,
                                  PW_ID_ANY,
                                  PW_STREAM_FLAG_AUTOCONNECT |
                                      PW_STREAM_FLAG_MAP_BUFFERS,
                                  params,
                                  1) == 0);
        call_king_terry("Pipewire: capture stream connected");
    }

    // --- play stream (speakers out) ---
    {
        struct pw_properties* props =
            pw_properties_new("media.name", "war-play", NULL);
        env->ctx_pw->play_stream = pw_stream_new(core, "war-play", props);
        // format: F32, 48000, stereo
        struct spa_audio_info_raw play_info = {
            .format = SPA_AUDIO_FORMAT_F32,
            .rate = 48000,
            .channels = 2,
            .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR},
        };
        uint8_t buf[1024];
        struct spa_pod_builder bld = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
        const struct spa_pod* params[1];
        params[0] =
            spa_format_audio_raw_build(&bld, SPA_PARAM_EnumFormat, &play_info);
        play_events.version = PW_VERSION_STREAM_EVENTS;
        play_events.process = on_pw_play_process;
        pw_stream_add_listener(
            env->ctx_pw->play_stream, &play_listener, &play_events, env);
        WASSERT(pw_stream_connect(env->ctx_pw->play_stream,
                                  PW_DIRECTION_OUTPUT,
                                  PW_ID_ANY,
                                  PW_STREAM_FLAG_AUTOCONNECT |
                                      PW_STREAM_FLAG_MAP_BUFFERS,
                                  params,
                                  1) == 0);
        call_king_terry("Pipewire: play stream connected");
    }

    // --- loopback capture stream (sink monitor → ring buffer) ---
    {
        struct pw_properties* props =
            pw_properties_new(PW_KEY_STREAM_CAPTURE_SINK,
                              "true",
                              "media.name",
                              "war-loopback",
                              NULL);
        env->ctx_pw->loopback_capture_stream =
            pw_stream_new(core, "war-loopback", props);
        // format: F32, 48000, stereo (matches whatever is playing through
        // speakers)
        struct spa_audio_info_raw loopback_info = {
            .format = SPA_AUDIO_FORMAT_F32,
            .rate = 48000,
            .channels = 2,
            .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR},
        };
        uint8_t buf[1024];
        struct spa_pod_builder bld = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
        const struct spa_pod* params[1];
        params[0] = spa_format_audio_raw_build(
            &bld, SPA_PARAM_EnumFormat, &loopback_info);
        loopback_events.version = PW_VERSION_STREAM_EVENTS;
        loopback_events.process = on_pw_loopback_process;
        pw_stream_add_listener(env->ctx_pw->loopback_capture_stream,
                               &loopback_listener,
                               &loopback_events,
                               env);
        WASSERT(pw_stream_connect(env->ctx_pw->loopback_capture_stream,
                                  PW_DIRECTION_INPUT,
                                  PW_ID_ANY,
                                  PW_STREAM_FLAG_AUTOCONNECT |
                                      PW_STREAM_FLAG_MAP_BUFFERS,
                                  params,
                                  1) == 0);
        call_king_terry("Pipewire: loopback stream connected");
        env->atomics->capture_loopback = 1; // enabled by default
    }

    // blocks until pw_main_loop_quit is called
    pw_main_loop_run(loop);

    // cleanup (only reached after quit signal)
    pw_stream_destroy(env->ctx_pw->capture_stream);
    env->ctx_pw->capture_stream = NULL;
    pw_stream_destroy(env->ctx_pw->loopback_capture_stream);
    env->ctx_pw->loopback_capture_stream = NULL;
    pw_stream_destroy(env->ctx_pw->play_stream);
    env->ctx_pw->play_stream = NULL;
    pw_context_destroy(ctx);
    pw_main_loop_destroy(loop);
    env->ctx_pw->main_loop = NULL;
    pw_deinit();
    return NULL;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    CALL_KING_TERRY("war");
    //--------------------------------------------------------------------
    // KEY CHECK
    //--------------------------------------------------------------------
    war_key();
    //-------------------------------------------------------------------------
    // BOOTSTRAP
    //-------------------------------------------------------------------------
    war_config_context* tmp_ctx_config = calloc(1, sizeof(war_config_context));
    war_config_default(tmp_ctx_config);
    war_pool_context* tmp_ctx_pool = calloc(1, sizeof(war_pool_context));
    tmp_ctx_pool->size =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint64_t));
    tmp_ctx_pool->offset =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint64_t));
    tmp_ctx_pool->alignment =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint32_t));
    tmp_ctx_pool->id =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(war_pool_id));
    war_pool_default(tmp_ctx_pool, tmp_ctx_config);
    for (uint32_t i = 0; i < tmp_ctx_pool->count; i++) {
        tmp_ctx_pool->total_size += tmp_ctx_pool->size[i];
    }
    tmp_ctx_pool->pool = mmap(NULL,
                              tmp_ctx_pool->total_size,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS,
                              -1,
                              0);
    if (tmp_ctx_pool->pool == MAP_FAILED) {
        call_king_terry("tmp pool map failed, total_size: %llu",
                        (unsigned long long)tmp_ctx_pool->total_size);
        exit(1);
    }
    memset(tmp_ctx_pool->pool, 0, tmp_ctx_pool->total_size);
    war_hot_context* tmp_ctx_hot =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT);
    tmp_ctx_hot->function =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FUNCTION);
    tmp_ctx_hot->handle =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT_HANDLE);
    tmp_ctx_hot->fn_id =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FN_ID);
    tmp_ctx_hot->name =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT_NAME);
    tmp_ctx_hot->name[WAR_HOT_ID_CONFIG] = "war_config_override";
    tmp_ctx_hot->name[WAR_HOT_ID_COMMAND] = "war_command_override";
    tmp_ctx_hot->name[WAR_HOT_ID_COLOR] = "war_color_override";
    tmp_ctx_hot->name[WAR_HOT_ID_PLUGIN] = "war_plugin_override";
    tmp_ctx_hot->name[WAR_HOT_ID_POOL] = "war_pool_override";
    tmp_ctx_hot->name[WAR_HOT_ID_KEYMAP] = "war_keymap_override";
    war_env* tmp_env = war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_ENV);
    tmp_env->ctx_config = tmp_ctx_config;
    tmp_env->ctx_hot = tmp_ctx_hot;
    war_mkdir(tmp_ctx_config->DIR_CONFIG, 0755);
    war_mkdir(tmp_ctx_config->DIR_CACHE, 0755);
    war_mkdir(tmp_ctx_config->DIR_OVERRIDE, 0755);
    war_mkdir(tmp_ctx_config->DIR_UNDO, 0755);
    war_mkdir(tmp_ctx_config->DIR_WARPOON, 0755);
    war_mkdir(tmp_ctx_config->DIR_JUMPLIST, 0755);
    war_mkdir(tmp_ctx_config->DIR_LOG, 0755);
    tmp_ctx_hot->fn_id[0] = WAR_HOT_ID_CONFIG;
    tmp_ctx_hot->fn_count = 1;
    war_override(tmp_ctx_hot->fn_count, tmp_ctx_hot->fn_id, tmp_env);
    war_pool_context* ctx_pool = calloc(1, sizeof(war_pool_context));
    ctx_pool->size =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint64_t));
    ctx_pool->offset =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint64_t));
    ctx_pool->alignment =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint32_t));
    ctx_pool->id =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(war_pool_id));
    war_pool_default(ctx_pool, tmp_ctx_config);
    tmp_ctx_hot->fn_id[0] = WAR_HOT_ID_POOL;
    tmp_ctx_hot->fn_count = 1;
    tmp_env->ctx_pool = ctx_pool;
    war_override(tmp_ctx_hot->fn_count, tmp_ctx_hot->fn_id, tmp_env);
    for (uint32_t i = 0; i < ctx_pool->count; i++) {
        ctx_pool->total_size += ctx_pool->size[i];
    }
    ctx_pool->pool = mmap(NULL,
                          ctx_pool->total_size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          0);
    if (ctx_pool->pool == MAP_FAILED) {
        call_king_terry("pool map failed, total_size: %llu",
                        (unsigned long long)ctx_pool->total_size);
        exit(1);
    }
    memset(ctx_pool->pool, 0, ctx_pool->total_size);
    war_config_context* ctx_config =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_CONFIG_CONTEXT);
    memcpy((uint8_t*)ctx_config,
           (uint8_t*)tmp_ctx_config,
           war_pool_size(ctx_pool, WAR_POOL_ID_CONFIG_CONTEXT));
    war_hot_context* ctx_hot =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT);
    ctx_hot->function =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FUNCTION);
    ctx_hot->handle =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_HANDLE);
    ctx_hot->fn_id =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FN_ID);
    ctx_hot->name = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_NAME);
    memcpy((uint8_t*)ctx_hot->function,
           (uint8_t*)tmp_ctx_hot->function,
           war_pool_size(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FUNCTION));
    memcpy((uint8_t*)ctx_hot->handle,
           (uint8_t*)tmp_ctx_hot->handle,
           war_pool_size(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_HANDLE));
    memcpy((uint8_t*)ctx_hot->name,
           (uint8_t*)tmp_ctx_hot->name,
           war_pool_size(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_NAME));
    free(tmp_ctx_config);
    tmp_ctx_config = NULL;
    tmp_env = NULL;
    free(tmp_ctx_pool->size);
    free(tmp_ctx_pool->offset);
    free(tmp_ctx_pool->alignment);
    free(tmp_ctx_pool->id);
    if (munmap(tmp_ctx_pool->pool, tmp_ctx_pool->total_size) == -1) {
        call_king_terry("munmap failed");
    }
    free(tmp_ctx_pool);
    tmp_ctx_pool = NULL;
    //------------------------------------------------------------------------
    // INIT
    //------------------------------------------------------------------------
    war_color_context* ctx_color =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COLOR_CONTEXT);
    war_color_default(ctx_color);
    war_keymap_context* ctx_keymap =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT);
    ctx_keymap->function_id =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_FUNCTION_ID);
    ctx_keymap->function =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_FUNCTION);
    ctx_keymap->function_count =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_FUNCTION_COUNT);
    ctx_keymap->flags =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_FLAGS);
    ctx_keymap->next_state =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_NEXT_STATE);
    ctx_keymap->state_count =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_STATE_COUNT);
    for (uint32_t i = 0; i < ctx_config->KEYMAP_MODE_CAPACITY; i++) {
        ctx_keymap->state_count[i] = 1;
    }
    war_keymap_default(ctx_keymap, ctx_config);
    war_command_context* ctx_command =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT);
    ctx_command->function_id =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_FUNCTION_ID);
    ctx_command->function =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_FUNCTION);
    ctx_command->function_count = war_pool_alloc_new(
        ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_FUNCTION_COUNT);
    ctx_command->flags =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_FLAGS);
    ctx_command->next_state =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_NEXT_STATE);
    ctx_command->state_count = 1;
    war_command_default(ctx_command, ctx_config);
    war_hook_context* ctx_hook =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOOK_CONTEXT);
    ctx_hook->mode_flags =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOOK_CONTEXT_MODE_FLAGS);
    ctx_hook->event_flags =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOOK_CONTEXT_EVENT_FLAGS);
    ctx_hook->function =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOOK_CONTEXT_FUNCTION);
    war_env* env = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_ENV);
    env->ctx_color = ctx_color;
    env->ctx_keymap = ctx_keymap;
    env->ctx_command = ctx_command;
    env->ctx_hook = ctx_hook;
    env->ctx_pool = ctx_pool;
    env->ctx_config = ctx_config;
    env->ctx_hot = ctx_hot;
    env->cmd_active = 0;
    env->cmd_len = 0;
    ctx_hot->fn_id[0] = WAR_HOT_ID_COLOR;
    ctx_hot->fn_id[1] = WAR_HOT_ID_KEYMAP;
    ctx_hot->fn_id[2] = WAR_HOT_ID_COMMAND;
    ctx_hot->fn_id[3] = WAR_HOT_ID_PLUGIN;
    ctx_hot->fn_count = 4;
    war_override(ctx_hot->fn_count, ctx_hot->fn_id, env);
    war_wayland_context* ctx_wayland =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_WAYLAND_CONTEXT);
    ctx_wayland->env = env;
    env->ctx_wayland = ctx_wayland;
    war_vulkan_context* ctx_vk =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_VULKAN);
    ctx_wayland->vk = ctx_vk;
    //-------------------------------------------------------------------------
    // WAYLAND SETUP
    //-------------------------------------------------------------------------
    ctx_wayland->width = 1920;
    ctx_wayland->height = 1080;
    ctx_wayland->display = wl_display_connect_to_fd(war_wayland_make_fd());
    WASSERT(ctx_wayland->display);
    ctx_wayland->registry = wl_display_get_registry(ctx_wayland->display);
    WASSERT(ctx_wayland->registry);
    wl_registry_add_listener(
        ctx_wayland->registry, &war_wayland_registry_listener, ctx_wayland);
    wl_display_roundtrip(ctx_wayland->display);
    WASSERT(ctx_wayland->compositor && ctx_wayland->xdg_wm_base &&
            ctx_wayland->dmabuf && ctx_wayland->shm);
    ctx_wayland->surface =
        wl_compositor_create_surface(ctx_wayland->compositor);
    ctx_wayland->xdg_surface = xdg_wm_base_get_xdg_surface(
        ctx_wayland->xdg_wm_base, ctx_wayland->surface);
    ctx_wayland->toplevel = xdg_surface_get_toplevel(ctx_wayland->xdg_surface);
    WASSERT(ctx_wayland->surface && ctx_wayland->xdg_surface &&
            ctx_wayland->toplevel);
    xdg_toplevel_add_listener(
        ctx_wayland->toplevel, &war_xdg_toplevel_listener, ctx_wayland);
    xdg_toplevel_set_title(ctx_wayland->toplevel, "war");
    xdg_surface_add_listener(
        ctx_wayland->xdg_surface, &war_xdg_surface_listener, ctx_wayland);
    wl_surface_commit(ctx_wayland->surface);
    wl_display_roundtrip(ctx_wayland->display);
    WASSERT(ctx_wayland->configured);
    ctx_wayland->keyboard = wl_seat_get_keyboard(ctx_wayland->seat);
    wl_keyboard_add_listener(
        ctx_wayland->keyboard, &war_keyboard_listener, ctx_wayland);
    ctx_wayland->running = 1;
    ctx_wayland->rendering = 1;
    ctx_wayland->zoom = 2.0f;
    ctx_wayland->initial_zoom = ctx_wayland->zoom;
    ctx_wayland->panning[0] = 0;
    ctx_wayland->panning[1] = 0;
    ctx_wayland->right_bound = 1000000.0;
    ctx_wayland->top_bound = 127.0;
    ctx_wayland->keymap_state = 0;
    ctx_wayland->keymap_state_time = 0;
    ctx_wayland->repeat_active = 0;
    ctx_wayland->repeat_rate = 20;
    ctx_wayland->repeat_delay = 200;
    ctx_wayland->repeat_timer_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    WASSERT(ctx_wayland->repeat_timer_fd >= 0);

    //-------------------------------------------------------------------------
    // VULKAN + DMABUF SETUP
    //-------------------------------------------------------------------------
    war_vulkan_init(ctx_wayland, ctx_vk);

    struct zwp_linux_buffer_params_v1* params =
        zwp_linux_dmabuf_v1_create_params(ctx_wayland->dmabuf);
    zwp_linux_buffer_params_v1_add(params,
                                   ctx_vk->dmabuf_fd,
                                   0,
                                   (uint32_t)ctx_vk->img_layout.offset,
                                   (uint32_t)ctx_vk->img_layout.rowPitch,
                                   0,
                                   0);
    ctx_wayland->buffer =
        zwp_linux_buffer_params_v1_create_immed(params,
                                                ctx_wayland->width,
                                                ctx_wayland->height,
                                                DRM_FORMAT_ARGB8888,
                                                0);
    //-------------------------------------------------------------------------
    // FIRST FRAME RENDER SETUP (render pass, framebuffer)
    //-------------------------------------------------------------------------
    war_render_init(ctx_wayland, ctx_vk);

    //-------------------------------------------------------------------------
    // CURSOR INIT (needs render pass to exist)
    //-------------------------------------------------------------------------
    war_cursor_context* ctx_cursor =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR);
    env->ctx_cursor = ctx_cursor;
    // init freetype and derive cell size from FreeMono.otf
    if (FT_Init_FreeType(&env->ft_lib)) {
        call_king_terry("freetype init failed");
    }
    if (FT_New_Face(env->ft_lib, "assets/fonts/FreeMono.otf", 0, &env->ft_face)) {
        call_king_terry("freetype: failed to load FreeMono.otf");
    }
    FT_Set_Pixel_Sizes(env->ft_face, 0, 24);
    FT_Load_Char(env->ft_face, '*', FT_LOAD_DEFAULT);
    ctx_cursor->cell_width = (double)(env->ft_face->glyph->advance.x >> 6);
    ctx_cursor->cell_height = (double)(env->ft_face->size->metrics.height >> 6);
    ctx_wayland->gutter_rows = 3;
    ctx_wayland->gutter_cols = 4;
    ctx_wayland->num_cols = ctx_wayland->width / ctx_cursor->cell_width;
    ctx_wayland->num_rows = ctx_wayland->height / ctx_cursor->cell_height;
    ctx_cursor->x_width =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_X_WIDTH);
    ctx_cursor->x_width[0] = 1;
    war_cursor_init(ctx_cursor, ctx_pool, ctx_config, ctx_vk);
    ctx_cursor->instance_count = 1;
    ctx_cursor->instance[0].pos[0] = ctx_wayland->gutter_cols;
    ctx_cursor->instance[0].pos[1] = 60;
    ctx_cursor->instance[0].size[0] = 1;
    ctx_cursor->instance[0].size[1] = 1;
    ctx_cursor->layer = 1;
    ctx_cursor->octave = 4;
    double vis_cols = (double)ctx_wayland->width /
                          (ctx_cursor->cell_width * ctx_wayland->zoom) -
                      ctx_wayland->gutter_cols;
    double vis_rows = (double)ctx_wayland->height /
                          (ctx_cursor->cell_height * ctx_wayland->zoom) -
                      ctx_wayland->gutter_rows;
    if (vis_cols < 1) vis_cols = 1;
    if (vis_rows < 1) vis_rows = 1;
    ctx_wayland->panning[0] = ctx_cursor->instance[0].pos[0] - vis_cols / 2.0;
    ctx_wayland->panning[1] = ctx_cursor->instance[0].pos[1] - vis_rows / 2.0;
    if (ctx_wayland->panning[0] < 0) ctx_wayland->panning[0] = 0;
    if (ctx_wayland->panning[1] < 0) ctx_wayland->panning[1] = 0;
    uint32_t c = env->ctx_color->layer_1;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    memcpy(ctx_cursor->instance[0].color, rgba, sizeof(rgba));
    //-------------------------------------------------------------------------
    // PIPEWIRE AUDIO THREAD
    //-------------------------------------------------------------------------
    // atomics: shared state between main thread and audio thread
    env->atomics = calloc(1, sizeof(war_atomics));

    // capture ring buffer: audio thread writes captured mic samples,
    // main thread reads them via war_pc_from_a(pc_capture, ...)
    env->pc_capture = calloc(1, sizeof(war_producer_consumer));
    env->pc_capture->to_wr = calloc(1, ctx_config->PC_CAPTURE_BUFFER_SIZE);
    env->pc_capture->to_a = calloc(1, ctx_config->PC_CAPTURE_BUFFER_SIZE);
    env->pc_capture->size = ctx_config->PC_CAPTURE_BUFFER_SIZE;

    // loopback ring buffer: audio thread writes laptop speaker samples,
    // main thread reads them via war_pc_from_a(pc_loopback, ...)
    env->pc_loopback = calloc(1, sizeof(war_producer_consumer));
    env->pc_loopback->to_wr = calloc(1, ctx_config->PC_CAPTURE_BUFFER_SIZE);
    env->pc_loopback->to_a = calloc(1, ctx_config->PC_CAPTURE_BUFFER_SIZE);
    env->pc_loopback->size = ctx_config->PC_CAPTURE_BUFFER_SIZE;

    // play ring buffer: main thread writes audio samples to play,
    // audio thread reads them via war_pc_from_wr(pc_play, ...)
    env->pc_play = calloc(1, sizeof(war_producer_consumer));
    env->pc_play->to_a = calloc(1, ctx_config->PC_PLAY_BUFFER_SIZE);
    env->pc_play->to_wr = calloc(1, ctx_config->PC_PLAY_BUFFER_SIZE);
    env->pc_play->size = ctx_config->PC_PLAY_BUFFER_SIZE;

    // pipewire context (struct allocated from pool, sub-fields filled in
    // war_pipewire thread)
    env->ctx_pw = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_AUDIO_CTX_PW);

    // spawn dedicated audio thread (war_pipewire runs pw_main_loop_run
    // internally)
    pthread_t pw_thread;
    WASSERT(pthread_create(&pw_thread, NULL, war_pipewire, env) == 0);

    // init capture slots and accumulator
    memset(env->capture_slots, 0, sizeof(env->capture_slots));
    env->capture_accumulator = NULL;
    env->capture_accumulator_count = 0;
    env->capture_accumulator_capacity = 0;

    //-------------------------------------------------------------------------
    // PIANO GUTTER INIT
    //-------------------------------------------------------------------------
    war_piano_gutter_context* ctx_pg =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER);
    war_piano_gutter_init(ctx_pg, ctx_pool, ctx_config, ctx_vk);
    war_piano_gutter_generate(
        ctx_pg, ctx_wayland->gutter_cols, ctx_wayland->gutter_rows);
    env->ctx_piano_gutter = ctx_pg;
    //-------------------------------------------------------------------------
    // GRIDLINES INIT
    //-------------------------------------------------------------------------
    war_gridlines_context* ctx_gl =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_GRIDLINES);
    war_gridlines_init(ctx_gl, ctx_pool, ctx_config, ctx_vk);
    war_gridlines_generate(
        ctx_gl,
        (double)ctx_wayland->gutter_cols,
        (double)(ctx_wayland->gutter_cols + ctx_wayland->num_cols),
        128,
        (uint32_t)ctx_config->HUD_GRIDLINES_INSTANCE_MAX,
        ctx_wayland->gutter_rows);
    env->ctx_gridlines = ctx_gl;
    //-------------------------------------------------------------------------
    // NOTE INIT (single instance at middle C, layer 2 colour)
    //-------------------------------------------------------------------------
    war_note_context* ctx_note =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE);
    war_note_init(ctx_note, ctx_pool, ctx_vk);
    env->ctx_note = ctx_note;
    //-------------------------------------------------------------------------
    // LINE CONTEXT INIT
    //-------------------------------------------------------------------------
    war_simple_line_context* ctx_line =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_LINE);
    uint32_t line_max =
        ctx_config->LINE_CELL_INSTANCE_MAX + ctx_config->LINE_BPM_INSTANCE_MAX;
    war_line_init(ctx_line, ctx_pool, ctx_vk, line_max);
    env->ctx_line = ctx_line;
    // playback bar (vertical green line at gutter start)
    ctx_line->instance[0].pos[0] = (float)ctx_wayland->gutter_cols;
    ctx_line->instance[0].pos[1] = ctx_wayland->gutter_rows;
    ctx_line->instance[0].pos[2] = 0.0f;
    ctx_line->instance[0].size[0] = 0.0f; // vertical
    ctx_line->instance[0].size[1] =
        128.0f - ctx_wayland->gutter_rows; // span full MIDI range
    ctx_line->instance[0].width = 0.08f;   // thickness
    ctx_line->instance[0].color[0] = 0.0f;
    ctx_line->instance[0].color[1] = 0.8f;
    ctx_line->instance[0].color[2] = 0.0f;
    ctx_line->instance[0].color[3] = 1.0f;
    ctx_line->instance[0].flags = 0;
    ctx_line->instance_count = 1;
    // playback bar state
    env->play_bar_playing = 0;
    env->play_bar_position_seconds = 0.0;
    env->play_bar_last_frame_ms = 0;
    env->play_bar_prev_cell_pos = (double)ctx_wayland->gutter_cols;
    env->play_bar_preview_active = 0;
    //-------------------------------------------------------------------------
    // FONT INIT (renders 'M' glyph, sets up Vulkan text pipeline)
    //-------------------------------------------------------------------------
    env->ctx_font = calloc(1, sizeof(war_font_context));
    war_font_init(env->ctx_font, ctx_vk, env->ft_face,
                  ctx_cursor->cell_width, ctx_cursor->cell_height);
    //-------------------------------------------------------------------------
    // FIRST FRAME RENDER (record + submit)
    //-------------------------------------------------------------------------
    war_render_frame(ctx_wayland, ctx_vk);
    ctx_wayland->frame_callback = wl_surface_frame(ctx_wayland->surface);
    wl_callback_add_listener(
        ctx_wayland->frame_callback, &war_frame_listener, ctx_wayland);

    wl_surface_attach(ctx_wayland->surface, ctx_wayland->buffer, 0, 0);
    wl_surface_damage_buffer(
        ctx_wayland->surface, 0, 0, ctx_wayland->width, ctx_wayland->height);
    wl_surface_commit(ctx_wayland->surface);

    //-------------------------------------------------------------------------
    // MAIN LOOP
    //-------------------------------------------------------------------------
    int wayland_fd = wl_display_get_fd(ctx_wayland->display);
    struct pollfd pfds[2] = {
        {.fd = wayland_fd, .events = POLLIN},
        {.fd = ctx_wayland->repeat_timer_fd, .events = POLLIN},
    };
    while (ctx_wayland->running) {
        wl_display_flush(ctx_wayland->display);
        if (wl_display_prepare_read(ctx_wayland->display) == 0) {
            poll(pfds, 2, -1);
            if (pfds[0].revents & POLLIN)
                wl_display_read_events(ctx_wayland->display);
            else
                wl_display_cancel_read(ctx_wayland->display);
        }
        wl_display_dispatch_pending(ctx_wayland->display);
        // drain timerfd (periodic, no re-arm needed)
        struct pollfd tfd = {.fd = ctx_wayland->repeat_timer_fd,
                             .events = POLLIN};
        if (poll(&tfd, 1, 0) > 0) {
            uint64_t exp;
            read(ctx_wayland->repeat_timer_fd, &exp, sizeof(exp));
            if (ctx_wayland->repeat_active &&
                ctx_wayland->repeat_sym != XKB_KEY_NoSymbol &&
                !ctx_wayland->env->cmd_active) {
                // skip digits – they are prefix accumulators, not commands
                if (ctx_wayland->repeat_sym >= XKB_KEY_0 &&
                    ctx_wayland->repeat_sym <= XKB_KEY_9)
                    continue;
                for (uint64_t i = 0; i < exp; i++) {
                    war_env* env = ctx_wayland->env;
                    war_keymap_context* keymap = env->ctx_keymap;
                    war_config_context* config = env->ctx_config;
                    size_t ti = (size_t)ctx_wayland->repeat_sym *
                                    config->KEYMAP_MOD_CAPACITY +
                                ctx_wayland->repeat_mod;
                    uint64_t next = keymap->next_state[ti];
                    if (next) {
                        uint8_t cnt = keymap->function_count[next];
                        size_t fb =
                            next * (size_t)config->KEYMAP_FUNCTION_CAPACITY;
                        for (uint8_t j = 0; j < cnt; j++) {
                            void (*fn)(war_env*) = keymap->function[fb + j];
                            if (fn) fn(env);
                        }
                    }
                }
            }
        }
        // drain loopback (system audio) ring buffer into accumulator
        if (env->atomics->capture) {
            uint32_t hdr, sz;
            uint8_t buf[65536];
            while (war_pc_from_a(env->pc_loopback, &hdr, &sz, buf) && sz > 0) {
                uint32_t n_floats = sz / sizeof(float);
                uint64_t needed = env->capture_accumulator_count + n_floats;
                if (needed > env->capture_accumulator_capacity) {
                    uint64_t new_cap =
                        env->capture_accumulator_capacity ?
                            env->capture_accumulator_capacity * 2 :
                            4096;
                    while (new_cap < needed) new_cap *= 2;
                    float* tmp = realloc(env->capture_accumulator,
                                         new_cap * sizeof(float));
                    if (!tmp) break;
                    env->capture_accumulator = tmp;
                    env->capture_accumulator_capacity = new_cap;
                }
                memcpy(env->capture_accumulator +
                           env->capture_accumulator_count,
                       buf,
                       sz);
                env->capture_accumulator_count += n_floats;
            }
        }
        // preview playback: write stereo capture slot → pc_play
        // Slot contains interleaved stereo F32 (L,R,L,R,...).
        if (env->preview_active && !env->play_bar_playing) {
            uint32_t note = env->preview_note;
            if (note > 127) note = 127;
            uint32_t layer = env->preview_layer;
            if (layer < 1 || layer > 9) {
                env->preview_active = 0;
                env->preview_read_pos = 0;
            }
            uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
            war_capture_slot* slot = &env->capture_slots[idx];
            if (!slot->samples || env->preview_read_pos >= slot->count) {
                env->preview_active = 0;
                env->preview_read_pos = 0;
            } else {
                enum { PW_CHUNK_FLOATS = 256 }; // 128 stereo frames
                uint64_t remaining = slot->count - env->preview_read_pos;
                while (remaining >= 2) {
                    uint64_t batch = remaining < PW_CHUNK_FLOATS ?
                                         (remaining & ~1ULL) :
                                         PW_CHUNK_FLOATS;
                    uint32_t stereo_bytes = (uint32_t)(batch * sizeof(float));
                    if (!war_pc_to_a(env->pc_play,
                                     0,
                                     stereo_bytes,
                                     slot->samples + env->preview_read_pos))
                        break;
                    env->preview_read_pos += batch;
                    remaining -= batch;
                }
            }
        }
        // playback bar streaming: preview-style → pc_play
        if (env->play_bar_playing && env->play_bar_preview_active) {
            uint32_t note = env->play_bar_preview_note;
            if (note > 127) note = 127;
            uint32_t layer = env->play_bar_preview_layer;
            if (layer < 1 || layer > 9) {
                env->play_bar_preview_active = 0;
                env->play_bar_preview_read_pos = 0;
            }
            uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
            war_capture_slot* slot = &env->capture_slots[idx];
            if (!slot->samples ||
                env->play_bar_preview_read_pos >= slot->count) {
                env->play_bar_preview_active = 0;
                env->play_bar_preview_read_pos = 0;
            } else {
                enum { PW_CHUNK_FLOATS = 256 };
                uint64_t remaining =
                    slot->count - env->play_bar_preview_read_pos;
                while (remaining >= 2) {
                    uint64_t batch = remaining < PW_CHUNK_FLOATS ?
                                         (remaining & ~1ULL) :
                                         PW_CHUNK_FLOATS;
                    uint32_t stereo_bytes = (uint32_t)(batch * sizeof(float));
                    if (!war_pc_to_a(env->pc_play,
                                     0,
                                     stereo_bytes,
                                     slot->samples +
                                         env->play_bar_preview_read_pos))
                        break;
                    env->play_bar_preview_read_pos += batch;
                    remaining -= batch;
                }
            }
        }
    }

    //-------------------------------------------------------------------------
    // CLEANUP (audio thread first — needs to stop before Vulkan/wayland
    // teardown)
    //-------------------------------------------------------------------------
    // signal audio thread to stop and quit its main loop
    if (env->atomics) env->atomics->capture = 0;
    if (env->ctx_pw && env->ctx_pw->main_loop)
        pw_main_loop_quit(env->ctx_pw->main_loop);
    pthread_join(pw_thread, NULL);
    // free ring buffer memory
    if (env->pc_capture) {
        free(env->pc_capture->to_wr);
        free(env->pc_capture->to_a);
        free(env->pc_capture);
    }
    if (env->pc_loopback) {
        free(env->pc_loopback->to_wr);
        free(env->pc_loopback->to_a);
        free(env->pc_loopback);
    }
    if (env->pc_play) {
        free(env->pc_play->to_a);
        free(env->pc_play->to_wr);
        free(env->pc_play);
    }
    free(env->atomics);
    // free capture slots and accumulator
    for (uint32_t i = 0; i < 128 * WAR_CAPTURE_SLOT_LAYERS; i++)
        free(env->capture_slots[i].samples);
    free(env->capture_accumulator);
    env->atomics = NULL;
    env->pc_capture = NULL;
    env->pc_loopback = NULL;
    env->pc_play = NULL;

    vkDeviceWaitIdle(ctx_vk->device);
    // font cleanup (must happen before device teardown)
    if (env->ctx_font) {
        vkDestroyPipeline(ctx_vk->device, env->ctx_font->pipeline, NULL);
        vkDestroyPipelineLayout(ctx_vk->device, env->ctx_font->pipeline_layout, NULL);
        vkDestroyShaderModule(ctx_vk->device, env->ctx_font->vert_module, NULL);
        vkDestroyShaderModule(ctx_vk->device, env->ctx_font->frag_module, NULL);
        vkDestroyDescriptorSetLayout(ctx_vk->device, env->ctx_font->desc_set_layout, NULL);
        vkDestroyDescriptorPool(ctx_vk->device, env->ctx_font->desc_pool, NULL);
        vkDestroySampler(ctx_vk->device, env->ctx_font->sampler, NULL);
        vkDestroyImageView(ctx_vk->device, env->ctx_font->atlas_view, NULL);
        vkDestroyImage(ctx_vk->device, env->ctx_font->atlas_image, NULL);
        vkFreeMemory(ctx_vk->device, env->ctx_font->atlas_memory, NULL);
        vkDestroyBuffer(ctx_vk->device, env->ctx_font->quad_vbo, NULL);
        vkDestroyBuffer(ctx_vk->device, env->ctx_font->instance_vbo, NULL);
        vkFreeMemory(ctx_vk->device, env->ctx_font->quad_vbo_memory, NULL);
        vkFreeMemory(ctx_vk->device, env->ctx_font->instance_vbo_memory, NULL);
        free(env->ctx_font);
        env->ctx_font = NULL;
    }
    vkDestroyCommandPool(ctx_vk->device, ctx_vk->cmd_pool, NULL);
    vkDestroyFramebuffer(ctx_vk->device, ctx_vk->framebuffer, NULL);
    vkDestroyImageView(ctx_vk->device, ctx_vk->image_view, NULL);
    vkDestroyRenderPass(ctx_vk->device, ctx_vk->render_pass, NULL);
    vkFreeMemory(ctx_vk->device, ctx_vk->memory, NULL);
    vkDestroyImage(ctx_vk->device, ctx_vk->image, NULL);
    vkDestroyDevice(ctx_vk->device, NULL);
    vkDestroyInstance(ctx_vk->instance, NULL);
    if (ctx_wayland->repeat_timer_fd >= 0) close(ctx_wayland->repeat_timer_fd);
    wl_buffer_destroy(ctx_wayland->buffer);
    xkb_state_unref(ctx_wayland->xkb_state);
    xkb_keymap_unref(ctx_wayland->xkb_keymap);
    xkb_context_unref(ctx_wayland->xkb_ctx);
    wl_keyboard_destroy(ctx_wayland->keyboard);
    FT_Done_Face(env->ft_face);
    FT_Done_FreeType(env->ft_lib);
    wl_display_disconnect(ctx_wayland->display);
}
