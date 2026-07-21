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
#include "h/war_embed_font.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
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
#include <dirent.h>
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
    war_env* env = ctx_wayland->env;
    // keep playbar timing alive for main-loop advancement
    if (env->play_bar_playing && env->play_bar_last_frame_ms == 0) {
        env->play_bar_last_frame_ms = (uint32_t)(war_get_monotonic_time_us() / 1000);
        env->play_bar_last_us = war_get_monotonic_time_us();
    }
    // sync playbar line position from main-loop advancement
    if (env->play_bar_playing) {
        double _bpm = env->atomics->bpm;
        if (_bpm <= 0.0) _bpm = 100.0;
        double _spc = 15.0 / _bpm;
        double _ccp = (double)ctx_wayland->gutter_cols + env->play_bar_position_seconds / _spc;
        env->ctx_line->instance[0].pos[0] = (float)_ccp;
    }
    // advance recording position based on real-time delta
    if (env->recording_active) {
        if (env->recording_last_frame_ms != 0) {
            uint32_t delta_ms = time - env->recording_last_frame_ms;
            double bpm = env->atomics->bpm;
            if (bpm <= 0.0) bpm = 100.0;
            double seconds_per_cell = 15.0 / bpm;
            env->recording_position += (double)delta_ms / 1000.0 / seconds_per_cell;
        }
        env->recording_last_frame_ms = time;
    }
    if (ctx_wayland->rendering) {
        if (env->ctx_cursor->instance_count && env->ctx_cursor->instance[0].pos[1] < ctx_wayland->gutter_rows)
            env->ctx_cursor->instance[0].pos[1] = ctx_wayland->gutter_rows;
        war_render_frame(ctx_wayland, ctx_wayland->vk, env->ctx_color);
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
static void war_export_wav(war_env* env, const char* filename) {
    if (!env->ctx_note || !env->ctx_note->instance_count) {
        snprintf(env->status_msg, sizeof(env->status_msg), "wwav FAILED: no notes");
        fprintf(stderr, "EXPORT: no notes to export\n");
        return;
    }
    double bpm = env->atomics->bpm;
    if (bpm <= 0.0) bpm = 100.0;
    double sec_per_cell = 15.0 / bpm;
    uint32_t sr = 48000;
    uint32_t num_notes = env->ctx_note->instance_count;

    // compute total length: end of last note
    double total_sec = 0;
    for (uint32_t i = 0; i < num_notes; i++) {
        double _ex = env->ctx_note->instance[i].pos[0];
        double _ey = env->ctx_note->instance[i].pos[1];
        if (_ex < (double)env->ctx_wayland->gutter_cols ||
            _ey < (double)env->ctx_wayland->gutter_rows)
            continue;
        int32_t _pitch = (int32_t)(_ey - (double)env->ctx_wayland->gutter_rows);
        if (_pitch < 0 || _pitch > 127) continue;
        double dur_sec = env->ctx_note->instance[i].size[0] * sec_per_cell;
        double sample_sec = 0;
        for (uint32_t l = 0; l < WAR_CAPTURE_SLOT_LAYERS; l++) {
            uint32_t idx = _pitch * WAR_CAPTURE_SLOT_LAYERS + l;
            if (env->capture_slots[idx].samples && env->capture_slots[idx].count > 0) {
                double d = (double)env->capture_slots[idx].count / (double)(sr * 2);
                if (d > sample_sec) sample_sec = d;
                break;
            }
        }
        if (dur_sec < sample_sec) sample_sec = dur_sec;
        double start = (double)env->ctx_note->instance[i].pos[0] * sec_per_cell;
        double end = start + sample_sec;
        if (end > total_sec) total_sec = end;
    }
    if (total_sec <= 0) {
        snprintf(env->status_msg, sizeof(env->status_msg), "wwav FAILED: no audio data");
        fprintf(stderr, "EXPORT: no audio data found for any note\n");
        return;
    }

    uint64_t total_frames = (uint64_t)(total_sec * sr) + 1;
    uint64_t total_floats = total_frames * 2;
    float* mix = calloc(total_floats, sizeof(float));
    if (!mix) {
        snprintf(env->status_msg, sizeof(env->status_msg), "wwav FAILED: out of memory");
        fprintf(stderr, "EXPORT: out of memory\n");
        return;
    }

    // mix each note
    for (uint32_t i = 0; i < num_notes; i++) {
        double _ex2 = env->ctx_note->instance[i].pos[0];
        double _ey2 = env->ctx_note->instance[i].pos[1];
        if (_ex2 < (double)env->ctx_wayland->gutter_cols ||
            _ey2 < (double)env->ctx_wayland->gutter_rows)
            continue;
        int32_t pitch = (int32_t)(_ey2 - (double)env->ctx_wayland->gutter_rows);
        if (pitch < 0 || pitch > 127) continue;
        uint32_t _nlayer = (env->ctx_note->instance[i].flags >> 4) & 0xF;
        if (_nlayer < 1 || _nlayer > 9) continue;
        if (!(env->layer_visible & (1 << (_nlayer - 1)))) continue;
        uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (_nlayer - 1);
        if (!env->capture_slots[idx].samples || env->capture_slots[idx].count < 2) continue;
        float* _s = env->capture_slots[idx].samples;
        uint64_t _sc = env->capture_slots[idx].count;
        float _sg = (env->capture_slots[idx].gain + 10000.0f) / 10000.0f;
        int _sp = env->capture_slots[idx].pan;
        float _pe = (float)(_sp + 1000) / 2000.0f;
        float _ple = sinf((1.0f - _pe) * (float)(M_PI / 2.0));
        float _pre = sinf(_pe * (float)(M_PI / 2.0));
        double _start_sec = (double)env->ctx_note->instance[i].pos[0] * sec_per_cell;
        uint64_t _start_frame = (uint64_t)(_start_sec * sr);
        double _dur_sec = env->ctx_note->instance[i].size[0] * sec_per_cell;
        uint64_t _dur_frames = (uint64_t)(_dur_sec * sr);
        uint64_t _src_frames = _sc / 2;
        if (_dur_frames < _src_frames) _src_frames = _dur_frames;
        // add PASS filter state
        float _exp_lp0 = 0.0f, _exp_lp1 = 0.0f;
        int _eq_val = env->capture_slots[idx].eq;
        for (uint64_t f = 0; f < _src_frames && _start_frame + f < total_frames; f++) {
            float _sl = _s[f * 2 + 0];
            float _sr = _s[f * 2 + 1];
            if (f == 0) { _exp_lp0 = _sl; _exp_lp1 = _sr; }
            float _a_lp = 0.1f;
            _exp_lp0 = _exp_lp0 + _a_lp * (_sl - _exp_lp0);
            _exp_lp1 = _exp_lp1 + _a_lp * (_sr - _exp_lp1);
            if (_eq_val <= 0) {
                float _t = (float)(-_eq_val) / 1000.0f;
                _sl = _sl + _t * (_exp_lp0 - _sl);
                _sr = _sr + _t * (_exp_lp1 - _sr);
            } else {
                float _t = (float)_eq_val / 1000.0f;
                _sl = _sl - _t * _exp_lp0;
                _sr = _sr - _t * _exp_lp1;
            }
            mix[(_start_frame + f) * 2 + 0] += _sl * _sg * _ple;
            mix[(_start_frame + f) * 2 + 1] += _sr * _sg * _pre;
        }
    }

    // apply master gain
    if (env->master_gain != 0.0f) {
        float _mgm = (env->master_gain + 10000.0f) / 10000.0f;
        for (uint64_t i = 0; i < total_floats; i++)
            mix[i] *= _mgm;
    }

    // normalize to prevent clipping (scale by peak)
    float peak = 0.0f;
    for (uint64_t i = 0; i < total_floats; i++) {
        float a = fabsf(mix[i]);
        if (a > peak) peak = a;
    }
    float scale = 1.0f;
    if (peak > 1.0f) scale = 0.99f / peak;

    // write WAV
    char path[1024];
    snprintf(path, sizeof(path), "%s", filename);
    FILE* f = fopen(path, "wb");
    if (!f) {
        snprintf(env->status_msg, sizeof(env->status_msg), "wwav FAILED: %s",
                 strlen(path) > 85 ? path + strlen(path) - 85 : path);
        fprintf(stderr, "EXPORT: failed to open %s\n", path);
        free(mix);
        return;
    }

    uint32_t data_bytes = (uint32_t)(total_frames * 2 * 2); // 16-bit stereo
    uint32_t chunk_size = 36 + data_bytes;
    // RIFF
    fwrite("RIFF", 1, 4, f);
    fwrite(&chunk_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    // fmt
    uint32_t fmt_size = 16;
    uint16_t audio_fmt = 1; // PCM
    uint16_t channels = 2;
    uint32_t sample_rate = sr;
    uint32_t byte_rate = sr * 2 * 2; // 16-bit * 2 channels
    uint16_t block_align = 2 * 2;
    uint16_t bits = 16;
    fwrite("fmt ", 1, 4, f);
    fwrite(&fmt_size, 4, 1, f);
    fwrite(&audio_fmt, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    fwrite(&block_align, 2, 1, f);
    fwrite(&bits, 2, 1, f);
    // data
    fwrite("data", 1, 4, f);
    fwrite(&data_bytes, 4, 1, f);

    // write 16-bit PCM samples
    for (uint64_t i = 0; i < total_floats; i++) {
        float s = mix[i] * scale;
        if (s < -1.0f) s = -1.0f;
        if (s > 1.0f) s = 1.0f;
        int16_t sample = (int16_t)(s * 32767.0f);
        fwrite(&sample, 2, 1, f);
    }

    fclose(f);
    free(mix);
    snprintf(env->status_msg, sizeof(env->status_msg), "%s written (%.1fs)",
             strlen(path) > 80 ? path + strlen(path) - 80 : path, total_sec);
    fprintf(stderr, "EXPORT: wrote %s (%u frames, %.2f sec)\n",
            path, (unsigned)total_frames, total_sec);
}

static void war_save_project(war_env* env, const char* filename) {
    char path[1024];
    snprintf(path, sizeof(path), "%s", filename);
    FILE* f = fopen(path, "wb");
    if (!f) {
        snprintf(env->status_msg, sizeof(env->status_msg), "save FAILED: %s",
                 strlen(path) > 85 ? path + strlen(path) - 85 : path);
        fprintf(stderr, "SAVE: failed to open %s\n", path);
        return;
    }
    fwrite("WARP", 1, 4, f);
    uint32_t version = 3;
    fwrite(&version, 4, 1, f);
    float bpm = env->atomics->bpm;
    if (bpm <= 0.0f) bpm = 100.0f;
    fwrite(&bpm, 4, 1, f);
    uint32_t note_count = env->ctx_note ? env->ctx_note->instance_count : 0;
    fwrite(&note_count, 4, 1, f);
    for (uint32_t i = 0; i < note_count; i++) {
        fwrite(&env->ctx_note->instance[i].pos, sizeof(float), 3, f);
        fwrite(&env->ctx_note->instance[i].size, sizeof(float), 2, f);
        fwrite(&env->ctx_note->instance[i].color, sizeof(float), 4, f);
        fwrite(&env->ctx_note->instance[i].flags, sizeof(war_vulkan_flags), 1, f);
        fwrite(&env->ctx_note->instance[i].tick, sizeof(uint64_t), 1, f);
    }
    // count non-empty capture slots
    uint32_t slot_count = 0;
    for (int i = 0; i < 128 * WAR_CAPTURE_SLOT_LAYERS; i++) {
        if (env->capture_slots[i].samples && env->capture_slots[i].count > 0)
            slot_count++;
    }
    fwrite(&slot_count, 4, 1, f);
    for (int i = 0; i < 128 * WAR_CAPTURE_SLOT_LAYERS; i++) {
        if (env->capture_slots[i].samples && env->capture_slots[i].count > 0) {
            uint32_t idx = (uint32_t)i;
            fwrite(&idx, 4, 1, f);
            fwrite(&env->capture_slots[i].count, sizeof(uint64_t), 1, f);
            fwrite(&env->capture_slots[i].attack, sizeof(float), 1, f);
            fwrite(&env->capture_slots[i].sustain, sizeof(float), 1, f);
            fwrite(&env->capture_slots[i].release, sizeof(float), 1, f);
            fwrite(&env->capture_slots[i].eq, sizeof(int), 1, f);
            fwrite(&env->capture_slots[i].gain, sizeof(float), 1, f);
            fwrite(&env->capture_slots[i].pan, sizeof(int), 1, f);
            fwrite(env->capture_slots[i].samples, sizeof(float),
                   env->capture_slots[i].count, f);
        }
    }
    fclose(f);
    snprintf(env->status_msg, sizeof(env->status_msg), "%s saved (%u notes, %u slots)",
             strlen(path) > 75 ? path + strlen(path) - 75 : path, note_count, slot_count);
    fprintf(stderr, "SAVE: wrote %s (%u notes, %u slots)\n",
            path, note_count, slot_count);
}

static void war_export_mp3(war_env* env, const char* filename) {
    // write WAV to a temp file, then convert with ffmpeg
    char wav_path[1080];
    snprintf(wav_path, sizeof(wav_path), "/tmp/war_export_%u.wav", (unsigned)war_get_monotonic_time_us());
    war_export_wav(env, wav_path);
    // check if the WAV was written (war_export_wav sets status_msg on failure)
    if (env->status_msg[0] == 'w' && strstr(env->status_msg, "FAILED")) return;
    char cmd[2060];
    snprintf(cmd, sizeof(cmd), "ffmpeg -y -i \"%s\" -codec:a libmp3lame -b:a 192k \"%s\" 2>/dev/null",
             wav_path, filename);
    int ret = system(cmd);
    remove(wav_path);
    if (ret == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg), "%s written (mp3)",
                 strlen(filename) > 90 ? filename + strlen(filename) - 90 : filename);
        fprintf(stderr, "MP3: wrote %s\n", filename);
    } else {
        snprintf(env->status_msg, sizeof(env->status_msg), "wmp3 FAILED: ffmpeg error (install ffmpeg)");
        fprintf(stderr, "MP3: ffmpeg conversion failed for %s\n", filename);
    }
}

static void war_load_project(war_env* env, const char* filename) {
    char path[1024];
    snprintf(path, sizeof(path), "%s", filename);
    FILE* f = fopen(path, "rb");
    if (!f) {
        snprintf(env->status_msg, sizeof(env->status_msg), "load FAILED: %s",
                 strlen(path) > 85 ? path + strlen(path) - 85 : path);
        fprintf(stderr, "LOAD: failed to open %s\n", path);
        return;
    }
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "WARP", 4) != 0) {
        snprintf(env->status_msg, sizeof(env->status_msg), "load FAILED: bad magic");
        fprintf(stderr, "LOAD: invalid magic\n");
        fclose(f);
        return;
    }
    uint32_t version;
    fread(&version, 4, 1, f);
    float bpm;
    fread(&bpm, 4, 1, f);
    if (bpm > 0.0f) env->atomics->bpm = bpm;
    // clear existing notes
    if (env->ctx_note) env->ctx_note->instance_count = 0;
    // clear existing capture slots
    for (int i = 0; i < 128 * WAR_CAPTURE_SLOT_LAYERS; i++) {
        if (env->capture_slots[i].samples) {
            free(env->capture_slots[i].samples);
            env->capture_slots[i].samples = NULL;
            env->capture_slots[i].count = 0;
            env->capture_slots[i].capacity = 0;
        }
    }
    uint32_t note_count;
    fread(&note_count, 4, 1, f);
    if (env->ctx_note) {
        uint32_t max = env->ctx_note->max_instances;
        if (note_count > max) note_count = max;
        for (uint32_t i = 0; i < note_count; i++) {
            fread(&env->ctx_note->instance[i].pos, sizeof(float), 3, f);
            fread(&env->ctx_note->instance[i].size, sizeof(float), 2, f);
            fread(&env->ctx_note->instance[i].color, sizeof(float), 4, f);
            fread(&env->ctx_note->instance[i].flags, sizeof(war_vulkan_flags), 1, f);
            fread(&env->ctx_note->instance[i].tick, sizeof(uint64_t), 1, f);
            env->ctx_note->instance[i].outline_color[3] = 1.0f;
        }
        env->ctx_note->instance_count = note_count;
        env->ctx_note->tick_counter = note_count;
    }
    uint32_t slot_count;
    fread(&slot_count, 4, 1, f);
    for (uint32_t s = 0; s < slot_count; s++) {
        uint32_t idx;
        fread(&idx, 4, 1, f);
        uint64_t cnt;
        fread(&cnt, sizeof(uint64_t), 1, f);
        float _sa = 100.0f, _ss = 100.0f, _sr = 100.0f;
        int _eq = 0;
        float _gain = 0.0f;
        int _pan = 0;
        if (version >= 1) {
            fread(&_sa, sizeof(float), 1, f);
            fread(&_ss, sizeof(float), 1, f);
            fread(&_sr, sizeof(float), 1, f);
        }
        if (version >= 2) {
            fread(&_eq, sizeof(int), 1, f);
        }
        if (version >= 3) {
            fread(&_gain, sizeof(float), 1, f);
            fread(&_pan, sizeof(int), 1, f);
        }
        if (idx < 128 * WAR_CAPTURE_SLOT_LAYERS && cnt > 0) {
            float* samples = malloc(cnt * sizeof(float));
            if (samples) {
                fread(samples, sizeof(float), cnt, f);
                env->capture_slots[idx].samples = samples;
                env->capture_slots[idx].count = cnt;
                env->capture_slots[idx].capacity = cnt;
                env->capture_slots[idx].attack = (_sa == 100.0f) ? 0.0f : _sa;
                env->capture_slots[idx].sustain = (_ss == 100.0f) ? 0.0f : _ss;
                env->capture_slots[idx].release = (_sr == 100.0f) ? 0.0f : _sr;
                env->capture_slots[idx].eq = (_eq == 500 || _eq == 100) ? 0 : _eq;
                env->capture_slots[idx].gain = (_gain == 100.0f) ? 0.0f : _gain;
                env->capture_slots[idx].pan = _pan;
            } else {
                fseek(f, cnt * sizeof(float), SEEK_CUR);
            }
        } else {
            fseek(f, cnt * sizeof(float), SEEK_CUR);
        }
    }
    fclose(f);
    if (env->master_gain < -10000.0f) env->master_gain = 0.0f;
    snprintf(env->status_msg, sizeof(env->status_msg), "%s loaded (%u notes, %u slots)",
             strlen(path) > 75 ? path + strlen(path) - 75 : path, note_count, slot_count);
    fprintf(stderr, "LOAD: loaded %s (%u notes, %u slots, bpm=%.1f)\n",
            path, note_count, slot_count, bpm);
}

