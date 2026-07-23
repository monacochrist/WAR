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
    // if same note is already playing, kill ALL instances of it immediately
    uint8_t had_note = 0;
    for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
        if (env->preview_voice_active[v] && env->preview_voice_note[v] == note) {
            env->preview_voice_active[v] = 0;
            had_note = 1;
        }
    }
    if (had_note && env->recording_active && env->ctx_note) {
        // place the new note for the retrigger
        uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
        war_capture_slot* slot = &env->capture_slots[idx];
        if (slot->samples && slot->count >= 2) {
            int free_v = -1;
            for (uint32_t v = 0; v < WAR_PREVIEW_VOICES; v++) {
                if (!env->preview_voice_active[v]) { free_v = (int)v; break; }
            }
            if (free_v >= 0) _war_record_place_note(env, note, free_v);
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

// effect system helpers
#define WAR_EFFECT_BIT(e)   (1ULL << ((e) - 1))
#define WAR_EFFECT_PARAM_IDX(e, p)  (((e) - 1) * WAR_EFFECT_PARAMS + (p))

static inline uint8_t _war_effect_active(war_capture_slot* slot, uint8_t type) {
    return (slot->effect_flags & WAR_EFFECT_BIT(type)) != 0;
}
static inline void _war_effect_set_active(war_capture_slot* slot, uint8_t type, uint8_t active) {
    if (active) slot->effect_flags |= WAR_EFFECT_BIT(type);
    else slot->effect_flags &= ~WAR_EFFECT_BIT(type);
}
static inline double _war_effect_get_param(war_capture_slot* slot, uint8_t type, uint8_t p) {
    return slot->effect_params[WAR_EFFECT_PARAM_IDX(type, p)];
}
static inline void _war_effect_set_param(war_capture_slot* slot, uint8_t type, uint8_t p, double v) {
    slot->effect_params[WAR_EFFECT_PARAM_IDX(type, p)] = v;
}

// real-time effect processing: modifies *l and *r in-place for a single sample pair
// state: per-voice 32-float array
static inline void _war_process_effects(war_capture_slot* slot, float* state, float* l, float* r) {
    // Compressor (state[0]=rms_l, [1]=rms_r, [2]=gr_state)
    if (_war_effect_active(slot, WAR_EFFECT_COMPRESS)) {
        double th = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 0);
        double rt = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 1);
        double at = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 2);
        double rt2 = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 3);
        double mk = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 4);
        if (rt < 1.0) rt = 4.0; if (at < 0.1) at = 1.0; if (rt2 < 0.1) rt2 = 40.0;
        double ac = exp(-1.0/(at*0.001*48000.0)), rc = exp(-1.0/(rt2*0.001*48000.0));
        double ml = pow(10.0, mk/20.0), knee = 6.0, sl = 1.0 - 1.0/rt;
        double il = *l, ir = *r;
        state[0] = (float)(ac * state[0] + (1.0-ac) * il*il);
        state[1] = (float)(ac * state[1] + (1.0-ac) * ir*ir);
        double env = sqrt(state[0] > state[1] ? state[0] : state[1]);
        double tg = 1.0;
        if (env > 1e-10) {
            double db = 20.0 * log10(env), x = db - th, gd = 0;
            if (x > knee*0.5) gd = -x*sl;
            else if (x > -knee*0.5) { double xk = x+knee*0.5; gd = -sl*xk*xk/(2.0*knee); }
            tg = pow(10.0, gd/20.0);
        }
        if (tg < state[2]) state[2] = (float)(ac * state[2] + (1.0-ac) * tg);
        else state[2] = (float)(rc * state[2] + (1.0-rc) * tg);
        *l = (float)(*l * state[2] * ml);
        *r = (float)(*r * state[2] * ml);
    }
    // Saturate (no state needed)
    if (_war_effect_active(slot, WAR_EFFECT_SATURATE)) {
        double dr = _war_effect_get_param(slot, WAR_EFFECT_SATURATE, 0);
        double mx = _war_effect_get_param(slot, WAR_EFFECT_SATURATE, 1);
        double mk = _war_effect_get_param(slot, WAR_EFFECT_SATURATE, 2);
        if (dr < 0.01) dr = 3.0;
        double ml = pow(10.0, mk/20.0);
        double sl = tanh((double)*l * dr) * ml, sr = tanh((double)*r * dr) * ml;
        *l = (float)(*l * (1.0-mx) + sl * mx);
        *r = (float)(*r * (1.0-mx) + sr * mx);
    }
    // Gate (state[3]=env_l, [4]=env_r, [5]=hold_counter)
    if (_war_effect_active(slot, WAR_EFFECT_GATE)) {
        double th = _war_effect_get_param(slot, WAR_EFFECT_GATE, 0);
        double at = _war_effect_get_param(slot, WAR_EFFECT_GATE, 1);
        double hd = _war_effect_get_param(slot, WAR_EFFECT_GATE, 2);
        double rt = _war_effect_get_param(slot, WAR_EFFECT_GATE, 3);
        double fl = _war_effect_get_param(slot, WAR_EFFECT_GATE, 4);
        if (at < 0.1) at = 2.0;
        double ac = exp(-1.0/(at*0.001*48000.0));
        double tl = pow(10.0, th/20.0), fl2 = pow(10.0, fl/20.0);
        double al = fabs(*l), ar = fabs(*r);
        state[3] = (float)(ac * state[3] + (1.0-ac) * al);
        state[4] = (float)(ac * state[4] + (1.0-ac) * ar);
        double env = state[3] > state[4] ? state[3] : state[4];
        double gg = 1.0;
        if (env > 1e-10) {
            double db = 20.0 * log10(env), x = db - th, gd = 0;
            double rr = 10.0, sl = 1.0 - 1.0/rr, knee = 3.0;
            if (x > knee*0.5) {}
            else if (x > -knee*0.5) { double xk = x+knee*0.5; gd = -sl*xk*xk/(2.0*knee); }
            else gd = -x*sl;
            gg = pow(10.0, gd/20.0);
            double fg = fl2 / env; if (gg < fg) gg = fg;
        }
        uint64_t hf = (uint64_t)(hd * 0.001 * 48000.0);
        uint32_t hc = (uint32_t)state[5];
        if (env >= tl) hc = (uint32_t)hf;
        else if (hc > 0) { hc--; gg = 1.0; }
        state[5] = (float)hc;
        *l *= (float)gg; *r *= (float)gg;
    }
    // De-esser (state[6]=s1_l, [7]=s1_r, [8]=s2_l, [9]=s2_r, [10]=rms, [11]=gr)
    if (_war_effect_active(slot, WAR_EFFECT_DEESSER)) {
        double th = _war_effect_get_param(slot, WAR_EFFECT_DEESSER, 0);
        double fq = _war_effect_get_param(slot, WAR_EFFECT_DEESSER, 1);
        double at = _war_effect_get_param(slot, WAR_EFFECT_DEESSER, 2);
        double rt = _war_effect_get_param(slot, WAR_EFFECT_DEESSER, 3);
        if (at < 0.1) at = 1.0; if (rt < 0.1) rt = 30.0;
        double ac = exp(-1.0/(at*0.001*48000.0)), rc = exp(-1.0/(rt*0.001*48000.0));
        float g = tanf((float)M_PI * (float)fq / 48000.0f);
        float R = 4.0f;
        float v0_l = ((float)*l - state[8]*R - state[6]) / (1.0f + g*(g+R));
        float v0_r = ((float)*r - state[9]*R - state[7]) / (1.0f + g*(g+R));
        state[6] = state[6] + g * v0_l;
        state[7] = state[7] + g * v0_r;
        state[8] = state[8] + g * state[6];
        state[9] = state[9] + g * state[7];
        float bp = fabs(state[8]) > fabs(state[9]) ? fabs(state[8]) : fabs(state[9]);
        state[10] = (float)(ac * state[10] + (1.0-ac) * (double)(bp*bp));
        double env = sqrt(state[10]);
        double tg = 1.0;
        if (env > 1e-10) {
            double db = 20.0 * log10(env), excess = db - th;
            if (excess > 0) { double gd = -excess * (1.0 - 1.0/10.0); tg = pow(10.0, gd/20.0); }
        }
        if (tg < state[11]) state[11] = (float)(ac * state[11] + (1.0-ac) * tg);
        else state[11] = (float)(rc * state[11] + (1.0-rc) * tg);
        *l *= state[11]; *r *= state[11];
    }
}

