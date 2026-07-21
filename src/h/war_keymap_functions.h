//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_keymap_functions.h
//-----------------------------------------------------------------------------

#ifndef WAR_KEYMAP_FUNCTIONS_H
#define WAR_KEYMAP_FUNCTIONS_H

#include "war_data.h"
#include "war_debug_macros.h"
#include "war_functions.h"

extern void war_reconnect_capture(war_env* env, const char* target);
extern void war_reconnect_loopback(war_env* env, const char* target);

#include <assert.h>
#include <ctype.h>
#include <float.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lualib.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

static inline void war_fat(war_env* env) {
    if (env->ctx_cursor->prefix == 0) { env->ctx_cursor->prefix = 1; }
    env->ctx_cursor->x_width[0] = env->ctx_cursor->prefix;
    env->ctx_cursor->instance[0].size[0] = (float)env->ctx_cursor->x_width[0];
}

static inline void war_thin(war_env* env) {
    if (env->ctx_cursor->prefix == 0) { env->ctx_cursor->prefix = 1; }
    env->ctx_cursor->x_width[0] /= env->ctx_cursor->prefix;
    env->ctx_cursor->instance[0].size[0] = (float)env->ctx_cursor->x_width[0];
}

static inline void war_pan_follow(war_env* env);

static inline void war_goto_col(war_env* env) {
    war_wayland_context* wl = env->ctx_wayland;
    war_cursor_context* cur = env->ctx_cursor;
    double col;
    if (cur->prefix == 0) {
        double vis_cols = (double)wl->width / (cur->cell_width * wl->zoom) - wl->gutter_cols;
        if (vis_cols < 1) vis_cols = 1;
        col = (double)(uint32_t)(wl->panning[0] + vis_cols + 0.5);
    } else {
        col = (double)cur->prefix;
    }
    if (col < wl->gutter_cols) col = wl->gutter_cols;
    if (col > wl->right_bound) col = wl->right_bound;
    cur->instance->pos[0] = (float)col;
    war_pan_follow(env);
}

static inline void war_roll_cursor_goto_right_bound_or_prefix_horizontal(war_env* env) {
    war_wayland_context* wl = env->ctx_wayland;
    war_cursor_context* cur = env->ctx_cursor;
    double col;
    if (cur->prefix == 0) {
        double vis_cols = (double)wl->width / (cur->cell_width * wl->zoom) - wl->gutter_cols;
        if (vis_cols < 1) vis_cols = 1;
        col = (double)(uint32_t)(wl->panning[0] + vis_cols + 0.5);
    } else {
        col = (double)cur->prefix + (double)(wl->gutter_cols - 1);
    }
    if (col < wl->gutter_cols) col = wl->gutter_cols;
    if (col > wl->right_bound) col = wl->right_bound;
    cur->instance->pos[0] = (float)col;
    war_pan_follow(env);
}

static inline void war_goto_left_visible_bound(war_env* env) {
    war_wayland_context* wl = env->ctx_wayland;
    war_cursor_context* cur = env->ctx_cursor;
    double col = wl->panning[0];
    if (col < wl->gutter_cols) col = wl->gutter_cols;
    cur->instance->pos[0] = (float)(uint32_t)(col + 0.5);
    war_pan_follow(env);
}

static inline void war_pan_center_on_cursor(war_env* env) {
    war_wayland_context* wl = env->ctx_wayland;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    double ch = cur->cell_height;
    double cw = cur->cell_width;
    float z = wl->zoom;
    if (z < 0.001f) z = 0.001f;
    double vis_rows = (double)wl->height / (ch * z) - wl->gutter_rows;
    double vis_cols = (double)wl->width / (cw * z) - wl->gutter_cols;
    if (vis_rows < 1) vis_rows = 1;
    if (vis_cols < 1) vis_cols = 1;
    double cy = cur->instance[0].pos[1];
    double cx = cur->instance[0].pos[0];
    double p_top = wl->panning[1];
    double p_bot = p_top + vis_rows + wl->gutter_rows;
    double p_left = wl->panning[0];
    double p_right = p_left + vis_cols + wl->gutter_cols;
    if (cy < p_top || cy >= p_bot || cx < p_left || cx >= p_right) {
        wl->panning[1] = (float)(cy - vis_rows / 2.0);
        wl->panning[0] = (float)(cx - vis_cols / 2.0);
        if (wl->panning[0] < 0) wl->panning[0] = 0;
        if (wl->panning[1] < 0) wl->panning[1] = 0;
        double max_pan = cy - (double)wl->gutter_rows;
        if ((double)wl->panning[1] > max_pan) wl->panning[1] = (float)max_pan;
    }
}

static inline void war_goto_row_127(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    cur->instance[0].pos[1] = 127.0 + (double)env->ctx_wayland->gutter_rows;
    war_pan_center_on_cursor(env);
}

static inline void war_goto_row_60(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    cur->instance[0].pos[1] = 60.0 + (double)env->ctx_wayland->gutter_rows;
    war_pan_center_on_cursor(env);
}

static inline void war_clamp_cursor_row(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    double min_row = (double)env->ctx_wayland->gutter_rows;
    if (cur->instance_count && cur->instance[0].pos[1] < min_row)
        cur->instance[0].pos[1] = (float)min_row;
}

static inline void war_goto_row_0(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_wayland_context* wl = env->ctx_wayland;
    cur->instance[0].pos[1] = wl->gutter_rows;
    war_pan_center_on_cursor(env);
}

static inline void war_pan_follow(war_env* env) {
    war_wayland_context* wl = env->ctx_wayland;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    double cw = cur->cell_width, ch = cur->cell_height;
    float z = wl->zoom;
    if (z < 0.001f) z = 0.001f;
    double vis_cols = (double)wl->width / (cw * z) - wl->gutter_cols;
    double vis_rows = (double)wl->height / (ch * z) - wl->gutter_rows;
    if (vis_cols < 1) vis_cols = 1;
    if (vis_rows < 1) vis_rows = 1;
    double cx = cur->instance[0].pos[0];
    double cy = cur->instance[0].pos[1];
    float* p = wl->panning;
    double const margin_down = 3.0, margin_up = 0.0;
    double const margin_right = 0.0, margin_left = 4.0;
    double const gg = wl->gutter_rows, gc = wl->gutter_cols;
    if (cx < p[0]) p[0] = (float)(cx - margin_left);
    if (cx >= p[0] + vis_cols + gc) p[0] = (float)(cx - vis_cols - gc + 1 + margin_right);
    if (cy < p[1]) p[1] = (float)(cy - margin_down);
    if (cy >= p[1] + vis_rows + gg) p[1] = (float)(cy - vis_rows - gg + 1 + margin_up);
    if (cx - p[0] < margin_left)
        p[0] = (float)(cx - margin_left);
    if (cy - p[1] < margin_down)
        p[1] = (float)(cy - margin_down);
    p[0] = (float)(int)(p[0] + 0.5);
    p[1] = (float)(int)(p[1] + 0.5);
    if (p[0] < 0) p[0] = 0;
    if (p[1] < 0) p[1] = 0;
    double max_pan_follow = cy - (double)wl->gutter_rows;
    if ((double)p[1] > max_pan_follow) p[1] = (float)max_pan_follow;
}

static inline void war_layer_1(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_1;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 1;
}

static inline void war_layer_2(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_2;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 2;
}

static inline void war_layer_3(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_3;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 3;
}

static inline void war_layer_4(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_4;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 4;
}

static inline void war_layer_5(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_5;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 5;
}

static inline void war_layer_6(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_6;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 6;
}

static inline void war_layer_7(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_7;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 7;
}

static inline void war_layer_8(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_8;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 8;
}

static inline void war_layer_9(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_9;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 9;
}

static inline void war_layer_0(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_none;
    float rgba[4] = {((c >> 24) & 0xFF) / 255.0f,
                     ((c >> 16) & 0xFF) / 255.0f,
                     ((c >> 8) & 0xFF) / 255.0f,
                     (c & 0xFF) / 255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
    ctx_cursor->layer = 0;
}

// toggle layer visibility (alt+shift+number)
static inline void war_toggle_layer_1(war_env* env) { if (env) env->layer_visible ^= (1 << 0); }
static inline void war_toggle_layer_2(war_env* env) { if (env) env->layer_visible ^= (1 << 1); }
static inline void war_toggle_layer_3(war_env* env) { if (env) env->layer_visible ^= (1 << 2); }
static inline void war_toggle_layer_4(war_env* env) { if (env) env->layer_visible ^= (1 << 3); }
static inline void war_toggle_layer_5(war_env* env) { if (env) env->layer_visible ^= (1 << 4); }
static inline void war_toggle_layer_6(war_env* env) { if (env) env->layer_visible ^= (1 << 5); }
static inline void war_toggle_layer_7(war_env* env) { if (env) env->layer_visible ^= (1 << 6); }
static inline void war_toggle_layer_8(war_env* env) { if (env) env->layer_visible ^= (1 << 7); }
static inline void war_toggle_layer_9(war_env* env) { if (env) env->layer_visible ^= (1 << 8); }
static inline void war_toggle_all_layers(war_env* env) { if (env) env->layer_visible ^= 0x1FF; }

//-----------------------------------------------------------------------------
// OCTAVES
//-----------------------------------------------------------------------------

static inline void war_octave_minus_1(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = -1;
}

static inline void war_octave_0(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 0;
}

static inline void war_octave_1(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 1;
}

static inline void war_octave_2(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 2;
}

static inline void war_octave_3(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 3;
}

static inline void war_octave_4(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 4;
}

static inline void war_octave_5(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 5;
}

static inline void war_octave_6(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 6;
}

static inline void war_octave_7(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 7;
}

static inline void war_octave_8(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 8;
}

static inline void war_octave_9(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 9;
}

static inline void war_octave_10(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    ctx_cursor->octave = 10;
}

//-----------------------------------------------------------------------------
// PLAYBACK
//-----------------------------------------------------------------------------

static inline int32_t war_octave_to_midi_base(int32_t octave) {
    return (octave + 1) * 12;
}

static inline void war_undo_save(war_env* env);

static inline void _war_record_place_note(war_env* env, uint32_t note, int voice) {
    war_note_context* note_ctx = env->ctx_note;
    if (!note_ctx || voice < 0) return;
    if (note_ctx->instance_count >= note_ctx->max_instances) return;
    uint32_t _rl = env->ctx_cursor->layer;
    if (_rl == 0) return;
    if (_rl >= 1 && _rl <= 9 && !(env->layer_visible & (1 << (_rl - 1)))) return;
    war_undo_save(env);
    uint32_t i = note_ctx->instance_count++;
    double visual_row = (double)note + (double)env->ctx_wayland->gutter_rows;
    uint32_t col = (&env->ctx_color->layer_none)[env->ctx_cursor->layer];
    // place note at current playback bar position
    double _pb_bpm = env->atomics->bpm;
    if (_pb_bpm <= 0.0) _pb_bpm = 100.0;
    double _pb_spc = 15.0 / _pb_bpm;
    float _pb_pos = (float)((double)env->ctx_wayland->gutter_cols + env->play_bar_position_seconds / _pb_spc);
    note_ctx->instance[i].pos[0] = _pb_pos;
    note_ctx->instance[i].pos[1] = (float)visual_row;
    note_ctx->instance[i].pos[2] = 0.0f;
    note_ctx->instance[i].size[0] = 1.0f;
    note_ctx->instance[i].size[1] = 1.0f;
    note_ctx->instance[i].color[0] = ((col >> 24) & 0xFF) / 255.0f;
    note_ctx->instance[i].color[1] = ((col >> 16) & 0xFF) / 255.0f;
    note_ctx->instance[i].color[2] = ((col >> 8) & 0xFF) / 255.0f;
    note_ctx->instance[i].color[3] = (col & 0xFF) / 255.0f;
    note_ctx->instance[i].outline_color[0] = 0.0f;
    note_ctx->instance[i].outline_color[1] = 0.0f;
    note_ctx->instance[i].outline_color[2] = 0.0f;
    note_ctx->instance[i].outline_color[3] = 1.0f;
    note_ctx->instance[i].flags = (uint32_t)env->ctx_cursor->layer << 4;
    note_ctx->instance[i].tick = note_ctx->tick_counter++;
    env->recording_start_col[voice] = _pb_pos;
    env->recording_note_idx[voice] = i;
    env->recording_press_time_us[voice] = war_get_monotonic_time_us();
    call_king_terry("RECORD: placed note=%u layer=%u col=%.2f voice=%u idx=%u t=%lu",
                    note, env->ctx_cursor->layer, env->recording_position, voice, i,
                    (unsigned long)env->recording_press_time_us[voice]);
}

static inline void war_record_midi(war_env* env) {
    env->recording_active = !env->recording_active;
    if (env->recording_active) {
        env->recording_position = (double)env->ctx_wayland->gutter_cols;
        env->recording_last_frame_ms = 0;
        for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++)
            env->recording_start_col[v] = 0.0;
        env->play_bar_playing = 1;
    }
}

static inline int _war_preview_start_voice(war_env* env, uint32_t note, uint32_t layer) {
    if (env->midi_toggle) {
        // toggle mode: if note is playing, stop it; otherwise start it
        for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
            if (env->preview_voice_active[v] && env->preview_voice_note[v] == note) {
                if (env->recording_active && env->ctx_note) {
                    uint32_t ni = env->recording_note_idx[v];
                    uint64_t elapsed_us = war_get_monotonic_time_us() - env->recording_press_time_us[v];
                    double bpm = env->atomics->bpm;
                    if (bpm <= 0.0) bpm = 100.0;
                    double sec_per_cell = 15.0 / bpm;
                    double width = (double)elapsed_us / 1000000.0 / sec_per_cell;
                    if (width < 1.0) width = 1.0;
                    if (ni < env->ctx_note->instance_count)
                        env->ctx_note->instance[ni].size[0] = (float)width;
                }
                env->preview_voice_active[v] = 0;
                return (int)v;
            }
        }
    }
    // if same note is already active (including releasing), retrigger in place
    for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
        if (env->preview_voice_active[v] && env->preview_voice_note[v] == note) {
            uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
            war_capture_slot* slot = &env->capture_slots[idx];
            if (!slot->samples || slot->count < 2) { env->preview_voice_active[v] = 0; return -1; }
            env->preview_voice_layer[v] = layer;
            env->preview_voice_read_pos[v] = 0;
            env->preview_voice_read_limit[v] = slot->count;
            env->preview_voice_filter_lp[v][0] = 0.0f;
            env->preview_voice_filter_lp[v][1] = 0.0f;
            env->preview_voice_env_samples[v] = 0;
            if (env->recording_active) _war_record_place_note(env, note, (int)v);
            return (int)v;
        }
    }
    // find a completely free slot
    for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
        if (!env->preview_voice_active[v]) {
            uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
            war_capture_slot* slot = &env->capture_slots[idx];
            if (!slot->samples || slot->count < 2) return -1;
            uint32_t voice = v;
            env->preview_voice_note[voice] = note;
            env->preview_voice_layer[voice] = layer;
            env->preview_voice_read_pos[voice] = 0;
            env->preview_voice_read_limit[voice] = slot->count;
            env->preview_voice_filter_lp[voice][0] = 0.0f;
            env->preview_voice_filter_lp[voice][1] = 0.0f;
            env->preview_voice_env_samples[voice] = 0;
            env->preview_voice_active[voice] = 1;
            if (env->recording_active) _war_record_place_note(env, note, (int)voice);
            return (int)voice;
        }
    }
    return -1;
}