static void war_write_inst(war_env* env, const char* filename) {
    int layer = (int)env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) layer = 1;
    char path[1024];
    snprintf(path, sizeof(path), "%s", filename);
    FILE* f = fopen(path, "wb");
    if (!f) {
        snprintf(env->status_msg, sizeof(env->status_msg), "winst FAILED: %s",
                 strlen(path) > 85 ? path + strlen(path) - 85 : path);
        fprintf(stderr, "WINST: failed to open %s\n", path);
        return;
    }
    fwrite("WARI", 1, 4, f);
    uint32_t ver = 0;
    fwrite(&ver, 4, 1, f);
    uint32_t li = (uint32_t)(layer - 1);
    // count non-empty slots
    uint32_t count = 0;
    for (uint32_t p = 0; p < 128; p++) {
        if (env->capture_slots[p * WAR_CAPTURE_SLOT_LAYERS + li].samples &&
            env->capture_slots[p * WAR_CAPTURE_SLOT_LAYERS + li].count > 0)
            count++;
    }
    fwrite(&count, 4, 1, f);
    for (uint32_t p = 0; p < 128; p++) {
        war_capture_slot* s = &env->capture_slots[p * WAR_CAPTURE_SLOT_LAYERS + li];
        if (s->samples && s->count > 0) {
            fwrite(&p, 4, 1, f);
            fwrite(&s->count, sizeof(uint64_t), 1, f);
            fwrite(s->samples, sizeof(float), s->count, f);
        }
    }
    fclose(f);
    snprintf(env->status_msg, sizeof(env->status_msg), "%s written (layer %d, %u pitches)",
             strlen(path) > 65 ? path + strlen(path) - 65 : path, layer, count);
    fprintf(stderr, "WINST: wrote %s (layer=%d, %u pitches)\n", path, layer, count);
}

static void war_load_inst(war_env* env, const char* filename) {
    int layer = (int)env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) layer = 1;
    char path[1024];
    snprintf(path, sizeof(path), "%s", filename);
    FILE* f = fopen(path, "rb");
    if (!f) {
        snprintf(env->status_msg, sizeof(env->status_msg), "loadinst FAILED: %s",
                 strlen(path) > 85 ? path + strlen(path) - 85 : path);
        fprintf(stderr, "LOADINST: failed to open %s\n", path);
        return;
    }
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "WARI", 4) != 0) {
        snprintf(env->status_msg, sizeof(env->status_msg), "loadinst FAILED: bad magic");
        fprintf(stderr, "LOADINST: invalid magic\n");
        fclose(f);
        return;
    }
    uint32_t ver;
    fread(&ver, 4, 1, f);
    uint32_t li = (uint32_t)(layer - 1);
    // clear existing slots for this layer
    for (uint32_t p = 0; p < 128; p++) {
        war_capture_slot* s = &env->capture_slots[p * WAR_CAPTURE_SLOT_LAYERS + li];
        if (s->samples) { free(s->samples); s->samples = NULL; }
        s->count = 0;
        s->capacity = 0;
        s->attack = 0.0f;
        s->sustain = 0.0f;
        s->release = 0.0f;
        s->gain = 0.0f;
        s->eq = 0;
    }
    uint32_t count;
    fread(&count, 4, 1, f);
    for (uint32_t c = 0; c < count; c++) {
        uint32_t pitch;
        fread(&pitch, 4, 1, f);
        uint64_t cnt;
        fread(&cnt, sizeof(uint64_t), 1, f);
        if (pitch < 128 && cnt > 0) {
            float* samples = malloc(cnt * sizeof(float));
            if (samples) {
                fread(samples, sizeof(float), cnt, f);
                env->capture_slots[pitch * WAR_CAPTURE_SLOT_LAYERS + li].samples = samples;
                env->capture_slots[pitch * WAR_CAPTURE_SLOT_LAYERS + li].count = cnt;
                env->capture_slots[pitch * WAR_CAPTURE_SLOT_LAYERS + li].capacity = cnt;
                env->capture_slots[pitch * WAR_CAPTURE_SLOT_LAYERS + li].attack = 0.0f;
                env->capture_slots[pitch * WAR_CAPTURE_SLOT_LAYERS + li].sustain = 0.0f;
                env->capture_slots[pitch * WAR_CAPTURE_SLOT_LAYERS + li].release = 0.0f;
                env->capture_slots[pitch * WAR_CAPTURE_SLOT_LAYERS + li].gain = 0.0f;
                env->capture_slots[pitch * WAR_CAPTURE_SLOT_LAYERS + li].eq = 0;
            } else {
                fseek(f, cnt * sizeof(float), SEEK_CUR);
            }
        } else {
            fseek(f, cnt * sizeof(float), SEEK_CUR);
        }
    }
    fclose(f);
    snprintf(env->status_msg, sizeof(env->status_msg), "%s loaded (layer %d, %u pitches)",
             strlen(path) > 65 ? path + strlen(path) - 65 : path, layer, count);
    fprintf(stderr, "LOADINST: loaded %s (layer=%d, %u pitches)\n", path, layer, count);
}

static void war_loop_notes(war_env* env, int quarter_notes, int repeats) {
    war_note_context* note = env->ctx_note;
    if (!note || !note->instance_count) {
        fprintf(stderr, "LOOP: no notes\n");
        return;
    }
    double cursor_col = env->ctx_cursor->instance[0].pos[0];
    double section_cells = (double)quarter_notes * 4.0;
    if (section_cells <= 0) {
        fprintf(stderr, "LOOP: invalid length\n");
        return;
    }
    if (repeats < 2) {
        fprintf(stderr, "LOOP: repeats must be >= 2\n");
        return;
    }
    int added = 0;
    uint32_t max = note->max_instances;
    uint32_t count = note->instance_count;
    for (int r = 1; r < repeats; r++) {
        double shift = section_cells * r;
        for (uint32_t i = 0; i < count; i++) {
            double ns = note->instance[i].pos[0];
            if (ns >= cursor_col && ns < cursor_col + section_cells) {
                uint32_t lv = (note->instance[i].flags >> 4) & 0xF;
                if (lv >= 1 && lv <= 9 && !(env->layer_visible & (1 << (lv - 1)))) continue;
                if (note->instance_count >= max) break;
                uint32_t dst = note->instance_count++;
                note->instance[dst] = note->instance[i];
                note->instance[dst].pos[0] += (float)shift;
                note->instance[dst].tick = note->tick_counter++;
                added++;
            }
        }
    }
    fprintf(stderr, "LOOP: section=%.1f cells repeats=%d added=%d notes total=%u\n",
            section_cells, repeats, added, note->instance_count);
}

// handle crop mode arrow key adjustment (returns 1 if handled)
static int _war_crop_adjust(war_env* env, uint32_t raw_sym, uint32_t mod) {
    if (!env->crop_active) return 0;
    if (env->crop_pitch > 127 || env->crop_layer < 1 || env->crop_layer > 9) {
        env->crop_active = 0; return 1;
    }
    uint32_t cidx = env->crop_pitch * WAR_CAPTURE_SLOT_LAYERS + (env->crop_layer - 1);
    float* csamples = env->capture_slots[cidx].samples;
    uint64_t ctotal = env->capture_slots[cidx].count / 2;
    if (!csamples || ctotal < 1) { env->crop_active = 0; return 1; }
    int trim_amount = 480;
    int modded = 0;
    if (raw_sym == XKB_KEY_Left && !(mod & MOD_SHIFT)) {
        if (env->crop_start_frame >= (uint64_t)trim_amount) {
            env->crop_start_frame -= trim_amount; modded = 1;
        }
    } else if (raw_sym == XKB_KEY_Right && !(mod & MOD_SHIFT)) {
        if (env->crop_start_frame + trim_amount < env->crop_end_frame) {
            env->crop_start_frame += trim_amount; modded = 1;
        }
    } else if (raw_sym == XKB_KEY_Left && (mod & MOD_SHIFT)) {
        if (env->crop_end_frame > env->crop_start_frame + trim_amount) {
            env->crop_end_frame -= trim_amount; modded = 1;
        }
    } else if (raw_sym == XKB_KEY_Right && (mod & MOD_SHIFT)) {
        if (env->crop_end_frame + trim_amount <= ctotal) {
            env->crop_end_frame += trim_amount; modded = 1;
        }
    }
    if (modded)
        call_king_terry("CROP: [%llu, %llu) of %llu frames",
                        (unsigned long long)env->crop_start_frame,
                        (unsigned long long)env->crop_end_frame,
                        (unsigned long long)(env->crop_end_frame - env->crop_start_frame));
    return modded;
}

// Reconnect capture stream to a specific PipeWire node by name
void war_reconnect_capture(war_env* env, const char* target) {
    if (!env->ctx_pw || !env->ctx_pw->capture_stream) return;
    pw_stream_disconnect(env->ctx_pw->capture_stream);
    struct pw_properties* _cp = pw_stream_get_properties(env->ctx_pw->capture_stream);
    if (_cp) {
        if (target) pw_properties_set(_cp, "target.object", target);
        else pw_properties_set(_cp, "target.object", NULL);
    }
    struct spa_audio_info_raw info = { .format = SPA_AUDIO_FORMAT_F32, .rate = 48000, .channels = 2 };
    uint8_t buf[1024];
    struct spa_pod_builder bld = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&bld, SPA_PARAM_EnumFormat, &info);
    pw_stream_connect(env->ctx_pw->capture_stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
                      params, 1);
}