static inline void war_offall(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    env->capture_slots[idx].effect_flags = 0;
    snprintf(env->status_msg, sizeof(env->status_msg), "all effects OFF");
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
    int cmdlen = (int)env->cmd_len;
    uint8_t turn_on = 0, turn_off = 0, set_params = 0;
    double threshold_db = -20.0, ratio = 4.0, attack_ms = 1.0, release_ms = 40.0, makeup_db = 4.0;
    if (!env->cmd_active || cmdlen < 9) {
        // show state
        int a = _war_effect_active(slot, WAR_EFFECT_COMPRESS);
        if (a) {
            threshold_db = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 0);
            ratio = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 1);
            attack_ms = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 2);
            release_ms = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 3);
            makeup_db = _war_effect_get_param(slot, WAR_EFFECT_COMPRESS, 4);
        }
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "compress ON: thresh=%.0f ratio=%.1f attack=%.0f release=%.0f makeup=%.0f"
                   : "compress OFF",
                 threshold_db, ratio, attack_ms, release_ms, makeup_db);
        return;
    }
    const char* rest = env->cmd_buf + 9;
    while (*rest == ' ' || *rest == '\t') rest++;
    if (strcmp(rest, "on") == 0) turn_on = 1;
    else if (strcmp(rest, "off") == 0) turn_off = 1;
    else if (strcmp(rest, "usage") == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "usage: :compress [thresh(dB) ratio attack(ms) release(ms) makeup(dB)] | on | off | status"); return;
    }
    else if (strcmp(rest, "status") == 0) {
        int a = _war_effect_active(slot, WAR_EFFECT_COMPRESS);
        double t = a ? _war_effect_get_param(slot,WAR_EFFECT_COMPRESS,0) : threshold_db;
        double r = a ? _war_effect_get_param(slot,WAR_EFFECT_COMPRESS,1) : ratio;
        double at = a ? _war_effect_get_param(slot,WAR_EFFECT_COMPRESS,2) : attack_ms;
        double rt = a ? _war_effect_get_param(slot,WAR_EFFECT_COMPRESS,3) : release_ms;
        double m = a ? _war_effect_get_param(slot,WAR_EFFECT_COMPRESS,4) : makeup_db;
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "compress ON: thresh=%.0f ratio=%.1f attack=%.0f release=%.0f makeup=%.0f"
                   : "compress OFF", t, r, at, rt, m); return;
    }
    else if (strcmp(rest, "default") == 0) {
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 0, -20.0);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 1, 4.0);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 2, 1.0);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 3, 40.0);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 4, 4.0);
        _war_effect_set_active(slot, WAR_EFFECT_COMPRESS, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "compress ON (defaults)"); return;
    }
    else if (*rest) {
        set_params = 1;
        sscanf(rest, " %lf %lf %lf %lf %lf", &threshold_db, &ratio, &attack_ms, &release_ms, &makeup_db);
        if (threshold_db >= 0.0 || ratio < 1.0 || attack_ms < 0.0 || release_ms < 0.0) {
            snprintf(env->status_msg, sizeof(env->status_msg), "compress: bad args"); return;
        }
    }
    // handle on/off toggle
    uint8_t was_active = _war_effect_active(slot, WAR_EFFECT_COMPRESS);
    if (turn_off && was_active) {
        _war_effect_set_active(slot, WAR_EFFECT_COMPRESS, 0);
        snprintf(env->status_msg, sizeof(env->status_msg), "compress OFF"); return;
    }
    if (turn_on && !was_active) {
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 0, -20.0);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 1, 4.0);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 2, 1.0);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 3, 40.0);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 4, 4.0);
        _war_effect_set_active(slot, WAR_EFFECT_COMPRESS, 1);
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "compress ON (defaults)"); return;
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 0, threshold_db);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 1, ratio);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 2, attack_ms);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 3, release_ms);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 4, makeup_db);
        _war_effect_set_active(slot, WAR_EFFECT_COMPRESS, 1);
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "compress ON: thresh=%.0f ratio=%.1f attack=%.0f release=%.0f makeup=%.0f",
                 threshold_db, ratio, attack_ms, release_ms, makeup_db);
        return; // dead code but harmless
    }
    if (set_params) {
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 0, threshold_db);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 1, ratio);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 2, attack_ms);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 3, release_ms);
        _war_effect_set_param(slot, WAR_EFFECT_COMPRESS, 4, makeup_db);
        _war_effect_set_active(slot, WAR_EFFECT_COMPRESS, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "compress ON: thresh=%.0f ratio=%.1f attack=%.0f release=%.0f makeup=%.0f", threshold_db, ratio, attack_ms, release_ms, makeup_db);
    }
}