static inline void war_play_q(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(0 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_w(war_env* env) {
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(1 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_e(war_env* env) {
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(2 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_r(war_env* env) {
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(3 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_t(war_env* env) {
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(4 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_y(war_env* env) {
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(5 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_u(war_env* env) {
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(6 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_i(war_env* env) {
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(7 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_o(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(8 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_p(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(9 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_left_bracket(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(10 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_play_right_bracket(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(11 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    if (!env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1)].samples)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_step_mode_fat(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    cursor->step = cursor->prefix > 0 ? (double)cursor->prefix : 4.0;
}

static inline void war_step_mode_thin(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    cursor->step = cursor->prefix > 0 ? 1.0 / (double)cursor->prefix : 0.5;
}

static inline void war_reset_step(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    env->ctx_cursor->step = 1.0;
    env->ctx_cursor->x_width[0] = 1.0;
    env->ctx_cursor->instance[0].size[0] = 1.0f;
    // snap cursor to nearest cell
    double gc = env->ctx_wayland->gutter_cols;
    double gr = env->ctx_wayland->gutter_rows;
    double col = floor(cur->instance[0].pos[0] - gc + 0.5) + gc;
    double row = floor(cur->instance[0].pos[1] - gr + 0.5) + gr;
    if (col < gc) col = gc;
    if (row < gr) row = gr;
    cur->instance[0].pos[0] = col;
    cur->instance[0].pos[1] = row;
    war_pan_follow(env);
}

static inline void war_midi_mode(war_env* env) {
    env->status_msg[0] = '\0';
    env->active_mode = (env->active_mode == WAR_MODE_ID_MIDI)
                           ? WAR_MODE_ID_ROLL
                           : WAR_MODE_ID_MIDI;
}

static inline int _war_keysym_to_midi_offset(uint32_t keysym) {
    if (keysym == XKB_KEY_q) return 0;
    if (keysym == XKB_KEY_w) return 1;
    if (keysym == XKB_KEY_e) return 2;
    if (keysym == XKB_KEY_r) return 3;
    if (keysym == XKB_KEY_t) return 4;
    if (keysym == XKB_KEY_y) return 5;
    if (keysym == XKB_KEY_u) return 6;
    if (keysym == XKB_KEY_i) return 7;
    if (keysym == XKB_KEY_o) return 8;
    if (keysym == XKB_KEY_p) return 9;
    if (keysym == XKB_KEY_bracketleft) return 10;
    if (keysym == XKB_KEY_bracketright) return 11;
    return -1;
}

static inline void war_move_cursor_right(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    double bound = env->ctx_wayland->right_bound;
    double step = cursor->step > 0.0 ? cursor->step : 1.0;
    if (cursor->instance_count && cursor->instance[0].pos[0] < bound)
        cursor->instance[0].pos[0] += step;
    war_pan_follow(env);
}

static inline void war_move_cursor_left(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    double bound = env->ctx_wayland->gutter_cols + 0.5;
    double step = cursor->step > 0.0 ? cursor->step : 1.0;
    if (cursor->instance_count && cursor->instance[0].pos[0] > bound)
        cursor->instance[0].pos[0] -= step;
    war_pan_follow(env);
}

static inline void war_move_cursor_up(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    double bound = env->ctx_wayland->top_bound;
    if (cursor->instance_count) {
        double pos = cursor->instance[0].pos[1];
        if (pos < bound - 0.5)
            cursor->instance[0].pos[1] = pos + 1.0;
    }
    war_pan_follow(env);
}

static inline void war_move_cursor_down(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    double bound = env->ctx_wayland->gutter_rows;
    if (cursor->instance_count) {
        double pos = cursor->instance[0].pos[1];
        if (pos > bound)
            cursor->instance[0].pos[1] = pos - 1.0;
    }
    war_pan_follow(env);
}

static inline void war_move_cursor_down_leap(war_env* env) {
    for (int i = 0; i < 13; i++) war_move_cursor_down(env);
}

static inline void war_move_cursor_up_leap(war_env* env) {
    for (int i = 0; i < 13; i++) war_move_cursor_up(env);
}

static inline void war_goto_viewport_bottom(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_wayland_context* wl = env->ctx_wayland;
    double bottom = wl->panning[1] + wl->gutter_rows;
    double min_bottom = wl->gutter_rows;
    if (bottom < min_bottom) bottom = min_bottom;
    if (bottom > wl->top_bound) bottom = wl->top_bound;
    cur->instance[0].pos[1] = (uint32_t)(bottom + 0.5);
    war_clamp_cursor_row(env);
}

static inline void war_goto_viewport_top(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_wayland_context* wl = env->ctx_wayland;
    if (cur->prefix == 0) {
        double total_vis_rows = (double)wl->height / (cur->cell_height * wl->zoom);
        double top = wl->panning[1] + total_vis_rows - 1;
        if (top < 0) top = 0;
        if (top > wl->top_bound) top = wl->top_bound;
        cur->instance[0].pos[1] = (uint32_t)(top + 0.5);
        war_clamp_cursor_row(env);
        war_pan_follow(env);
    } else {
        double row = (double)cur->prefix;
        if (row < 0) row = 0;
        if (row > wl->top_bound) row = wl->top_bound;
        cur->instance[0].pos[1] = (float)(row + (double)wl->gutter_rows);
        war_clamp_cursor_row(env);
        war_pan_center_on_cursor(env);
    }
}

static inline void war_move_cursor_left_leap(war_env* env) {
    for (int i = 0; i < 13; i++) war_move_cursor_left(env);
}

static inline void war_move_cursor_right_leap(war_env* env) {
    for (int i = 0; i < 13; i++) war_move_cursor_right(env);
}

static inline void war_next_note_same_row(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_note_context* note = env->ctx_note;
    if (!cur->instance_count || !note || !note->instance_count) return;
    float cy = cur->instance[0].pos[1];
    float cx = cur->instance[0].pos[0];
    float best = 1e9f;
    for (uint32_t i = 0; i < note->instance_count; i++) {
        if (note->instance[i].pos[1] != cy) continue;
        uint32_t _nl = (note->instance[i].flags >> 4) & 0xF;
        if (_nl >= 1 && _nl <= 9 && !(env->layer_visible & (1 << (_nl - 1)))) continue;
        if (note->instance[i].pos[0] > cx && note->instance[i].pos[0] < best)
            best = note->instance[i].pos[0];
    }
    if (best < 1e9f) {
        cur->instance[0].pos[0] = best;
        war_pan_follow(env);
    }
}

static inline void war_go_to_note_end(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_note_context* note = env->ctx_note;
    if (!cur->instance_count || !note || !note->instance_count) return;
    float cy = cur->instance[0].pos[1];
    float cx = cur->instance[0].pos[0];
    // check if cursor is inside a note on this row
    for (uint32_t i = 0; i < note->instance_count; i++) {
        if (note->instance[i].pos[1] != cy) continue;
        uint32_t _nl = (note->instance[i].flags >> 4) & 0xF;
        if (_nl >= 1 && _nl <= 9 && !(env->layer_visible & (1 << (_nl - 1)))) continue;
        float ns = note->instance[i].pos[0];
        float ne = ns + note->instance[i].size[0];
        if (cx >= ns && cx < ne) {
            cur->instance[0].pos[0] = ne;
            war_pan_follow(env);
            return;
        }
    }
    // not inside a note — find next note on this row and go to its end
    float best = 1e9f;
    float best_end = 0;
    for (uint32_t i = 0; i < note->instance_count; i++) {
        if (note->instance[i].pos[1] != cy) continue;
        uint32_t _nl = (note->instance[i].flags >> 4) & 0xF;
        if (_nl >= 1 && _nl <= 9 && !(env->layer_visible & (1 << (_nl - 1)))) continue;
        float ns = note->instance[i].pos[0];
        if (ns > cx && ns < best) {
            best = ns;
            best_end = ns + note->instance[i].size[0];
        }
    }
    if (best < 1e9f) {
        cur->instance[0].pos[0] = best_end;
        war_pan_follow(env);
    }
}

static inline void war_set_width_to_note(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_note_context* note = env->ctx_note;
    if (!cur->instance_count || !note || !note->instance_count) return;
    float cy = cur->instance[0].pos[1];
    float cx = cur->instance[0].pos[0];
    for (uint32_t i = 0; i < note->instance_count; i++) {
        if (note->instance[i].pos[1] != cy) continue;
        uint32_t _nl = (note->instance[i].flags >> 4) & 0xF;
        if (_nl >= 1 && _nl <= 9 && !(env->layer_visible & (1 << (_nl - 1)))) continue;
        float ns = note->instance[i].pos[0];
        float ne = ns + note->instance[i].size[0];
        if (cx >= ns && cx < ne) {
            cur->x_width[0] = (double)note->instance[i].size[0];
            cur->instance[0].size[0] = note->instance[i].size[0];
            snprintf(env->status_msg, sizeof(env->status_msg), "width %.1f", note->instance[i].size[0]);
            return;
        }
    }
}

static inline void war_set_loop_end(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    env->loop_end_col = cur->instance[0].pos[0];
    if (env->ctx_line && env->ctx_line->instance_count >= 2)
        env->ctx_line->instance[1].pos[0] = env->loop_end_col;
    snprintf(env->status_msg, sizeof(env->status_msg), "loop end %.1f", env->loop_end_col);
}

static inline void war_set_loop_start(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    env->loop_start_col = cur->instance[0].pos[0];
    if (env->ctx_line && env->ctx_line->instance_count >= 3)
        env->ctx_line->instance[2].pos[0] = env->loop_start_col;
    snprintf(env->status_msg, sizeof(env->status_msg), "loop start %.1f", env->loop_start_col);
}

static inline void war_prev_note_same_row(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_note_context* note = env->ctx_note;
    if (!cur->instance_count || !note || !note->instance_count) return;
    float cy = cur->instance[0].pos[1];
    float cx = cur->instance[0].pos[0];
    float best = -1e9f;
    for (uint32_t i = 0; i < note->instance_count; i++) {
        if (note->instance[i].pos[1] != cy) continue;
        uint32_t _nl = (note->instance[i].flags >> 4) & 0xF;
        if (_nl >= 1 && _nl <= 9 && !(env->layer_visible & (1 << (_nl - 1)))) continue;
        if (note->instance[i].pos[0] < cx && note->instance[i].pos[0] > best)
            best = note->instance[i].pos[0];
    }
    if (best > -1e9f) {
        cur->instance[0].pos[0] = best;
        war_pan_follow(env);
    }
}

static inline void war_zoom_in(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_wayland_context* wl = env->ctx_wayland;
    float old_z = wl->zoom;
    float z = old_z * 1.25f;
    if (z > 100.0f) z = 100.0f;
    if (cur->instance_count) {
        float ratio = old_z / z;
        double cy = cur->instance[0].pos[1];
        double cx = cur->instance[0].pos[0];
        wl->panning[0] = (float)(cx - (cx - (double)wl->panning[0]) * (double)ratio);
        wl->panning[1] = (float)(cy - (cy - (double)wl->panning[1]) * (double)ratio);
    }
    wl->zoom = z;
    war_pan_follow(env);
}

static inline void war_zoom_out(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_wayland_context* wl = env->ctx_wayland;
    float old_z = wl->zoom;
    float z = old_z * 0.80f;
    if (z < 0.01f) z = 0.01f;
    if (cur->instance_count) {
        float ratio = old_z / z;
        double cy = cur->instance[0].pos[1];
        double cx = cur->instance[0].pos[0];
        wl->panning[0] = (float)(cx - (cx - (double)wl->panning[0]) * (double)ratio);
        wl->panning[1] = (float)(cy - (cy - (double)wl->panning[1]) * (double)ratio);
    }
    wl->zoom = z;
    war_pan_follow(env);
}

static inline void war_zoom_reset(war_env* env) {
    env->ctx_wayland->zoom = env->ctx_wayland->initial_zoom;
}

static inline void war_visual_mode(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    env->status_msg[0] = '\0';
    cur->visual_active = !cur->visual_active;
    if (cur->visual_active) {
        env->active_mode = WAR_MODE_ID_VISUAL;
        cur->visual_anchor_col = cur->instance[0].pos[0];
        cur->visual_anchor_row = cur->instance[0].pos[1];
        call_king_terry("VISUAL: ON at col=%.1f row=%.1f",
                        cur->visual_anchor_col, cur->visual_anchor_row);
    } else {
        env->active_mode = WAR_MODE_ID_ROLL;
        cur->visual_stretch_active = 0;
        uint32_t lc = (&env->ctx_color->layer_none)[cur->layer];
        cur->instance[0].color[0] = ((lc >> 24) & 0xFF) / 255.0f;
        cur->instance[0].color[1] = ((lc >> 16) & 0xFF) / 255.0f;
        cur->instance[0].color[2] = ((lc >> 8) & 0xFF) / 255.0f;
        cur->instance[0].color[3] = (lc & 0xFF) / 255.0f;
        call_king_terry("VISUAL: OFF");
    }
}

static inline void war_visual_swap_anchor(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->visual_active || !cur->instance_count) return;
    float tmp_col = cur->instance[0].pos[0];
    float tmp_row = cur->instance[0].pos[1];
    cur->instance[0].pos[0] = cur->visual_anchor_col;
    cur->instance[0].pos[1] = cur->visual_anchor_row;
    war_clamp_cursor_row(env);
    cur->visual_anchor_col = tmp_col;
    cur->visual_anchor_row = tmp_row;
}

static inline void war_visual_stretch_toggle(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->visual_active) return;
    cur->visual_stretch_active = !cur->visual_stretch_active;
    call_king_terry("STRETCH: %s", cur->visual_stretch_active ? "ON" : "OFF");
}

static inline void _war_stretch_slot(war_env* env, uint32_t note, uint32_t layer, double ratio) {
    if (!env || note > 127 || layer < 1 || layer > 9) return;
    uint32_t li = layer - 1;
    war_capture_slot* slot = &env->capture_slots[note * WAR_CAPTURE_SLOT_LAYERS + li];
    float* src_data = slot->samples;
    uint64_t src_cnt = slot->count;
    if (!src_data || src_cnt < 4) return;
    uint64_t src_frames = src_cnt / 2;
    uint64_t dst_frames = (uint64_t)((double)src_frames * ratio);
    if (dst_frames < 1) dst_frames = 1;
    uint64_t dst_cnt = dst_frames * 2;
    double* wrk = malloc(dst_cnt * sizeof(double));
    if (!wrk) return;
    double step = 1.0 / ratio;
    for (uint64_t i = 0; i < dst_frames; i++) {
        double sp = (double)i * step;
        uint64_t si = (uint64_t)sp;
        double fr = sp - (double)si;
        if (si >= src_frames - 1) {
            wrk[i*2] = src_data[(src_frames-1)*2];
            wrk[i*2+1] = src_data[(src_frames-1)*2+1];
        } else {
            wrk[i*2] = src_data[si*2]*(1.0-fr) + src_data[(si+1)*2]*fr;
            wrk[i*2+1] = src_data[si*2+1]*(1.0-fr) + src_data[(si+1)*2+1]*fr;
        }
    }
    free(slot->samples);
    slot->samples = malloc(dst_cnt * sizeof(float));
    if (slot->samples) {
        for (uint64_t i = 0; i < dst_cnt; i++)
            slot->samples[i] = (float)wrk[i];
        slot->count = dst_cnt;
        slot->capacity = dst_cnt;
    }
    free(wrk);
}

static inline void war_undo_save(war_env* env);

static inline void _war_visual_move_selection(war_env* env, float dx, float dy) {
    war_cursor_context* cur = env->ctx_cursor;
    war_note_context* note = env->ctx_note;
    if (!cur->visual_active || !note || !note->instance_count) return;
    if (dx != 0 && cur->visual_stretch_active) {
        war_undo_save(env);
        war_wayland_context* ctx_wayland = env->ctx_wayland;
        float x0 = cur->visual_anchor_col;
        float x1 = cur->instance[0].pos[0];
        float y0 = cur->visual_anchor_row;
        float y1 = cur->instance[0].pos[1];
        if (x1 < x0) { float t = x0; x0 = x1; x1 = t; }
        if (y1 < y0) { float t = y0; y0 = y1; y1 = t; }
        uint32_t stretched = 0;
        for (uint32_t i = 0; i < note->instance_count; i++) {
            float nx0 = note->instance[i].pos[0];
            float nx1 = nx0 + note->instance[i].size[0];
            float ny = note->instance[i].pos[1];
            if (nx0 < x1 && nx1 > x0 && ny >= y0 && ny <= y1) {
                uint32_t _vl = (note->instance[i].flags >> 4) & 0xF;
                if (_vl >= 1 && _vl <= 9 && !(env->layer_visible & (1 << (_vl - 1)))) continue;
                float old_sz = note->instance[i].size[0];
                float new_sz = old_sz + dx;
                if (new_sz < 1.0f) new_sz = 1.0f;
                note->instance[i].size[0] = new_sz;
                double sr = (double)old_sz > 0.0 ? (double)new_sz / (double)old_sz : 1.0;
                uint32_t _pp = (uint32_t)(ny - (double)ctx_wayland->gutter_rows);
                if (_pp <= 127) _war_stretch_slot(env, _pp, _vl, sr);
                stretched++;
            }
        }
        call_king_terry("STRETCH: %u notes %.0f", stretched, dx);
        return;
    }
    war_undo_save(env);
    float x0 = cur->visual_anchor_col;
    float x1 = cur->instance[0].pos[0];
    float y0 = cur->visual_anchor_row;
    float y1 = cur->instance[0].pos[1];
    if (x1 < x0) { float t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { float t = y0; y0 = y1; y1 = t; }
    uint32_t moved = 0;
    for (uint32_t i = 0; i < note->instance_count; i++) {
        float nx0 = note->instance[i].pos[0];
        float nx1 = nx0 + note->instance[i].size[0];
        float ny = note->instance[i].pos[1];
        if (nx0 < x1 && nx1 > x0 && ny >= y0 && ny <= y1) {
            uint32_t _vl = (note->instance[i].flags >> 4) & 0xF;
            if (_vl >= 1 && _vl <= 9 && !(env->layer_visible & (1 << (_vl - 1)))) continue;
            note->instance[i].pos[0] += dx;
            note->instance[i].pos[1] += dy;
            if (note->instance[i].pos[0] < (float)env->ctx_wayland->gutter_cols)
                note->instance[i].pos[0] = (float)env->ctx_wayland->gutter_cols;
            if (note->instance[i].pos[1] < (float)env->ctx_wayland->gutter_rows)
                note->instance[i].pos[1] = (float)env->ctx_wayland->gutter_rows;
            moved++;
        }
    }
    cur->instance[0].pos[0] += dx;
    cur->instance[0].pos[1] += dy;
    war_clamp_cursor_row(env);
    cur->visual_anchor_col += dx;
    cur->visual_anchor_row += dy;
    call_king_terry("VISUAL: moved %u notes (%.0f,%.0f)", moved, dx, dy);
}

static inline void war_visual_move_right(war_env* env) { _war_visual_move_selection(env, (float)env->ctx_cursor->step, 0.0f); }
static inline void war_visual_move_left(war_env* env)  { _war_visual_move_selection(env, -(float)env->ctx_cursor->step, 0.0f); }
static inline void war_visual_move_up(war_env* env)    { _war_visual_move_selection(env, 0.0f, (float)env->ctx_cursor->step); }
static inline void war_visual_move_down(war_env* env)  { _war_visual_move_selection(env, 0.0f, -(float)env->ctx_cursor->step); }

static inline void war_toggle_playback(war_env* env) {
    war_simple_line_context* line = env->ctx_line;
    if (!line) return;
    if (env->play_bar_playing) {
        env->play_bar_playing = 0;
        memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
        memset(env->play_bar_direct_filter_lp, 0, sizeof(env->play_bar_direct_filter_lp));
    } else {
        env->play_bar_playing = 1;
        env->play_bar_last_frame_ms = 0;
        env->play_bar_last_us = 0;
        double bpm = env->atomics->bpm;
        if (bpm <= 0.0) bpm = 100.0;
        double sec_per_cell = 15.0 / bpm;
        double gc = (double)env->ctx_wayland->gutter_cols;
        env->play_bar_prev_cell_pos = gc + env->play_bar_position_seconds / sec_per_cell;
        memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
        memset(env->play_bar_direct_filter_lp, 0, sizeof(env->play_bar_direct_filter_lp));
        // resume: activate any note whose body contains the playhead
        if (env->ctx_note && env->ctx_note->instance_count > 0) {
            double _cp = env->play_bar_prev_cell_pos;
            double _bpm2 = env->atomics->bpm;
            if (_bpm2 <= 0.0) _bpm2 = 100.0;
            double _spc2 = 15.0 / _bpm2;
            for (uint32_t _ri = 0; _ri < env->ctx_note->instance_count; _ri++) {
                double _ns = env->ctx_note->instance[_ri].pos[0];
                double _nw = env->ctx_note->instance[_ri].size[0];
                if (_cp > _ns && _cp < _ns + _nw) {
                    uint32_t _pp = (uint32_t)(env->ctx_note->instance[_ri].pos[1] - (double)env->ctx_wayland->gutter_rows);
                    if (_pp > 127) _pp = 127;
                    uint32_t _li = (env->ctx_note->instance[_ri].flags >> 4) & 0xF;
                    if (_li < 1 || _li > 9) _li = 1;
                    uint32_t _si = _pp * WAR_CAPTURE_SLOT_LAYERS + (_li - 1);
                    war_capture_slot* _sl = &env->capture_slots[_si];
                    if (_sl->samples && _sl->count > 0) {
                        double _off_cells = _cp - _ns;
                        double _rem_cells = _nw - _off_cells;
                        if (_rem_cells < 0.01) continue;
                        uint64_t _offset = (uint64_t)(_off_cells * _spc2 * 48000.0 * 2.0);
                        if (_offset & 1) _offset &= ~1ULL;
                        uint64_t _limit = _offset + (uint64_t)(_rem_cells * _spc2 * 48000.0 * 2.0);
                        if (_limit > _sl->count) _limit = _sl->count;
                        for (uint32_t _v = 0; _v < WAR_PLAY_BAR_VOICES; _v++) {
                            if (!env->play_bar_voice_active[_v]) {
                                env->play_bar_voice_note[_v] = _pp;
                                env->play_bar_voice_layer[_v] = _li;
                                env->play_bar_voice_tick[_v] = env->ctx_note->instance[_ri].tick;
                                env->play_bar_voice_read_pos[_v] = _offset;
                                env->play_bar_voice_read_limit[_v] = _limit;
                                env->play_bar_voice_filter_lp[_v][0] = 0.0f;
                                env->play_bar_voice_filter_lp[_v][1] = 0.0f;
                                env->play_bar_voice_active[_v] = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

static inline void war_goto_playback(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    double bpm = env->atomics->bpm;
    if (bpm <= 0.0) bpm = 100.0;
    double sec_per_cell = 15.0 / bpm;
    double gc = (double)env->ctx_wayland->gutter_cols;
    double pb_col = gc + env->play_bar_position_seconds / sec_per_cell;
    if (pb_col < gc) pb_col = gc;
    if (pb_col > env->ctx_wayland->right_bound) pb_col = env->ctx_wayland->right_bound;
    cur->instance[0].pos[0] = (float)pb_col;
    war_pan_follow(env);
}

static inline void war_playbar_goto_cursor(war_env* env) {
    war_simple_line_context* line = env->ctx_line;
    if (!line) return;
    float cursor_col = env->ctx_cursor->instance[0].pos[0];
    double bpm = env->atomics->bpm;
    if (bpm <= 0.0) bpm = 100.0;
    double sec_per_cell = 15.0 / bpm;
    double gc = (double)env->ctx_wayland->gutter_cols;
    env->play_bar_position_seconds = ((double)cursor_col - gc) * sec_per_cell;
    if (env->play_bar_position_seconds < 0.0) env->play_bar_position_seconds = 0.0;
    env->play_bar_last_frame_ms = 0;
    env->play_bar_last_us = 0;
    env->play_bar_prev_cell_pos = (double)cursor_col;
    memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
    memset(env->play_bar_direct_filter_lp, 0, sizeof(env->play_bar_direct_filter_lp));
    line->instance[0].pos[0] = cursor_col;
    // seek: activate note at cursor offset
    if (env->ctx_note && env->ctx_note->instance_count > 0) {
        float _cy2 = env->ctx_cursor->instance[0].pos[1];
        double _spc3 = 15.0 / bpm;
        for (uint32_t _ri = 0; _ri < env->ctx_note->instance_count; _ri++) {
            double _ns2 = env->ctx_note->instance[_ri].pos[0];
            double _nw2 = env->ctx_note->instance[_ri].size[0];
            double _ny2 = env->ctx_note->instance[_ri].pos[1];
            if (cursor_col > _ns2 && cursor_col < _ns2 + _nw2 && fabs(_ny2 - (double)_cy2) < 0.5) {
                uint32_t _pp2 = (uint32_t)(_ny2 - (double)env->ctx_wayland->gutter_rows);
                if (_pp2 > 127) _pp2 = 127;
                uint32_t _li2 = (env->ctx_note->instance[_ri].flags >> 4) & 0xF;
                if (_li2 < 1 || _li2 > 9) _li2 = 1;
                uint32_t _si2 = _pp2 * WAR_CAPTURE_SLOT_LAYERS + (_li2 - 1);
                war_capture_slot* _sl2 = &env->capture_slots[_si2];
                if (_sl2->samples && _sl2->count > 0) {
                    double _oc = (double)cursor_col - _ns2;
                    uint64_t _off2 = (uint64_t)(_oc * _spc3 * 48000.0 * 2.0);
                    if (_off2 & 1) _off2 &= ~1ULL;
                    double _rc = _nw2 - _oc;
                    if (_rc > 0.01) {
                        uint64_t _lim2 = _off2 + (uint64_t)(_rc * _spc3 * 48000.0 * 2.0);
                        if (_lim2 > _sl2->count) _lim2 = _sl2->count;
                        for (uint32_t _v2 = 0; _v2 < WAR_PLAY_BAR_VOICES; _v2++) {
                            if (!env->play_bar_voice_active[_v2]) {
                                 env->play_bar_voice_note[_v2] = _pp2;
                                 env->play_bar_voice_layer[_v2] = _li2;
                                 env->play_bar_voice_tick[_v2] = env->ctx_note->instance[_ri].tick;
                                env->play_bar_voice_read_pos[_v2] = _off2;
                                env->play_bar_voice_read_limit[_v2] = _lim2;
                                env->play_bar_voice_filter_lp[_v2][0] = 0.0f;
                                env->play_bar_voice_filter_lp[_v2][1] = 0.0f;
                                env->play_bar_voice_active[_v2] = 1;
                                break;
                            }
                        }
                    }
                }
                break;
            }
        }
    }
}

static inline void war_toggle_playbar_loop(war_env* env) {
    if (!env) return;
    env->play_bar_loop = !env->play_bar_loop;
    call_king_terry("PLAYBAR LOOP: %s", env->play_bar_loop ? "ON" : "OFF");
}

static inline void war_playbar_goto_start(war_env* env) {
    war_simple_line_context* line = env->ctx_line;
    if (!line) return;
    float gc = (float)env->ctx_wayland->gutter_cols;
    env->play_bar_position_seconds = 0.0;
    env->play_bar_last_frame_ms = 0;
    env->play_bar_last_us = 0;
    env->play_bar_prev_cell_pos = (double)gc;
    memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
    memset(env->play_bar_direct_filter_lp, 0, sizeof(env->play_bar_direct_filter_lp));
    line->instance[0].pos[0] = gc;
}

static inline void war_toggle_loop(war_env* env) {
    env->loop_mode = !env->loop_mode;
    call_king_terry("LOOP: %s", env->loop_mode ? "ON" : "OFF");
}

static inline void war_midi_toggle(war_env* env) {
    env->midi_toggle = !env->midi_toggle;
    call_king_terry("TOGGLE: %s", env->midi_toggle ? "ON" : "OFF");
}

static inline void war_tap_tempo(war_env* env) {
    env->tap_tempo_active = !env->tap_tempo_active;
    env->tap_tempo_count = 0;
    memset(env->tap_tempo_times, 0, sizeof(env->tap_tempo_times));
    if (env->tap_tempo_active)
        snprintf(env->status_msg, sizeof(env->status_msg), "TAP TEMPO: Space to tap");
    call_king_terry("TAP TEMPO: %s", env->tap_tempo_active ? "ON" : "OFF");
}

static inline void war_toggle_across(war_env* env) {
    env->across_mode = !env->across_mode;
    call_king_terry("ACROSS: %s", env->across_mode ? "ON" : "OFF");
}

static inline void war_toggle_resample(war_env* env) {
    env->across_resample = !env->across_resample;
    call_king_terry("RESAMPLE: %s", env->across_resample ? "OFF" : "ON");
}

static inline void war_toggle_crop(war_env* env) {
    if (!env || !env->ctx_cursor || !env->ctx_wayland) return;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count || !cur->instance) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || env->capture_slots[idx].count < 2) return;
    uint64_t frames = env->capture_slots[idx].count / 2;
    env->crop_active = !env->crop_active;
    if (env->crop_active) {
        env->crop_pitch = pitch;
        env->crop_layer = layer;
        env->crop_start_frame = 0;
        env->crop_end_frame = frames;
        call_king_terry("CROP: note=%u layer=%u (%llu frames)", pitch, layer,
                        (unsigned long long)frames);
    } else {
        uint64_t start = env->crop_start_frame;
        uint64_t end = env->crop_end_frame;
        if (start < end) {
            uint64_t new_frames = end - start;
            if (new_frames > 0 && new_frames < frames) {
                uint64_t new_count = new_frames * 2;
                float* new_data = malloc(new_count * sizeof(float));
                if (new_data) {
                    memcpy(new_data, env->capture_slots[idx].samples + start * 2,
                           new_count * sizeof(float));
                    free(env->capture_slots[idx].samples);
                    env->capture_slots[idx].samples = new_data;
                    env->capture_slots[idx].count = new_count;
                    env->capture_slots[idx].capacity = new_count;
                    call_king_terry("CROP: applied [%llu, %llu) -> %llu frames",
                                    (unsigned long long)start, (unsigned long long)end,
                                    (unsigned long long)new_frames);
                }
            }
        }
    }
}

static inline void war_compress(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    if (!slot->samples || slot->count < 4) return;
    double threshold_db = -24.0, ratio = 3.0, attack_ms = 2.0, release_ms = 50.0, makeup_db = 6.0;
    if (env->cmd_active && env->cmd_len > 9) {
        int n = sscanf(env->cmd_buf + 9, " %lf %lf %lf %lf %lf",
                       &threshold_db, &ratio, &attack_ms, &release_ms, &makeup_db);
        if (n < 1) threshold_db = -24.0;
        if (n < 2) ratio = 3.0;
        if (n < 3) attack_ms = 2.0;
        if (n < 4) release_ms = 50.0;
        if (n < 5) makeup_db = 6.0;
    }
    if (threshold_db >= 0.0 || ratio < 1.0 || attack_ms < 0.0 || release_ms < 0.0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "compress: usage :compress [thresh(dB) ratio attack(ms) release(ms) makeup(dB)]");
        return;
    }
    uint64_t frames = slot->count / 2;
    double threshold_lin = pow(10.0, threshold_db / 20.0);
    double attack_coef = attack_ms > 0.0 ? exp(-1.0 / (attack_ms * 0.001 * 48000.0)) : 0.0;
    double release_coef = release_ms > 0.0 ? exp(-1.0 / (release_ms * 0.001 * 48000.0)) : 0.0;
    double makeup_lin = pow(10.0, makeup_db / 20.0);
    float* new_data = malloc(slot->count * sizeof(float));
    if (!new_data) return;
    double rms_l = 0.0, rms_r = 0.0;
    double gr_state = 1.0; // smoothed gain reduction (stereo linked)
    double knee = 6.0;
    double slope = 1.0 - 1.0 / ratio;
    for (uint64_t f = 0; f < frames; f++) {
        double in_l = (double)slot->samples[f * 2];
        double in_r = (double)slot->samples[f * 2 + 1];
        // RMS envelope (one-pole on squared signal)
        rms_l = attack_coef * rms_l + (1.0 - attack_coef) * in_l * in_l;
        rms_r = attack_coef * rms_r + (1.0 - attack_coef) * in_r * in_r;
        double env_l = sqrt(rms_l);
        double env_r = sqrt(rms_r);
        double env = env_l > env_r ? env_l : env_r; // stereo link
        // soft knee gain reduction
        double target_gr = 1.0;
        if (env > 0.0) {
            double level_db = 20.0 * log10(env);
            double x = level_db - threshold_db;
            double gain_db = 0.0;
            if (x > knee * 0.5) {
                gain_db = -x * slope;
            } else if (x > -knee * 0.5) {
                double xk = x + knee * 0.5;
                gain_db = -slope * xk * xk / (2.0 * knee);
            }
            target_gr = pow(10.0, gain_db / 20.0);
        }
        // smooth gain reduction (attack = down, release = up)
        if (target_gr < gr_state)
            gr_state = attack_coef * gr_state + (1.0 - attack_coef) * target_gr;
        else
            gr_state = release_coef * gr_state + (1.0 - release_coef) * target_gr;
        new_data[f * 2] = (float)(in_l * gr_state * makeup_lin);
        new_data[f * 2 + 1] = (float)(in_r * gr_state * makeup_lin);
    }
    // save pre-compress audio state for undo
    uint64_t buf_size = 4 + 4 + 8 + 8 + slot->count * sizeof(float);
    uint8_t* audio_data = malloc(buf_size);
    if (audio_data) {
        uint8_t* p = audio_data;
        *(uint32_t*)p = 1; p += 4;
        *(uint32_t*)p = idx; p += 4;
        *(uint64_t*)p = slot->count; p += 8;
        *(uint64_t*)p = slot->capacity; p += 8;
        memcpy(p, slot->samples, slot->count * sizeof(float));
    }
    war_undo_save(env);
    if (audio_data) {
        uint32_t audio_idx = env->undo_pos - 1;
        free(env->undo_audio_data[audio_idx]);
        env->undo_audio_data[audio_idx] = audio_data;
        env->undo_audio_size[audio_idx] = buf_size;
    }
    free(slot->samples);
    slot->samples = new_data;
    snprintf(env->status_msg, sizeof(env->status_msg),
             "compress: thresh=%.0f ratio=%.1f attack=%.0fms release=%.0fms makeup=%.0fdB",
             threshold_db, ratio, attack_ms, release_ms, makeup_db);
}

static inline void war_saturate(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    if (!slot->samples || slot->count < 4) return;
    double drive = 2.0, mix = 1.0, makeup_db = 0.0;
    if (env->cmd_active && env->cmd_len > 10) {
        int n = sscanf(env->cmd_buf + 10, " %lf %lf %lf", &drive, &mix, &makeup_db);
        if (n < 1) drive = 2.0;
        if (n < 2) mix = 1.0;
        if (n < 3) makeup_db = 0.0;
    }
    if (drive < 0.0 || mix < 0.0 || mix > 1.0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "saturate: usage :saturate [drive(>=0) mix(0-1) makeup(dB)]");
        return;
    }
    uint64_t frames = slot->count / 2;
    double makeup_lin = pow(10.0, makeup_db / 20.0);
    // 2x oversampled processing to reduce aliasing
    uint64_t os_frames = frames * 2;
    float* os_buf = malloc(os_frames * 2 * sizeof(float));
    if (!os_buf) return;
    // upsample: even = original, odd = linear interpolation
    for (uint64_t f = 0; f < frames; f++) {
        os_buf[f * 4] = slot->samples[f * 2];
        os_buf[f * 4 + 1] = slot->samples[f * 2 + 1];
        if (f + 1 < frames) {
            os_buf[f * 4 + 2] = (slot->samples[f * 2] + slot->samples[(f + 1) * 2]) * 0.5f;
            os_buf[f * 4 + 3] = (slot->samples[f * 2 + 1] + slot->samples[(f + 1) * 2 + 1]) * 0.5f;
        } else {
            os_buf[f * 4 + 2] = slot->samples[f * 2];
            os_buf[f * 4 + 3] = slot->samples[f * 2 + 1];
        }
    }
    for (uint64_t f = 0; f < os_frames; f++) {
        double in_l = (double)os_buf[f * 2];
        double in_r = (double)os_buf[f * 2 + 1];
        double sat_l = tanh(in_l * drive) * makeup_lin;
        double sat_r = tanh(in_r * drive) * makeup_lin;
        os_buf[f * 2] = (float)(sat_l * mix + in_l * (1.0 - mix));
        os_buf[f * 2 + 1] = (float)(sat_r * mix + in_r * (1.0 - mix));
    }
    // downsample: keep every other sample
    float* new_data = malloc(slot->count * sizeof(float));
    if (!new_data) { free(os_buf); return; }
    for (uint64_t f = 0; f < frames; f++) {
        new_data[f * 2] = os_buf[f * 4];
        new_data[f * 2 + 1] = os_buf[f * 4 + 1];
    }
    free(os_buf);
    uint64_t buf_size = 4 + 4 + 8 + 8 + slot->count * sizeof(float);
    uint8_t* audio_data = malloc(buf_size);
    if (audio_data) {
        uint8_t* p = audio_data;
        *(uint32_t*)p = 1; p += 4;
        *(uint32_t*)p = idx; p += 4;
        *(uint64_t*)p = slot->count; p += 8;
        *(uint64_t*)p = slot->capacity; p += 8;
        memcpy(p, slot->samples, slot->count * sizeof(float));
    }
    war_undo_save(env);
    if (audio_data) {
        uint32_t audio_idx = env->undo_pos - 1;
        free(env->undo_audio_data[audio_idx]);
        env->undo_audio_data[audio_idx] = audio_data;
        env->undo_audio_size[audio_idx] = buf_size;
    }
    free(slot->samples);
    slot->samples = new_data;
    snprintf(env->status_msg, sizeof(env->status_msg),
             "saturate: drive=%.1f mix=%.2f makeup=%.0fdB", drive, mix, makeup_db);
}

static inline void war_reverb(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    if (!slot->samples || slot->count < 4) return;
    double decay = 0.5, mix = 0.3, predelay_ms = 0.0, damping = 0.3;
    if (env->cmd_active && env->cmd_len > 7) {
        int n = sscanf(env->cmd_buf + 7, " %lf %lf %lf %lf", &decay, &mix, &predelay_ms, &damping);
        if (n < 1) decay = 0.5;
        if (n < 2) mix = 0.3;
        if (n < 3) predelay_ms = 0.0;
        if (n < 4) damping = 0.3;
    }
    if (decay < 0.0 || decay >= 1.0 || mix < 0.0 || mix > 1.0 || predelay_ms < 0.0 || damping < 0.0 || damping > 1.0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "reverb: usage :reverb [decay(0-0.99) mix(0-1) predelay(ms) damping(0-1)]");
        return;
    }
    uint64_t frames = slot->count / 2;
    static const uint32_t comb_delays[] = {1427, 1783, 1979, 2341};
    uint32_t n_combs = 4;
    double comb_fb = decay * 0.7;
    double damp_coef = damping * damping * 0.95; // map 0-1 to 0-0.95 LP coefficient
    static const uint32_t ap_delays[] = {241, 83};
    double ap_gain = 0.5;
    uint64_t pd_samp = (uint64_t)(predelay_ms * 0.001 * 48000.0);
    float* comb_l[4]; float* comb_r[4];
    float* comb_dl[4]; float* comb_dr[4]; // damping state (one-pole LP per comb)
    uint32_t comb_pos[4] = {0, 0, 0, 0};
    for (uint32_t c = 0; c < n_combs; c++) {
        comb_l[c] = calloc(comb_delays[c], sizeof(float));
        comb_r[c] = calloc(comb_delays[c], sizeof(float));
        comb_dl[c] = calloc(1, sizeof(float));
        comb_dr[c] = calloc(1, sizeof(float));
        if (!comb_l[c] || !comb_r[c] || !comb_dl[c] || !comb_dr[c]) {
            for (uint32_t j = 0; j <= c; j++) { free(comb_l[j]); free(comb_r[j]); free(comb_dl[j]); free(comb_dr[j]); }
            return;
        }
    }
    // all-pass delay buffer (large enough for both stages, reused in series)
    uint32_t ap_max = ap_delays[0] > ap_delays[1] ? ap_delays[0] : ap_delays[1];
    float* ap_l = calloc(ap_max, sizeof(float));
    float* ap_r = calloc(ap_max, sizeof(float));
    uint32_t ap_pos = 0;
    // pre-delay buffer
    float* pd_l = calloc(pd_samp + 1, sizeof(float));
    float* pd_r = calloc(pd_samp + 1, sizeof(float));
    uint32_t pd_pos = 0;
    float* out = malloc(slot->count * sizeof(float));
    if (!out || !ap_l || !ap_r || !pd_l || !pd_r) {
        for (uint32_t c = 0; c < n_combs; c++) { free(comb_l[c]); free(comb_r[c]); }
        free(ap_l); free(ap_r); free(pd_l); free(pd_r); free(out);
        return;
    }
    for (uint64_t f = 0; f < frames; f++) {
        double in_l = (double)slot->samples[f * 2];
        double in_r = (double)slot->samples[f * 2 + 1];
        // pre-delay
        pd_l[pd_pos] = (float)in_l;
        pd_r[pd_pos] = (float)in_r;
        uint32_t pd_rp = (uint32_t)((pd_pos + pd_samp + 1 - pd_samp) % (pd_samp + 1));
        double dry_l = (double)pd_l[pd_rp];
        double dry_r = (double)pd_r[pd_rp];
        pd_pos = (pd_pos + 1) % (uint32_t)(pd_samp + 1);
        // comb filters (parallel) with damping
        double sum_l = 0.0, sum_r = 0.0;
        for (uint32_t c = 0; c < n_combs; c++) {
            uint32_t d = comb_delays[c];
            double dl = (double)comb_l[c][comb_pos[c]];
            double dr = (double)comb_r[c][comb_pos[c]];
            // damping: one-pole LP on feedback signal
            *comb_dl[c] += (float)(damp_coef * ((float)dl - *comb_dl[c]));
            *comb_dr[c] += (float)(damp_coef * ((float)dr - *comb_dr[c]));
            comb_l[c][comb_pos[c]] = (float)(dry_l + comb_fb * (double)*comb_dl[c]);
            comb_r[c][comb_pos[c]] = (float)(dry_r + comb_fb * (double)*comb_dr[c]);
            sum_l += dl;
            sum_r += dr;
            comb_pos[c] = (comb_pos[c] + 1) % d;
        }
        // all-pass filters (series, 2 stages in same buffer)
        double ap_in_l = sum_l, ap_in_r = sum_r;
        for (uint32_t a = 0; a < 2; a++) {
            uint32_t d = ap_delays[a];
            double dl = (double)ap_l[ap_pos];
            double dr = (double)ap_r[ap_pos];
            // y[n] = -g*x[n] + s[n-D], where s[n] = x[n] + g*y[n] stored in buffer
            double y_l = -ap_gain * ap_in_l + dl;
            double y_r = -ap_gain * ap_in_r + dr;
            ap_l[ap_pos] = (float)(ap_in_l + ap_gain * y_l);
            ap_r[ap_pos] = (float)(ap_in_r + ap_gain * y_r);
            ap_in_l = y_l; ap_in_r = y_r;
            ap_pos = (ap_pos + 1) % d;
        }
        // normalize by number of combs to prevent overload
        double norm = 1.0 / (double)n_combs;
        double wet_l = ap_in_l * norm;
        double wet_r = ap_in_r * norm;
        out[f * 2] = (float)(dry_l * (1.0 - mix) + wet_l * mix);
        out[f * 2 + 1] = (float)(dry_r * (1.0 - mix) + wet_r * mix);
    }
    for (uint32_t c = 0; c < n_combs; c++) { free(comb_l[c]); free(comb_r[c]); free(comb_dl[c]); free(comb_dr[c]); }
    free(ap_l); free(ap_r); free(pd_l); free(pd_r);
    uint64_t buf_size = 4 + 4 + 8 + 8 + slot->count * sizeof(float);
    uint8_t* audio_data = malloc(buf_size);
    if (audio_data) {
        uint8_t* p = audio_data;
        *(uint32_t*)p = 1; p += 4;
        *(uint32_t*)p = idx; p += 4;
        *(uint64_t*)p = slot->count; p += 8;
        *(uint64_t*)p = slot->capacity; p += 8;
        memcpy(p, slot->samples, slot->count * sizeof(float));
    }
    war_undo_save(env);
    if (audio_data) {
        uint32_t audio_idx = env->undo_pos - 1;
        free(env->undo_audio_data[audio_idx]);
        env->undo_audio_data[audio_idx] = audio_data;
        env->undo_audio_size[audio_idx] = buf_size;
    }
    free(slot->samples);
    slot->samples = out;
    snprintf(env->status_msg, sizeof(env->status_msg),
             "reverb: decay=%.2f mix=%.2f predelay=%.0fms damping=%.2f", decay, mix, predelay_ms, damping);
}

static inline void war_delay(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    if (!slot->samples || slot->count < 4) return;
    double time_ms = 200.0, feedback = 0.3, mix = 0.5, fb_damping = 0.0;
    if (env->cmd_active && env->cmd_len > 7) {
        int n = sscanf(env->cmd_buf + 7, " %lf %lf %lf %lf", &time_ms, &feedback, &mix, &fb_damping);
        if (n < 1) time_ms = 200.0;
        if (n < 2) feedback = 0.3;
        if (n < 3) mix = 0.5;
        if (n < 4) fb_damping = 0.0;
    }
    if (time_ms < 1.0 || feedback < 0.0 || feedback >= 1.0 || mix < 0.0 || mix > 1.0 || fb_damping < 0.0 || fb_damping > 1.0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "delay: usage :delay [time_ms(>=1) feedback(0-0.99) mix(0-1) fb_damping(0-1)]");
        return;
    }
    uint64_t frames = slot->count / 2;
    uint64_t delay_samp = (uint64_t)(time_ms * 0.001 * 48000.0);
    if (delay_samp < 1) delay_samp = 1;
    float* delay_line = calloc(delay_samp * 2, sizeof(float));
    if (!delay_line) return;
    float* out = malloc(slot->count * sizeof(float));
    if (!out) { free(delay_line); return; }
    float damp_l = 0.0f, damp_r = 0.0f;
    float damp_coef = (float)(fb_damping * fb_damping * 0.95);
    uint64_t wp = 0;
    for (uint64_t f = 0; f < frames; f++) {
        float in_l = slot->samples[f * 2];
        float in_r = slot->samples[f * 2 + 1];
        float dl = delay_line[wp * 2];
        float dr = delay_line[wp * 2 + 1];
        // damping: low-pass filtered feedback
        damp_l += damp_coef * (dl - damp_l);
        damp_r += damp_coef * (dr - damp_r);
        delay_line[wp * 2] = in_l + (float)feedback * damp_l;
        delay_line[wp * 2 + 1] = in_r + (float)feedback * damp_r;
        out[f * 2] = in_l * (1.0f - (float)mix) + dl * (float)mix;
        out[f * 2 + 1] = in_r * (1.0f - (float)mix) + dr * (float)mix;
        wp = (wp + 1) % delay_samp;
    }
    free(delay_line);
    uint64_t buf_size = 4 + 4 + 8 + 8 + slot->count * sizeof(float);
    uint8_t* audio_data = malloc(buf_size);
    if (audio_data) {
        uint8_t* p = audio_data;
        *(uint32_t*)p = 1; p += 4;
        *(uint32_t*)p = idx; p += 4;
        *(uint64_t*)p = slot->count; p += 8;
        *(uint64_t*)p = slot->capacity; p += 8;
        memcpy(p, slot->samples, slot->count * sizeof(float));
    }
    war_undo_save(env);
    if (audio_data) {
        uint32_t audio_idx = env->undo_pos - 1;
        free(env->undo_audio_data[audio_idx]);
        env->undo_audio_data[audio_idx] = audio_data;
        env->undo_audio_size[audio_idx] = buf_size;
    }
    free(slot->samples);
    slot->samples = out;
    snprintf(env->status_msg, sizeof(env->status_msg),
             "delay: time=%.0fms feedback=%.2f mix=%.2f fb_damping=%.2f", time_ms, feedback, mix, fb_damping);
}

static inline void war_gate(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    if (!slot->samples || slot->count < 4) return;
    double threshold_db = -40.0, attack_ms = 2.0, hold_ms = 10.0, release_ms = 50.0, floor_db = -80.0;
    if (env->cmd_active && env->cmd_len > 6) {
        int n = sscanf(env->cmd_buf + 6, " %lf %lf %lf %lf %lf",
                       &threshold_db, &attack_ms, &hold_ms, &release_ms, &floor_db);
        if (n < 1) threshold_db = -40.0;
        if (n < 2) attack_ms = 2.0;
        if (n < 3) hold_ms = 10.0;
        if (n < 4) release_ms = 50.0;
        if (n < 5) floor_db = -80.0;
    }
    if (threshold_db >= 0.0 || attack_ms < 0.0 || hold_ms < 0.0 || release_ms < 0.0 || floor_db >= 0.0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "gate: usage :gate [thresh(dB) attack(ms) hold(ms) release(ms) floor(dB)]");
        return;
    }
    uint64_t frames = slot->count / 2;
    double threshold_lin = pow(10.0, threshold_db / 20.0);
    double floor_lin = pow(10.0, floor_db / 20.0);
    double attack_coef = attack_ms > 0.0 ? exp(-1.0 / (attack_ms * 0.001 * 48000.0)) : 0.0;
    double release_coef = release_ms > 0.0 ? exp(-1.0 / (release_ms * 0.001 * 48000.0)) : 0.0;
    uint64_t hold_frames = (uint64_t)(hold_ms * 0.001 * 48000.0);
    float* out = malloc(slot->count * sizeof(float));
    if (!out) return;
    double env_l = 0.0, env_r = 0.0;
    double gate_gain = 1.0;
    uint64_t hold_counter = 0;
    double knee = 3.0;
    for (uint64_t f = 0; f < frames; f++) {
        double in_l = (double)slot->samples[f * 2];
        double in_r = (double)slot->samples[f * 2 + 1];
        // peak envelope
        double abs_l = fabs(in_l);
        double abs_r = fabs(in_r);
        env_l = attack_coef * env_l + (1.0 - attack_coef) * abs_l;
        env_r = attack_coef * env_r + (1.0 - attack_coef) * abs_r;
        double env = env_l > env_r ? env_l : env_r;
        // downward expander with soft knee
        double target = 1.0;
        double env_db = 20.0 * log10(env > 0.0 ? env : 1e-10);
        double x = env_db - threshold_db;
        double gain_db = 0.0;
        double ratio = 10.0;
        double slope = 1.0 - 1.0 / ratio;
        if (x > knee * 0.5) {
            // above threshold: no reduction
        } else if (x > -knee * 0.5) {
            double xk = x + knee * 0.5;
            gain_db = -slope * xk * xk / (2.0 * knee);
        } else {
            gain_db = -x * slope;
        }
        target = pow(10.0, gain_db / 20.0);
        double floor_gain = floor_lin / (env > 0.0 ? env : 1.0);
        if (target < floor_gain) target = floor_gain;
        // hold: keep gate open briefly after signal dips
        if (env >= threshold_lin) {
            hold_counter = hold_frames;
        } else if (hold_counter > 0) {
            hold_counter--;
            target = 1.0; // keep open during hold
        }
        // smooth gain (attack=close, release=open)
        if (target < gate_gain)
            gate_gain = attack_coef * gate_gain + (1.0 - attack_coef) * target;
        else
            gate_gain = release_coef * gate_gain + (1.0 - release_coef) * target;
        if (gate_gain < 0.0) gate_gain = 0.0;
        if (gate_gain > 1.0) gate_gain = 1.0;
        out[f * 2] = (float)(in_l * gate_gain);
        out[f * 2 + 1] = (float)(in_r * gate_gain);
    }
    uint64_t buf_size = 4 + 4 + 8 + 8 + slot->count * sizeof(float);
    uint8_t* audio_data = malloc(buf_size);
    if (audio_data) {
        uint8_t* p = audio_data;
        *(uint32_t*)p = 1; p += 4;
        *(uint32_t*)p = idx; p += 4;
        *(uint64_t*)p = slot->count; p += 8;
        *(uint64_t*)p = slot->capacity; p += 8;
        memcpy(p, slot->samples, slot->count * sizeof(float));
    }
    war_undo_save(env);
    if (audio_data) {
        uint32_t audio_idx = env->undo_pos - 1;
        free(env->undo_audio_data[audio_idx]);
        env->undo_audio_data[audio_idx] = audio_data;
        env->undo_audio_size[audio_idx] = buf_size;
    }
    free(slot->samples);
    slot->samples = out;
    snprintf(env->status_msg, sizeof(env->status_msg),
             "gate: thresh=%.0fdB attack=%.0fms hold=%.0fms release=%.0fms floor=%.0fdB",
             threshold_db, attack_ms, hold_ms, release_ms, floor_db);
}

static inline void war_deesser(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    if (!slot->samples || slot->count < 4) return;
    double threshold_db = -30.0, freq_hz = 6000.0, attack_ms = 1.0, release_ms = 30.0;
    if (env->cmd_active && env->cmd_len > 9) {
        int n = sscanf(env->cmd_buf + 9, " %lf %lf %lf %lf",
                       &threshold_db, &freq_hz, &attack_ms, &release_ms);
        if (n < 1) threshold_db = -30.0;
        if (n < 2) freq_hz = 6000.0;
        if (n < 3) attack_ms = 1.0;
        if (n < 4) release_ms = 30.0;
    }
    if (threshold_db >= 0.0 || freq_hz < 100.0 || freq_hz > 20000.0 || attack_ms < 0.0 || release_ms < 0.0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "deesser: usage :deesser [thresh(dB) freq(100-20000Hz) attack(ms) release(ms)]");
        return;
    }
    uint64_t frames = slot->count / 2;
    double attack_coef = attack_ms > 0.0 ? exp(-1.0 / (attack_ms * 0.001 * 48000.0)) : 0.0;
    double release_coef = release_ms > 0.0 ? exp(-1.0 / (release_ms * 0.001 * 48000.0)) : 0.0;
    float* out = malloc(slot->count * sizeof(float));
    if (!out) return;
    // TPT SVF for sidechain band-pass at sibilance frequency
    float g = tanf((float)M_PI * (float)freq_hz / 48000.0f);
    float R = 4.0f; // high Q for narrow band
    float s1_l = 0.0f, s1_r = 0.0f; // lp state
    float s2_l = 0.0f, s2_r = 0.0f; // bp state
    double rms = 0.0;
    double gr_state = 1.0;
    for (uint64_t f = 0; f < frames; f++) {
        float in_l = slot->samples[f * 2];
        float in_r = slot->samples[f * 2 + 1];
        // TPT SVF → bandpass output
        float v0_l = (in_l - s2_l * R - s1_l) / (1.0f + g * (g + R));
        float v0_r = (in_r - s2_r * R - s1_r) / (1.0f + g * (g + R));
        float ns1_l = s1_l + g * v0_l;
        float ns1_r = s1_r + g * v0_r;
        float ns2_l = s2_l + g * ns1_l; // bandpass = s2
        float ns2_r = s2_r + g * ns1_r;
        s1_l = ns1_l; s1_r = ns1_r;
        s2_l = ns2_l; s2_r = ns2_r;
        // RMS envelope of bandpass signal (stereo link)
        float bp = fabs(ns2_l) > fabs(ns2_r) ? fabs(ns2_l) : fabs(ns2_r);
        rms = attack_coef * rms + (1.0 - attack_coef) * (double)(bp * bp);
        double env = sqrt(rms);
        // gain reduction
        double target_gr = 1.0;
        if (env > 1e-10) {
            double env_db = 20.0 * log10(env);
            double excess = env_db - threshold_db;
            if (excess > 0.0) {
                double ratio = 10.0;
                double gain_db = -excess * (1.0 - 1.0 / ratio);
                target_gr = pow(10.0, gain_db / 20.0);
            }
        }
        if (target_gr < gr_state)
            gr_state = attack_coef * gr_state + (1.0 - attack_coef) * target_gr;
        else
            gr_state = release_coef * gr_state + (1.0 - release_coef) * target_gr;
        out[f * 2] = (float)((double)in_l * gr_state);
        out[f * 2 + 1] = (float)((double)in_r * gr_state);
    }
    uint64_t buf_size = 4 + 4 + 8 + 8 + slot->count * sizeof(float);
    uint8_t* audio_data = malloc(buf_size);
    if (audio_data) {
        uint8_t* p = audio_data;
        *(uint32_t*)p = 1; p += 4;
        *(uint32_t*)p = idx; p += 4;
        *(uint64_t*)p = slot->count; p += 8;
        *(uint64_t*)p = slot->capacity; p += 8;
        memcpy(p, slot->samples, slot->count * sizeof(float));
    }
    war_undo_save(env);
    if (audio_data) {
        uint32_t audio_idx = env->undo_pos - 1;
        free(env->undo_audio_data[audio_idx]);
        env->undo_audio_data[audio_idx] = audio_data;
        env->undo_audio_size[audio_idx] = buf_size;
    }
    free(slot->samples);
    slot->samples = out;
    snprintf(env->status_msg, sizeof(env->status_msg),
             "deesser: thresh=%.0fdB freq=%.0fHz attack=%.0fms release=%.0fms",
             threshold_db, freq_hz, attack_ms, release_ms);
}

static inline void war_chorus(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    if (!slot->samples || slot->count < 4) return;
    double rate_hz = 0.5, depth_ms = 5.0, mix = 0.5, base_ms = 20.0;
    if (env->cmd_active && env->cmd_len > 8) {
        int n = sscanf(env->cmd_buf + 8, " %lf %lf %lf %lf",
                       &rate_hz, &depth_ms, &mix, &base_ms);
        if (n < 1) rate_hz = 0.5;
        if (n < 2) depth_ms = 5.0;
        if (n < 3) mix = 0.5;
        if (n < 4) base_ms = 20.0;
    }
    if (rate_hz <= 0.0 || depth_ms <= 0.0 || mix < 0.0 || mix > 1.0 || base_ms < 1.0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "chorus: usage :chorus [rate(Hz) depth(ms) mix(0-1) base(ms)]");
        return;
    }
    uint64_t frames = slot->count / 2;
    uint64_t base_samp = (uint64_t)(base_ms * 0.001 * 48000.0);
    uint64_t depth_samp = (uint64_t)(depth_ms * 0.001 * 48000.0);
    if (depth_samp < 1) depth_samp = 1;
    uint64_t delay_len = base_samp + depth_samp + 1;
    float* delay_l = calloc(delay_len, sizeof(float));
    float* delay_r = calloc(delay_len, sizeof(float));
    float* out = malloc(slot->count * sizeof(float));
    if (!delay_l || !delay_r || !out) { free(delay_l); free(delay_r); free(out); return; }
    uint64_t wp = 0;
    double phase = 0.0;
    double phase_inc = rate_hz / 48000.0;
    for (uint64_t f = 0; f < frames; f++) {
        float in_l = slot->samples[f * 2];
        float in_r = slot->samples[f * 2 + 1];
        delay_l[wp] = in_l;
        delay_r[wp] = in_r;
        // LFO: left channel uses sin, right channel uses sin + 90deg for stereo spread
        float lfo_l = sinf((float)(2.0 * M_PI * phase));
        float lfo_r = sinf((float)(2.0 * M_PI * phase + M_PI_2));
        float mod_l = (float)base_samp + (float)depth_samp * (0.5f + 0.5f * lfo_l);
        float mod_r = (float)base_samp + (float)depth_samp * (0.5f + 0.5f * lfo_r);
        // linear interpolation of delay read
        float rp_l = fmodf((float)wp + (float)delay_len - mod_l, (float)delay_len);
        float rp_r = fmodf((float)wp + (float)delay_len - mod_r, (float)delay_len);
        uint64_t ri_l = (uint64_t)rp_l;
        uint64_t ri_r = (uint64_t)rp_r;
        float frac_l = rp_l - (float)ri_l;
        float frac_r = rp_r - (float)ri_r;
        uint64_t ri_l2 = (ri_l + 1) % delay_len;
        uint64_t ri_r2 = (ri_r + 1) % delay_len;
        float dl = delay_l[ri_l] + frac_l * (delay_l[ri_l2] - delay_l[ri_l]);
        float dr = delay_r[ri_r] + frac_r * (delay_r[ri_r2] - delay_r[ri_r]);
        out[f * 2] = in_l * (1.0f - (float)mix) + dl * (float)mix;
        out[f * 2 + 1] = in_r * (1.0f - (float)mix) + dr * (float)mix;
        wp = (wp + 1) % delay_len;
        phase += phase_inc;
        if (phase >= 1.0) phase -= 1.0;
    }
    free(delay_l); free(delay_r);
    uint64_t buf_size = 4 + 4 + 8 + 8 + slot->count * sizeof(float);
    uint8_t* audio_data = malloc(buf_size);
    if (audio_data) {
        uint8_t* p = audio_data;
        *(uint32_t*)p = 1; p += 4;
        *(uint32_t*)p = idx; p += 4;
        *(uint64_t*)p = slot->count; p += 8;
        *(uint64_t*)p = slot->capacity; p += 8;
        memcpy(p, slot->samples, slot->count * sizeof(float));
    }
    war_undo_save(env);
    if (audio_data) {
        uint32_t audio_idx = env->undo_pos - 1;
        free(env->undo_audio_data[audio_idx]);
        env->undo_audio_data[audio_idx] = audio_data;
        env->undo_audio_size[audio_idx] = buf_size;
    }
    free(slot->samples);
    slot->samples = out;
    snprintf(env->status_msg, sizeof(env->status_msg),
             "chorus: rate=%.1fHz depth=%.1fms mix=%.2f base=%.0fms",
             rate_hz, depth_ms, mix, base_ms);
}

static inline void war_autotune(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    if (!slot->samples || slot->count < 4) return;
    double retune_ms = 20.0;
    if (env->cmd_active && env->cmd_len > 10) {
        sscanf(env->cmd_buf + 10, " %lf", &retune_ms);
    }
    if (retune_ms < 0.0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "autotune: usage :autotune [retune(ms)]");
        return;
    }
    uint64_t total_frames = slot->count / 2;
    // analyze in overlapping frames, detect pitch, shift to nearest semitone
    uint64_t win_size = 2048;
    uint64_t hop = win_size / 4; // 75% overlap
    uint64_t n_frames = total_frames / hop;
    if (n_frames < 2) return;
    float* out = calloc(total_frames * 2, sizeof(float));
    if (!out) return;
    // fill output with original signal
    memcpy(out, slot->samples, total_frames * 2 * sizeof(float));
    double smooth_ratio = 1.0;
    double smooth_coef = retune_ms > 0.0 ? exp(-1.0 / (retune_ms * 0.001 * 48000.0 * 0.25)) : 0.0;
    float* window = malloc(win_size * sizeof(float));
    if (!window) { free(out); return; }
    for (uint64_t i = 0; i < win_size; i++)
        window[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)(win_size - 1)));
    float* frame_buf = malloc(win_size * sizeof(float));
    if (!frame_buf) { free(window); free(out); return; }
    for (uint64_t fr = 0; fr < n_frames; fr++) {
        uint64_t start = fr * hop;
        if (start + win_size > total_frames) break;
        // copy frame from original source for clean pitch detection
        for (uint64_t i = 0; i < win_size; i++)
            frame_buf[i] = (slot->samples[(start + i) * 2] + slot->samples[(start + i) * 2 + 1]) * 0.5f;
        // ACF-based pitch detection
        uint64_t min_lag = 48000 / 2000;
        uint64_t max_lag = 48000 / 50;
        if (max_lag > win_size / 2) max_lag = win_size / 2;
        if (min_lag < 1) min_lag = 1;
        double best_norm = 0.0;
        uint64_t best_lag = 0;
        double energy = 0.0;
        for (uint64_t i = 0; i < win_size; i++) energy += (double)(frame_buf[i] * frame_buf[i]);
        if (energy < 1e-10) continue;
        for (uint64_t lag = min_lag; lag <= max_lag; lag++) {
            double acf = 0.0;
            for (uint64_t i = 0; i < win_size - lag; i++)
                acf += (double)(frame_buf[i] * frame_buf[i + lag]);
            double norm = acf / energy;
            if (norm > best_norm) { best_norm = norm; best_lag = lag; }
        }
        if (best_lag == 0 || best_norm < 0.1) continue;
        double detected_hz = 48000.0 / (double)best_lag;
        double semitone = 12.0 * log2(detected_hz / 440.0);
        double nearest = round(semitone);
        double target_hz = 440.0 * pow(2.0, nearest / 12.0);
        double ratio = target_hz / detected_hz;
        if (ratio < 0.5 || ratio > 2.0) continue;
        if (smooth_coef > 0.0)
            smooth_ratio = smooth_coef * smooth_ratio + (1.0 - smooth_coef) * ratio;
        else
            smooth_ratio = ratio;
        uint64_t dst_frames = (uint64_t)((double)win_size / smooth_ratio);
        if (dst_frames < 1) dst_frames = 1;
        if (dst_frames > win_size * 2) dst_frames = win_size * 2;
        float* resampled = malloc(dst_frames * sizeof(float));
        if (!resampled) continue;
        for (uint64_t i = 0; i < dst_frames; i++) {
            double sp = (double)i * smooth_ratio;
            uint64_t si = (uint64_t)sp;
            double fr = sp - (double)si;
            if (si >= win_size - 1)
                resampled[i] = frame_buf[win_size - 1];
            else
                resampled[i] = (float)((double)frame_buf[si] * (1.0 - fr) + (double)frame_buf[si + 1] * fr);
        }
        // overlap-add into output (clear original first, then add)
        for (uint64_t i = 0; i < win_size && i < dst_frames; i++) {
            double w = (double)window[i];
            double val = (double)resampled[i] * w;
            out[(start + i) * 2] += (float)(val * 0.25);
            out[(start + i) * 2 + 1] += (float)(val * 0.25);
        }
        free(resampled);
    }
    free(frame_buf);
    free(window);
    uint64_t buf_size = 4 + 4 + 8 + 8 + slot->count * sizeof(float);
    uint8_t* audio_data = malloc(buf_size);
    if (audio_data) {
        uint8_t* p = audio_data;
        *(uint32_t*)p = 1; p += 4;
        *(uint32_t*)p = idx; p += 4;
        *(uint64_t*)p = slot->count; p += 8;
        *(uint64_t*)p = slot->capacity; p += 8;
        memcpy(p, slot->samples, slot->count * sizeof(float));
    }
    war_undo_save(env);
    if (audio_data) {
        uint32_t audio_idx = env->undo_pos - 1;
        free(env->undo_audio_data[audio_idx]);
        env->undo_audio_data[audio_idx] = audio_data;
        env->undo_audio_size[audio_idx] = buf_size;
    }
    free(slot->samples);
    slot->samples = out;
    snprintf(env->status_msg, sizeof(env->status_msg),
             "autotune: retune=%.0fms", retune_ms);
}

static inline void _war_across_pitch_shift(war_env* env, uint32_t src_note, uint32_t layer, int32_t radius) {
    if (!env || src_note > 127 || layer < 1 || layer > 9) return;
    uint32_t li = layer - 1;
    float* src_data = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].samples;
    uint64_t src_cnt = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].count;
    if (!src_data || src_cnt < 4) {
        call_king_terry("ACROSS: no data at note=%u layer=%u", src_note, layer);
        return;
    }
    uint64_t src_frames = src_cnt / 2;
    int32_t rad = radius > 0 ? radius : (int32_t)env->across_radius;
    uint32_t t_start = src_note > (uint32_t)rad ? src_note - (uint32_t)rad : 0;
    uint32_t t_end = src_note + (uint32_t)rad + 1;
    if (t_end > 128) t_end = 128;
    for (uint32_t t = t_start; t < t_end; t++) {
        if (t == src_note) continue;
        int32_t semi = (int32_t)t - (int32_t)src_note;
        double ratio = pow(2.0, (double)semi / 12.0);
        if (!env->across_resample) {
            // resample: changes pitch and duration
            uint64_t dst_frames = (uint64_t)((double)src_frames / ratio);
            if (dst_frames < 1) dst_frames = 1;
            uint64_t dst_cnt = dst_frames * 2;
            float* dst = malloc(dst_cnt * sizeof(float));
            if (!dst) continue;
            for (uint64_t i = 0; i < dst_frames; i++) {
                double sp = (double)i * ratio;
                uint64_t si = (uint64_t)sp;
                double fr = sp - (double)si;
                if (si >= src_frames - 1) {
                    dst[i*2] = src_data[(src_frames-1)*2];
                    dst[i*2+1] = src_data[(src_frames-1)*2+1];
                } else {
                    dst[i*2] = (float)(src_data[si*2]*(1.0-fr) + src_data[(si+1)*2]*fr);
                    dst[i*2+1] = (float)(src_data[si*2+1]*(1.0-fr) + src_data[(si+1)*2+1]*fr);
                }
            }
            uint32_t tidx = t * WAR_CAPTURE_SLOT_LAYERS + li;
            free(env->capture_slots[tidx].samples);
            env->capture_slots[tidx].samples = dst;
            env->capture_slots[tidx].count = dst_cnt;
            env->capture_slots[tidx].capacity = dst_cnt;
            env->capture_slots[tidx].gain = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].gain;
            env->capture_slots[tidx].pan = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].pan;
            env->capture_slots[tidx].eq = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].eq;
            env->capture_slots[tidx].attack = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].attack;
            env->capture_slots[tidx].sustain = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].sustain;
            env->capture_slots[tidx].release = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].release;
        } else {
            // non-resample: changes pitch, preserves duration
            uint64_t dst_frames = src_frames;
            uint64_t dst_cnt = dst_frames * 2;
            float* dst = malloc(dst_cnt * sizeof(float));
            if (!dst) continue;
            for (uint64_t i = 0; i < dst_frames; i++) {
                double sp = (double)i * ratio;
                uint64_t si = (uint64_t)sp;
                double fr = sp - (double)si;
                if (si >= src_frames - 1) {
                    dst[i*2] = src_data[(src_frames-1)*2];
                    dst[i*2+1] = src_data[(src_frames-1)*2+1];
                } else {
                    dst[i*2] = (float)(src_data[si*2]*(1.0-fr) + src_data[(si+1)*2]*fr);
                    dst[i*2+1] = (float)(src_data[si*2+1]*(1.0-fr) + src_data[(si+1)*2+1]*fr);
                }
            }
            uint32_t tidx = t * WAR_CAPTURE_SLOT_LAYERS + li;
            free(env->capture_slots[tidx].samples);
            env->capture_slots[tidx].samples = dst;
            env->capture_slots[tidx].count = dst_cnt;
            env->capture_slots[tidx].capacity = dst_cnt;
            env->capture_slots[tidx].gain = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].gain;
            env->capture_slots[tidx].pan = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].pan;
            env->capture_slots[tidx].eq = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].eq;
            env->capture_slots[tidx].attack = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].attack;
            env->capture_slots[tidx].sustain = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].sustain;
            env->capture_slots[tidx].release = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].release;
        }
    }
    call_king_terry("ACROSS: pitch-shifted note=%u radius=%d resample=%d", src_note, rad, env->across_resample);
}

static inline void war_capture_audio(war_env* env) {
    if (env->atomics->capture) {
        // second press: stop capture and save to current note/layer
        uint32_t layer = env->ctx_cursor->layer;
        if (layer >= 1 && layer <= 9) {
            uint32_t note = (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
            if (note > 127) note = 127;
            uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
            free(env->capture_slots[idx].samples);
            env->capture_slots[idx].samples = env->capture_accumulator;
            env->capture_slots[idx].count = env->capture_accumulator_count;
            env->capture_slots[idx].capacity =
                env->capture_accumulator_capacity;
            env->capture_accumulator = NULL;
            env->capture_accumulator_count = 0;
            env->capture_accumulator_capacity = 0;
            // ACROSS: pitch-shift within radius
            if (env->across_mode) {
                _war_across_pitch_shift(env, note, layer, env->across_radius);
            }
        }
        env->atomics->capture = 0;
        call_king_terry("CAPTURE: saved to note=%u layer=%u",
                        (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows),
                        env->ctx_cursor->layer);
    } else {
        // clear existing slot data at current note/layer
        uint32_t layer = env->ctx_cursor->layer;
        if (layer >= 1 && layer <= 9) {
            uint32_t note = (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
            if (note > 127) note = 127;
            uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
            free(env->capture_slots[idx].samples);
            env->capture_slots[idx].samples = NULL;
            env->capture_slots[idx].count = 0;
            env->capture_slots[idx].capacity = 0;
        }
        env->atomics->capture = 1;
        // flush stale data from previous capture
        const char* _dcname = env->dev_nodes[env->capture_mode > 0 ? env->capture_mode - 1 : 0];
        int _use_mic2 = _dcname && strstr(_dcname, "monitor") == NULL && strstr(_dcname, "loopback") == NULL;
        war_producer_consumer* _dcap2 = _use_mic2 ? env->pc_capture : env->pc_loopback;
        _dcap2->i_from_a = _dcap2->i_to_wr;
        free(env->capture_accumulator);
        env->capture_accumulator = NULL;
        env->capture_accumulator_count = 0;
        env->capture_accumulator_capacity = 0;
        call_king_terry("CAPTURE: ON at note=%u layer=%u",
                        (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows),
                        env->ctx_cursor->layer);
    }
}

static inline void war_capture_audio_midi(war_env* env) {
    int was_capturing = env->atomics->capture;
    war_capture_audio(env);
    // device routing — runs in any mode when capture starts
    if (!was_capturing && env->atomics->capture) {
        const char* _dname = env->dev_nodes[env->capture_mode > 0 ? env->capture_mode - 1 : 0];
        if (_dname && strstr(_dname, "loopback")) {
        } else if (_dname && strstr(_dname, ".monitor")) {
            char _snk[1024];
            size_t _sln = strlen(_dname);
            size_t _clen = _sln > 8 ? _sln - 8 : 0;
            if (_clen > sizeof(_snk) - 1) _clen = sizeof(_snk) - 1;
            memcpy(_snk, _dname, _clen);
            _snk[_clen] = '\0';
            char _cmd[1024];
            snprintf(_cmd, sizeof(_cmd), "pactl set-default-sink \"%s\" 2>/dev/null", _snk);
            system(_cmd);
        } else if (_dname) {
            char _cmd[1024];
            snprintf(_cmd, sizeof(_cmd), "pactl set-default-source \"%s\" 2>/dev/null", _dname);
            system(_cmd);
        }
    }
    // MIDI note placement
    if (env->active_mode == WAR_MODE_ID_MIDI && env->play_bar_playing && env->ctx_cursor->instance_count) {
        uint32_t layer = env->ctx_cursor->layer;
        if (layer < 1 || layer > 9) layer = 1;
        uint32_t note = (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
        if (note > 127) note = 127;
        if (!was_capturing && env->atomics->capture) {
            double _pb_bpm = env->atomics->bpm;
            if (_pb_bpm <= 0.0) _pb_bpm = 100.0;
            double _pb_spc = 15.0 / _pb_bpm;
            float _pb_pos = (float)((double)env->ctx_wayland->gutter_cols + env->play_bar_position_seconds / _pb_spc);
            if (env->ctx_note && env->ctx_note->instance_count < env->ctx_note->max_instances) {
                war_undo_save(env);
                uint32_t i = env->ctx_note->instance_count++;
                uint32_t col = (&env->ctx_color->layer_none)[layer];
                env->ctx_note->instance[i].pos[0] = _pb_pos;
                env->ctx_note->instance[i].pos[1] = (float)note + (float)env->ctx_wayland->gutter_rows;
                env->ctx_note->instance[i].pos[2] = 0.0f;
                env->ctx_note->instance[i].size[0] = 0.02f;
                env->ctx_note->instance[i].size[1] = 1.0f;
                env->ctx_note->instance[i].color[0] = ((col >> 24) & 0xFF) / 255.0f;
                env->ctx_note->instance[i].color[1] = ((col >> 16) & 0xFF) / 255.0f;
                env->ctx_note->instance[i].color[2] = ((col >> 8) & 0xFF) / 255.0f;
                env->ctx_note->instance[i].color[3] = (col & 0xFF) / 255.0f;
                env->ctx_note->instance[i].flags = (uint32_t)layer << 4;
                env->ctx_note->instance[i].tick = env->ctx_note->tick_counter++;
                env->capture_note_idx = (int32_t)i;
            }
        } else if (was_capturing && !env->atomics->capture && env->capture_note_idx >= 0) {
            uint32_t ni = (uint32_t)env->capture_note_idx;
            if (ni < env->ctx_note->instance_count) {
                double _pb_bpm2 = env->atomics->bpm;
                if (_pb_bpm2 <= 0.0) _pb_bpm2 = 100.0;
                double _pb_spc2 = 15.0 / _pb_bpm2;
                float _pb_pos2 = (float)((double)env->ctx_wayland->gutter_cols + env->play_bar_position_seconds / _pb_spc2);
                float _start = env->ctx_note->instance[ni].pos[0];
                env->ctx_note->instance[ni].size[0] = _pb_pos2 - _start;
                if (env->ctx_note->instance[ni].size[0] < 0.02f)
                    env->ctx_note->instance[ni].size[0] = 0.02f;
            }
            env->capture_note_idx = -1;
        }
    }
}

static inline void war_capture_mode1(war_env* env) { env->capture_mode = 1; war_capture_audio_midi(env); }
static inline void war_capture_mode2(war_env* env) { env->capture_mode = 2; war_capture_audio_midi(env); }
static inline void war_capture_mode3(war_env* env) { env->capture_mode = 3; war_capture_audio_midi(env); }
static inline void war_capture_mode4(war_env* env) { env->capture_mode = 4; war_capture_audio_midi(env); }

static inline void war_capture_advance(war_env* env) {
    if (!env->atomics->capture) return;
    if (!env->ctx_cursor || !env->ctx_cursor->instance_count) return;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t note = (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (note > 127) note = 127;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (env->capture_accumulator_count > 0) {
        free(env->capture_slots[idx].samples);
        env->capture_slots[idx].samples = env->capture_accumulator;
        env->capture_slots[idx].count = env->capture_accumulator_count;
        env->capture_slots[idx].capacity = env->capture_accumulator_capacity;
        env->capture_accumulator = NULL;
        env->capture_accumulator_count = 0;
        env->capture_accumulator_capacity = 0;
        if (env->across_mode)
            _war_across_pitch_shift(env, note, layer, env->across_radius);
    }
    // move cursor down 1 row
    float new_row = env->ctx_cursor->instance[0].pos[1] + 1.0f;
    if (new_row > 127.0f + env->ctx_wayland->gutter_rows)
        new_row = 127.0f + env->ctx_wayland->gutter_rows;
    env->ctx_cursor->instance[0].pos[1] = new_row;
    // clear the destination slot and reset accumulator
    note = (uint32_t)(new_row - (double)env->ctx_wayland->gutter_rows);
    if (note > 127) note = 127;
    idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    free(env->capture_slots[idx].samples);
    env->capture_slots[idx].samples = NULL;
    env->capture_slots[idx].count = 0;
    env->capture_slots[idx].capacity = 0;
    free(env->capture_accumulator);
    env->capture_accumulator = NULL;
    env->capture_accumulator_count = 0;
    env->capture_accumulator_capacity = 0;
    env->pc_loopback->i_from_a = env->pc_loopback->i_to_wr;
    call_king_terry("CAPTURE: advanced to note=%u", note);
}

static inline void war_preview_toggle(war_env* env) {
    uint32_t note = (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    _war_preview_start_voice(env, note, layer);
}

static inline void war_set_width_to_duration(war_env* env) {
    uint32_t note = (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    float new_w;
    if (env->capture_slots[idx].samples && env->capture_slots[idx].count > 0) {
        double bpm = env->atomics->bpm;
        if (bpm <= 0.0) bpm = 100.0;
        double sec_per_cell = 15.0 / bpm;
        double duration_sec = (double)env->capture_slots[idx].count / 48000.0 / 2.0;
        new_w = (float)(duration_sec / sec_per_cell);
        call_king_terry(
            "WIDTH: note=%u layer=%u count=%lu duration_sec=%.3f cells=%.2f",
            note, layer, env->capture_slots[idx].count, duration_sec, new_w);
    } else {
        new_w = 1.0f;
        call_king_terry(
            "WIDTH: no capture at note=%u layer=%u, default=1.0", note, layer);
    }
    env->ctx_cursor->instance[0].size[0] = new_w;
}

// forward decl for undo functions used by place/delete
static inline void war_undo_save(war_env* env);

static inline void war_chord_maj7(war_env* env) {
    war_note_context* note = env->ctx_note;
    war_cursor_context* cur = env->ctx_cursor;
    if (!note || !cur->instance_count) return;
    float root_row = cur->instance[0].pos[1];
    float col = cur->instance[0].pos[0];
    float w = cur->instance[0].size[0];
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t col32 = (&env->ctx_color->layer_none)[layer];
    float r = ((col32 >> 24) & 0xFF) / 255.0f;
    float g = ((col32 >> 16) & 0xFF) / 255.0f;
    float b = ((col32 >> 8) & 0xFF) / 255.0f;
    float a = (col32 & 0xFF) / 255.0f;
    int intervals[] = {0, 4, 7, 11};
    int n_intervals = 4;
    war_undo_save(env);
    for (int j = 0; j < n_intervals; j++) {
        float row = root_row + (float)intervals[j];
        if (row < 0 || row > 127.0f + (float)env->ctx_wayland->gutter_rows) continue;
        if (note->instance_count >= note->max_instances) break;
        uint32_t i = note->instance_count++;
        note->instance[i].pos[0] = col;
        note->instance[i].pos[1] = row;
        note->instance[i].pos[2] = 0.0f;
        note->instance[i].size[0] = w;
        note->instance[i].size[1] = cur->instance[0].size[1];
        note->instance[i].color[0] = r;
        note->instance[i].color[1] = g;
        note->instance[i].color[2] = b;
        note->instance[i].color[3] = a;
        note->instance[i].outline_color[0] = 0.0f;
        note->instance[i].outline_color[1] = 0.0f;
        note->instance[i].outline_color[2] = 0.0f;
        note->instance[i].outline_color[3] = 1.0f;
        note->instance[i].flags = (uint32_t)layer << 4;
        note->instance[i].tick = note->tick_counter++;
    }
    call_king_terry("MAJ7: root=%.0f width=%.1f", root_row - (double)env->ctx_wayland->gutter_rows, w);
}

static inline void _war_chord_generic(war_env* env, const int* intervals, int n_intervals, const char* name) {
    war_note_context* note = env->ctx_note;
    war_cursor_context* cur = env->ctx_cursor;
    if (!note || !cur->instance_count) return;
    uint32_t _chord_layer = env->ctx_cursor->layer;
    if (_chord_layer >= 1 && _chord_layer <= 9 && !(env->layer_visible & (1 << (_chord_layer - 1)))) return;
    float root_row = cur->instance[0].pos[1];
    float col = cur->instance[0].pos[0];
    float w = cur->instance[0].size[0];
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t col32 = (&env->ctx_color->layer_none)[layer];
    float r = ((col32 >> 24) & 0xFF) / 255.0f;
    float g = ((col32 >> 16) & 0xFF) / 255.0f;
    float b = ((col32 >> 8) & 0xFF) / 255.0f;
    float a = (col32 & 0xFF) / 255.0f;
    war_undo_save(env);
    for (int j = 0; j < n_intervals; j++) {
        float row = root_row + (float)intervals[j];
        if (row < 0 || row > 127.0f + (float)env->ctx_wayland->gutter_rows) continue;
        if (note->instance_count >= note->max_instances) break;
        uint32_t i = note->instance_count++;
        note->instance[i].pos[0] = col;
        note->instance[i].pos[1] = row;
        note->instance[i].pos[2] = 0.0f;
        note->instance[i].size[0] = w;
        note->instance[i].size[1] = cur->instance[0].size[1];
        note->instance[i].color[0] = r;
        note->instance[i].color[1] = g;
        note->instance[i].color[2] = b;
        note->instance[i].color[3] = a;
        note->instance[i].outline_color[0] = 0.0f;
        note->instance[i].outline_color[1] = 0.0f;
        note->instance[i].outline_color[2] = 0.0f;
        note->instance[i].flags = (uint32_t)layer << 4;
        note->instance[i].tick = note->tick_counter++;
    }
    call_king_terry("%s: root=%.0f width=%.1f", name, root_row - (double)env->ctx_wayland->gutter_rows, w);
}

static inline void war_chord_min9(war_env* env) {
    int intervals[] = {0, 3, 7, 10, 14};
    _war_chord_generic(env, intervals, 5, "MIN9");
}
static inline void war_chord_13(war_env* env) {
    int intervals[] = {0, 4, 7, 10, 14, 21};
    _war_chord_generic(env, intervals, 6, "DOM13");
}
static inline void war_chord_maj11(war_env* env) {
    int intervals[] = {0, 4, 7, 11, 14, 17};
    _war_chord_generic(env, intervals, 6, "MAJ11");
}
static inline void war_chord_maj13(war_env* env) {
    int intervals[] = {0, 4, 7, 11, 14, 21};
    _war_chord_generic(env, intervals, 6, "MAJ13");
}
static inline void war_chord_min11(war_env* env) {
    int intervals[] = {0, 3, 7, 10, 14, 17};
    _war_chord_generic(env, intervals, 6, "MIN11");
}
static inline void war_chord_min13(war_env* env) {
    int intervals[] = {0, 3, 7, 10, 14, 21};
    _war_chord_generic(env, intervals, 6, "MIN13");
}
static inline void war_chord_7(war_env* env) {
    int intervals[] = {0, 4, 7, 10};
    _war_chord_generic(env, intervals, 4, "DOM7");
}
static inline void war_chord_min7(war_env* env) {
    int intervals[] = {0, 3, 7, 10};
    _war_chord_generic(env, intervals, 4, "MIN7");
}
static inline void war_chord_9(war_env* env) {
    int intervals[] = {0, 4, 7, 10, 14};
    _war_chord_generic(env, intervals, 5, "DOM9");
}
static inline void war_chord_maj9(war_env* env) {
    int intervals[] = {0, 4, 7, 11, 14};
    _war_chord_generic(env, intervals, 5, "MAJ9");
}
static inline void war_chord_6(war_env* env) {
    int intervals[] = {0, 4, 7, 9};
    _war_chord_generic(env, intervals, 4, "MAJ6");
}
static inline void war_chord_2(war_env* env) {
    int intervals[] = {0, 2, 7};
    _war_chord_generic(env, intervals, 3, "SUS2");
}

static inline void war_place_note(war_env* env) {
    war_note_context* note = env->ctx_note;
    if (!note) return;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    if (note->instance_count >= note->max_instances) return;
    uint32_t _place_layer = env->ctx_cursor->layer;
    if (_place_layer == 0) {
        war_undo_save(env);
        uint32_t i = note->instance_count++;
        note->instance[i].pos[0] = cur->instance[0].pos[0];
        note->instance[i].pos[1] = cur->instance[0].pos[1];
        note->instance[i].pos[2] = 0.0f;
        note->instance[i].size[0] = cur->instance[0].size[0];
        note->instance[i].size[1] = cur->instance[0].size[1];
        uint32_t _mcol = env->ctx_color->layer_none;
        note->instance[i].color[0] = ((_mcol >> 24) & 0xFF) / 255.0f;
        note->instance[i].color[1] = ((_mcol >> 16) & 0xFF) / 255.0f;
        note->instance[i].color[2] = ((_mcol >> 8) & 0xFF) / 255.0f;
        note->instance[i].color[3] = (_mcol & 0xFF) / 255.0f;
        note->instance[i].outline_color[0] = 0.0f;
        note->instance[i].outline_color[1] = 0.0f;
        note->instance[i].outline_color[2] = 0.0f;
        note->instance[i].outline_color[3] = 1.0f;
        note->instance[i].flags = WAR_NEW_VULKAN_FLAGS_MUTE | (env->layer_visible << 8);
        note->instance[i].tick = note->tick_counter++;
        call_king_terry(
            "MUTE: placed #%u at pos=(%.1f,%.1f) size=(%.1f,%.1f) tick=%lu mask=%u",
            i,
            note->instance[i].pos[0],
            note->instance[i].pos[1],
            note->instance[i].size[0],
            note->instance[i].size[1],
            note->instance[i].tick,
            (unsigned)env->layer_visible);
        return;
    }
    if (_place_layer >= 1 && _place_layer <= 9 && !(env->layer_visible & (1 << (_place_layer - 1)))) return;
    war_undo_save(env);
    uint32_t i = note->instance_count++;
    note->instance[i].pos[0] = cur->instance[0].pos[0];
    note->instance[i].pos[1] = cur->instance[0].pos[1];
    note->instance[i].pos[2] = 0.0f;
    note->instance[i].size[0] = cur->instance[0].size[0];
    note->instance[i].size[1] = cur->instance[0].size[1];
    uint32_t col = (&env->ctx_color->layer_none)[env->ctx_cursor->layer];
    note->instance[i].color[0] = ((col >> 24) & 0xFF) / 255.0f;
    note->instance[i].color[1] = ((col >> 16) & 0xFF) / 255.0f;
    note->instance[i].color[2] = ((col >> 8) & 0xFF) / 255.0f;
    note->instance[i].color[3] = (col & 0xFF) / 255.0f;
    note->instance[i].outline_color[0] = 0.0f;
    note->instance[i].outline_color[1] = 0.0f;
    note->instance[i].outline_color[2] = 0.0f;
    note->instance[i].outline_color[3] = 1.0f;
    note->instance[i].flags = (uint32_t)env->ctx_cursor->layer << 4;
    note->instance[i].tick = note->tick_counter++;
    call_king_terry(
        "NOTE: placed #%u at pos=(%.1f,%.1f) size=(%.1f,%.1f) tick=%lu",
        i,
        note->instance[i].pos[0],
        note->instance[i].pos[1],
        note->instance[i].size[0],
        note->instance[i].size[1],
        note->instance[i].tick);
}

static inline void war_undo_save(war_env* env) {
    war_note_context* note = env->ctx_note;
    if (!note) return;
    // discard any redo branches beyond current position
    for (uint32_t i = env->undo_pos + 1; i < env->undo_count && i < WAR_UNDO_MAX; i++) {
        free(env->undo_notes[i]);
        env->undo_notes[i] = NULL;
        free(env->undo_audio_data[i]);
        env->undo_audio_data[i] = NULL;
        env->undo_audio_size[i] = 0;
    }
    env->undo_count = env->undo_pos + 1;
    // shift if full
    if (env->undo_count > WAR_UNDO_MAX) {
        free(env->undo_notes[0]);
        free(env->undo_audio_data[0]);
        for (uint32_t i = 1; i < WAR_UNDO_MAX; i++) {
            env->undo_notes[i - 1] = env->undo_notes[i];
            env->undo_note_counts[i - 1] = env->undo_note_counts[i];
            env->undo_audio_data[i - 1] = env->undo_audio_data[i];
            env->undo_audio_size[i - 1] = env->undo_audio_size[i];
        }
        env->undo_notes[WAR_UNDO_MAX - 1] = NULL;
        env->undo_note_counts[WAR_UNDO_MAX - 1] = 0;
        env->undo_audio_data[WAR_UNDO_MAX - 1] = NULL;
        env->undo_audio_size[WAR_UNDO_MAX - 1] = 0;
        env->undo_count--;
        env->undo_pos--;
    }
    // save current state at undo_pos, then advance
    uint32_t idx = env->undo_pos;
    env->undo_note_counts[idx] = note->instance_count;
    size_t sz = note->instance_count * sizeof(war_new_vulkan_note_instance);
    free(env->undo_notes[idx]);
    env->undo_notes[idx] = malloc(sz);
    if (env->undo_notes[idx])
        memcpy(env->undo_notes[idx], note->instance, sz);
    // free stale audio data at this index
    free(env->undo_audio_data[idx]);
    env->undo_audio_data[idx] = NULL;
    env->undo_audio_size[idx] = 0;
    env->undo_pos++;
    if (env->undo_pos > env->undo_count)
        env->undo_count = env->undo_pos;
}

// save current capture_slot state for the same slots encoded in entry save_idx-1
static inline void _war_undo_save_current_audio(war_env* env, uint32_t save_idx) {
    uint8_t* prev = env->undo_audio_data[save_idx - 1];
    if (!prev || env->undo_audio_size[save_idx - 1] < 4) return;
    uint32_t n = *(uint32_t*)prev;
    if (n == 0 || n > 256) return;
    uint32_t slots[256];
    uint8_t* rp = prev + 4;
    for (uint32_t i = 0; i < n; i++) {
        slots[i] = *(uint32_t*)rp; rp += 4;
        uint64_t cnt = *(uint64_t*)rp; rp += 8;
        rp += 8; // skip capacity
        rp += cnt * sizeof(float);
    }
    uint64_t total = 4;
    for (uint32_t i = 0; i < n; i++) {
        total += 4 + 8 + 8 + env->capture_slots[slots[i]].count * sizeof(float);
    }
    uint8_t* buf = malloc(total);
    if (!buf) return;
    uint8_t* wp = buf;
    *(uint32_t*)wp = n; wp += 4;
    for (uint32_t i = 0; i < n; i++) {
        war_capture_slot* sl = &env->capture_slots[slots[i]];
        *(uint32_t*)wp = slots[i]; wp += 4;
        *(uint64_t*)wp = sl->count; wp += 8;
        *(uint64_t*)wp = sl->capacity; wp += 8;
        if (sl->samples && sl->count > 0)
            memcpy(wp, sl->samples, sl->count * sizeof(float));
        wp += sl->count * sizeof(float);
    }
    free(env->undo_audio_data[save_idx]);
    env->undo_audio_data[save_idx] = buf;
    env->undo_audio_size[save_idx] = total;
}

// restore capture_slot state from undo entry restore_idx
static inline void _war_undo_restore_audio(war_env* env, uint32_t restore_idx) {
    uint8_t* data = env->undo_audio_data[restore_idx];
    if (!data || env->undo_audio_size[restore_idx] < 4) return;
    uint32_t n = *(uint32_t*)data;
    if (n == 0 || n > 256) return;
    uint8_t* rp = data + 4;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t si = *(uint32_t*)rp; rp += 4;
        uint64_t cnt = *(uint64_t*)rp; rp += 8;
        uint64_t cap = *(uint64_t*)rp; rp += 8;
        if (si < 128 * WAR_CAPTURE_SLOT_LAYERS) {
            war_capture_slot* sl = &env->capture_slots[si];
            free(sl->samples);
            if (cnt > 0) {
                sl->samples = malloc(cnt * sizeof(float));
                if (sl->samples) {
                    memcpy(sl->samples, rp, cnt * sizeof(float));
                    sl->count = cnt;
                    sl->capacity = cap;
                }
            } else {
                sl->samples = NULL;
                sl->count = 0;
                sl->capacity = 0;
            }
        }
        rp += cnt * sizeof(float);
    }
}

static inline void war_undo(war_env* env) {
    war_note_context* note = env->ctx_note;
    if (!note || env->undo_pos == 0) return;
    uint32_t save_idx = env->undo_pos;
    // save current audio state of slots from the previous operation (for redo)
    if (save_idx > 0)
        _war_undo_save_current_audio(env, save_idx);
    // save current live note state at undo_pos so redo can find it
    env->undo_note_counts[save_idx] = note->instance_count;
    size_t sz = note->instance_count * sizeof(war_new_vulkan_note_instance);
    free(env->undo_notes[save_idx]);
    env->undo_notes[save_idx] = malloc(sz);
    if (env->undo_notes[save_idx])
        memcpy(env->undo_notes[save_idx], note->instance, sz);
    if (save_idx >= env->undo_count)
        env->undo_count = save_idx + 1;
    // restore previous state
    uint32_t restore_idx = env->undo_pos - 1;
    // restore audio
    _war_undo_restore_audio(env, restore_idx);
    // restore notes
    uint32_t cnt = env->undo_note_counts[restore_idx];
    if (cnt > note->max_instances) cnt = note->max_instances;
    if (env->undo_notes[restore_idx]) {
        memcpy(note->instance, env->undo_notes[restore_idx], cnt * sizeof(war_new_vulkan_note_instance));
        note->instance_count = cnt;
    }
    env->undo_pos = restore_idx;
}

static inline void war_redo(war_env* env) {
    war_note_context* note = env->ctx_note;
    if (!note || env->undo_count < 2 || env->undo_pos >= env->undo_count - 1) return;
    uint32_t restore_idx = env->undo_pos + 1;
    // restore audio
    _war_undo_restore_audio(env, restore_idx);
    // restore notes
    uint32_t cnt = env->undo_note_counts[restore_idx];
    if (cnt > note->max_instances) cnt = note->max_instances;
    if (env->undo_notes[restore_idx]) {
        memcpy(note->instance, env->undo_notes[restore_idx], cnt * sizeof(war_new_vulkan_note_instance));
        note->instance_count = cnt;
    }
    env->undo_pos = restore_idx;
}

static inline void war_trim_note_under_cursor(war_env* env) {
    war_note_context* note = env->ctx_note;
    if (!note || !note->instance_count) return;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    war_undo_save(env);
    float cx = cur->instance[0].pos[0];
    float cy = cur->instance[0].pos[1];
    // find the most recently placed note whose body contains the cursor
    uint32_t best = UINT32_MAX;
    uint64_t best_tick = 0;
    for (uint32_t i = 0; i < note->instance_count; i++) {
        float nx0 = note->instance[i].pos[0];
        float nx1 = nx0 + note->instance[i].size[0];
        float ny0 = note->instance[i].pos[1];
        float ny1 = ny0 + note->instance[i].size[1];
        if (cx >= nx0 && cx < nx1 && cy >= ny0 && cy < ny1) {
            uint32_t _tl = (note->instance[i].flags >> 4) & 0xF;
            if (_tl >= 1 && _tl <= 9 && !(env->layer_visible & (1 << (_tl - 1)))) continue;
            if (best == UINT32_MAX || note->instance[i].tick > best_tick) {
                best = i;
                best_tick = note->instance[i].tick;
            }
        }
    }
    if (best == UINT32_MAX) return;
    float new_w = cx - note->instance[best].pos[0];
    if (new_w < 0.01f) new_w = 0.01f;
    note->instance[best].size[0] = new_w;
    call_king_terry("TRIM: note #%u new width=%.2f (cursor at %.1f)",
                    best, new_w, cx);
}

static inline void war_delete_note_under_cursor(war_env* env) {
    war_note_context* note = env->ctx_note;
    if (!note || !note->instance_count) return;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    war_undo_save(env);
    if (cur->visual_active) {
        // visual mode: delete all notes in selection rectangle
        float x0 = cur->visual_anchor_col;
        float x1 = cur->instance[0].pos[0];
        float y0 = cur->visual_anchor_row;
        float y1 = cur->instance[0].pos[1];
        if (x1 < x0) { float t = x0; x0 = x1; x1 = t; }
        if (y1 < y0) { float t = y0; y0 = y1; y1 = t; }
        uint32_t deleted = 0;
        for (int32_t i = (int32_t)note->instance_count - 1; i >= 0; i--) {
            float nx0 = note->instance[i].pos[0];
            float nx1 = nx0 + note->instance[i].size[0];
            float ny = note->instance[i].pos[1];
            if (nx0 < x1 && nx1 > x0 && ny >= y0 && ny <= y1) {
                uint32_t _dl = (note->instance[i].flags >> 4) & 0xF;
                if (_dl >= 1 && _dl <= 9 && !(env->layer_visible & (1 << (_dl - 1)))) continue;
                uint32_t last = note->instance_count - 1;
                if ((uint32_t)i != last)
                    note->instance[i] = note->instance[last];
                note->instance_count--;
                deleted++;
            }
        }
        call_king_terry("VISUAL DELETE: removed %u notes", deleted);
    } else {
        // normal mode: delete single note under cursor
        float cx0 = cur->instance[0].pos[0];
        float cx1 = cx0 + cur->instance[0].size[0];
        float cy0 = cur->instance[0].pos[1];
        float cy1 = cy0 + cur->instance[0].size[1];
        uint32_t best = UINT32_MAX;
        uint64_t best_tick = 0;
        for (uint32_t i = 0; i < note->instance_count; i++) {
            float nx0 = note->instance[i].pos[0];
            float nx1 = nx0 + note->instance[i].size[0];
            float ny0 = note->instance[i].pos[1];
            float ny1 = ny0 + note->instance[i].size[1];
            if (cx0 < nx1 && cx1 > nx0 && cy0 < ny1 && cy1 > ny0) {
                uint32_t _dl2 = (note->instance[i].flags >> 4) & 0xF;
                if (_dl2 >= 1 && _dl2 <= 9 && !(env->layer_visible & (1 << (_dl2 - 1)))) continue;
                if (best == UINT32_MAX) {
                    best = i;
                    best_tick = note->instance[i].tick;
                } else {
                    uint32_t _prev_l = (note->instance[best].flags >> 4) & 0xF;
                    // prefer higher layer (topmost first), then higher tick (most recent)
                    if (_dl2 > _prev_l || (_dl2 == _prev_l && note->instance[i].tick > best_tick)) {
                        best = i;
                        best_tick = note->instance[i].tick;
                    }
                }
            }
        }
        if (best == UINT32_MAX) return;
        uint32_t last = note->instance_count - 1;
        if (best != last) { note->instance[best] = note->instance[last]; }
        note->instance_count--;
        call_king_terry("NOTE: deleted #%u at pos=(%.1f,%.1f), remaining=%u",
                        best,
                        note->instance[best].pos[0],
                        note->instance[best].pos[1],
                        note->instance_count);
    }
}

static inline void war_split_note(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_note_context* note = env->ctx_note;
    if (!cur->instance_count || !note || !note->instance_count) return;
    float cx = cur->instance[0].pos[0];
    float cy = cur->instance[0].pos[1];
    uint32_t best = UINT32_MAX;
    uint64_t best_tick = 0;
    for (uint32_t i = 0; i < note->instance_count; i++) {
        float nx0 = note->instance[i].pos[0];
        float nx1 = nx0 + note->instance[i].size[0];
        float ny = note->instance[i].pos[1];
        if (cx >= nx0 && cx < nx1 && ny == cy) {
            uint32_t _nl = (note->instance[i].flags >> 4) & 0xF;
            if (_nl >= 1 && _nl <= 9 && !(env->layer_visible & (1 << (_nl - 1)))) continue;
            if (best == UINT32_MAX || note->instance[i].tick > best_tick) {
                best = i; best_tick = note->instance[i].tick;
            }
        }
    }
    if (best == UINT32_MAX) return;
    float note_start = note->instance[best].pos[0];
    float note_end = note_start + note->instance[best].size[0];
    float left_w = cx - note_start;
    float right_w = note_end - cx;
    if (left_w < 0.02f || right_w < 0.02f) return;
    uint32_t src_pitch = (uint32_t)(cy - (double)env->ctx_wayland->gutter_rows);
    if (src_pitch > 127) return;
    uint32_t layer = (note->instance[best].flags >> 4) & 0xF;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t src_idx = src_pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    // compute sample split point: left_w columns worth of audio
    double bpm = env->atomics->bpm;
    if (bpm <= 0.0) bpm = 100.0;
    double sec_per_cell = 15.0 / bpm;
    uint64_t split_frames = (uint64_t)((double)left_w * sec_per_cell * 48000.0);
    if (split_frames < 1) return;
    uint64_t split_samples = split_frames * 2;
    war_capture_slot* src_slot = &env->capture_slots[src_idx];
    if (!src_slot->samples || split_samples >= src_slot->count) return;
    uint64_t right_samples = src_slot->count - split_samples;
    if (right_samples & 1) right_samples &= ~1ULL;
    // find empty dest slot first
    uint32_t dest_pitch = UINT32_MAX;
    uint32_t mi = 0;
    for (uint32_t p = src_pitch + 1; p < 128; p++) {
        mi = p * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
        if (!env->capture_slots[mi].samples || env->capture_slots[mi].count < 2) {
            dest_pitch = p;
            break;
        }
    }
    if (dest_pitch == UINT32_MAX) return;
    // save pre-split audio state for both slots
    uint64_t src_size = src_slot->count * sizeof(float);
    uint64_t dst_cnt = env->capture_slots[mi].count;
    uint64_t dst_size = dst_cnt * sizeof(float);
    // flat buffer: [n(4) src_idx(4) cnt(8) cap(8) samples(n*4) dst_idx(4) cnt(8) cap(8) samples(n*4)]
    uint64_t audio_total = 4 + 4+8+8+src_size + 4+8+8+dst_size;
    uint8_t* audio_data = malloc(audio_total);
    if (!audio_data) return;
    {
        uint8_t* p = audio_data;
        *(uint32_t*)p = 2; p += 4;
        *(uint32_t*)p = src_idx; p += 4;
        *(uint64_t*)p = src_slot->count; p += 8;
        *(uint64_t*)p = src_slot->capacity; p += 8;
        if (src_slot->samples && src_slot->count > 0)
            memcpy(p, src_slot->samples, src_size);
        p += src_size;
        *(uint32_t*)p = mi; p += 4;
        *(uint64_t*)p = dst_cnt; p += 8;
        *(uint64_t*)p = env->capture_slots[mi].capacity; p += 8;
        if (env->capture_slots[mi].samples && dst_cnt > 0)
            memcpy(p, env->capture_slots[mi].samples, dst_size);
    }
    // save note state
    war_undo_save(env);
    // store audio snapshot at the same undo index
    uint32_t audio_idx = env->undo_pos - 1;
    free(env->undo_audio_data[audio_idx]);
    env->undo_audio_data[audio_idx] = audio_data;
    env->undo_audio_size[audio_idx] = audio_total;
    // now perform the split
    float keep_w, move_w;
    uint32_t keep_i;
    float move_pitch_row = (float)dest_pitch + (float)env->ctx_wayland->gutter_rows;
    uint32_t col = (&env->ctx_color->layer_none)[layer];
    if (left_w >= right_w) {
        // left (larger) stays at original pitch, right (smaller) moves up
        keep_w = left_w; move_w = right_w;
        keep_i = best;
        // copy only RIGHT portion audio to new slot (from split_samples onward)
        float* copy = malloc(right_samples * sizeof(float));
        if (copy) {
            memcpy(copy, src_slot->samples + split_samples, right_samples * sizeof(float));
            free(env->capture_slots[mi].samples);
            env->capture_slots[mi] = *src_slot;
            env->capture_slots[mi].samples = copy;
            env->capture_slots[mi].count = right_samples;
            env->capture_slots[mi].capacity = right_samples;
        }
        // trim original slot to LEFT portion only (first split_samples)
        float* trim = malloc(split_samples * sizeof(float));
        if (trim) {
            memcpy(trim, src_slot->samples, split_samples * sizeof(float));
            free(src_slot->samples);
            src_slot->samples = trim;
            src_slot->count = split_samples;
            src_slot->capacity = split_samples;
        }
        note->instance[keep_i].size[0] = keep_w;
        if (note->instance_count < note->max_instances) {
            uint32_t ni = note->instance_count++;
            note->instance[ni] = note->instance[keep_i];
            note->instance[ni].pos[0] = cx;
            note->instance[ni].pos[1] = move_pitch_row;
            note->instance[ni].size[0] = move_w;
            note->instance[ni].color[0] = ((col >> 24) & 0xFF) / 255.0f;
            note->instance[ni].color[1] = ((col >> 16) & 0xFF) / 255.0f;
            note->instance[ni].color[2] = ((col >> 8) & 0xFF) / 255.0f;
            note->instance[ni].color[3] = (col & 0xFF) / 255.0f;
            note->instance[ni].tick = note->tick_counter++;
        }
    } else {
        // right (larger) stays at original pitch, left (smaller) moves up
        keep_w = right_w; move_w = left_w;
        note->instance[best].pos[0] = cx;
        note->instance[best].size[0] = keep_w;
        keep_i = best;
        // copy only LEFT portion audio to new slot (first split_samples)
        float* copy = malloc(split_samples * sizeof(float));
        if (copy) {
            memcpy(copy, src_slot->samples, split_samples * sizeof(float));
            free(env->capture_slots[mi].samples);
            env->capture_slots[mi] = *src_slot;
            env->capture_slots[mi].samples = copy;
            env->capture_slots[mi].count = split_samples;
            env->capture_slots[mi].capacity = split_samples;
        }
        // trim original slot to RIGHT portion only (from split_samples onward)
        if (right_samples > 0) {
            float* trim = malloc(right_samples * sizeof(float));
            if (trim) {
                memcpy(trim, src_slot->samples + split_samples, right_samples * sizeof(float));
                free(src_slot->samples);
                src_slot->samples = trim;
                src_slot->count = right_samples;
                src_slot->capacity = right_samples;
            }
        }
        if (note->instance_count < note->max_instances) {
            uint32_t ni = note->instance_count++;
            note->instance[ni] = note->instance[keep_i];
            note->instance[ni].pos[0] = note_start;
            note->instance[ni].pos[1] = move_pitch_row;
            note->instance[ni].size[0] = move_w;
            note->instance[ni].color[0] = ((col >> 24) & 0xFF) / 255.0f;
            note->instance[ni].color[1] = ((col >> 16) & 0xFF) / 255.0f;
            note->instance[ni].color[2] = ((col >> 8) & 0xFF) / 255.0f;
            note->instance[ni].color[3] = (col & 0xFF) / 255.0f;
            note->instance[ni].tick = note->tick_counter++;
        }
    }
    snprintf(env->status_msg, sizeof(env->status_msg), "split: left=%.1f right=%.1f", left_w, right_w);
}

static inline void war_wave_view(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    if (!(env->layer_visible & (1 << (layer - 1)))) return;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || env->capture_slots[idx].count < 2) return;
    // find a note at this pitch to get its width
    float nw = 1.0f;
    if (env->ctx_note) {
        float cy = cur->instance[0].pos[1];
        for (uint32_t i = 0; i < env->ctx_note->instance_count; i++) {
            uint32_t _wl = (env->ctx_note->instance[i].flags >> 4) & 0xF;
            if (_wl >= 1 && _wl <= 9 && !(env->layer_visible & (1 << (_wl - 1)))) continue;
            if (fabsf(env->ctx_note->instance[i].pos[1] - cy) < 0.5f) {
                nw = env->ctx_note->instance[i].size[0];
                break;
            }
        }
    }
    env->wave_view_active = 1;
    env->wave_view_pitch = pitch;
    env->wave_view_layer = layer;
    env->wave_view_note_width = nw;
    env->active_mode = WAR_MODE_ID_WAV;
    env->ctx_wayland->panning[0] = 0;
    call_king_terry("WAVE: pitch=%u layer=%u width=%.1f (%llu samples)", pitch, layer, nw,
                    (unsigned long long)env->capture_slots[idx].count);
}

static inline void war_yank(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->visual_active || !cur->instance_count) return;
    war_note_context* note = env->ctx_note;
    if (!note || !note->instance_count) return;
    float x0 = cur->visual_anchor_col;
    float x1 = cur->instance[0].pos[0];
    float y0 = cur->visual_anchor_row;
    float y1 = cur->instance[0].pos[1];
    if (x1 < x0) { float t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { float t = y0; y0 = y1; y1 = t; }
    // count notes in selection
    uint32_t count = 0;
    for (uint32_t i = 0; i < note->instance_count; i++) {
        float nx0 = note->instance[i].pos[0];
        float nx1 = nx0 + note->instance[i].size[0];
        float ny = note->instance[i].pos[1];
        if (nx0 < x1 && nx1 > x0 && ny >= y0 && ny <= y1) {
            uint32_t _yl = (note->instance[i].flags >> 4) & 0xF;
            if (_yl >= 1 && _yl <= 9 && !(env->layer_visible & (1 << (_yl - 1)))) continue;
            count++;
        }
    }
    if (count == 0) return;
    // allocate yank buffer
    if (count > env->yank_capacity) {
        uint32_t new_cap = env->yank_capacity ? env->yank_capacity * 2 : 64;
        while (new_cap < count) new_cap *= 2;
        struct war_vulkan_note_instance* tmp = realloc(env->yank_buffer, new_cap * sizeof(struct war_vulkan_note_instance));
        if (!tmp) return;
        env->yank_buffer = tmp;
        env->yank_capacity = new_cap;
    }
    uint32_t j = 0;
    for (uint32_t i = 0; i < note->instance_count; i++) {
        float nx0 = note->instance[i].pos[0];
        float nx1 = nx0 + note->instance[i].size[0];
        float ny = note->instance[i].pos[1];
        if (nx0 < x1 && nx1 > x0 && ny >= y0 && ny <= y1) {
            uint32_t _yl2 = (note->instance[i].flags >> 4) & 0xF;
            if (_yl2 >= 1 && _yl2 <= 9 && !(env->layer_visible & (1 << (_yl2 - 1)))) continue;
            env->yank_buffer[j] = note->instance[i];
            env->yank_buffer[j].pos[0] -= cur->visual_anchor_col;
            env->yank_buffer[j].pos[1] -= cur->visual_anchor_row;
            j++;
        }
    }
    env->yank_count = count;
    env->yank_anchor_col = cur->visual_anchor_col;
    env->yank_anchor_row = cur->visual_anchor_row;
    call_king_terry("YANK: %u notes", count);
}

static inline void war_paste(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    war_note_context* note = env->ctx_note;
    if (!note || env->yank_count == 0 || !env->yank_buffer) return;
    if (note->instance_count >= note->max_instances ||
        note->instance_count + env->yank_count > note->max_instances) {
        snprintf(env->status_msg, sizeof(env->status_msg), "paste: note limit (%u + %u > %u)",
                 note->instance_count, env->yank_count, note->max_instances);
        return;
    }
    war_undo_save(env);
    float cx = cur->instance[0].pos[0];
    float cy = cur->instance[0].pos[1];
    for (uint32_t i = 0; i < env->yank_count; i++) {
        uint32_t dst = note->instance_count++;
        note->instance[dst] = env->yank_buffer[i];
        note->instance[dst].pos[0] += cx;
        note->instance[dst].pos[1] += cy;
        if (note->instance[dst].pos[0] < (float)env->ctx_wayland->gutter_cols)
            note->instance[dst].pos[0] = (float)env->ctx_wayland->gutter_cols;
        if (note->instance[dst].pos[1] < (float)env->ctx_wayland->gutter_rows)
            note->instance[dst].pos[1] = (float)env->ctx_wayland->gutter_rows;
        note->instance[dst].tick = note->tick_counter++;
    }
    call_king_terry("PASTE: %u notes at (%.1f,%.1f)", env->yank_count, cx, cy);
}

#endif // WAR_KEYMAP_FUNCTIONS_H
