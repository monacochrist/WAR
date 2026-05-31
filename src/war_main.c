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
                        (uint32_t)(env->ctx_note->instance[i].pos[1] - (double)ctx_wayland->gutter_rows);
                    if (pitch > 127) pitch = 127;
                    uint32_t layer_idx = (env->ctx_note->instance[i].flags >> 4) & 0xF;
                    if (layer_idx < 1 || layer_idx > 9) layer_idx = 1;
                    uint32_t idx =
                        pitch * WAR_CAPTURE_SLOT_LAYERS + (layer_idx - 1);
                    war_capture_slot* slot = &env->capture_slots[idx];
                    if (slot->samples && slot->count > 0) {
                        double dur_cells = env->ctx_note->instance[i].size[0];
                        uint64_t max_floats = (uint64_t)(dur_cells * 60.0 / bpm * 48000.0 * 2.0);
                        if (max_floats > slot->count) max_floats = slot->count;
                        if (max_floats & 1) max_floats &= ~1ULL;
                        for (uint32_t v = 0; v < WAR_PLAY_BAR_VOICES; v++) {
                            if (!env->play_bar_voice_active[v]) {
                                env->play_bar_voice_note[v] = pitch;
                                env->play_bar_voice_layer[v] = layer_idx;
                                env->play_bar_voice_read_pos[v] = 0;
                                env->play_bar_voice_read_limit[v] = max_floats;
                                env->play_bar_voice_active[v] = 1;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
        env->play_bar_prev_cell_pos = current_cell_pos;
        env->ctx_line->instance[0].pos[0] = (float)current_cell_pos;
    }
    // advance recording position based on real-time delta
    if (env->recording_active) {
        if (env->recording_last_frame_ms != 0) {
            uint32_t delta_ms = time - env->recording_last_frame_ms;
            double bpm = env->atomics->bpm;
            if (bpm <= 0.0) bpm = 100.0;
            double seconds_per_cell = 60.0 / bpm;
            env->recording_position += (double)delta_ms / 1000.0 / seconds_per_cell;
        }
        env->recording_last_frame_ms = time;
    }
    if (ctx_wayland->rendering) {
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
        fprintf(stderr, "EXPORT: no notes to export\n");
        return;
    }
    double bpm = env->atomics->bpm;
    if (bpm <= 0.0) bpm = 100.0;
    double sec_per_cell = 60.0 / bpm;
    uint32_t sr = 48000;
    uint32_t num_notes = env->ctx_note->instance_count;

    // compute total length: end of last note
    double total_sec = 0;
    for (uint32_t i = 0; i < num_notes; i++) {
        uint32_t pitch = (uint32_t)(env->ctx_note->instance[i].pos[1] - (double)env->ctx_wayland->gutter_rows);
        if (pitch > 127) pitch = 127;
        double dur_sec = env->ctx_note->instance[i].size[0] * sec_per_cell;
        double sample_sec = 0;
        for (uint32_t l = 0; l < WAR_CAPTURE_SLOT_LAYERS; l++) {
            uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + l;
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
        fprintf(stderr, "EXPORT: no audio data found for any note\n");
        return;
    }

    uint64_t total_frames = (uint64_t)(total_sec * sr) + 1;
    uint64_t total_floats = total_frames * 2;
    float* mix = calloc(total_floats, sizeof(float));
    if (!mix) {
        fprintf(stderr, "EXPORT: out of memory\n");
        return;
    }

    // mix each note
    for (uint32_t i = 0; i < num_notes; i++) {
        uint32_t pitch = (uint32_t)(env->ctx_note->instance[i].pos[1] - (double)env->ctx_wayland->gutter_rows);
        if (pitch > 127) pitch = 127;
        float* src = NULL;
        uint64_t src_count = 0;
        for (uint32_t l = 0; l < WAR_CAPTURE_SLOT_LAYERS; l++) {
            uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + l;
            if (env->capture_slots[idx].samples && env->capture_slots[idx].count > 0) {
                src = env->capture_slots[idx].samples;
                src_count = env->capture_slots[idx].count;
                break;
            }
        }
        if (!src) continue;

        double start_sec = (double)env->ctx_note->instance[i].pos[0] * sec_per_cell;
        uint64_t start_frame = (uint64_t)(start_sec * sr);
        double dur_sec = env->ctx_note->instance[i].size[0] * sec_per_cell;
        uint64_t dur_frames = (uint64_t)(dur_sec * sr);
        uint64_t src_frames = src_count / 2;
        if (dur_frames < src_frames) src_frames = dur_frames;
        for (uint64_t f = 0; f < src_frames && start_frame + f < total_frames; f++) {
            mix[(start_frame + f) * 2 + 0] += src[f * 2 + 0];
            mix[(start_frame + f) * 2 + 1] += src[f * 2 + 1];
        }
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
    fprintf(stderr, "EXPORT: wrote %s (%u frames, %.2f sec)\n",
            path, (unsigned)total_frames, total_sec);
}

static void war_save_project(war_env* env, const char* filename) {
    char path[1024];
    snprintf(path, sizeof(path), "%s", filename);
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "SAVE: failed to open %s\n", path);
        return;
    }
    fwrite("WARP", 1, 4, f);
    uint32_t version = 0;
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
            fwrite(env->capture_slots[i].samples, sizeof(float),
                   env->capture_slots[i].count, f);
        }
    }
    fclose(f);
    fprintf(stderr, "SAVE: wrote %s (%u notes, %u slots)\n",
            path, note_count, slot_count);
}

