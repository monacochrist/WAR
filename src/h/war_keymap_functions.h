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

static inline void _war_record_place_note(war_env* env, uint32_t note, int voice) {
    war_note_context* note_ctx = env->ctx_note;
    if (!note_ctx || voice < 0) return;
    if (note_ctx->instance_count >= note_ctx->max_instances) return;
    uint32_t i = note_ctx->instance_count++;
    double visual_row = (double)note + (double)env->ctx_wayland->gutter_rows;
    uint32_t col = (&env->ctx_color->layer_none)[env->ctx_cursor->layer];
    note_ctx->instance[i].pos[0] = (float)env->recording_position;
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
    note_ctx->instance[i].flags = 0;
    note_ctx->instance[i].tick = note_ctx->tick_counter++;
    env->recording_start_col[voice] = env->recording_position;
    env->recording_note_idx[voice] = i;
    env->recording_press_time_us[voice] = war_get_monotonic_time_us();
    call_king_terry("RECORD: placed note=%u layer=%u col=%.2f voice=%u idx=%u t=%lu",
                    note, env->ctx_cursor->layer, env->recording_position, voice, i,
                    (unsigned long)env->recording_press_time_us[voice]);
}

static inline void war_record_midi(war_env* env) {
    env->recording_active = !env->recording_active;
    if (env->recording_active) {
        env->recording_position = env->ctx_cursor->instance[0].pos[0];
        env->recording_last_frame_ms = 0;
        for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++)
            env->recording_start_col[v] = 0.0;
    }
}

static inline int _war_preview_start_voice(war_env* env, uint32_t note, uint32_t layer) {
    for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
        if (env->preview_voice_note[v] == note) {
            // restart voice for this note (handles re-press during follow-through)
            env->preview_voice_note[v] = note;
            env->preview_voice_layer[v] = layer;
            env->preview_voice_read_pos[v] = 0;
            env->preview_voice_active[v] = 1;
            if (env->recording_active) _war_record_place_note(env, note, (int)v);
            return (int)v;
        }
        if (!env->preview_voice_active[v]) {
            env->preview_voice_note[v] = note;
            env->preview_voice_layer[v] = layer;
            env->preview_voice_read_pos[v] = 0;
            env->preview_voice_active[v] = 1;
            if (env->recording_active) _war_record_place_note(env, note, (int)v);
            return (int)v;
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
    env->ctx_cursor->step = 0.0;
    env->ctx_cursor->x_width[0] = 1.0;
    env->ctx_cursor->instance[0].size[0] = 1.0f;
}

static inline void war_midi_mode(war_env* env) {
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
    double bound = env->ctx_wayland->gutter_rows + 0.5;
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
    if (bottom < wl->gutter_rows) bottom = wl->gutter_rows;
    if (bottom > wl->top_bound) bottom = wl->top_bound;
    cur->instance[0].pos[1] = (uint32_t)(bottom + 0.5);
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
        war_pan_follow(env);
    } else {
        double row = (double)cur->prefix;
        if (row < 0) row = 0;
        if (row > wl->top_bound) row = wl->top_bound;
        cur->instance[0].pos[1] = (float)row;
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
        if (note->instance[i].pos[0] > cx && note->instance[i].pos[0] < best)
            best = note->instance[i].pos[0];
    }
    if (best < 1e9f) {
        cur->instance[0].pos[0] = best;
        war_pan_follow(env);
    }
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
        if (note->instance[i].pos[0] < cx && note->instance[i].pos[0] > best)
            best = note->instance[i].pos[0];
    }
    if (best > -1e9f) {
        cur->instance[0].pos[0] = best;
        war_pan_follow(env);
    }
}

static inline void war_zoom_in(war_env* env) {
    float z = env->ctx_wayland->zoom * 1.25f;
    if (z > 100.0f) z = 100.0f;
    env->ctx_wayland->zoom = z;
}

static inline void war_zoom_out(war_env* env) {
    float z = env->ctx_wayland->zoom * 0.80f;
    if (z < 0.01f) z = 0.01f;
    env->ctx_wayland->zoom = z;
}

static inline void war_zoom_reset(war_env* env) {
    env->ctx_wayland->zoom = env->ctx_wayland->initial_zoom;
}