static inline void war_clear(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    free(slot->samples);
    slot->samples = NULL;
    slot->count = 0;
    slot->capacity = 0;
    slot->gain = 0.0f;
    slot->pan = 0;
    slot->eq = 0;
    slot->attack = 0.0f;
    slot->sustain = 0.0f;
    slot->release = 0.0f;
    slot->effect_flags = 0;
    memset(slot->effect_params, 0, sizeof(double) * WAR_EFFECT_COUNT * WAR_EFFECT_PARAMS);
    snprintf(env->status_msg, sizeof(env->status_msg), "cleared slot pitch=%u layer=%u", pitch, layer);
}

static inline void war_whatson(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    if (!cur || !cur->instance_count) return;
    uint32_t pitch = (uint32_t)(cur->instance[0].pos[1] - (double)env->ctx_wayland->gutter_rows);
    if (pitch > 127) return;
    uint32_t layer = cur->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = pitch * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    war_capture_slot* slot = &env->capture_slots[idx];
    static const char* names[] = {NULL,"CMP","SAT","REV","DEL","CHO","GAT","DEE","AUT"};
    char buf[128]; buf[0] = '\0'; int pos = 0;
    for (int i = 1; i < WAR_EFFECT_COUNT; i++) {
        if (_war_effect_active(slot, i)) {
            if (pos > 0 && pos < (int)sizeof(buf)-4) { buf[pos++] = ','; buf[pos++] = ' '; }
            if (pos < (int)sizeof(buf)-4) {
                int n = snprintf(buf+pos, sizeof(buf)-pos, "%s", names[i]);
                if (n > 0) pos += n;
            }
        }
    }
    if (pos == 0) snprintf(buf, sizeof(buf), "none");
    snprintf(env->status_msg, sizeof(env->status_msg), "active: %s", buf);
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
    int cmdlen = (int)env->cmd_len;
    uint8_t turn_on = 0, turn_off = 0, set_params = 0;
    double drive = 2.0, mix = 0.3, makeup_db = 0.0;
    if (!env->cmd_active || cmdlen < 9) {
        int a = _war_effect_active(slot, WAR_EFFECT_SATURATE);
        if (a) {
            drive = _war_effect_get_param(slot, WAR_EFFECT_SATURATE, 0);
            mix = _war_effect_get_param(slot, WAR_EFFECT_SATURATE, 1);
            makeup_db = _war_effect_get_param(slot, WAR_EFFECT_SATURATE, 2);
        }
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "saturate ON: drive=%.1f mix=%.2f makeup=%.0f" : "saturate OFF",
                 drive, mix, makeup_db);
        return;
    }
    const char* rest = env->cmd_buf + 9;
    while (*rest == ' ' || *rest == '\t') rest++;
    if (strcmp(rest, "on") == 0) turn_on = 1;
    else if (strcmp(rest, "off") == 0) turn_off = 1;
    else if (strcmp(rest, "usage") == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "usage: :saturate [drive mix makeup] | on | off | status"); return;
    }
    else if (strcmp(rest, "status") == 0) {
        int a = _war_effect_active(slot, WAR_EFFECT_SATURATE);
        double d = a ? _war_effect_get_param(slot,WAR_EFFECT_SATURATE,0) : drive;
        double m = a ? _war_effect_get_param(slot,WAR_EFFECT_SATURATE,1) : mix;
        double mk = a ? _war_effect_get_param(slot,WAR_EFFECT_SATURATE,2) : makeup_db;
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "saturate ON: drive=%.1f mix=%.2f makeup=%.0f"
                   : "saturate OFF", d, m, mk); return;
    }
    else if (strcmp(rest, "default") == 0) {
        _war_effect_set_param(slot, WAR_EFFECT_SATURATE, 0, 3.0);
        _war_effect_set_param(slot, WAR_EFFECT_SATURATE, 1, 0.4);
        _war_effect_set_param(slot, WAR_EFFECT_SATURATE, 2, 2.0);
        _war_effect_set_active(slot, WAR_EFFECT_SATURATE, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "saturate ON (defaults)"); return;
    }
    else if (*rest) {
        set_params = 1;
        sscanf(rest, "%lf %lf %lf", &drive, &mix, &makeup_db);
        if (drive < 0 || mix < 0 || mix > 1) {
            snprintf(env->status_msg, sizeof(env->status_msg), "saturate: bad args"); return;
        }
    }
    uint8_t was_active = _war_effect_active(slot, WAR_EFFECT_SATURATE);
    if (turn_on && !was_active) {
        _war_effect_set_param(slot, WAR_EFFECT_SATURATE, 0, 3.0);
        _war_effect_set_param(slot, WAR_EFFECT_SATURATE, 1, 0.4);
        _war_effect_set_param(slot, WAR_EFFECT_SATURATE, 2, 2.0);
        _war_effect_set_active(slot, WAR_EFFECT_SATURATE, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "saturate ON (defaults)"); return;
    }
    if (turn_off && was_active) { _war_effect_set_active(slot, WAR_EFFECT_SATURATE, 0); snprintf(env->status_msg, sizeof(env->status_msg), "saturate OFF"); return; }
    if (set_params) {
        _war_effect_set_param(slot, WAR_EFFECT_SATURATE, 0, drive);
        _war_effect_set_param(slot, WAR_EFFECT_SATURATE, 1, mix);
        _war_effect_set_param(slot, WAR_EFFECT_SATURATE, 2, makeup_db);
        _war_effect_set_active(slot, WAR_EFFECT_SATURATE, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "saturate ON: drive=%.1f mix=%.2f makeup=%.0f", drive, mix, makeup_db);
    }
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
    int cmdlen = (int)env->cmd_len;
    uint8_t turn_on = 0, turn_off = 0, set_params = 0;
    double decay = 0.4, mix = 0.15, predelay_ms = 0.0, damping = 0.3;
    if (!env->cmd_active || cmdlen < 7) {
        int a = _war_effect_active(slot, WAR_EFFECT_REVERB);
        if (a) {
            decay = _war_effect_get_param(slot, WAR_EFFECT_REVERB, 0);
            mix = _war_effect_get_param(slot, WAR_EFFECT_REVERB, 1);
            predelay_ms = _war_effect_get_param(slot, WAR_EFFECT_REVERB, 2);
            damping = _war_effect_get_param(slot, WAR_EFFECT_REVERB, 3);
        }
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "reverb ON: decay=%.2f mix=%.2f predelay=%.0fms damping=%.2f" : "reverb OFF",
                 decay, mix, predelay_ms, damping);
        return;
    }
    const char* rest = env->cmd_buf + 7;
    while (*rest == ' ' || *rest == '\t') rest++;
    if (strcmp(rest, "on") == 0) turn_on = 1;
    else if (strcmp(rest, "off") == 0) turn_off = 1;
    else if (strcmp(rest, "usage") == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "usage: :reverb [decay mix predelay damping] | on | off | status"); return;
    }
    else if (strcmp(rest, "status") == 0) {
        int a = _war_effect_active(slot, WAR_EFFECT_REVERB);
        double d = a ? _war_effect_get_param(slot,WAR_EFFECT_REVERB,0) : decay;
        double m = a ? _war_effect_get_param(slot,WAR_EFFECT_REVERB,1) : mix;
        double p = a ? _war_effect_get_param(slot,WAR_EFFECT_REVERB,2) : predelay_ms;
        double dm = a ? _war_effect_get_param(slot,WAR_EFFECT_REVERB,3) : damping;
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "reverb ON: decay=%.2f mix=%.2f predelay=%.0fms damping=%.2f"
                   : "reverb OFF", d, m, p, dm); return;
    }
    else if (strcmp(rest, "default") == 0) {
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 0, 0.4);
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 1, 0.15);
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 2, 0.0);
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 3, 0.3);
        _war_effect_set_active(slot, WAR_EFFECT_REVERB, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "reverb ON (defaults)"); return;
    }
    else if (*rest) {
        set_params = 1;
        sscanf(rest, "%lf %lf %lf %lf", &decay, &mix, &predelay_ms, &damping);
        if (decay < 0.0 || decay >= 1.0 || mix < 0.0 || mix > 1.0 || predelay_ms < 0.0 || damping < 0.0 || damping > 1.0) {
            snprintf(env->status_msg, sizeof(env->status_msg), "reverb: bad args"); return;
        }
    }
    uint8_t was_active = _war_effect_active(slot, WAR_EFFECT_REVERB);
    if (turn_on && !was_active) {
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 0, 0.4);
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 1, 0.15);
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 2, 0.0);
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 3, 0.3);
        _war_effect_set_active(slot, WAR_EFFECT_REVERB, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "reverb ON (defaults)"); return;
    }
    if (turn_off && was_active) { _war_effect_set_active(slot, WAR_EFFECT_REVERB, 0); snprintf(env->status_msg, sizeof(env->status_msg), "reverb OFF"); return; }
    if (set_params) {
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 0, decay);
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 1, mix);
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 2, predelay_ms);
        _war_effect_set_param(slot, WAR_EFFECT_REVERB, 3, damping);
        _war_effect_set_active(slot, WAR_EFFECT_REVERB, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "reverb ON: decay=%.2f mix=%.2f predelay=%.0f damping=%.2f", decay, mix, predelay_ms, damping);
    }
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
    int cmdlen = (int)env->cmd_len;
    uint8_t turn_on = 0, turn_off = 0, set_params = 0;
    double time_ms = 60.0, feedback = 0.2, mix = 0.4, fb_damping = 0.3;
    if (!env->cmd_active || cmdlen < 6) {
        int a = _war_effect_active(slot, WAR_EFFECT_DELAY);
        if (a) {
            time_ms = _war_effect_get_param(slot, WAR_EFFECT_DELAY, 0);
            feedback = _war_effect_get_param(slot, WAR_EFFECT_DELAY, 1);
            mix = _war_effect_get_param(slot, WAR_EFFECT_DELAY, 2);
            fb_damping = _war_effect_get_param(slot, WAR_EFFECT_DELAY, 3);
        }
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "delay ON: time=%.0fms feedback=%.2f mix=%.2f fb_damping=%.2f" : "delay OFF",
                 time_ms, feedback, mix, fb_damping);
        return;
    }
    const char* rest = env->cmd_buf + 6;
    while (*rest == ' ' || *rest == '\t') rest++;
    if (strcmp(rest, "on") == 0) turn_on = 1;
    else if (strcmp(rest, "off") == 0) turn_off = 1;
    else if (strcmp(rest, "usage") == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "usage: :delay [time feedback mix fb_damping] | on | off | status"); return;
    }
    else if (strcmp(rest, "status") == 0) {
        int a = _war_effect_active(slot, WAR_EFFECT_DELAY);
        double t = a ? _war_effect_get_param(slot,WAR_EFFECT_DELAY,0) : time_ms;
        double f = a ? _war_effect_get_param(slot,WAR_EFFECT_DELAY,1) : feedback;
        double m = a ? _war_effect_get_param(slot,WAR_EFFECT_DELAY,2) : mix;
        double fd = a ? _war_effect_get_param(slot,WAR_EFFECT_DELAY,3) : fb_damping;
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "delay ON: time=%.0fms feedback=%.2f mix=%.2f fb_damping=%.2f"
                   : "delay OFF", t, f, m, fd); return;
    }
    else if (strcmp(rest, "default") == 0) {
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 0, 60.0);
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 1, 0.2);
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 2, 0.4);
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 3, 0.3);
        _war_effect_set_active(slot, WAR_EFFECT_DELAY, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "delay ON (defaults)"); return;
    }
    else if (*rest) {
        set_params = 1;
        sscanf(rest, "%lf %lf %lf %lf", &time_ms, &feedback, &mix, &fb_damping);
        if (time_ms < 1.0 || feedback < 0.0 || feedback >= 1.0 || mix < 0.0 || mix > 1.0 || fb_damping < 0.0 || fb_damping > 1.0) {
            snprintf(env->status_msg, sizeof(env->status_msg), "delay: bad args"); return;
        }
    }
    uint8_t was_active = _war_effect_active(slot, WAR_EFFECT_DELAY);
    if (turn_on && !was_active) {
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 0, 60.0);
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 1, 0.2);
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 2, 0.4);
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 3, 0.3);
        _war_effect_set_active(slot, WAR_EFFECT_DELAY, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "delay ON (defaults)"); return;
    }
    if (turn_off && was_active) { _war_effect_set_active(slot, WAR_EFFECT_DELAY, 0); snprintf(env->status_msg, sizeof(env->status_msg), "delay OFF"); return; }
    if (set_params) {
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 0, time_ms);
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 1, feedback);
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 2, mix);
        _war_effect_set_param(slot, WAR_EFFECT_DELAY, 3, fb_damping);
        _war_effect_set_active(slot, WAR_EFFECT_DELAY, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "delay ON: time=%.0f feedback=%.2f mix=%.2f damp=%.2f", time_ms, feedback, mix, fb_damping);
    }
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
    int cmdlen = (int)env->cmd_len;
    uint8_t turn_on = 0, turn_off = 0, set_params = 0;
    double threshold_db = -40.0, attack_ms = 2.0, hold_ms = 10.0, release_ms = 50.0, floor_db = -80.0;
    if (!env->cmd_active || cmdlen < 5) {
        int a = _war_effect_active(slot, WAR_EFFECT_GATE);
        if (a) {
            threshold_db = _war_effect_get_param(slot, WAR_EFFECT_GATE, 0);
            attack_ms = _war_effect_get_param(slot, WAR_EFFECT_GATE, 1);
            hold_ms = _war_effect_get_param(slot, WAR_EFFECT_GATE, 2);
            release_ms = _war_effect_get_param(slot, WAR_EFFECT_GATE, 3);
            floor_db = _war_effect_get_param(slot, WAR_EFFECT_GATE, 4);
        }
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "gate ON: thresh=%.0fdB attack=%.0fms hold=%.0fms release=%.0fms floor=%.0fdB" : "gate OFF",
                 threshold_db, attack_ms, hold_ms, release_ms, floor_db);
        return;
    }
    const char* rest = env->cmd_buf + 5;
    while (*rest == ' ' || *rest == '\t') rest++;
    if (strcmp(rest, "on") == 0) turn_on = 1;
    else if (strcmp(rest, "off") == 0) turn_off = 1;
    else if (strcmp(rest, "usage") == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "usage: :gate [thresh attack hold release floor] | on | off | status"); return;
    }
    else if (strcmp(rest, "status") == 0) {
        int a = _war_effect_active(slot, WAR_EFFECT_GATE);
        double t = a ? _war_effect_get_param(slot,WAR_EFFECT_GATE,0) : threshold_db;
        double at = a ? _war_effect_get_param(slot,WAR_EFFECT_GATE,1) : attack_ms;
        double h = a ? _war_effect_get_param(slot,WAR_EFFECT_GATE,2) : hold_ms;
        double r = a ? _war_effect_get_param(slot,WAR_EFFECT_GATE,3) : release_ms;
        double f = a ? _war_effect_get_param(slot,WAR_EFFECT_GATE,4) : floor_db;
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "gate ON: thresh=%.0fdB attack=%.0fms hold=%.0fms release=%.0fms floor=%.0fdB"
                   : "gate OFF", t, at, h, r, f); return;
    }
    else if (strcmp(rest, "default") == 0) {
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 0, -40.0);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 1, 2.0);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 2, 10.0);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 3, 50.0);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 4, -80.0);
        _war_effect_set_active(slot, WAR_EFFECT_GATE, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "gate ON (defaults)"); return;
    }
    else if (*rest) {
        set_params = 1;
        sscanf(rest, "%lf %lf %lf %lf %lf", &threshold_db, &attack_ms, &hold_ms, &release_ms, &floor_db);
        if (threshold_db >= 0.0 || attack_ms < 0.0 || hold_ms < 0.0 || release_ms < 0.0 || floor_db >= 0.0) {
            snprintf(env->status_msg, sizeof(env->status_msg), "gate: bad args"); return;
        }
    }
    uint8_t was_active = _war_effect_active(slot, WAR_EFFECT_GATE);
    if (turn_on && !was_active) {
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 0, -40.0);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 1, 2.0);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 2, 10.0);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 3, 50.0);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 4, -80.0);
        _war_effect_set_active(slot, WAR_EFFECT_GATE, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "gate ON (defaults)"); return;
    }
    if (turn_off && was_active) { _war_effect_set_active(slot, WAR_EFFECT_GATE, 0); snprintf(env->status_msg, sizeof(env->status_msg), "gate OFF"); return; }
    if (set_params) {
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 0, threshold_db);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 1, attack_ms);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 2, hold_ms);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 3, release_ms);
        _war_effect_set_param(slot, WAR_EFFECT_GATE, 4, floor_db);
        _war_effect_set_active(slot, WAR_EFFECT_GATE, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "gate ON: thresh=%.0f attack=%.0f hold=%.0f release=%.0f floor=%.0f", threshold_db, attack_ms, hold_ms, release_ms, floor_db);
    }
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
    int cmdlen = (int)env->cmd_len;
    uint8_t turn_on = 0, turn_off = 0, set_params = 0;
    double threshold_db = -30.0, freq_hz = 6000.0, attack_ms = 1.0, release_ms = 30.0;
    if (!env->cmd_active || cmdlen < 8) {
        int a = _war_effect_active(slot, WAR_EFFECT_DEESSER);
        if (a) {
            threshold_db = _war_effect_get_param(slot, WAR_EFFECT_DEESSER, 0);
            freq_hz = _war_effect_get_param(slot, WAR_EFFECT_DEESSER, 1);
            attack_ms = _war_effect_get_param(slot, WAR_EFFECT_DEESSER, 2);
            release_ms = _war_effect_get_param(slot, WAR_EFFECT_DEESSER, 3);
        }
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "deesser ON: thresh=%.0fdB freq=%.0fHz attack=%.0fms release=%.0fms" : "deesser OFF",
                 threshold_db, freq_hz, attack_ms, release_ms);
        return;
    }
    const char* rest = env->cmd_buf + 8;
    while (*rest == ' ' || *rest == '\t') rest++;
    if (strcmp(rest, "on") == 0) turn_on = 1;
    else if (strcmp(rest, "off") == 0) turn_off = 1;
    else if (strcmp(rest, "usage") == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "usage: :deesser [thresh freq attack release] | on | off | status"); return;
    }
    else if (strcmp(rest, "status") == 0) {
        int a = _war_effect_active(slot, WAR_EFFECT_DEESSER);
        double t = a ? _war_effect_get_param(slot,WAR_EFFECT_DEESSER,0) : threshold_db;
        double f = a ? _war_effect_get_param(slot,WAR_EFFECT_DEESSER,1) : freq_hz;
        double at = a ? _war_effect_get_param(slot,WAR_EFFECT_DEESSER,2) : attack_ms;
        double r = a ? _war_effect_get_param(slot,WAR_EFFECT_DEESSER,3) : release_ms;
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "deesser ON: thresh=%.0fdB freq=%.0fHz attack=%.0fms release=%.0fms"
                   : "deesser OFF", t, f, at, r); return;
    }
    else if (strcmp(rest, "default") == 0) {
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 0, -30.0);
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 1, 6000.0);
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 2, 1.0);
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 3, 30.0);
        _war_effect_set_active(slot, WAR_EFFECT_DEESSER, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "deesser ON (defaults)"); return;
    }
    else if (*rest) {
        set_params = 1;
        sscanf(rest, "%lf %lf %lf %lf", &threshold_db, &freq_hz, &attack_ms, &release_ms);
        if (threshold_db >= 0.0 || freq_hz < 100.0 || freq_hz > 20000.0 || attack_ms < 0.0 || release_ms < 0.0) {
            snprintf(env->status_msg, sizeof(env->status_msg), "deesser: bad args"); return;
        }
    }
    uint8_t was_active = _war_effect_active(slot, WAR_EFFECT_DEESSER);
    if (turn_on && !was_active) {
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 0, -30.0);
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 1, 6000.0);
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 2, 1.0);
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 3, 30.0);
        _war_effect_set_active(slot, WAR_EFFECT_DEESSER, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "deesser ON (defaults)"); return;
    }
    if (turn_off && was_active) { _war_effect_set_active(slot, WAR_EFFECT_DEESSER, 0); snprintf(env->status_msg, sizeof(env->status_msg), "deesser OFF"); return; }
    if (set_params) {
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 0, threshold_db);
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 1, freq_hz);
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 2, attack_ms);
        _war_effect_set_param(slot, WAR_EFFECT_DEESSER, 3, release_ms);
        _war_effect_set_active(slot, WAR_EFFECT_DEESSER, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "deesser ON: thresh=%.0f freq=%.0f attack=%.0f release=%.0f", threshold_db, freq_hz, attack_ms, release_ms);
    }
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
    int cmdlen = (int)env->cmd_len;
    uint8_t turn_on = 0, turn_off = 0, set_params = 0;
    double rate_hz = 0.3, depth_ms = 8.0, mix = 0.3, base_ms = 25.0;
    if (!env->cmd_active || cmdlen < 7) {
        int a = _war_effect_active(slot, WAR_EFFECT_CHORUS);
        if (a) {
            rate_hz = _war_effect_get_param(slot, WAR_EFFECT_CHORUS, 0);
            depth_ms = _war_effect_get_param(slot, WAR_EFFECT_CHORUS, 1);
            mix = _war_effect_get_param(slot, WAR_EFFECT_CHORUS, 2);
            base_ms = _war_effect_get_param(slot, WAR_EFFECT_CHORUS, 3);
        }
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "chorus ON: rate=%.1fHz depth=%.1fms mix=%.2f base=%.0fms" : "chorus OFF",
                 rate_hz, depth_ms, mix, base_ms);
        return;
    }
    const char* rest = env->cmd_buf + 7;
    while (*rest == ' ' || *rest == '\t') rest++;
    if (strcmp(rest, "on") == 0) turn_on = 1;
    else if (strcmp(rest, "off") == 0) turn_off = 1;
    else if (strcmp(rest, "usage") == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "usage: :chorus [rate depth mix base] | on | off | status"); return;
    }
    else if (strcmp(rest, "status") == 0) {
        int a = _war_effect_active(slot, WAR_EFFECT_CHORUS);
        double r = a ? _war_effect_get_param(slot,WAR_EFFECT_CHORUS,0) : rate_hz;
        double d = a ? _war_effect_get_param(slot,WAR_EFFECT_CHORUS,1) : depth_ms;
        double m = a ? _war_effect_get_param(slot,WAR_EFFECT_CHORUS,2) : mix;
        double b = a ? _war_effect_get_param(slot,WAR_EFFECT_CHORUS,3) : base_ms;
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "chorus ON: rate=%.1fHz depth=%.1fms mix=%.2f base=%.0fms"
                   : "chorus OFF", r, d, m, b); return;
    }
    else if (strcmp(rest, "default") == 0) {
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 0, 0.3);
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 1, 8.0);
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 2, 0.3);
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 3, 25.0);
        _war_effect_set_active(slot, WAR_EFFECT_CHORUS, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "chorus ON (defaults)"); return;
    }
    else if (*rest) {
        set_params = 1;
        sscanf(rest, "%lf %lf %lf %lf", &rate_hz, &depth_ms, &mix, &base_ms);
        if (rate_hz <= 0.0 || depth_ms <= 0.0 || mix < 0.0 || mix > 1.0 || base_ms < 1.0) {
            snprintf(env->status_msg, sizeof(env->status_msg), "chorus: bad args"); return;
        }
    }
    uint8_t was_active = _war_effect_active(slot, WAR_EFFECT_CHORUS);
    if (turn_on && !was_active) {
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 0, 0.3);
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 1, 8.0);
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 2, 0.3);
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 3, 25.0);
        _war_effect_set_active(slot, WAR_EFFECT_CHORUS, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "chorus ON (defaults)"); return;
    }
    if (turn_off && was_active) { _war_effect_set_active(slot, WAR_EFFECT_CHORUS, 0); snprintf(env->status_msg, sizeof(env->status_msg), "chorus OFF"); return; }
    if (set_params) {
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 0, rate_hz);
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 1, depth_ms);
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 2, mix);
        _war_effect_set_param(slot, WAR_EFFECT_CHORUS, 3, base_ms);
        _war_effect_set_active(slot, WAR_EFFECT_CHORUS, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "chorus ON: rate=%.1f depth=%.1f mix=%.2f base=%.0f", rate_hz, depth_ms, mix, base_ms);
    }
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
    int cmdlen = (int)env->cmd_len;
    uint8_t turn_on = 0, turn_off = 0, set_params = 0;
    double retune_ms = 3.0;
    if (!env->cmd_active || cmdlen < 9) {
        int a = _war_effect_active(slot, WAR_EFFECT_AUTOTUNE);
        if (a) {
            retune_ms = _war_effect_get_param(slot, WAR_EFFECT_AUTOTUNE, 0);
        }
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "autotune ON: retune=%.0fms" : "autotune OFF",
                 retune_ms);
        return;
    }
    const char* rest = env->cmd_buf + 9;
    while (*rest == ' ' || *rest == '\t') rest++;
    if (strcmp(rest, "on") == 0) turn_on = 1;
    else if (strcmp(rest, "off") == 0) turn_off = 1;
    else if (strcmp(rest, "usage") == 0) {
        snprintf(env->status_msg, sizeof(env->status_msg),
                 "usage: :autotune [retune] | on | off | status"); return;
    }
    else if (strcmp(rest, "status") == 0) {
        int a = _war_effect_active(slot, WAR_EFFECT_AUTOTUNE);
        double r = a ? _war_effect_get_param(slot,WAR_EFFECT_AUTOTUNE,0) : retune_ms;
        snprintf(env->status_msg, sizeof(env->status_msg),
                 a ? "autotune ON: retune=%.0fms"
                   : "autotune OFF", r); return;
    }
    else if (strcmp(rest, "default") == 0) {
        _war_effect_set_param(slot, WAR_EFFECT_AUTOTUNE, 0, 3.0);
        _war_effect_set_active(slot, WAR_EFFECT_AUTOTUNE, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "autotune ON (defaults)"); return;
    }
    else if (*rest) {
        set_params = 1;
        sscanf(rest, "%lf", &retune_ms);
        if (retune_ms < 0.0) {
            snprintf(env->status_msg, sizeof(env->status_msg), "autotune: bad args"); return;
        }
    }
    uint8_t was_active = _war_effect_active(slot, WAR_EFFECT_AUTOTUNE);
    if (turn_on && !was_active) {
        _war_effect_set_param(slot, WAR_EFFECT_AUTOTUNE, 0, 3.0);
        _war_effect_set_active(slot, WAR_EFFECT_AUTOTUNE, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "autotune ON (defaults)"); return;
    }
    if (turn_off && was_active) { _war_effect_set_active(slot, WAR_EFFECT_AUTOTUNE, 0); snprintf(env->status_msg, sizeof(env->status_msg), "autotune OFF"); return; }
    if (set_params) {
        _war_effect_set_param(slot, WAR_EFFECT_AUTOTUNE, 0, retune_ms);
        _war_effect_set_active(slot, WAR_EFFECT_AUTOTUNE, 1);
        snprintf(env->status_msg, sizeof(env->status_msg), "autotune ON: retune=%.0fms", retune_ms);
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
            env->capture_slots[tidx].gain = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].gain;
            env->capture_slots[tidx].pan = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].pan;
            env->capture_slots[tidx].eq = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].eq;
            env->capture_slots[tidx].attack = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].attack;
            env->capture_slots[tidx].sustain = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].sustain;
            env->capture_slots[tidx].release = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].release;
            env->capture_slots[tidx].effect_flags = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].effect_flags;
            memcpy(env->capture_slots[tidx].effect_params, env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].effect_params, sizeof(double) * WAR_EFFECT_COUNT * WAR_EFFECT_PARAMS);
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
            env->capture_slots[tidx].effect_flags = env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].effect_flags;
            memcpy(env->capture_slots[tidx].effect_params, env->capture_slots[src_note * WAR_CAPTURE_SLOT_LAYERS + li].effect_params, sizeof(double) * WAR_EFFECT_COUNT * WAR_EFFECT_PARAMS);
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
    if (mi >= 128 * WAR_CAPTURE_SLOT_LAYERS) return;
    // verify dest slot is actually empty before using it
    if (env->capture_slots[mi].samples && env->capture_slots[mi].count >= 2) return;
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
            if (split_samples + right_samples <= src_slot->count)
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
            if (split_samples <= src_slot->count)
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