static void war_load_project(war_env* env, const char* filename) {
    char path[1024];
    snprintf(path, sizeof(path), "%s", filename);
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "LOAD: failed to open %s\n", path);
        return;
    }
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "WARP", 4) != 0) {
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
        if (idx < 128 * WAR_CAPTURE_SLOT_LAYERS && cnt > 0) {
            float* samples = malloc(cnt * sizeof(float));
            if (samples) {
                fread(samples, sizeof(float), cnt, f);
                env->capture_slots[idx].samples = samples;
                env->capture_slots[idx].count = cnt;
                env->capture_slots[idx].capacity = cnt;
            } else {
                fseek(f, cnt * sizeof(float), SEEK_CUR);
            }
        } else {
            fseek(f, cnt * sizeof(float), SEEK_CUR);
        }
    }
    fclose(f);
    fprintf(stderr, "LOAD: loaded %s (%u notes, %u slots, bpm=%.1f)\n",
            path, note_count, slot_count, bpm);
}

static void war_write_inst(war_env* env, int layer, const char* filename) {
    if (layer < 1 || layer > 9) {
        fprintf(stderr, "WRITEINST: layer must be 1-9\n");
        return;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s", filename);
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "WRITEINST: failed to open %s\n", path); return; }
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
    fprintf(stderr, "WRITEINST: wrote %s (layer=%d, %u pitches)\n", path, layer, count);
}