static inline void war_visual_mode(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    cur->visual_active = !cur->visual_active;
    if (cur->visual_active) {
        env->active_mode = WAR_MODE_ID_VISUAL;
        cur->visual_anchor_col = cur->instance[0].pos[0];
        cur->visual_anchor_row = cur->instance[0].pos[1];
        call_king_terry("VISUAL: ON at col=%.1f row=%.1f",
                        cur->visual_anchor_col, cur->visual_anchor_row);
    } else {
        env->active_mode = WAR_MODE_ID_ROLL;
        call_king_terry("VISUAL: OFF");
    }
}

static inline void _war_visual_move_selection(war_env* env, float dx, float dy) {
    war_cursor_context* cur = env->ctx_cursor;
    war_note_context* note = env->ctx_note;
    if (!cur->visual_active || !note || !note->instance_count) return;
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
        if (nx0 <= x1 && nx1 >= x0 && ny >= y0 && ny <= y1) {
            note->instance[i].pos[0] += dx;
            note->instance[i].pos[1] += dy;
            moved++;
        }
    }
    cur->instance[0].pos[0] += dx;
    cur->instance[0].pos[1] += dy;
    cur->visual_anchor_col += dx;
    cur->visual_anchor_row += dy;
    call_king_terry("VISUAL: moved %u notes (%.0f,%.0f)", moved, dx, dy);
}

static inline void war_visual_move_right(war_env* env) { _war_visual_move_selection(env, 1.0f, 0.0f); }
static inline void war_visual_move_left(war_env* env)  { _war_visual_move_selection(env, -1.0f, 0.0f); }
static inline void war_visual_move_up(war_env* env)    { _war_visual_move_selection(env, 0.0f, 1.0f); }
static inline void war_visual_move_down(war_env* env)  { _war_visual_move_selection(env, 0.0f, -1.0f); }

static inline void war_toggle_playback(war_env* env) {
    war_simple_line_context* line = env->ctx_line;
    if (!line) return;
    if (env->play_bar_playing) {
        env->play_bar_playing = 0;
        memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
    } else {
        env->play_bar_playing = 1;
        env->play_bar_last_frame_ms = 0;
        double bpm = env->atomics->bpm;
        if (bpm <= 0.0) bpm = 100.0;
        double sec_per_cell = 60.0 / bpm;
        double gc = (double)env->ctx_wayland->gutter_cols;
        env->play_bar_prev_cell_pos = gc + env->play_bar_position_seconds / sec_per_cell;
        memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
    }
}

static inline void war_playbar_goto_cursor(war_env* env) {
    war_simple_line_context* line = env->ctx_line;
    if (!line) return;
    float cursor_col = env->ctx_cursor->instance[0].pos[0];
    double bpm = env->atomics->bpm;
    if (bpm <= 0.0) bpm = 100.0;
    double sec_per_cell = 60.0 / bpm;
    double gc = (double)env->ctx_wayland->gutter_cols;
    env->play_bar_position_seconds = ((double)cursor_col - gc) * sec_per_cell;
    if (env->play_bar_position_seconds < 0.0) env->play_bar_position_seconds = 0.0;
    env->play_bar_last_frame_ms = 0;
    env->play_bar_prev_cell_pos = (double)cursor_col;
    memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
    line->instance[0].pos[0] = cursor_col;
}

static inline void war_playbar_goto_start(war_env* env) {
    war_simple_line_context* line = env->ctx_line;
    if (!line) return;
    float gc = (float)env->ctx_wayland->gutter_cols;
    env->play_bar_position_seconds = 0.0;
    env->play_bar_last_frame_ms = 0;
    env->play_bar_prev_cell_pos = (double)gc;
    memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
    line->instance[0].pos[0] = gc;
}

static inline void war_toggle_loop(war_env* env) {
    env->loop_mode = !env->loop_mode;
    call_king_terry("LOOP: %s", env->loop_mode ? "ON" : "OFF");
}

