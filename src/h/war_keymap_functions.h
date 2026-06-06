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
        // reset playback bar to start and begin playing
        float _gc = (float)env->ctx_wayland->gutter_cols;
        env->play_bar_position_seconds = 0.0;
        env->play_bar_last_frame_ms = 0;
        env->play_bar_last_us = 0;
        env->play_bar_prev_cell_pos = (double)_gc;
        memset(env->play_bar_voice_active, 0, sizeof(env->play_bar_voice_active));
        if (env->ctx_line)
            env->ctx_line->instance[0].pos[0] = _gc;
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
    // first try to find a completely free slot
    for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
        if (!env->preview_voice_active[v]) {
            uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
            war_capture_slot* slot = &env->capture_slots[idx];
            if (!slot->samples || slot->count < 2) return -1;
            uint32_t voice = v;
            env->preview_voice_note[voice] = note;
            env->preview_voice_layer[voice] = layer;
            env->preview_voice_read_pos[voice] = 0;
            env->preview_voice_read_limit[voice] = 0;
            env->preview_voice_active[voice] = 1;
            if (env->recording_active) _war_record_place_note(env, note, (int)voice);
            return (int)voice;
        }
    }
    // no free slots — steal the slot for the same note if already playing (retrigger)
    for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
        if (env->preview_voice_note[v] == note) {
            uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
            war_capture_slot* slot = &env->capture_slots[idx];
            if (!slot->samples || slot->count < 2) return -1;
            uint32_t voice = v;
            env->preview_voice_note[voice] = note;
            env->preview_voice_layer[voice] = layer;
            env->preview_voice_read_pos[voice] = 0;
            env->preview_voice_read_limit[voice] = 0;
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
    env->ctx_cursor->step = 1.0;
    env->ctx_cursor->x_width[0] = 1.0;
    env->ctx_cursor->instance[0].size[0] = 1.0f;
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
        cur->instance[0].pos[1] = (float)(row + (double)wl->gutter_rows);
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
    cur->visual_anchor_col = tmp_col;
    cur->visual_anchor_row = tmp_row;
}

static inline void war_undo_save(war_env* env);

static inline void _war_visual_move_selection(war_env* env, float dx, float dy) {
    war_cursor_context* cur = env->ctx_cursor;
    war_note_context* note = env->ctx_note;
    if (!cur->visual_active || !note || !note->instance_count) return;
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
        if (nx0 <= x1 && nx1 >= x0 && ny >= y0 && ny <= y1) {
            uint32_t _vl = (note->instance[i].flags >> 4) & 0xF;
            if (_vl >= 1 && _vl <= 9 && !(env->layer_visible & (1 << (_vl - 1)))) continue;
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
                                env->play_bar_voice_read_pos[_v] = _offset;
                                env->play_bar_voice_read_limit[_v] = _limit;
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
                                env->play_bar_voice_read_pos[_v2] = _off2;
                                env->play_bar_voice_read_limit[_v2] = _lim2;
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
        note->instance[i].outline_color[3] = 1.0f;
        note->instance[i].flags = (uint32_t)layer << 4;
        note->instance[i].tick = note->tick_counter++;
    }
    call_king_terry("%s: root=%.0f width=%.1f", name, root_row - (double)env->ctx_wayland->gutter_rows, w);
}

static inline void war_chord_min9(war_env* env) {
    int intervals[] = {0, 3, 7, 10, 14};
    _war_chord_generic(env, intervals, 5, "MIN9");
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
    for (uint32_t i = env->undo_pos + 1; i < env->undo_count; i++) {
        free(env->undo_notes[i]);
        env->undo_notes[i] = NULL;
    }
    env->undo_count = env->undo_pos + 1;
    // shift if full
    if (env->undo_count > WAR_UNDO_MAX) {
        free(env->undo_notes[0]);
        for (uint32_t i = 1; i < env->undo_count; i++)
            env->undo_notes[i - 1] = env->undo_notes[i];
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
    env->undo_pos++;
    if (env->undo_pos > env->undo_count)
        env->undo_count = env->undo_pos;
}

static inline void war_undo(war_env* env) {
    war_note_context* note = env->ctx_note;
    if (!note || env->undo_pos == 0) return;
    // save current live state at undo_pos so redo can find it
    uint32_t save_idx = env->undo_pos;
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
    if (!note || env->undo_pos >= env->undo_count - 1) return;
    uint32_t restore_idx = env->undo_pos + 1;
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
            if (nx0 <= x1 && nx1 >= x0 && ny >= y0 && ny <= y1) {
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
        if (nx0 <= x1 && nx1 >= x0 && ny >= y0 && ny <= y1) {
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
        if (nx0 <= x1 && nx1 >= x0 && ny >= y0 && ny <= y1) {
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
    if (note->instance_count + env->yank_count > note->max_instances) return;
    war_undo_save(env);
    float cx = cur->instance[0].pos[0];
    float cy = cur->instance[0].pos[1];
    for (uint32_t i = 0; i < env->yank_count; i++) {
        uint32_t dst = note->instance_count++;
        note->instance[dst] = env->yank_buffer[i];
        note->instance[dst].pos[0] += cx;
        note->instance[dst].pos[1] += cy;
        note->instance[dst].tick = note->tick_counter++;
    }
    call_king_terry("PASTE: %u notes at (%.1f,%.1f)", env->yank_count, cx, cy);
}

#endif // WAR_KEYMAP_FUNCTIONS_H