void war_reconnect_loopback(war_env* env, const char* target) {
    if (!env->ctx_pw || !env->ctx_pw->loopback_capture_stream) return;
    pw_stream_disconnect(env->ctx_pw->loopback_capture_stream);
    struct pw_properties* _lp = pw_stream_get_properties(env->ctx_pw->loopback_capture_stream);
    if (_lp) {
        pw_properties_set(_lp, "target.object", NULL);
        pw_properties_set(_lp, "node.target", NULL);
    }
    struct spa_audio_info_raw info = { .format = SPA_AUDIO_FORMAT_F32, .rate = 48000, .channels = 2 };
    uint8_t buf[1024];
    struct spa_pod_builder bld = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&bld, SPA_PARAM_EnumFormat, &info);
    pw_stream_connect(env->ctx_pw->loopback_capture_stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
                      params, 1);
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
        if (ctx_wayland->env) {
            xkb_keysym_t rk = xkb_state_key_get_one_sym(ctx_wayland->xkb_state, key + 8);
            uint32_t rk_norm = war_normalize_keysym(rk);
            int offset = _war_keysym_to_midi_offset(rk_norm);
            if (offset >= 0) {
                int32_t base = war_octave_to_midi_base((int32_t)ctx_wayland->env->ctx_cursor->octave);
                uint32_t rel_note = (uint32_t)(offset + base);
                if (rel_note > 127) rel_note = 127;
                if (ctx_wayland->env->midi_toggle) return; // toggle mode: release does nothing
                for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
                    if (ctx_wayland->env->preview_voice_active[v] &&
                        ctx_wayland->env->preview_voice_note[v] == rel_note) {
                        if (ctx_wayland->env->recording_active) {
                            uint32_t ni = ctx_wayland->env->recording_note_idx[v];
                            double start_col = ctx_wayland->env->recording_start_col[v];
                            uint64_t elapsed_us = war_get_monotonic_time_us() -
                                ctx_wayland->env->recording_press_time_us[v];
                            double bpm = ctx_wayland->env->atomics->bpm;
                            if (bpm <= 0.0) bpm = 100.0;
                            double sec_per_cell = 15.0 / bpm;
                            double width = (double)elapsed_us / 1000000.0 / sec_per_cell;
                            if (width < 1.0) width = 1.0;
                            if (ni < ctx_wayland->env->ctx_note->instance_count) {
                                ctx_wayland->env->ctx_note->instance[ni].size[0] = (float)width;
                            }
                        }
                        // graceful release: set read_limit so release envelope plays out
                        uint32_t _rlidx = rel_note * WAR_CAPTURE_SLOT_LAYERS + (ctx_wayland->env->preview_voice_layer[v] - 1);
                        float _rel_target = ctx_wayland->env->capture_slots[_rlidx].release / 1000.0f * 48000.0f;
                        uint64_t _rel_min = (uint64_t)_rel_target;
                        if (_rel_min < 2400) _rel_min = 2400;
                        ctx_wayland->env->preview_voice_read_limit[v] = ctx_wayland->env->preview_voice_read_pos[v] + _rel_min;
                    }
                }
                return;
            }
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

    uint32_t mode = env->active_mode;
    // clamp cursor above gutter before processing any key
    if (cur->instance_count && cur->instance[0].pos[1] < ctx_wayland->gutter_rows)
        cur->instance[0].pos[1] = ctx_wayland->gutter_rows;
    // check timeout for pending prefix state (500ms)
    if (ctx_wayland->keymap_state &&
        time - ctx_wayland->keymap_state_time > 500) {
        ctx_wayland->keymap_state = 0;
        cur->prefix = 0;
    }
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
    if (is_digit && mode != WAR_MODE_ID_MIDI && mode != WAR_MODE_ID_WAV) {
        cur->prefix = cur->prefix * 10 + (uint32_t)(keysym - XKB_KEY_0);
    }

    // command mode handling
    if (env->cmd_active) {
        if (raw_sym == XKB_KEY_Escape) {
            env->cmd_active = 0;
            env->cmd_len = 0;
            cur->prefix = 0;
            memset(env->preview_voice_active, 0, sizeof(env->preview_voice_active));
            return;
        }
        if (raw_sym == XKB_KEY_Return || raw_sym == XKB_KEY_KP_Enter) {
            env->cmd_buf[env->cmd_len < 256 ? env->cmd_len : 255] = '\0';
            // chord inversions: match ":chordnamei<N>" pattern, handle before if-else
            if (env->cmd_len >= 4 && env->cmd_buf[0] == ':') {
                char* i_pos = strchr(env->cmd_buf + 1, 'i');
                if (i_pos && i_pos > env->cmd_buf + 1 && i_pos[1] >= '0' && i_pos[1] <= '9') {
                    int inv = 0;
                    if (sscanf(i_pos + 1, "%d", &inv) == 1 && inv >= 0) {
                        char chord[16] = {0};
                        int clen = (int)(i_pos - (env->cmd_buf + 1));
                        if (clen > 15) clen = 15;
                        memcpy(chord, env->cmd_buf + 1, clen);
                        int intervals[8], n_int = 0;
                        if (0) {}
                        else if (strcmp(chord, "maj7") == 0) { int _i[]={0,4,7,11}; n_int=4; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "min7") == 0) { int _i[]={0,3,7,10}; n_int=4; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "7") == 0) { int _i[]={0,4,7,10}; n_int=4; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "6") == 0) { int _i[]={0,4,7,9}; n_int=4; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "2") == 0) { int _i[]={0,2,7}; n_int=3; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "9") == 0) { int _i[]={0,4,7,10,14}; n_int=5; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "maj9") == 0) { int _i[]={0,4,7,11,14}; n_int=5; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "min9") == 0) { int _i[]={0,3,7,10,14}; n_int=5; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "13") == 0) { int _i[]={0,4,7,10,14,21}; n_int=6; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "maj11") == 0) { int _i[]={0,4,7,11,14,17}; n_int=6; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "min11") == 0) { int _i[]={0,3,7,10,14,17}; n_int=6; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "maj13") == 0) { int _i[]={0,4,7,11,14,21}; n_int=6; memcpy(intervals,_i,sizeof(_i)); }
                        else if (strcmp(chord, "min13") == 0) { int _i[]={0,3,7,10,14,21}; n_int=6; memcpy(intervals,_i,sizeof(_i)); }
                        if (n_int > 0) {
                            if (inv >= n_int) inv = n_int - 1;
                            int inv_intervals[8];
                            for (int j = 0; j < n_int; j++) {
                                inv_intervals[j] = intervals[j];
                                if (j < inv) inv_intervals[j] += 12;
                            }
                            for (int j = 0; j < n_int - 1; j++)
                                for (int k = j + 1; k < n_int; k++)
                                    if (inv_intervals[j] > inv_intervals[k])
                                        { int t = inv_intervals[j]; inv_intervals[j] = inv_intervals[k]; inv_intervals[k] = t; }
                            _war_chord_generic(env, inv_intervals, n_int, chord);
                            char buf[64];
                            snprintf(buf, sizeof(buf), "%si%d", chord, inv);
                            snprintf(env->status_msg, sizeof(env->status_msg), "%s", buf);
                            env->cmd_active = 0; env->cmd_len = 0; cur->prefix = 0;
                            return;
                        }
                    }
                }
            }
            if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'l' && env->cmd_buf[2] == 'o' && env->cmd_buf[3] == 'o' && env->cmd_buf[4] == 'p') {
                int x = 0, y = 0;
                if (sscanf(env->cmd_buf + 5, " %d %d", &x, &y) >= 2 && x > 0 && y > 0) {
                    war_loop_notes(env, x, y);
                    snprintf(env->status_msg, sizeof(env->status_msg), "loop %d x %d", x, y);
                } else
                    fprintf(stderr, "LOOP: usage :loop <quarter_notes> <repeats>\n");
            } else if (env->cmd_len >= 9 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'l' && env->cmd_buf[2] == 'o' && env->cmd_buf[3] == 'a' && env->cmd_buf[4] == 'd' && env->cmd_buf[5] == 'i' && env->cmd_buf[6] == 'n' && env->cmd_buf[7] == 's' && env->cmd_buf[8] == 't') {
                const char* name = env->cmd_buf + 9;
                while (*name == ' ') name++;
                if (name[0])
                    war_load_inst(env, name);
                else
                    fprintf(stderr, "LOADINST: usage :loadinst <name>\n");
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'l' && env->cmd_buf[2] == 'o' && env->cmd_buf[3] == 'a' && env->cmd_buf[4] == 'd') {
                const char* name = env->cmd_buf + 5;
                while (*name == ' ') name++;
                if (name[0])
                    war_load_project(env, name);
                else
                    fprintf(stderr, "LOAD: usage :load <name>\n");
            } else if (env->cmd_len >= 4 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'b' && env->cmd_buf[2] == 'p' && env->cmd_buf[3] == 'm') {
                double val = 0;
                if (sscanf(env->cmd_buf + 4, " %lf", &val) == 1 && val > 0) {
                    env->atomics->bpm = (float)val;
                    snprintf(env->status_msg, sizeof(env->status_msg), "bpm = %.1f", val);
                } else if (env->cmd_len == 4) {
                    double _cur = env->atomics->bpm;
                    if (_cur <= 0.0) _cur = 100.0;
                    snprintf(env->status_msg, sizeof(env->status_msg), "bpm = %.1f", _cur);
                } else
                    fprintf(stderr, "BPM: usage :bpm <value>\n");
            } else if (env->cmd_len >= 7 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'r' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 'd' && env->cmd_buf[4] == 'i' && env->cmd_buf[5] == 'u' && env->cmd_buf[6] == 's') {
                int val = 0;
                if (sscanf(env->cmd_buf + 7, " %d", &val) == 1 && val >= 0) {
                    env->across_radius = (uint32_t)val;
                    snprintf(env->status_msg, sizeof(env->status_msg), "radius = %d", val);
                } else
                    fprintf(stderr, "RADIUS: usage :radius <n>\n");
              } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'c' && env->cmd_buf[2] == 'p' && env->cmd_buf[3] == 'u') {
                int n = 0;
                if (sscanf(env->cmd_buf + 4, " %d", &n) == 1 && n > 0) {
                    double row = cur->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    int32_t pitch = row > 0 ? (int32_t)(row + 0.5) : 0;
                    if (pitch + n > 127) n = 127 - pitch;
                    if (n > 0) {
                        uint32_t layer = cur->layer;
                        if (layer < 1 || layer > 9) layer = 1;
                        uint32_t src = (uint32_t)pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                        uint32_t dst = ((uint32_t)pitch + (uint32_t)n) * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                        if (env->capture_slots[src].samples && env->capture_slots[src].count > 0) {
                            uint64_t cnt = env->capture_slots[src].count;
                            float* copy = malloc(cnt * sizeof(float));
                            if (copy) {
                                memcpy(copy, env->capture_slots[src].samples, cnt * sizeof(float));
                                free(env->capture_slots[dst].samples);
                                env->capture_slots[dst].samples = copy;
                                env->capture_slots[dst].count = cnt;
                                env->capture_slots[dst].capacity = cnt;
                                env->capture_slots[dst].gain = env->capture_slots[src].gain;
                                env->capture_slots[dst].pan = env->capture_slots[src].pan;
                                env->capture_slots[dst].eq = env->capture_slots[src].eq;
                                env->capture_slots[dst].attack = env->capture_slots[src].attack;
                                env->capture_slots[dst].sustain = env->capture_slots[src].sustain;
                                env->capture_slots[dst].release = env->capture_slots[src].release;
                                snprintf(env->status_msg, sizeof(env->status_msg), "cpu: pitch %d -> %u", pitch, pitch + n);
                            }
                        } else {
                            snprintf(env->status_msg, sizeof(env->status_msg), "cpu FAILED: no capture at pitch %d", pitch);
                        }
                    }
                } else {
                    fprintf(stderr, "CPU: usage :cpu <n>\n");
                }
              } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'c' && env->cmd_buf[2] == 'p' && env->cmd_buf[3] == 'd') {
                int n = 0;
                if (sscanf(env->cmd_buf + 4, " %d", &n) == 1 && n > 0) {
                    double row = cur->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    int32_t pitch = row > 0 ? (int32_t)(row + 0.5) : 0;
                    if ((int32_t)pitch - n < 0) n = (int32_t)pitch;
                    if (n > 0) {
                        uint32_t layer = cur->layer;
                        if (layer < 1 || layer > 9) layer = 1;
                        uint32_t src = (uint32_t)pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                        uint32_t dst = ((uint32_t)pitch - (uint32_t)n) * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                        if (env->capture_slots[src].samples && env->capture_slots[src].count > 0) {
                            uint64_t cnt = env->capture_slots[src].count;
                            float* copy = malloc(cnt * sizeof(float));
                            if (copy) {
                                memcpy(copy, env->capture_slots[src].samples, cnt * sizeof(float));
                                free(env->capture_slots[dst].samples);
                                env->capture_slots[dst].samples = copy;
                                env->capture_slots[dst].count = cnt;
                                env->capture_slots[dst].capacity = cnt;
                                env->capture_slots[dst].gain = env->capture_slots[src].gain;
                                env->capture_slots[dst].pan = env->capture_slots[src].pan;
                                env->capture_slots[dst].eq = env->capture_slots[src].eq;
                                env->capture_slots[dst].attack = env->capture_slots[src].attack;
                                env->capture_slots[dst].sustain = env->capture_slots[src].sustain;
                                env->capture_slots[dst].release = env->capture_slots[src].release;
                                snprintf(env->status_msg, sizeof(env->status_msg), "cpd: pitch %d -> %u", pitch, pitch - n);
                            }
                        } else {
                            snprintf(env->status_msg, sizeof(env->status_msg), "cpd FAILED: no capture at pitch %d", pitch);
                        }
                    }
                } else {
                    fprintf(stderr, "CPD: usage :cpd <n>\n");
                }
              } else if (env->cmd_len >= 3 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'v' && env->cmd_buf[3] != 'u' && env->cmd_buf[3] != 'd') {
                int to_layer = 0;
                if (sscanf(env->cmd_buf + 3, " %d", &to_layer) == 1 && to_layer >= 1 && to_layer <= 9) {
                    double row = env->ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    uint32_t pitch = row > 0 ? (uint32_t)(row + 0.5) : 0;
                    if (pitch > 127) pitch = 127;
                    uint32_t cur_layer = env->ctx_cursor->layer;
                    if (cur_layer < 1 || cur_layer > 9) cur_layer = 1;
                    uint32_t src_idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (cur_layer - 1);
                    uint32_t dst_idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (to_layer - 1);
                    if ((int32_t)cur_layer == to_layer) {
                        snprintf(env->status_msg, sizeof(env->status_msg), "mv FAILED: same layer");
                        fprintf(stderr, "MV: source and destination are the same layer\n");
                    } else if (env->capture_slots[src_idx].samples && env->capture_slots[src_idx].count > 0) {
                        free(env->capture_slots[dst_idx].samples);
                        env->capture_slots[dst_idx] = env->capture_slots[src_idx];
                        env->capture_slots[src_idx].samples = NULL;
                        env->capture_slots[src_idx].count = 0;
                        env->capture_slots[src_idx].capacity = 0;
                        snprintf(env->status_msg, sizeof(env->status_msg), "mv: pitch %u layer %d -> %d", pitch, cur_layer, to_layer);
                        fprintf(stderr, "MV: moved pitch=%u from layer %d to layer %d\n", pitch, cur_layer, to_layer);
                    } else {
                        snprintf(env->status_msg, sizeof(env->status_msg), "mv FAILED: no capture at pitch %u", pitch);
                        fprintf(stderr, "MV: no capture at pitch=%u layer=%d\n", pitch, cur_layer);
                    }
                } else {
                    fprintf(stderr, "MV: usage :mv <layer>\n");
                }
             } else if (env->cmd_len >= 3 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'c' && env->cmd_buf[2] == 'p') {
                int _to_layer = 0;
                if (sscanf(env->cmd_buf + 3, " %d", &_to_layer) == 1 && _to_layer >= 1 && _to_layer <= 9) {
                    double _row = env->ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    uint32_t _pitch = _row > 0 ? (uint32_t)(_row + 0.5) : 0;
                    if (_pitch > 127) _pitch = 127;
                    uint32_t _cur_layer = env->ctx_cursor->layer;
                    if (_cur_layer < 1 || _cur_layer > 9) _cur_layer = 1;
                    uint32_t _src_idx = _pitch * WAR_CAPTURE_SLOT_LAYERS + (_cur_layer - 1);
                    uint32_t _dst_idx = _pitch * WAR_CAPTURE_SLOT_LAYERS + (_to_layer - 1);
                    if ((int32_t)_cur_layer == _to_layer) {
                        snprintf(env->status_msg, sizeof(env->status_msg), "cp FAILED: same layer");
                        fprintf(stderr, "CP: source and destination are the same layer\n");
                    } else if (env->capture_slots[_src_idx].samples && env->capture_slots[_src_idx].count > 0) {
                        uint64_t _cnt = env->capture_slots[_src_idx].count;
                        float* _copy = malloc(_cnt * sizeof(float));
                        if (_copy) {
                            memcpy(_copy, env->capture_slots[_src_idx].samples, _cnt * sizeof(float));
                            free(env->capture_slots[_dst_idx].samples);
                            env->capture_slots[_dst_idx].samples = _copy;
                            env->capture_slots[_dst_idx].count = _cnt;
                            env->capture_slots[_dst_idx].capacity = _cnt;
                            env->capture_slots[_dst_idx].gain = env->capture_slots[_src_idx].gain;
                            env->capture_slots[_dst_idx].pan = env->capture_slots[_src_idx].pan;
                            env->capture_slots[_dst_idx].eq = env->capture_slots[_src_idx].eq;
                            snprintf(env->status_msg, sizeof(env->status_msg), "cp: pitch %u layer %d -> %d", _pitch, _cur_layer, _to_layer);
                            fprintf(stderr, "CP: copied pitch=%u from layer %d to layer %d\n", _pitch, _cur_layer, _to_layer);
                        }
                    } else {
                        snprintf(env->status_msg, sizeof(env->status_msg), "cp FAILED: no capture at pitch %u", _pitch);
                        fprintf(stderr, "CP: no capture at pitch=%u layer=%d\n", _pitch, _cur_layer);
                    }
                } else {
                    fprintf(stderr, "CP: usage :cp <layer>\n");
                }
             } else if (env->cmd_len >= 7 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'a' && env->cmd_buf[2] == 'c' && env->cmd_buf[3] == 'r' && env->cmd_buf[4] == 'o' && env->cmd_buf[5] == 's' && env->cmd_buf[6] == 's') {
                int radius = 0;
                if (sscanf(env->cmd_buf + 7, " %d", &radius) == 1 && radius >= 0) {
                    double row = env->ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    uint32_t pitch = row > 0 ? (uint32_t)(row + 0.5) : 0;
                    if (pitch > 127) pitch = 127;
                    uint32_t layer = env->ctx_cursor->layer;
                    if (layer < 1 || layer > 9) layer = 1;
                    _war_across_pitch_shift(env, pitch, layer, radius);
                    snprintf(env->status_msg, sizeof(env->status_msg), "across radius %d", radius);
                } else {
                     fprintf(stderr, "ACROSS: usage :across <radius>\n");
                }
             } else if (env->cmd_len >= 4 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'v' && env->cmd_buf[3] == 'u') {
                int n = 0;
                if (sscanf(env->cmd_buf + 4, " %d", &n) == 1 && n > 0) {
                    double row = env->ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    uint32_t pitch = row > 0 ? (uint32_t)(row + 0.5) : 0;
                    if (pitch + (uint32_t)n > 127) n = 127 - pitch;
                    if (n > 0) {
                        uint32_t layer = env->ctx_cursor->layer;
                        if (layer < 1 || layer > 9) layer = 1;
                        uint32_t src_idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                        uint32_t dst_pitch = pitch + (uint32_t)n;
                        uint32_t dst_idx = dst_pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                         if (env->capture_slots[src_idx].samples && env->capture_slots[src_idx].count > 0) {
                             free(env->capture_slots[dst_idx].samples);
                             env->capture_slots[dst_idx] = env->capture_slots[src_idx];
                             env->capture_slots[src_idx].samples = NULL;
                             env->capture_slots[src_idx].count = 0;
                             env->capture_slots[src_idx].capacity = 0;
                             snprintf(env->status_msg, sizeof(env->status_msg), "mvu: pitch %u -> %u", pitch, dst_pitch);
                             fprintf(stderr, "MVU: moved pitch=%u up %d to pitch=%u\n", pitch, n, dst_pitch);
                         } else {
                             snprintf(env->status_msg, sizeof(env->status_msg), "mvu FAILED: no capture at pitch %u", pitch);
                             fprintf(stderr, "MVU: no capture at pitch=%u layer=%d\n", pitch, layer);
                         }
                    }
                } else {
                    fprintf(stderr, "MVU: usage :mvu <n>\n");
                }
             } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'g' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 'i' && env->cmd_buf[4] == 'n') {
                double _gv = 0;
                if (sscanf(env->cmd_buf + 5, " %lf", &_gv) == 1 && _gv >= -10000 && _gv <= 10000) {
                    double _gr = cur->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    uint32_t _gp = _gr > 0 ? (uint32_t)(_gr + 0.5) : 0;
                    if (_gp > 127) _gp = 127;
                    uint32_t _gl = cur->layer;
                    if (_gl < 1 || _gl > 9) _gl = 1;
                    uint32_t _gi = _gp * WAR_CAPTURE_SLOT_LAYERS + (_gl - 1);
                    env->capture_slots[_gi].gain = (float)_gv;
                    snprintf(env->status_msg, sizeof(env->status_msg), "G%+.0f", (float)_gv);
                } else {
                    fprintf(stderr, "GAIN: usage :gain <-10000..10000>\n");
                }
             } else if (env->cmd_len >= 4 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'p' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 'n') {
                int _pv = 0;
                if (sscanf(env->cmd_buf + 4, " %d", &_pv) == 1 && _pv >= -1000 && _pv <= 1000) {
                    double _pr = cur->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    uint32_t _pp = _pr > 0 ? (uint32_t)(_pr + 0.5) : 0;
                    if (_pp > 127) _pp = 127;
                    uint32_t _pl = cur->layer;
                    if (_pl < 1 || _pl > 9) _pl = 1;
                    uint32_t _pi = _pp * WAR_CAPTURE_SLOT_LAYERS + (_pl - 1);
                    env->capture_slots[_pi].pan = _pv;
                    snprintf(env->status_msg, sizeof(env->status_msg), "P%+d", _pv);
                } else {
                    fprintf(stderr, "PAN: usage :pan <-1000..1000>\n");
                }
             } else if (env->cmd_len >= 3 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'e' && env->cmd_buf[2] == 'q') {
                int _ev = 0;
                if (sscanf(env->cmd_buf + 3, " %d", &_ev) == 1 && _ev >= -1000 && _ev <= 1000) {
                    double _er = cur->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    uint32_t _ep = _er > 0 ? (uint32_t)(_er + 0.5) : 0;
                    if (_ep > 127) _ep = 127;
                    uint32_t _el = cur->layer;
                    if (_el < 1 || _el > 9) _el = 1;
                    uint32_t _ei = _ep * WAR_CAPTURE_SLOT_LAYERS + (_el - 1);
                    env->capture_slots[_ei].eq = _ev;
                    snprintf(env->status_msg, sizeof(env->status_msg), "P%+d", _ev);
                } else {
                    fprintf(stderr, "PASS: usage :eq <-1000..1000>\n");
                }
             } else if (env->cmd_len >= 4 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'v' && env->cmd_buf[3] == 'd') {
                int n = 0;
                if (sscanf(env->cmd_buf + 4, " %d", &n) == 1 && n > 0) {
                    double row = env->ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                    uint32_t pitch = row > 0 ? (uint32_t)(row + 0.5) : 0;
                    if ((int32_t)pitch - n < 0) n = (int32_t)pitch;
                    if (n > 0) {
                        uint32_t layer = env->ctx_cursor->layer;
                        if (layer < 1 || layer > 9) layer = 1;
                        uint32_t src_idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                        uint32_t dst_pitch = pitch - (uint32_t)n;
                        uint32_t dst_idx = dst_pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                         if (env->capture_slots[src_idx].samples && env->capture_slots[src_idx].count > 0) {
                             free(env->capture_slots[dst_idx].samples);
                             env->capture_slots[dst_idx] = env->capture_slots[src_idx];
                             env->capture_slots[src_idx].samples = NULL;
                             env->capture_slots[src_idx].count = 0;
                             env->capture_slots[src_idx].capacity = 0;
                             snprintf(env->status_msg, sizeof(env->status_msg), "mvd: pitch %u -> %u", pitch, dst_pitch);
                             fprintf(stderr, "MVD: moved pitch=%u down %d to pitch=%u\n", pitch, n, dst_pitch);
                         } else {
                             snprintf(env->status_msg, sizeof(env->status_msg), "mvd FAILED: no capture at pitch %u", pitch);
                             fprintf(stderr, "MVD: no capture at pitch=%u layer=%d\n", pitch, layer);
                         }
                    }
                } else {
                    fprintf(stderr, "MVD: usage :mvd <n>\n");
                }
             } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'w' && env->cmd_buf[2] == 'i' && env->cmd_buf[3] == 'n' && env->cmd_buf[4] == 's' && env->cmd_buf[5] == 't') {
                const char* _wname = env->cmd_buf + 6;
                while (*_wname == ' ') _wname++;
                if (_wname[0])
                    war_write_inst(env, _wname);
                else
                    fprintf(stderr, "WINST: usage :winst <name>\n");
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'w' && env->cmd_buf[2] == 'w' && env->cmd_buf[3] == 'a' && env->cmd_buf[4] == 'v') {
                const char* name = NULL;
                if (env->cmd_len > 5 && env->cmd_buf[5] == ' ')
                    name = env->cmd_buf + 6;
                if (!name || !name[0]) name = "output";
                war_export_wav(env, name);
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'w' && env->cmd_buf[2] == 'm' && env->cmd_buf[3] == 'p' && env->cmd_buf[4] == '3') {
                const char* name = NULL;
                if (env->cmd_len > 5 && env->cmd_buf[5] == ' ')
                    name = env->cmd_buf + 6;
                if (!name || !name[0]) name = "output.mp3";
                war_export_mp3(env, name);
            } else if (env->cmd_len >= 9 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'c' && env->cmd_buf[2] == 'o' && env->cmd_buf[3] == 'm' && env->cmd_buf[4] == 'p' && env->cmd_buf[5] == 'r' && env->cmd_buf[6] == 'e' && env->cmd_buf[7] == 's' && env->cmd_buf[8] == 's') {
                war_compress(env);
            } else if (env->cmd_len >= 9 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 's' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 't' && env->cmd_buf[4] == 'u' && env->cmd_buf[5] == 'r' && env->cmd_buf[6] == 'a' && env->cmd_buf[7] == 't' && env->cmd_buf[8] == 'e') {
                war_saturate(env);
            } else if (env->cmd_len >= 7 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'r' && env->cmd_buf[2] == 'e' && env->cmd_buf[3] == 'v' && env->cmd_buf[4] == 'e' && env->cmd_buf[5] == 'r' && env->cmd_buf[6] == 'b') {
                war_reverb(env);
            } else if (env->cmd_len >= 7 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'd' && env->cmd_buf[2] == 'e' && env->cmd_buf[3] == 'l' && env->cmd_buf[4] == 'a' && env->cmd_buf[5] == 'y') {
                war_delay(env);
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'g' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 't' && env->cmd_buf[4] == 'e') {
                war_gate(env);
            } else if (env->cmd_len >= 8 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'd' && env->cmd_buf[2] == 'e' && env->cmd_buf[3] == 'e' && env->cmd_buf[4] == 's' && env->cmd_buf[5] == 's' && env->cmd_buf[6] == 'e' && env->cmd_buf[7] == 'r') {
                war_deesser(env);
            } else if (env->cmd_len >= 2 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'q') {
                ctx_wayland->running = 0;
            } else if (env->cmd_len == 3 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'g' && env->cmd_buf[2] == 'p') {
                war_goto_playback(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "goto playback");
            } else if (env->cmd_len >= 6 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 'j' && env->cmd_buf[4] == '1' && env->cmd_buf[5] == '1') {
                war_chord_maj11(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "maj11");
            } else if (env->cmd_len >= 6 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 'j' && env->cmd_buf[4] == '1' && env->cmd_buf[5] == '3') {
                war_chord_maj13(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "maj13");
            } else if (env->cmd_len >= 6 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'i' && env->cmd_buf[3] == 'n' && env->cmd_buf[4] == '1' && env->cmd_buf[5] == '1') {
                war_chord_min11(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "min11");
            } else if (env->cmd_len >= 6 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'i' && env->cmd_buf[3] == 'n' && env->cmd_buf[4] == '1' && env->cmd_buf[5] == '3') {
                war_chord_min13(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "min13");
            } else if (env->cmd_len == 3 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == '1' && env->cmd_buf[2] == '3') {
                war_chord_13(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "13");
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 'j' && env->cmd_buf[4] == '7') {
                war_chord_maj7(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "maj7");
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'i' && env->cmd_buf[3] == 'n' && env->cmd_buf[4] == '9') {
                war_chord_min9(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "min9");
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'i' && env->cmd_buf[3] == 'n' && env->cmd_buf[4] == '7') {
                war_chord_min7(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "min7");
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'm' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 'j' && env->cmd_buf[4] == '9') {
                war_chord_maj9(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "maj9");
            } else if (env->cmd_len == 2 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == '9') {
                war_chord_9(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "9");
            } else if (env->cmd_len == 2 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == '7') {
                war_chord_7(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "7");
            } else if (env->cmd_len == 2 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == '6') {
                war_chord_6(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "6");
            } else if (env->cmd_len == 2 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == '2') {
                war_chord_2(env);
                snprintf(env->status_msg, sizeof(env->status_msg), "2");
            } else if (env->cmd_len >= 2 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'w') {
                const char* name = NULL;
                if (env->cmd_len > 2 && env->cmd_buf[2] == ' ')
                    name = env->cmd_buf + 3;
                if (name && name[0]) {
                    war_save_project(env, name);
                } else {
                    fprintf(stderr, "SAVE: usage :w <name>\n");
                }
            }
            env->cmd_active = 0;
            env->cmd_len = 0;
            cur->prefix = 0;
            return;
        }
        if (raw_sym == XKB_KEY_BackSpace) {
            if (env->cmd_len > 1) env->cmd_len--;
            env->cmd_buf[env->cmd_len] = '\0';
            env->cmd_tab_count = 0; // reset tab completion
            cur->prefix = 0;
            return;
        }
        // tab completion for filename arguments
        if (raw_sym == XKB_KEY_Tab || raw_sym == XKB_KEY_Up || raw_sym == XKB_KEY_Down) {
            cur->prefix = 0;
            // find the start of the filename argument (after the command and space)
            const char* _cmd = env->cmd_buf;
            int _c_len = (int)env->cmd_len;
            // find position after the first space (which separates command from name)
            int _name_start = -1;
            for (int _ci = 0; _ci < _c_len; _ci++) {
                if (_cmd[_ci] == ' ' && _ci + 1 < _c_len) {
                    _name_start = _ci + 1;
                    break;
                }
            }
            if (_name_start < 0) { return; } // no name part yet
            const char* _prefix = _cmd + _name_start;
            // on Tab: scan directory for matches
            if (raw_sym == XKB_KEY_Tab) {
                // if user typed a new prefix (different from stored), reset matches
                if (env->cmd_tab_count == 0 || strcmp(env->cmd_tab_prefix, _prefix) != 0) {
                    strncpy(env->cmd_tab_prefix, _prefix, sizeof(env->cmd_tab_prefix) - 1);
                    env->cmd_tab_prefix[sizeof(env->cmd_tab_prefix) - 1] = '\0';
                    env->cmd_tab_count = 0;
                    // scan current directory
                    DIR* _dir = opendir(".");
                    if (_dir) {
                        struct dirent* _entry;
                        size_t _plen = strlen(_prefix);
                        while ((_entry = readdir(_dir)) != NULL && env->cmd_tab_count < 64) {
                            if (_entry->d_name[0] == '.') continue; // skip hidden
                            if (_plen == 0 || strncmp(_entry->d_name, _prefix, _plen) == 0) {
                                strncpy(env->cmd_tab_matches[env->cmd_tab_count], _entry->d_name, 127);
                                env->cmd_tab_matches[env->cmd_tab_count][127] = '\0';
                                env->cmd_tab_count++;
                            }
                        }
                        closedir(_dir);
                    }
                    env->cmd_tab_index = -1; // reset to before first
                }
                // cycle to next match
                if (env->cmd_tab_count > 0) {
                    env->cmd_tab_index = (env->cmd_tab_index + 1) % env->cmd_tab_count;
                    const char* _match = env->cmd_tab_matches[env->cmd_tab_index];
                    // replace the name part with the match
                    int _new_len = _name_start + (int)strlen(_match);
                    if (_new_len < 256) {
                    memcpy(env->cmd_buf + _name_start, _match, strlen(_match) + 1);
                    env->cmd_len = (uint32_t)_new_len;
                }
                } else {
                    // no matches
                }
            } else if (env->cmd_tab_count > 0) {
                // Up/Down: navigate through matches
                int _step = (raw_sym == XKB_KEY_Up) ? -1 : 1;
                env->cmd_tab_index = (env->cmd_tab_index + _step + env->cmd_tab_count) % env->cmd_tab_count;
                const char* _match = env->cmd_tab_matches[env->cmd_tab_index];
                int _new_len = _name_start + (int)strlen(_match);
                if (_new_len < 256) {
                    memcpy(env->cmd_buf + _name_start, _match, strlen(_match) + 1);
                    env->cmd_len = (uint32_t)_new_len;
                }
            }
            return;
        }
        // printable ASCII via utf8 from raw sym
        char utf8[8] = {0};
        int n = xkb_keysym_to_utf8(raw_sym, utf8, sizeof(utf8));
        if (n > 1 && utf8[0] >= 32 && utf8[0] <= 126) {
            env->cmd_tab_count = 0; // reset tab completion
            if (env->cmd_len < 255) {
                env->cmd_buf[env->cmd_len++] = utf8[0];
                env->cmd_buf[env->cmd_len] = '\0';
            }
        }
        cur->prefix = 0;
        return;
    }

    // device selector HUD
    if (env->dev_sel_active) {
        int _dprev = env->dev_sel_cursor;
        if (raw_sym == XKB_KEY_Escape) {
            env->dev_sel_active = 0;
        } else if (raw_sym == XKB_KEY_Up || raw_sym == XKB_KEY_j) {
            if (env->dev_sel_cursor > 0) {
                env->dev_sel_cursor--;
                if ((uint32_t)env->dev_sel_cursor < env->dev_sel_offset)
                    env->dev_sel_offset = (uint32_t)env->dev_sel_cursor;
            }
            snprintf(env->status_msg, sizeof(env->status_msg), "[%d/%u] off=%u", env->dev_sel_cursor, env->dev_count, env->dev_sel_offset);
        } else if (raw_sym == XKB_KEY_Down || raw_sym == XKB_KEY_k) {
            if ((uint32_t)(env->dev_sel_cursor + 1) < env->dev_count) {
                env->dev_sel_cursor++;
                if ((uint32_t)(env->dev_sel_cursor) >= env->dev_sel_offset + 15)
                    env->dev_sel_offset = (uint32_t)(env->dev_sel_cursor) - 14;
            }
            snprintf(env->status_msg, sizeof(env->status_msg), "[%d/%u] off=%u", env->dev_sel_cursor, env->dev_count, env->dev_sel_offset);
        } else if (raw_sym == XKB_KEY_Left || raw_sym == XKB_KEY_h) {
            if (ctx_wayland->panning[0] > 0) ctx_wayland->panning[0] -= 1.0f;
        } else if (raw_sym == XKB_KEY_Right || raw_sym == XKB_KEY_l) {
            ctx_wayland->panning[0] += 1.0f;
        } else if (raw_sym == XKB_KEY_Return || raw_sym == XKB_KEY_KP_Enter) {
            if (env->dev_count > 0 && env->dev_names && (uint32_t)env->dev_sel_cursor < env->dev_count) {
                free(env->dev_nodes[env->capture_mode - 1]);
                env->dev_nodes[env->capture_mode - 1] = strdup(env->dev_names[env->dev_sel_cursor]);
                snprintf(env->status_msg, sizeof(env->status_msg), "CAPTURE %u: %s",
                         env->capture_mode, env->dev_names[env->dev_sel_cursor]);
                // save global config
                FILE* _gs = fopen("global_war.config", "w");
                if (_gs) {
                    for (int _gsi = 0; _gsi < 4; _gsi++)
                        if (env->dev_nodes[_gsi])
                            fprintf(_gs, "%s\n", env->dev_nodes[_gsi]);
                        else
                            fprintf(_gs, "\n");
                    fclose(_gs);
                }
            }
            env->dev_sel_active = 0;
        }
        cur->prefix = 0;
        return;
    }
    
    // open device selector (Alt+O) — only during active capture
    if (raw_sym == XKB_KEY_o && (mod & MOD_ALT) && !env->cmd_active && env->atomics->capture) {
        if (env->dev_count > 0 && env->dev_names) {
            env->dev_sel_active = 1;
            env->dev_sel_cursor = 0;
            env->dev_sel_offset = 0;
        } else {
            snprintf(env->status_msg, sizeof(env->status_msg), "no devices found");
        }
        cur->prefix = 0;
        return;
    }

    // enter command mode on ':' (check raw sym before normalizer maps it to ';')
    if (raw_sym == XKB_KEY_Escape) {
        if (env->atomics->capture) {
            env->atomics->capture = 0;
            cur->prefix = 0;
            return;
        }
        if (env->crop_active) {
            env->crop_active = 0;
            cur->prefix = 0;
            return;
        }
        if (env->wave_view_active) {
            env->wave_view_active = 0;
            env->active_mode = WAR_MODE_ID_ROLL;
            cur->prefix = 0;
            return;
        }
        if (env->active_mode == WAR_MODE_ID_VISUAL) {
            env->active_mode = WAR_MODE_ID_ROLL;
            env->ctx_cursor->visual_active = 0;
            env->ctx_cursor->visual_stretch_active = 0;
            uint32_t lc = (&env->ctx_color->layer_none)[env->ctx_cursor->layer];
            cur->instance[0].color[0] = ((lc >> 24) & 0xFF) / 255.0f;
            cur->instance[0].color[1] = ((lc >> 16) & 0xFF) / 255.0f;
            cur->instance[0].color[2] = ((lc >> 8) & 0xFF) / 255.0f;
            cur->instance[0].color[3] = (lc & 0xFF) / 255.0f;
            cur->prefix = 0;
            return;
        }
        if (env->active_mode == WAR_MODE_ID_MIDI) {
            env->active_mode = WAR_MODE_ID_ROLL;
            env->recording_active = 0;
            env->loop_mode = 0;
            cur->prefix = 0;
            return;
        }
        if (env->active_mode == WAR_MODE_ID_MASTER) {
            env->active_mode = WAR_MODE_ID_ROLL;
            cur->prefix = 0;
            return;
        }
        // stop all preview voices (Space/P)
        memset(env->preview_voice_active, 0, sizeof(env->preview_voice_active));
        cur->prefix = 0;
        return;
    }
    // wave mode movement (hjkl moves cursor through waveform)
    if (mode == WAR_MODE_ID_WAV && env->wave_view_active) {
        if (raw_sym == XKB_KEY_l || raw_sym == XKB_KEY_Right) {
            cur->instance[0].pos[0] += 1.0f;
            war_pan_follow(env);
            cur->prefix = 0;
            return;
        }
        if (raw_sym == XKB_KEY_h || raw_sym == XKB_KEY_Left) {
            cur->instance[0].pos[0] -= 1.0f;
            if (cur->instance[0].pos[0] < 0) cur->instance[0].pos[0] = 0;
            war_pan_follow(env);
            cur->prefix = 0;
            return;
        }
        if (raw_sym == XKB_KEY_k || raw_sym == XKB_KEY_Up) {
            cur->instance[0].pos[1] += 1.0f;
            war_pan_follow(env);
            cur->prefix = 0;
            return;
        }
        if (raw_sym == XKB_KEY_j || raw_sym == XKB_KEY_Down) {
            cur->instance[0].pos[1] -= 1.0f;
            if (cur->instance[0].pos[1] < ctx_wayland->gutter_rows) cur->instance[0].pos[1] = ctx_wayland->gutter_rows;
            war_pan_follow(env);
            cur->prefix = 0;
            return;
        }
        // space = preview note, shift+space = playback bar toggle
        if (raw_sym == XKB_KEY_space) {
            if (mod & MOD_SHIFT) {
                war_toggle_playback(env);
            } else {
                _war_preview_start_voice(env, env->wave_view_pitch, env->wave_view_layer);
            }
            cur->prefix = 0;
            return;
        }
        if (raw_sym == XKB_KEY_p || raw_sym == XKB_KEY_P) {
            _war_preview_start_voice(env, env->wave_view_pitch, env->wave_view_layer);
            cur->prefix = 0;
            return;
        }
    }
    // crop mode: arrow keys adjust offset markers, space previews cropped range
    if (env->crop_active) {
        if (raw_sym == XKB_KEY_space) {
            int voice = _war_preview_start_voice(env, env->crop_pitch, env->crop_layer);
            if (voice >= 0) {
                env->preview_voice_read_pos[voice] = env->crop_start_frame * 2;
                env->preview_voice_read_limit[voice] = env->crop_end_frame * 2;
            }
            cur->prefix = 0;
            return;
        }
        if (_war_crop_adjust(env, raw_sym, mod)) {
            cur->prefix = 0;
            return;
        }
    }
    // visual mode: shift+motion moves selected notes
    if (mode == WAR_MODE_ID_VISUAL && (mod & MOD_SHIFT)) {
        if (raw_sym == XKB_KEY_l || raw_sym == XKB_KEY_Right) {
            war_visual_move_right(env); cur->prefix = 0; return;
        }
        if (raw_sym == XKB_KEY_h || raw_sym == XKB_KEY_Left) {
            war_visual_move_left(env); cur->prefix = 0; return;
        }
        if (raw_sym == XKB_KEY_k || raw_sym == XKB_KEY_Up) {
            war_visual_move_up(env); cur->prefix = 0; return;
        }
        if (raw_sym == XKB_KEY_j || raw_sym == XKB_KEY_Down) {
            war_visual_move_down(env); cur->prefix = 0; return;
        }
    }
    // master mode: up/down adjusts master gain (no mods), arrows+mod adjust ADSR
    if (mode == WAR_MODE_ID_MASTER) {
        if (raw_sym == XKB_KEY_Up && !(mod & (MOD_SHIFT | MOD_CTRL | MOD_ALT))) {
            env->master_gain += 10.0f;
            if (env->master_gain > 10000.0f) env->master_gain = 10000.0f;
            snprintf(env->status_msg, sizeof(env->status_msg), "MASTER %+.0f", env->master_gain);
            cur->prefix = 0;
            return;
        }
        if (raw_sym == XKB_KEY_Down && !(mod & (MOD_SHIFT | MOD_CTRL | MOD_ALT))) {
            env->master_gain -= 10.0f;
            if (env->master_gain < -10000.0f) env->master_gain = -10000.0f;
            snprintf(env->status_msg, sizeof(env->status_msg), "MASTER %+.0f", env->master_gain);
            cur->prefix = 0;
            return;
        }
        // M exits master mode
        if (raw_sym == XKB_KEY_M && (mod & MOD_SHIFT)) {
            env->active_mode = WAR_MODE_ID_ROLL;
            snprintf(env->status_msg, sizeof(env->status_msg), "MASTER %+.0f", env->master_gain);
            cur->prefix = 0;
            return;
        }
        // ADSR: adjust capture slot under cursor
        if (cur->instance_count) {
            double _agr = cur->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
            uint32_t _agp = _agr > 0 ? (uint32_t)(_agr + 0.5) : 0;
            if (_agp > 127) _agp = 127;
            uint32_t _agl = cur->layer;
            if (_agl < 1 || _agl > 9) _agl = 1;
            uint32_t _agi = _agp * WAR_CAPTURE_SLOT_LAYERS + (_agl - 1);
            war_capture_slot* _cs = &env->capture_slots[_agi];
            int8_t delta = 0;
            float* target = NULL;
            const char* name = "";
            float max_val = 1000.0f;
            if (mod & MOD_ALT) {
                target = &_cs->attack;
                name = "ATK";
            } else if (mod & MOD_SHIFT) {
                target = &_cs->sustain;
                name = "SUS";
            } else if (mod & MOD_CTRL) {
                target = &_cs->release;
                name = "REL";
            }
            if (target) {
                if (raw_sym == XKB_KEY_Up || raw_sym == XKB_KEY_Right) delta = 10;
                if (raw_sym == XKB_KEY_Down || raw_sym == XKB_KEY_Left) delta = -10;
                if (delta) {
                    float _minv = (target == &_cs->sustain) ? -max_val : 0.0f;
                    *target += (float)delta;
                    if (*target < _minv) *target = _minv;
                    if (*target > max_val) *target = max_val;
                    snprintf(env->status_msg, sizeof(env->status_msg), "%s %.0f A%.0f S%.0f R%.0f",
                             name, *target,
                             _cs->attack, _cs->sustain, _cs->release);
                    cur->prefix = 0;
                    return;
                }
            }
        }
    }
    // gain adjust: ctrl+up / ctrl+down
    if ((mode == WAR_MODE_ID_ROLL || mode == WAR_MODE_ID_VISUAL) && env->ctx_cursor->instance_count) {
        double _gr = cur->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
        uint32_t _gp = _gr > 0 ? (uint32_t)(_gr + 0.5) : 0;
        if (_gp > 127) _gp = 127;
        uint32_t _gl = cur->layer;
        if (_gl < 1 || _gl > 9) _gl = 1;
        uint32_t _gi = _gp * WAR_CAPTURE_SLOT_LAYERS + (_gl - 1);
        war_capture_slot* _gs = &env->capture_slots[_gi];
        if (_gs->samples && _gs->count > 0) {
            if (raw_sym == XKB_KEY_Up && (mod & MOD_CTRL) && !(mod & MOD_SHIFT)) {
                _gs->gain += 10.0f;
                if (_gs->gain > 10000.0f) _gs->gain = 10000.0f;
                snprintf(env->status_msg, sizeof(env->status_msg), "G%+.0f", _gs->gain);
                cur->prefix = 0;
                return;
            }
            if (raw_sym == XKB_KEY_Down && (mod & MOD_CTRL) && !(mod & MOD_SHIFT)) {
                _gs->gain -= 10.0f;
                if (_gs->gain < -10000.0f) _gs->gain = -10000.0f;
                snprintf(env->status_msg, sizeof(env->status_msg), "G%+.0f", _gs->gain);
                cur->prefix = 0;
                return;
            }
            if (raw_sym == XKB_KEY_Left && (mod & MOD_CTRL) && !(mod & MOD_SHIFT)) {
                _gs->pan -= 10;
                if (_gs->pan < -1000) _gs->pan = -1000;
                snprintf(env->status_msg, sizeof(env->status_msg), "P%+d", _gs->pan);
                cur->prefix = 0;
                return;
            }
            if (raw_sym == XKB_KEY_Right && (mod & MOD_CTRL) && !(mod & MOD_SHIFT)) {
                _gs->pan += 10;
                if (_gs->pan > 1000) _gs->pan = 1000;
                snprintf(env->status_msg, sizeof(env->status_msg), "P%+d", _gs->pan);
                cur->prefix = 0;
                return;
            }
            // pass filter adjust: ctrl+shift+up / ctrl+shift+down
            if ((mod & (MOD_CTRL | MOD_SHIFT)) == (MOD_CTRL | MOD_SHIFT)) {
                if (raw_sym == XKB_KEY_Up) {
                    _gs->eq += 10;
                    if (_gs->eq > 1000) _gs->eq = 1000;
                    snprintf(env->status_msg, sizeof(env->status_msg), "P%+d", _gs->eq);
                    cur->prefix = 0;
                    return;
                }
                if (raw_sym == XKB_KEY_Down) {
                    _gs->eq -= 10;
                    if (_gs->eq < -1000) _gs->eq = -1000;
                    snprintf(env->status_msg, sizeof(env->status_msg), "P%+d", _gs->eq);
                    cur->prefix = 0;
                    return;
                }
            }
        }
    }
    // tap tempo: space records a tap
    if (env->tap_tempo_active && raw_sym == XKB_KEY_space) {
        uint64_t _now = war_get_monotonic_time_us();
        uint32_t _idx = env->tap_tempo_count % 16;
        env->tap_tempo_times[_idx] = _now;
        env->tap_tempo_count++;
        uint32_t _n = env->tap_tempo_count;
        if (_n >= 2) {
            uint64_t _sum = 0;
            int _count = 0;
            uint32_t _start = _n > 4 ? (_n - 4) : 0;
            for (uint32_t _t = _start; _t < _n - 1; _t++) {
                _sum += env->tap_tempo_times[(_t + 1) % 16] - env->tap_tempo_times[_t % 16];
                _count++;
            }
            if (_count > 0) {
                double _avg_us = (double)_sum / (double)_count;
                double _bpm = 60.0 * 1000000.0 / _avg_us;
                if (_bpm > 20.0 && _bpm < 400.0) {
                    env->atomics->bpm = (float)_bpm;
                    int _disp = (int)(_bpm + 0.5);
                    snprintf(env->status_msg, sizeof(env->status_msg), "TAP: %d BPM", _disp);
                } else {
                    snprintf(env->status_msg, sizeof(env->status_msg), "TAP: %.0f BPM (out of range)", _bpm);
                }
            }
        } else {
            snprintf(env->status_msg, sizeof(env->status_msg), "TAP: keep tapping...");
        }
        cur->prefix = 0;
        return;
    }
    if (raw_sym == XKB_KEY_colon) {
        env->cmd_active = 1;
        env->status_msg[0] = '\0';
        env->cmd_len = 1;
        env->cmd_buf[0] = ':';
        env->cmd_buf[1] = '\0';
        cur->prefix = 0;
        // suppress repeat — command mode handles text separately
        ctx_wayland->repeat_active = 0;
        struct itimerspec off = {0};
        timerfd_settime(ctx_wayland->repeat_timer_fd, 0, &off, NULL);
        return;
    }

    // direct $ handler (bypasses keymap for reliability)
    if (raw_sym == XKB_KEY_dollar && !(mod & MOD_ALT) && mode == WAR_MODE_ID_ROLL && !env->cmd_active) {
        war_roll_cursor_goto_right_bound_or_prefix_horizontal(env);
        ctx_wayland->keymap_state = 0;
        if (!is_digit) cur->prefix = 0;
        return;
    }

    // zoom: '=' in, '-' out (no modifiers, any mode except cmd)
    if (!(mod & (MOD_SHIFT | MOD_CTRL | MOD_ALT)) && !env->cmd_active) {
        if (raw_sym == XKB_KEY_equal) { war_zoom_in(env); cur->prefix = 0; return; }
        if (raw_sym == XKB_KEY_minus) { war_zoom_out(env); cur->prefix = 0; return; }
    }

    // Shift+M enters master mode
    if (raw_sym == XKB_KEY_M && (mod & MOD_SHIFT) && !(mod & (MOD_CTRL | MOD_ALT)) && mode == WAR_MODE_ID_ROLL && !env->cmd_active) {
        env->active_mode = WAR_MODE_ID_MASTER;
        cur->prefix = 0;
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
    // clamp cursor above gutter after any keymap function dispatch
    if (cur->instance_count && cur->instance[0].pos[1] < ctx_wayland->gutter_rows)
        cur->instance[0].pos[1] = ctx_wayland->gutter_rows;
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
    (void)data;
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
        if (written < max)
            memset((uint8_t*)dst + written, 0, max - written);
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
            pw_properties_new("media.name", "war-capture",
                              "node.latency", "128/48000",
                              NULL);
        env->ctx_pw->capture_stream = pw_stream_new(core, "war-capture", props);
        struct spa_audio_info_raw capture_info = {
            .format = SPA_AUDIO_FORMAT_F32,
            .rate = 48000,
            .channels = 2,
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
            pw_properties_new("media.name", "war-play",
                              "node.latency", "128/48000",
                              NULL);
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
                                       PW_STREAM_FLAG_MAP_BUFFERS |
                                       PW_STREAM_FLAG_RT_PROCESS,
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
                              "node.latency", "128/48000",
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
    //war_key();
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
    env->active_mode = WAR_MODE_ID_ROLL;
    env->master_gain = 0.0f;
    env->undo_count = 0;
    env->undo_pos = 0;
    env->undo_note_counts = calloc(WAR_UNDO_MAX, sizeof(uint32_t));
    env->undo_notes = calloc(WAR_UNDO_MAX, sizeof(war_new_vulkan_note_instance*));
    env->undo_audio_data = calloc(WAR_UNDO_MAX, sizeof(uint8_t*));
    env->undo_audio_size = calloc(WAR_UNDO_MAX, sizeof(uint64_t));
    env->across_radius = 16;
    env->across_resample = 0;
    ctx_hot->fn_id[0] = WAR_HOT_ID_COLOR;
    ctx_hot->fn_id[1] = WAR_HOT_ID_KEYMAP;
    ctx_hot->fn_id[2] = WAR_HOT_ID_COMMAND;
    ctx_hot->fn_id[3] = WAR_HOT_ID_PLUGIN;
    ctx_hot->fn_count = 4;
    war_override(ctx_hot->fn_count, ctx_hot->fn_id, env);
    // set ADSR defaults after override (plugins may reset pool)
    for (int i = 0; i < 128 * WAR_CAPTURE_SLOT_LAYERS; i++) {
        env->capture_slots[i].attack = 0.0f;
        env->capture_slots[i].sustain = 0.0f;
        env->capture_slots[i].release = 0.0f;
        env->capture_slots[i].eq = 0;
    }
    call_king_terry("INIT: eq[0]=%d", env->capture_slots[0].eq);
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
    ctx_wayland->keymap_state = 0;
    ctx_wayland->keymap_state_time = 0;
    ctx_wayland->repeat_active = 0;
    ctx_wayland->repeat_rate = 20;
    ctx_wayland->repeat_delay = 200;
    ctx_wayland->repeat_timer_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    WASSERT(ctx_wayland->repeat_timer_fd >= 0);
    ctx_wayland->audio_timer_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    WASSERT(ctx_wayland->audio_timer_fd >= 0);
    struct itimerspec ats = {
        .it_value = {0, 500000L},     // 0.5ms first shot
        .it_interval = {0, 1000000L}, // 1ms period
    };
    timerfd_settime(ctx_wayland->audio_timer_fd, 0, &ats, NULL);

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
    // init freetype and derive cell size from embedded FreeMono.otf
    if (FT_Init_FreeType(&env->ft_lib)) {
        call_king_terry("freetype init failed");
    }
    if (FT_New_Memory_Face(env->ft_lib, assets_fonts_FreeMono_otf,
                           assets_fonts_FreeMono_otf_len, 0, &env->ft_face)) {
        call_king_terry("freetype: failed to load embedded FreeMono.otf");
    }
    FT_Set_Pixel_Sizes(env->ft_face, 0, 24);
    FT_Load_Char(env->ft_face, '*', FT_LOAD_DEFAULT);
    ctx_cursor->cell_width = (double)(env->ft_face->glyph->advance.x >> 6);
    ctx_cursor->cell_height = (double)(env->ft_face->size->metrics.height >> 6);
    ctx_wayland->gutter_rows = 4;
    ctx_wayland->top_bound = 127.0 + ctx_wayland->gutter_rows;
    ctx_wayland->gutter_cols = 4;
    ctx_wayland->num_cols = ctx_wayland->width / ctx_cursor->cell_width;
    ctx_wayland->num_rows = ctx_wayland->height / ctx_cursor->cell_height;
    ctx_cursor->x_width =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_X_WIDTH);
    ctx_cursor->x_width[0] = 1;
    war_cursor_init(ctx_cursor, ctx_pool, ctx_config, ctx_vk);
    ctx_cursor->instance_count = 1;
    ctx_cursor->instance[0].pos[0] = ctx_wayland->gutter_cols;
    ctx_cursor->instance[0].pos[1] = 60.0 + (double)ctx_wayland->gutter_rows;
    ctx_cursor->instance[0].size[0] = 1;
    ctx_cursor->instance[0].size[1] = 1;
    ctx_cursor->layer = 1;
    ctx_cursor->octave = 4;
    ctx_cursor->step = 1.0;
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
    double max_pan = (double)ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
    if ((double)ctx_wayland->panning[1] > max_pan) ctx_wayland->panning[1] = (float)max_pan;
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

    // enumerate audio sources for device selector HUD
    env->dev_count = 1;
    env->dev_names = calloc(128, sizeof(char*));
    env->dev_names[0] = strdup("loopback");
    // clean up any stale virtual loopback sinks from previous run
    system("pactl list-modules 2>/dev/null | grep 'war_loopback' | grep -o '[0-9]*' | while read m; do pactl unload-module $m 2>/dev/null; done");
    {
        char buf[256];
        FILE* pw_fp = popen("pactl list sources short 2>/dev/null | cut -f2", "r");
        if (pw_fp) {
            while (fgets(buf, sizeof(buf), pw_fp) && env->dev_count < 128) {
                size_t len = strlen(buf);
                if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
                if (len > 1 && env->dev_count < 128) {
                    env->dev_names[env->dev_count] = strdup(buf);
                    env->dev_count++;
                }
            }
            pclose(pw_fp);
        }
    }
    // create additional virtual loopback sinks for capture modes
    system("pactl unload-module $(pactl list-modules | grep 'war_loopback' | grep -o 'Module #[0-9]*' | cut -d# -f2) 2>/dev/null");
    for (int _li = 0; _li < 4; _li++) env->loopback_modules[_li] = 0;
    for (int _li = 2; _li <= 4; _li++) {
        char _lb[256];
        snprintf(_lb, sizeof(_lb), "war_loopback%d", _li);
        if (env->dev_count < 128) {
            char _nname[64];
            snprintf(_nname, sizeof(_nname), "%s.monitor", _lb);
            env->dev_names[env->dev_count++] = strdup(_nname);
        }
        char _cmd[256];
        snprintf(_cmd, sizeof(_cmd), "pactl load-module module-null-sink sink_name=%s", _lb);
        FILE* _fp = popen(_cmd, "r");
        if (_fp) {
            if (fgets(_cmd, sizeof(_cmd), _fp))
                env->loopback_modules[_li - 2] = (uint32_t)atol(_cmd);
            pclose(_fp);
        }
    }

    // load global device config
    {
        FILE* _gf = fopen("global_war.config", "r");
        if (_gf) {
            char _gbuf[256];
            for (int _gi = 0; _gi < 4 && fgets(_gbuf, sizeof(_gbuf), _gf); _gi++) {
                size_t _gl = strlen(_gbuf);
                if (_gl > 0 && _gbuf[_gl-1] == '\n') _gbuf[_gl-1] = '\0';
                if (_gbuf[0]) {
                    free(env->dev_nodes[_gi]);
                    env->dev_nodes[_gi] = strdup(_gbuf);
                }
            }
            fclose(_gf);
        }
    }

    // init capture slots and accumulator
    memset(env->capture_slots, 0, sizeof(env->capture_slots));
    for (int _gi = 0; _gi < 128 * WAR_CAPTURE_SLOT_LAYERS; _gi++) {
        env->capture_slots[_gi].gain = 0.0f;
        env->capture_slots[_gi].eq = 100;
    }
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
    // loop end marker (purple, instance 1)
    ctx_line->instance[1].pos[0] = 0.0f;
    ctx_line->instance[1].pos[1] = ctx_wayland->gutter_rows;
    ctx_line->instance[1].pos[2] = 0.0f;
    ctx_line->instance[1].size[0] = 0.0f;
    ctx_line->instance[1].size[1] = 128.0f - ctx_wayland->gutter_rows;
    ctx_line->instance[1].width = 0.08f;
    ctx_line->instance[1].color[0] = 0.6f;
    ctx_line->instance[1].color[1] = 0.0f;
    ctx_line->instance[1].color[2] = 0.8f; // purple
    ctx_line->instance[1].color[3] = 1.0f;
    ctx_line->instance[1].flags = 0;
    // loop start marker (orange, instance 2)
    ctx_line->instance[2].pos[0] = (float)ctx_wayland->gutter_cols;
    ctx_line->instance[2].pos[1] = ctx_wayland->gutter_rows;
    ctx_line->instance[2].pos[2] = 0.0f;
    ctx_line->instance[2].size[0] = 0.0f;
    ctx_line->instance[2].size[1] = 128.0f - ctx_wayland->gutter_rows;
    ctx_line->instance[2].width = 0.08f;
    ctx_line->instance[2].color[0] = 1.0f;
    ctx_line->instance[2].color[1] = 0.5f;
    ctx_line->instance[2].color[2] = 0.0f; // orange
    ctx_line->instance[2].color[3] = 1.0f;
    ctx_line->instance[2].flags = 0;
    ctx_line->instance_count = 3;
    // playback bar state
    env->play_bar_playing = 0;
    env->play_bar_mute_mask = 0;
    env->play_bar_position_seconds = 0.0;
    env->play_bar_last_frame_ms = 0;
    env->play_bar_last_us = 0;
    env->play_bar_prev_cell_pos = (double)ctx_wayland->gutter_cols;
    env->loop_start_col = 0.0f;
    env->loop_end_col = 0.0f;
    memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
    memset(env->play_bar_direct_filter_lp, 0, sizeof(env->play_bar_direct_filter_lp));
    env->layer_visible = 0x1FF; // all 9 layers visible
    //-------------------------------------------------------------------------
    // FONT INIT (renders 'M' glyph, sets up Vulkan text pipeline)
    //-------------------------------------------------------------------------
    env->ctx_font = calloc(1, sizeof(war_font_context));
    war_font_init(env->ctx_font, ctx_vk, env->ft_face,
                  ctx_cursor->cell_width, ctx_cursor->cell_height);
    //-------------------------------------------------------------------------
    // FIRST FRAME RENDER (record + submit)
    //-------------------------------------------------------------------------
    war_render_frame(ctx_wayland, ctx_vk, ctx_color);
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
    struct pollfd pfds[3] = {
        {.fd = wayland_fd, .events = POLLIN},
        {.fd = ctx_wayland->repeat_timer_fd, .events = POLLIN},
        {.fd = ctx_wayland->audio_timer_fd, .events = POLLIN},
    };
    while (ctx_wayland->running) {
        wl_display_flush(ctx_wayland->display);
        if (wl_display_prepare_read(ctx_wayland->display) == 0) {
            poll(pfds, 3, 100);
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
                // skip top-row play keys – they are hold-to-play
                if (ctx_wayland->repeat_sym == XKB_KEY_q ||
                    ctx_wayland->repeat_sym == XKB_KEY_w ||
                    ctx_wayland->repeat_sym == XKB_KEY_e ||
                    ctx_wayland->repeat_sym == XKB_KEY_r ||
                    ctx_wayland->repeat_sym == XKB_KEY_t ||
                    ctx_wayland->repeat_sym == XKB_KEY_y ||
                    ctx_wayland->repeat_sym == XKB_KEY_u ||
                    ctx_wayland->repeat_sym == XKB_KEY_i ||
                    ctx_wayland->repeat_sym == XKB_KEY_o ||
                    ctx_wayland->repeat_sym == XKB_KEY_p ||
                    ctx_wayland->repeat_sym == XKB_KEY_bracketleft ||
                    ctx_wayland->repeat_sym == XKB_KEY_bracketright)
                    continue;
                for (uint64_t i = 0; i < exp; i++) {
                    war_env* env = ctx_wayland->env;
                    // crop mode: arrow keys handled inline, skip keymap dispatch
                    if (env->crop_active) {
                        uint32_t rsym = ctx_wayland->repeat_sym;
                        uint32_t rmod = ctx_wayland->repeat_mod;
                        if (rsym == XKB_KEY_Left || rsym == XKB_KEY_Right) {
                            _war_crop_adjust(env, rsym, rmod);
                            continue;
                        }
                    }
                    // master mode repeat: up/down adjusts master gain, arrows+mod adjust ADSR
                    if (env->active_mode == WAR_MODE_ID_MASTER) {
                        uint32_t _mrsym = ctx_wayland->repeat_sym;
                        uint32_t _mrmod = ctx_wayland->repeat_mod;
                        // ADSR editing with modifiers
                        if (env->ctx_cursor->instance_count) {
                            double _rmgr = env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows;
                            uint32_t _rmgp = _rmgr > 0 ? (uint32_t)(_rmgr + 0.5) : 0;
                            if (_rmgp > 127) _rmgp = 127;
                            uint32_t _rmgl = env->ctx_cursor->layer;
                            if (_rmgl < 1 || _rmgl > 9) _rmgl = 1;
                            uint32_t _rmgi = _rmgp * WAR_CAPTURE_SLOT_LAYERS + (_rmgl - 1);
                            war_capture_slot* _rmcs = &env->capture_slots[_rmgi];
                            int8_t _rmdelta = 0;
                            if (_mrsym == XKB_KEY_Up || _mrsym == XKB_KEY_Right) _rmdelta = 10;
                            if (_mrsym == XKB_KEY_Down || _mrsym == XKB_KEY_Left) _rmdelta = -10;
                            if (_rmdelta != 0) {
                                if (_mrmod & MOD_ALT) {
                                    _rmcs->attack += (float)_rmdelta;
                                    if (_rmcs->attack < 0.0f) _rmcs->attack = 0.0f;
                                    if (_rmcs->attack > 1000.0f) _rmcs->attack = 1000.0f;
                                    continue;
                                } else if (_mrmod & MOD_SHIFT) {
                                    _rmcs->sustain += (float)_rmdelta;
                                    if (_rmcs->sustain < -1000.0f) _rmcs->sustain = -1000.0f;
                                    if (_rmcs->sustain > 1000.0f) _rmcs->sustain = 1000.0f;
                                    continue;
                                } else if (_mrmod & MOD_CTRL) {
                                    _rmcs->release += (float)_rmdelta;
                                    if (_rmcs->release < 0.0f) _rmcs->release = 0.0f;
                                    if (_rmcs->release > 1000.0f) _rmcs->release = 1000.0f;
                                    continue;
                                }
                            }
                        }
                        // fallback: master gain
                        if (_mrsym == XKB_KEY_Up) {
                            env->master_gain += 10.0f;
            if (env->master_gain > 10000.0f) env->master_gain = 10000.0f;
                            continue;
                        }
                        if (_mrsym == XKB_KEY_Down) {
                            env->master_gain -= 10.0f;
                            if (env->master_gain < -10000.0f) env->master_gain = -10000.0f;
                            continue;
                        }
                    }
                    // gain/pan repeat: ctrl+arrows handled inline
                    if (ctx_wayland->repeat_mod & MOD_CTRL) {
                        uint32_t _rsym = ctx_wayland->repeat_sym;
                        uint32_t _rmod = ctx_wayland->repeat_mod;
                        war_cursor_context* _rcur = env->ctx_cursor;
                        if ((_rsym == XKB_KEY_Up || _rsym == XKB_KEY_Down ||
                             _rsym == XKB_KEY_Left || _rsym == XKB_KEY_Right) &&
                            _rcur && _rcur->instance_count &&
                            (env->active_mode == WAR_MODE_ID_ROLL || env->active_mode == WAR_MODE_ID_VISUAL)) {
                            double _rgr = _rcur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows;
                            uint32_t _rgp = _rgr > 0 ? (uint32_t)(_rgr + 0.5) : 0;
                            if (_rgp > 127) _rgp = 127;
                            uint32_t _rgl = _rcur->layer;
                            if (_rgl < 1 || _rgl > 9) _rgl = 1;
                            uint32_t _rgi = _rgp * WAR_CAPTURE_SLOT_LAYERS + (_rgl - 1);
                            war_capture_slot* _rgs = &env->capture_slots[_rgi];
                            if (_rgs->samples && _rgs->count > 0) {
                                if (_rsym == XKB_KEY_Up && !(_rmod & MOD_SHIFT)) {
                                    _rgs->gain += 10.0f;
                                     if (_rgs->gain > 10000.0f) _rgs->gain = 10000.0f;
                                 } else if (_rsym == XKB_KEY_Down && !(_rmod & MOD_SHIFT)) {
                                     _rgs->gain -= 10.0f;
                                     if (_rgs->gain < -10000.0f) _rgs->gain = -10000.0f;
                                } else if (_rsym == XKB_KEY_Left && !(_rmod & MOD_SHIFT)) {
                                    _rgs->pan -= 10;
                                    if (_rgs->pan < -1000) _rgs->pan = -1000;
                                } else if (_rsym == XKB_KEY_Right && !(_rmod & MOD_SHIFT)) {
                                    _rgs->pan += 10;
                                    if (_rgs->pan > 1000) _rgs->pan = 1000;
                                }
                                // pass filter: ctrl+shift+up / ctrl+shift+down
                                if ((_rmod & (MOD_CTRL | MOD_SHIFT)) == (MOD_CTRL | MOD_SHIFT)) {
                                    if (_rsym == XKB_KEY_Up) { _rgs->eq += 10; if (_rgs->eq > 1000) _rgs->eq = 1000; }
                                    if (_rsym == XKB_KEY_Down) { _rgs->eq -= 10; if (_rgs->eq < -1000) _rgs->eq = -1000; }
                                }
                            }
                            continue;
                        }
                    }
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
                        war_cursor_context* _rc = env->ctx_cursor;
                        if (_rc->instance_count && _rc->instance[0].pos[1] < env->ctx_wayland->gutter_rows)
                            _rc->instance[0].pos[1] = env->ctx_wayland->gutter_rows;
                    }
                }
            }
        }
        // drain audio timerfd (keep main loop cycling for playback)
        {
            uint64_t _ax;
            read(ctx_wayland->audio_timer_fd, &_ax, sizeof(_ax));
            // save expiration count for playbar advancement below
            ctx_wayland->audio_timer_exp = _ax;
        }
        // drain loopback (system audio) ring buffer into accumulator
        if (env->atomics->capture) {
            uint32_t hdr, sz;
            uint8_t buf[65536];
            const char* _dname = env->dev_nodes[env->capture_mode - 1];
            int _use_mic = _dname && strstr(_dname, "monitor") == NULL && strstr(_dname, "loopback") == NULL;
            war_producer_consumer* _dcap = _use_mic ? env->pc_capture : env->pc_loopback;
            while (war_pc_from_a(_dcap, &hdr, &sz, buf) && sz > 0) {
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
        // playback bar advancement (runs even without frame callbacks)
        // MOVED BEFORE mixing loop so playbar rendering uses current playhead
        double _pb_ccp = 0.0, _pb_spc = 0.0;
        if (env->play_bar_playing) {
            uint64_t _now_us = war_get_monotonic_time_us();
            if (!env->play_bar_last_us)
                env->play_bar_last_us = _now_us - 1000;
            uint64_t _delta_us = _now_us - env->play_bar_last_us;
            env->play_bar_last_us = _now_us;
            env->play_bar_position_seconds += (double)_delta_us / 1000000.0;
            double _bpm = env->atomics->bpm;
            if (_bpm <= 0.0) _bpm = 100.0;
            _pb_spc = 15.0 / _bpm;
            _pb_ccp = (double)ctx_wayland->gutter_cols + env->play_bar_position_seconds / _pb_spc;
        }
        // unified audio mixing: preview (MIDI) voices + playbar voices
        // NOTE: playhead advancement is NOW ABOVE, so voices activated here
        // are picked up by the mixing loop in the SAME iteration
        {
            enum { PW_CHUNK_FLOATS = 64 };
            enum { _TOTAL_VOICES = WAR_PREVIEW_VOICES + WAR_PLAY_BAR_VOICES };
            uint64_t voice_batch[_TOTAL_VOICES];
            // activate playbar voices: scan notes at the current playhead position
            if (env->play_bar_playing && env->ctx_note) {
                uint32_t _nc = env->ctx_note->instance_count;
                // compute mute mask from mute notes
    env->play_bar_mute_mask = 0;
                for (uint32_t _mi = 0; _mi < _nc; _mi++) {
                    if (env->ctx_note->instance[_mi].flags & WAR_NEW_VULKAN_FLAGS_MUTE) {
                        double _ms = env->ctx_note->instance[_mi].pos[0];
                        double _me = _ms + env->ctx_note->instance[_mi].size[0];
                        if (_pb_ccp >= _ms && _pb_ccp < _me) {
                            uint32_t _mm = (env->ctx_note->instance[_mi].flags >> 8) & 0x1FF;
                            env->play_bar_mute_mask |= _mm;
                        }
                    }
                }
                for (uint32_t _i = 0; _i < _nc; _i++) {
                    double _ns = env->ctx_note->instance[_i].pos[0];
                    double _ne = _ns + env->ctx_note->instance[_i].size[0];
                    if (_pb_ccp >= _ns && _pb_ccp < _ne) {
                        uint32_t _pp = (uint32_t)(env->ctx_note->instance[_i].pos[1] - (double)ctx_wayland->gutter_rows);
                        if (_pp > 127) _pp = 127;
                        uint32_t _li = (env->ctx_note->instance[_i].flags >> 4) & 0xF;
                        if (env->ctx_note->instance[_i].flags & WAR_NEW_VULKAN_FLAGS_MUTE) continue;
                        if (_li >= 1 && _li <= 9 && (env->play_bar_mute_mask & (1 << (_li - 1)))) continue;
                        if (_li < 1 || _li > 9) _li = 1;
                        if (!(env->layer_visible & (1 << (_li - 1)))) continue;
                        uint32_t _si = _pp * WAR_CAPTURE_SLOT_LAYERS + (_li - 1);
                        war_capture_slot* _sl = &env->capture_slots[_si];
                        if (!_sl->samples || _sl->count < 2) continue;
                        uint64_t _tik = env->ctx_note->instance[_i].tick;
                        int _already = 0;
                        for (uint32_t _v2 = 0; _v2 < WAR_PLAY_BAR_VOICES; _v2++)
                            if (env->play_bar_voice_active[_v2] && env->play_bar_voice_note[_v2] == _pp && env->play_bar_voice_layer[_v2] == _li && env->play_bar_voice_tick[_v2] == _tik)
                                { _already = 1; break; }
                        if (_already) continue;
                        double _dc = env->ctx_note->instance[_i].size[0];
                        uint64_t _mf = (uint64_t)(_dc * _pb_spc * 48000.0 * 2.0);
                        if (_mf & 1) _mf &= ~1ULL;
                        uint64_t _off2 = 0;
                        if (_pb_ccp > _ns) {
                            double _oc2 = _pb_ccp - _ns;
                            _off2 = (uint64_t)(_oc2 * _pb_spc * 48000.0 * 2.0);
                            if (_off2 & 1) _off2 &= ~1ULL;
                        }
                        if (_off2 >= _mf) continue;
                        if (_sl->count > 0 && _off2 >= _sl->count)
                            continue;
                        for (uint32_t _v = 0; _v < WAR_PLAY_BAR_VOICES; _v++) {
                            if (env->play_bar_voice_active[_v] == 0) {
                                // skip if same note already playing as preview voice (recording mode double-play)
                                int _pv_active = 0;
                                for (uint32_t _pv = 0; _pv < WAR_PREVIEW_VOICES; _pv++) {
                                    if (env->preview_voice_active[_pv] && env->preview_voice_note[_pv] == _pp && env->preview_voice_layer[_pv] == _li) {
                                        _pv_active = 1; break;
                                    }
                                }
                                if (_pv_active) break;
                                env->play_bar_voice_note[_v] = _pp;
                                env->play_bar_voice_layer[_v] = _li;
                                env->play_bar_voice_tick[_v] = _tik;
                                env->play_bar_voice_read_pos[_v] = _off2;
                                env->play_bar_voice_read_limit[_v] = _mf;
                                if (_sl->count > 0 && env->play_bar_voice_read_limit[_v] > _sl->count)
                                    env->play_bar_voice_read_limit[_v] = _sl->count;
                                env->play_bar_voice_filter_lp[_v][0] = 0.0f;
                                env->play_bar_voice_filter_lp[_v][1] = 0.0f;
                                env->play_bar_voice_filter_lp[_v][2] = 0.0f;
                                env->play_bar_voice_env_samples[_v] = 0;
                                env->play_bar_voice_active[_v] = 1;
                                break;
                            }
                        }
                    }
                }
            }
            int any_active = 0;
            for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++)
                if (env->preview_voice_active[v]) { any_active = 1; break; }
            if (!any_active && env->play_bar_playing) {
                for (uint32_t v = 0; v < WAR_PLAY_BAR_VOICES; v++)
                    if (env->play_bar_voice_active[v] == 1) { any_active = 1; break; }
            }
            uint32_t _pb_chunks = 0;
            while ((any_active || env->play_bar_playing) && _pb_chunks < 2) {
                float mix[PW_CHUNK_FLOATS];
                memset(mix, 0, sizeof(mix));
                any_active = 0;
                // mix preview voices
                for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
                    voice_batch[v] = 0;
                    if (!env->preview_voice_active[v]) continue;
                    uint32_t note = env->preview_voice_note[v];
                    if (note > 127) note = 127;
                    uint32_t layer = env->preview_voice_layer[v];
                    if (layer < 1 || layer > 9) {
                        env->preview_voice_active[v] = 0;
                        continue;
                    }
                    if (!(env->layer_visible & (1 << (layer - 1)))) {
                        env->preview_voice_active[v] = 0;
                        continue;
                    }
                    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                    war_capture_slot* slot = &env->capture_slots[idx];
                    if (!slot->samples || slot->count < 2) {
                        env->preview_voice_active[v] = 0;
                        continue;
                    }
                    uint64_t read_pos = env->preview_voice_read_pos[v];
                    uint64_t slot_avail = slot->samples ? slot->count : 0;
                    uint64_t read_limit = env->preview_voice_read_limit[v];
                    if (read_limit > 0 && read_limit < slot_avail)
                        slot_avail = read_limit;
                    if (read_pos >= slot_avail) {
                        if (env->loop_mode) {
                            env->preview_voice_read_pos[v] = 0;
                            read_pos = 0;
                        } else {
                            env->preview_voice_active[v] = 0;
                            continue;
                        }
                    }
                    uint64_t avail = slot_avail - read_pos;
                    if (avail < 2) {
                        if (env->loop_mode) {
                            env->preview_voice_read_pos[v] = 0;
                            read_pos = 0;
                            avail = slot_avail;
                        } else {
                            env->preview_voice_active[v] = 0;
                            continue;
                        }
                    }
                    uint64_t batch = avail < PW_CHUNK_FLOATS ?
                                         (avail & ~1ULL) :
                                         PW_CHUNK_FLOATS;
                    voice_batch[v] = batch;
                    float _gm = (slot->gain + 10000.0f) / 10000.0f;
                    float _pp = (float)(slot->pan + 1000) / 2000.0f;
                    float _pl = sinf((1.0f - _pp) * (float)(M_PI / 2.0));
                    float _pr = sinf(_pp * (float)(M_PI / 2.0));
                    for (uint64_t f = 0; f < batch; f += 2) {
                        float _s_l = slot->samples[read_pos + f];
                        float _s_r = slot->samples[read_pos + f + 1];
                        float _ae = (float)fabsf((float)slot->eq);
                        float _fc;
                        if (slot->eq <= 0)
                            _fc = 20000.0f * expf(logf(20.0f / 20000.0f) * _ae / 1000.0f);
                        else
                            _fc = 20.0f * expf(logf(20000.0f / 20.0f) * _ae / 1000.0f);
                        float _alpha_target = 1.0f - expf(-2.0f * (float)M_PI * _fc / 48000.0f);
                        if (_alpha_target > 1.0f) _alpha_target = 1.0f;
                        if (f == 0 && env->preview_voice_env_samples[v] == 0) {
                            env->preview_voice_filter_lp[v][0] = _s_l;
                            env->preview_voice_filter_lp[v][1] = _s_r;
                            env->preview_voice_filter_lp[v][2] = _alpha_target;
                        }
                        float _alpha = env->preview_voice_filter_lp[v][2];
                        _alpha += 0.2f * (_alpha_target - _alpha);
                        env->preview_voice_filter_lp[v][2] = _alpha;
                        float _lp0 = env->preview_voice_filter_lp[v][0] + _alpha * (_s_l - env->preview_voice_filter_lp[v][0]);
                        float _lp1 = env->preview_voice_filter_lp[v][1] + _alpha * (_s_r - env->preview_voice_filter_lp[v][1]);
                        env->preview_voice_filter_lp[v][0] = _lp0;
                        env->preview_voice_filter_lp[v][1] = _lp1;
                        float _hp0 = _s_l - _lp0;
                        float _hp1 = _s_r - _lp1;
                        float _mix_l, _mix_r;
                        if (slot->eq <= 0) {
                            float _t = (float)(-slot->eq) / 1000.0f;
                            _mix_l = _s_l * (1.0f - _t) + _lp0 * _t;
                            _mix_r = _s_r * (1.0f - _t) + _lp1 * _t;
                        } else {
                            float _t = (float)slot->eq / 1000.0f;
                            _mix_l = _s_l * (1.0f - _t) + _hp0 * _t;
                            _mix_r = _s_r * (1.0f - _t) + _hp1 * _t;
                        }
                    float _a_g = _gm;
                    float _atk_samples = slot->attack / 1000.0f * 48000.0f;
                    float _sus_level = (slot->sustain + 1000.0f) / 1000.0f;
                    float _rel_samples = slot->release / 1000.0f * 48000.0f;
                    int64_t _rem_preview = (int64_t)(slot_avail - read_pos) - (int64_t)f;
                    float _env = _sus_level;
                    if (_atk_samples > 0.0f) {
                        uint64_t _elapsed = env->preview_voice_env_samples[v] + (uint64_t)f;
                        if (_elapsed < (uint64_t)_atk_samples)
                            _env = (float)_elapsed / _atk_samples * _sus_level;
                    }
                    if (_rem_preview < (int64_t)_rel_samples) {
                        if (_rem_preview <= 0) _env = 0.0f;
                        else _env = ((float)_rem_preview / _rel_samples) * _sus_level;
                    }
                    _a_g *= _env;
                    mix[f]   += _mix_l * _a_g * _pl;
                    mix[f+1] += _mix_r * _a_g * _pr;
                    mix[f]   += _mix_l * _a_g * _pl;
                    mix[f+1] += _mix_r * _a_g * _pr;
                    }
                    any_active = 1;
                }
                // mix playbar voices
                if (env->play_bar_playing) {
                    for (uint32_t v = 0; v < WAR_PLAY_BAR_VOICES; v++) {
                        uint32_t vi = WAR_PREVIEW_VOICES + v;
                        voice_batch[vi] = 0;
                        if (env->play_bar_voice_active[v] != 1) continue;
                        uint32_t note = env->play_bar_voice_note[v];
                        if (note > 127) note = 127;
                        uint32_t layer = env->play_bar_voice_layer[v];
                        if (layer < 1 || layer > 9) {
                            env->play_bar_voice_active[v] = 0;
                            continue;
                        }
                        if (!(env->layer_visible & (1 << (layer - 1)))) {
                            env->play_bar_voice_active[v] = 0;
                            continue;
                        }
                        if (env->play_bar_mute_mask & (1 << (layer - 1))) {
                            env->play_bar_voice_active[v] = 0;
                            continue;
                        }
                        uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                        war_capture_slot* slot = &env->capture_slots[idx];
                        uint64_t read_pos = env->play_bar_voice_read_pos[v];
                        uint64_t read_limit = env->play_bar_voice_read_limit[v];
                        if (read_pos >= read_limit) {
                            env->play_bar_voice_active[v] = 0;
                            continue;
                        }
                        uint64_t slot_avail = slot->samples ? slot->count : 0;
                        if (slot_avail == 0) {
                            env->play_bar_voice_active[v] = 0;
                            continue;
                        }
                        uint64_t remain = read_limit - read_pos;
                        if (remain < slot_avail && remain < PW_CHUNK_FLOATS) {
                            if (remain < 2) {
                                env->play_bar_voice_active[v] = 0;
                                continue;
                            }
                        }
                        uint64_t batch = PW_CHUNK_FLOATS;
                        if (batch > remain) batch = remain & ~1ULL;
                        // don't cross slot boundary within a batch
                        uint64_t slot_offset = read_pos;
                        uint64_t to_slot_end = slot_avail - slot_offset;
                        if (batch > to_slot_end) batch = to_slot_end & ~1ULL;
                        voice_batch[vi] = batch;
                        if (batch == 0) { env->play_bar_voice_active[v] = 0; continue; }
                        float _gm = (slot->gain + 10000.0f) / 10000.0f;
                        float _pp2 = (float)(slot->pan + 1000) / 2000.0f;
                        float _pl2 = sinf((1.0f - _pp2) * (float)(M_PI / 2.0));
                        float _pr2 = sinf(_pp2 * (float)(M_PI / 2.0));
                        for (uint64_t f = 0; f < batch; f += 2) {
                            float _s_l = slot->samples[slot_offset + f];
                            float _s_r = slot->samples[slot_offset + f + 1];
                            float _ae = (float)fabsf((float)slot->eq);
                            float _fc;
                            if (slot->eq <= 0)
                                _fc = 20000.0f * expf(logf(20.0f / 20000.0f) * _ae / 1000.0f);
                            else
                                _fc = 20.0f * expf(logf(20000.0f / 20.0f) * _ae / 1000.0f);
                            float _alpha_target = 1.0f - expf(-2.0f * (float)M_PI * _fc / 48000.0f);
                            if (_alpha_target > 1.0f) _alpha_target = 1.0f;
                            if (f == 0 && env->play_bar_voice_env_samples[v] == 0) {
                                env->play_bar_voice_filter_lp[v][0] = _s_l;
                                env->play_bar_voice_filter_lp[v][1] = _s_r;
                                env->play_bar_voice_filter_lp[v][2] = _alpha_target;
                            }
                            float _alpha = env->play_bar_voice_filter_lp[v][2];
                            _alpha += 0.2f * (_alpha_target - _alpha);
                            env->play_bar_voice_filter_lp[v][2] = _alpha;
                            float _lp0 = env->play_bar_voice_filter_lp[v][0] + _alpha * (_s_l - env->play_bar_voice_filter_lp[v][0]);
                            float _lp1 = env->play_bar_voice_filter_lp[v][1] + _alpha * (_s_r - env->play_bar_voice_filter_lp[v][1]);
                            env->play_bar_voice_filter_lp[v][0] = _lp0;
                            env->play_bar_voice_filter_lp[v][1] = _lp1;
                            float _hp0 = _s_l - _lp0;
                            float _hp1 = _s_r - _lp1;
                            float _mix_l, _mix_r;
                            if (slot->eq <= 0) {
                                float _t = (float)(-slot->eq) / 1000.0f;
                                _mix_l = _s_l * (1.0f - _t) + _lp0 * _t;
                                _mix_r = _s_r * (1.0f - _t) + _lp1 * _t;
                            } else {
                                float _t = (float)slot->eq / 1000.0f;
                                _mix_l = _s_l * (1.0f - _t) + _hp0 * _t;
                                _mix_r = _s_r * (1.0f - _t) + _hp1 * _t;
                            }
                            float _a_g2 = _gm;
                            float _atk_s = slot->attack / 1000.0f * 48000.0f;
                            float _sus_l = (slot->sustain + 1000.0f) / 1000.0f;
                            float _rel_s = slot->release / 1000.0f * 48000.0f;
                            int64_t _rem_playbar = (int64_t)remain - (int64_t)f;
                            float _env2 = _sus_l;
                            if (_atk_s > 0.0f) {
                                uint64_t _elapsed2 = env->play_bar_voice_env_samples[v] + (uint64_t)f;
                                if (_elapsed2 < (uint64_t)_atk_s)
                                    _env2 = (float)_elapsed2 / _atk_s * _sus_l;
                            }
                            if (_rem_playbar < (int64_t)_rel_s) {
                                if (_rem_playbar <= 0) _env2 = 0.0f;
                                else _env2 = ((float)_rem_playbar / _rel_s) * _sus_l;
                            }
                            _a_g2 *= _env2;
                            mix[f]   += _mix_l * _a_g2 * _pl2;
                            mix[f+1] += _mix_r * _a_g2 * _pr2;
                        }
                        any_active = 1;
                    }
                }
                if (!any_active && !env->play_bar_playing) break;
                if (env->master_gain != 0.0f) {
                    float _mg_live = (env->master_gain + 10000.0f) / 10000.0f;
                    for (int _mf = 0; _mf < PW_CHUNK_FLOATS; _mf++)
                        mix[_mf] *= _mg_live;
                }
                if (!war_pc_to_a(env->pc_play, 0, PW_CHUNK_FLOATS * 4, mix))
                    break;
                _pb_chunks++;
                // advance preview read positions
                for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
                    env->preview_voice_read_pos[v] += voice_batch[v];
                    if (env->preview_voice_active[v])
                        env->preview_voice_env_samples[v] += voice_batch[v];
                }
                // advance playbar read positions
                if (env->play_bar_playing) {
                    for (uint32_t v = 0; v < WAR_PLAY_BAR_VOICES; v++) {
                        env->play_bar_voice_read_pos[v] += voice_batch[WAR_PREVIEW_VOICES + v];
                        if (env->play_bar_voice_active[v] == 1)
                            env->play_bar_voice_env_samples[v] += voice_batch[WAR_PREVIEW_VOICES + v];
                    }
                }
                // continuously update note widths during recording
                if (env->recording_active) {
                    uint64_t now_us = war_get_monotonic_time_us();
                    double bpm = env->atomics->bpm;
                    if (bpm <= 0.0) bpm = 100.0;
                    double sec_per_cell = 15.0 / bpm;
                    for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
                        if (!env->preview_voice_active[v]) continue;
                        uint32_t ni = env->recording_note_idx[v];
                        if (ni >= env->ctx_note->instance_count) continue;
                        uint64_t elapsed_us = now_us - env->recording_press_time_us[v];
                        double width = (double)elapsed_us / 1000000.0 / sec_per_cell;
                        if (width < 1.0) width = 1.0;
                        env->ctx_note->instance[ni].size[0] = (float)width;
                    }
                }
            }
        }
        // playbar visual position update and loop detection
        if (env->play_bar_playing) {
            env->ctx_line->instance[0].pos[0] = (float)_pb_ccp;
            if (env->play_bar_loop) {
                double _rmax = env->loop_end_col > 0.0f ? (double)env->loop_end_col : 0.0;
                if (_rmax <= 0.0f && env->ctx_note && env->ctx_note->instance_count > 0) {
                    for (uint32_t _ri = 0; _ri < env->ctx_note->instance_count; _ri++) {
                        double _re = env->ctx_note->instance[_ri].pos[0] + env->ctx_note->instance[_ri].size[0];
                        if (_re > _rmax) _rmax = _re;
                    }
                }
                if (_rmax > 0.0f && _pb_ccp >= _rmax) {
                    // if capture is active, end capture and finalize note
                    if (env->atomics->capture && env->active_mode == WAR_MODE_ID_MIDI) {
                        env->atomics->capture = 0;
                        int32_t _cni = env->capture_note_idx;
                        if (_cni >= 0 && env->ctx_note && (uint32_t)_cni < env->ctx_note->instance_count) {
                            double _bpm3 = env->atomics->bpm;
                            if (_bpm3 <= 0.0) _bpm3 = 100.0;
                            double _spc3 = 15.0 / _bpm3;
                            float _pb3 = (float)((double)ctx_wayland->gutter_cols + env->play_bar_position_seconds / _spc3);
                            float _s2 = env->ctx_note->instance[_cni].pos[0];
                            env->ctx_note->instance[_cni].size[0] = _pb3 - _s2;
                            if (env->ctx_note->instance[_cni].size[0] < 0.02f)
                                env->ctx_note->instance[_cni].size[0] = 0.02f;
                        }
                        env->capture_note_idx = -1;
                    }
                    double _bpm2 = env->atomics->bpm;
                    if (_bpm2 <= 0.0) _bpm2 = 100.0;
                    double _spc2 = 15.0 / _bpm2;
                    double _start = env->loop_start_col > 0.0f ? (double)env->loop_start_col : (double)ctx_wayland->gutter_cols;
                    env->play_bar_position_seconds = (_start - (double)ctx_wayland->gutter_cols) * _spc2;
                    env->play_bar_last_frame_ms = 0;
                    env->play_bar_last_us = 0;
                    env->ctx_line->instance[0].pos[0] = (float)_start;
                    // reset filter state on loop
                    memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
                    memset(env->play_bar_direct_filter_lp, 0, sizeof(env->play_bar_direct_filter_lp));
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
    if (ctx_wayland->audio_timer_fd >= 0) close(ctx_wayland->audio_timer_fd);
    wl_buffer_destroy(ctx_wayland->buffer);
    xkb_state_unref(ctx_wayland->xkb_state);
    xkb_keymap_unref(ctx_wayland->xkb_keymap);
    xkb_context_unref(ctx_wayland->xkb_ctx);
    wl_keyboard_destroy(ctx_wayland->keyboard);
    FT_Done_Face(env->ft_face);
    FT_Done_FreeType(env->ft_lib);
    wl_display_disconnect(ctx_wayland->display);
    // unload virtual loopback sinks created at startup
    for (int _li = 0; _li < 3; _li++)
        if (env->loopback_modules[_li]) {
            char _uc[64];
            snprintf(_uc, sizeof(_uc), "pactl unload-module %u 2>/dev/null", env->loopback_modules[_li]);
            system(_uc);
        }
}