static void war_load_inst(war_env* env, int layer, const char* filename) {
    if (layer < 1 || layer > 9) {
        fprintf(stderr, "LOADINST: layer must be 1-9\n");
        return;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s", filename);
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "LOADINST: failed to open %s\n", path); return; }
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "WARI", 4) != 0) {
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
            } else {
                fseek(f, cnt * sizeof(float), SEEK_CUR);
            }
        } else {
            fseek(f, cnt * sizeof(float), SEEK_CUR);
        }
    }
    fclose(f);
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
                for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
                    if (ctx_wayland->env->preview_voice_active[v] &&
                        ctx_wayland->env->preview_voice_note[v] == rel_note) {
                        if (ctx_wayland->env->midi_toggle) break; // toggle mode: release does nothing
                        if (ctx_wayland->env->recording_active) {
                            uint32_t ni = ctx_wayland->env->recording_note_idx[v];
                            double start_col = ctx_wayland->env->recording_start_col[v];
                            uint64_t elapsed_us = war_get_monotonic_time_us() -
                                ctx_wayland->env->recording_press_time_us[v];
                            double bpm = ctx_wayland->env->atomics->bpm;
                            if (bpm <= 0.0) bpm = 100.0;
                            double sec_per_cell = 60.0 / bpm;
                            double width = (double)elapsed_us / 1000000.0 / sec_per_cell;
                            if (width < 1.0) width = 1.0;
                            call_king_terry("RECORD: release note=%u ni=%u start=%.2f elapsed_us=%lu width=%.2f",
                                            rel_note, ni, start_col,
                                            (unsigned long)elapsed_us, width);
                            if (ni < ctx_wayland->env->ctx_note->instance_count) {
                                ctx_wayland->env->ctx_note->instance[ni].size[0] = (float)width;
                                call_king_terry("RECORD: updated note #%u size[0]=%.2f", ni, (float)width);
                            }
                        }
                        ctx_wayland->env->preview_voice_active[v] = 0;
                        break;
                    }
                }
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
    if (is_digit && mode != WAR_MODE_ID_MIDI) {
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
            env->cmd_buf[env->cmd_len < 256 ? env->cmd_len : 255] = '\0';
            if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'l' && env->cmd_buf[2] == 'o' && env->cmd_buf[3] == 'o' && env->cmd_buf[4] == 'p') {
                int x = 0, y = 0;
                if (sscanf(env->cmd_buf + 5, " %d %d", &x, &y) >= 2 && x > 0 && y > 0)
                    war_loop_notes(env, x, y);
                else
                    fprintf(stderr, "LOOP: usage :loop <quarter_notes> <repeats>\n");
            } else if (env->cmd_len >= 9 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'l' && env->cmd_buf[2] == 'o' && env->cmd_buf[3] == 'a' && env->cmd_buf[4] == 'd' && env->cmd_buf[5] == 'i' && env->cmd_buf[6] == 'n' && env->cmd_buf[7] == 's' && env->cmd_buf[8] == 't') {
                int layer = 0;
                char name[256];
                if (sscanf(env->cmd_buf + 9, " %d %255s", &layer, name) >= 2)
                    war_load_inst(env, layer, name);
                else
                    fprintf(stderr, "LOADINST: usage :loadinst <layer> <name>\n");
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'l' && env->cmd_buf[2] == 'o' && env->cmd_buf[3] == 'a' && env->cmd_buf[4] == 'd') {
                const char* name = env->cmd_buf + 5;
                while (*name == ' ') name++;
                if (name[0])
                    war_load_project(env, name);
                else
                    fprintf(stderr, "LOAD: usage :load <name>\n");
            } else if (env->cmd_len >= 4 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'b' && env->cmd_buf[2] == 'p' && env->cmd_buf[3] == 'm') {
                double val = 0;
                if (sscanf(env->cmd_buf + 4, " %lf", &val) == 1 && val > 0)
                    env->atomics->bpm = (float)val;
                else
                    fprintf(stderr, "BPM: usage :bpm <value>\n");
            } else if (env->cmd_len >= 7 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'r' && env->cmd_buf[2] == 'a' && env->cmd_buf[3] == 'd' && env->cmd_buf[4] == 'i' && env->cmd_buf[5] == 'u' && env->cmd_buf[6] == 's') {
                int val = 0;
                if (sscanf(env->cmd_buf + 7, " %d", &val) == 1 && val >= 0)
                    env->across_radius = (uint32_t)val;
                else
                    fprintf(stderr, "RADIUS: usage :radius <n>\n");
            } else if (env->cmd_len >= 10 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'w' && env->cmd_buf[2] == 'r' && env->cmd_buf[3] == 'i' && env->cmd_buf[4] == 't' && env->cmd_buf[5] == 'e' && env->cmd_buf[6] == 'i' && env->cmd_buf[7] == 'n' && env->cmd_buf[8] == 's' && env->cmd_buf[9] == 't') {
                int layer = 0;
                char name[256];
                if (sscanf(env->cmd_buf + 10, " %d %255s", &layer, name) >= 2)
                    war_write_inst(env, layer, name);
                else
                    fprintf(stderr, "WRITEINST: usage :writeinst <layer> <name>\n");
            } else if (env->cmd_len >= 5 && env->cmd_buf[0] == ':' && env->cmd_buf[1] == 'w' && env->cmd_buf[2] == 'w' && env->cmd_buf[3] == 'a' && env->cmd_buf[4] == 'v') {
                const char* name = NULL;
                if (env->cmd_len > 5 && env->cmd_buf[5] == ' ')
                    name = env->cmd_buf + 6;
                if (!name || !name[0]) name = "output";
                war_export_wav(env, name);
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
            cur->prefix = 0;
            return;
        }
        // printable ASCII via utf8 from raw sym
        char utf8[8] = {0};
        int n = xkb_keysym_to_utf8(raw_sym, utf8, sizeof(utf8));
        if (n > 1 && utf8[0] >= 32 && utf8[0] <= 126) {
            if (env->cmd_len < 255) {
                env->cmd_buf[env->cmd_len++] = utf8[0];
                env->cmd_buf[env->cmd_len] = '\0';
            }
        }
        cur->prefix = 0;
        return;
    }

    // enter command mode on ':' (check raw sym before normalizer maps it to ';')
    if (raw_sym == XKB_KEY_Escape) {
        if (env->active_mode == WAR_MODE_ID_VISUAL) {
            env->active_mode = WAR_MODE_ID_ROLL;
            env->ctx_cursor->visual_active = 0;
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
    }
    if (raw_sym == XKB_KEY_colon) {
        env->cmd_active = 1;
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
    if (raw_sym == XKB_KEY_dollar && mode == WAR_MODE_ID_ROLL && !env->cmd_active) {
        war_roll_cursor_goto_right_bound_or_prefix_horizontal(env);
        ctx_wayland->keymap_state = 0;
        if (!is_digit) cur->prefix = 0;
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
                              "node.latency", "256/48000",
                              NULL);
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
            pw_properties_new("media.name", "war-play",
                              "node.latency", "256/48000",
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
    env->active_mode = WAR_MODE_ID_ROLL;
    env->undo_count = 0;
    env->undo_pos = 0;
    env->undo_note_counts = calloc(WAR_UNDO_MAX, sizeof(uint32_t));
    env->undo_notes = calloc(WAR_UNDO_MAX, sizeof(war_new_vulkan_note_instance*));
    env->across_radius = 16;
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
    ctx_wayland->gutter_rows = 3;
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
    ctx_cursor->step = 0.0;
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
    memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
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
        // preview playback: mix all active preview voices → pc_play
        if (!env->play_bar_playing) {
            enum { PW_CHUNK_FLOATS = 256 };
            uint64_t voice_batch[WAR_PREVIEW_VOICES];
            int any_active = 0;
            for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++)
                if (env->preview_voice_active[v]) { any_active = 1; break; }
            while (any_active) {
                float mix[PW_CHUNK_FLOATS];
                memset(mix, 0, sizeof(mix));
                any_active = 0;
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
                    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
                    war_capture_slot* slot = &env->capture_slots[idx];
                    if (!slot->samples || slot->count < 2) {
                        env->preview_voice_active[v] = 0;
                        continue;
                    }
                    uint64_t read_pos = env->preview_voice_read_pos[v];
                    uint64_t slot_avail = slot->samples ? slot->count : 0;
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
                    for (uint64_t f = 0; f < batch; f++)
                        mix[f] += slot->samples[read_pos + f];
                    any_active = 1;
                }
                if (!any_active) break;
                if (!war_pc_to_a(env->pc_play, 0, PW_CHUNK_FLOATS * 4, mix))
                    break;
                for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++)
                    env->preview_voice_read_pos[v] += voice_batch[v];
                // continuously update note widths during recording
                if (env->recording_active) {
                    uint64_t now_us = war_get_monotonic_time_us();
                    double bpm = env->atomics->bpm;
                    if (bpm <= 0.0) bpm = 100.0;
                    double sec_per_cell = 60.0 / bpm;
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
        // playback bar streaming: mix all active voices → pc_play
        if (env->play_bar_playing) {
            enum { PW_CHUNK_FLOATS = 256 };
            uint64_t voice_batch[WAR_PLAY_BAR_VOICES];
            int any_active = 0;
            for (uint32_t v = 0; v < WAR_PLAY_BAR_VOICES; v++)
                if (env->play_bar_voice_active[v]) { any_active = 1; break; }
            while (any_active) {
                float mix[PW_CHUNK_FLOATS];
                memset(mix, 0, sizeof(mix));
                any_active = 0;
                for (uint32_t v = 0; v < WAR_PLAY_BAR_VOICES; v++) {
                    voice_batch[v] = 0;
                    if (!env->play_bar_voice_active[v]) continue;
                    uint32_t note = env->play_bar_voice_note[v];
                    if (note > 127) note = 127;
                    uint32_t layer = env->play_bar_voice_layer[v];
                    if (layer < 1 || layer > 9) {
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
                    if (read_pos >= slot_avail) {
                        env->play_bar_voice_active[v] = 0;
                        continue;
                    }
                    uint64_t avail = (read_limit < slot_avail ? read_limit : slot_avail) - read_pos;
                    if (avail < 2) {
                        env->play_bar_voice_active[v] = 0;
                        continue;
                    }
                    uint64_t batch = avail < PW_CHUNK_FLOATS ?
                                         (avail & ~1ULL) :
                                         PW_CHUNK_FLOATS;
                    voice_batch[v] = batch;
                    for (uint64_t f = 0; f < batch; f++)
                        mix[f] += slot->samples[env->play_bar_voice_read_pos[v] + f];
                    any_active = 1;
                }
                if (!any_active) break;
                if (!war_pc_to_a(env->pc_play, 0, PW_CHUNK_FLOATS * 4, mix))
                    break;
                for (uint32_t v = 0; v < WAR_PLAY_BAR_VOICES; v++)
                    env->play_bar_voice_read_pos[v] += voice_batch[v];
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