static inline void war_capture_audio(war_env* env) {
    if (env->atomics->capture) {
        // second press: stop capture and save to current note/layer
        uint32_t layer = env->ctx_cursor->layer;
        if (layer >= 1 && layer <= 9) {
            uint32_t note = (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
            if (note > 127) note = 127;
            // trim trailing silence
            if (env->capture_accumulator_count >= 2) {
                uint64_t i = env->capture_accumulator_count - 2;
                while (i >= 2) {
                    float l = env->capture_accumulator[i];
                    float r = env->capture_accumulator[i + 1];
                    if (fabsf(l) >= 0.001f || fabsf(r) >= 0.001f)
                        break;
                    i -= 2;
                }
                i += 2; // last frame with signal
                uint64_t tail = 48000; // ~1s at 48kHz — preserves natural decay
                uint64_t keep = i + (tail > (env->capture_accumulator_count - i) ?
                                      env->capture_accumulator_count - i : tail);
                if (keep < env->capture_accumulator_count) {
                    call_king_terry("TRIM: %llu -> %llu floats",
                                    (unsigned long long)env->capture_accumulator_count,
                                    (unsigned long long)keep);
                    env->capture_accumulator_count = keep;
                }
            }
            uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
            free(env->capture_slots[idx].samples);
            env->capture_slots[idx].samples = env->capture_accumulator;
            env->capture_slots[idx].count = env->capture_accumulator_count;
            env->capture_slots[idx].capacity =
                env->capture_accumulator_capacity;
            env->capture_accumulator = NULL;
            env->capture_accumulator_count = 0;
            env->capture_accumulator_capacity = 0;
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
        // flush stale data from previous capture in the loopback ring buffer
        env->pc_loopback->i_from_a = env->pc_loopback->i_to_wr;
        free(env->capture_accumulator);
        env->capture_accumulator = NULL;
        env->capture_accumulator_count = 0;
        env->capture_accumulator_capacity = 0;
        call_king_terry("CAPTURE: ON at note=%u layer=%u",
                        (uint32_t)(env->ctx_cursor->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows),
                        env->ctx_cursor->layer);
    }
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
        double sec_per_cell = 60.0 / bpm;
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

static inline void war_place_note(war_env* env) {
    war_note_context* note = env->ctx_note;
    if (!note) return;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    if (note->instance_count >= note->max_instances) return;
    uint32_t i = note->instance_count++;
    note->instance[i].pos[0] = cur->instance[0].pos[0];
    note->instance[i].pos[1] = cur->instance[0].pos[1];
    note->instance[i].pos[2] = cur->instance[0].pos[2];
    note->instance[i].size[0] = cur->instance[0].size[0];
    note->instance[i].size[1] = cur->instance[0].size[1];
    note->instance[i].color[0] = cur->instance[0].color[0];
    note->instance[i].color[1] = cur->instance[0].color[1];
    note->instance[i].color[2] = cur->instance[0].color[2];
    note->instance[i].color[3] = cur->instance[0].color[3];
    note->instance[i].outline_color[0] = 0.0f;
    note->instance[i].outline_color[1] = 0.0f;
    note->instance[i].outline_color[2] = 0.0f;
    note->instance[i].outline_color[3] = 1.0f;
    note->instance[i].flags = 0;
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

static inline void war_delete_note_under_cursor(war_env* env) {
    war_note_context* note = env->ctx_note;
    if (!note || !note->instance_count) return;
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur->instance_count) return;
    float cx0 = cur->instance[0].pos[0];
    float cx1 = cx0 + cur->instance[0].size[0];
    float cy0 = cur->instance[0].pos[1];
    float cy1 = cy0 + cur->instance[0].size[1];
    // find candidates whose rect overlaps cursor rect
    uint32_t best = UINT32_MAX;
    uint64_t best_tick = 0;
    for (uint32_t i = 0; i < note->instance_count; i++) {
        float nx0 = note->instance[i].pos[0];
        float nx1 = nx0 + note->instance[i].size[0];
        float ny0 = note->instance[i].pos[1];
        float ny1 = ny0 + note->instance[i].size[1];
        if (cx0 < nx1 && cx1 > nx0 && cy0 < ny1 && cy1 > ny0) {
            if (best == UINT32_MAX || note->instance[i].tick > best_tick) {
                best = i;
                best_tick = note->instance[i].tick;
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

#endif // WAR_KEYMAP_FUNCTIONS_H
