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

static inline void war_goto_left_visible_bound(war_env* env) {
    war_wayland_context* wl = env->ctx_wayland;
    war_cursor_context* cur = env->ctx_cursor;
    double col = wl->panning[0];
    if (col < wl->gutter_cols) col = wl->gutter_cols;
    cur->instance->pos[0] = (float)(uint32_t)(col + 0.5);
    war_pan_follow(env);
}

static inline void war_goto_row_127(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    cur->instance[0].pos[1] = 127;
    war_pan_follow(env);
}

static inline void war_goto_row_60(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    cur->instance[0].pos[1] = 60;
    war_pan_follow(env);
}

static inline void war_goto_row_0(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_wayland_context* wl = env->ctx_wayland;
    cur->instance[0].pos[1] = wl->gutter_rows;
    war_pan_follow(env);
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

static inline void war_play_q(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    int32_t base = war_octave_to_midi_base((int32_t)env->ctx_cursor->octave);
    uint32_t note = (uint32_t)(0 + base);
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_w(war_env* env) {
    uint32_t note = 1 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_e(war_env* env) {
    uint32_t note = 2 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_r(war_env* env) {
    uint32_t note = 3 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_t(war_env* env) {
    uint32_t note = 4 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_y(war_env* env) {
    uint32_t note = 5 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_u(war_env* env) {
    uint32_t note = 6 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_i(war_env* env) {
    uint32_t note = 7 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_o(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    uint32_t note = 8 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_p(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    uint32_t note = 9 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_left_bracket(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    uint32_t note = 10 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_play_right_bracket(war_env* env) {
    if (env->ctx_cursor->octave == 10) { return; }
    uint32_t note = 11 + (env->ctx_cursor->octave + 1) * 12;
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_move_cursor_right(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    double bound = env->ctx_wayland->right_bound;
    if (cursor->instance_count && cursor->instance[0].pos[0] < bound)
        cursor->instance[0].pos[0] += 1;
    war_pan_follow(env);
}

static inline void war_move_cursor_left(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    double bound = env->ctx_wayland->gutter_cols + 0.5;
    if (cursor->instance_count && cursor->instance[0].pos[0] > bound)
        cursor->instance[0].pos[0] -= 1;
    war_pan_follow(env);
}

static inline void war_move_cursor_up(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    double bound = env->ctx_wayland->top_bound;
    if (cursor->instance_count && cursor->instance[0].pos[1] < bound)
        cursor->instance[0].pos[1] += 1;
    war_pan_follow(env);
}

static inline void war_move_cursor_down(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    double bound = env->ctx_wayland->gutter_rows + 0.5;
    if (cursor->instance_count && cursor->instance[0].pos[1] > bound)
        cursor->instance[0].pos[1] -= 1;
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
    double bottom = wl->panning[1];
    if (bottom < wl->gutter_rows) bottom = wl->gutter_rows;
    if (bottom > 127) bottom = 127;
    cur->instance[0].pos[1] = (uint32_t)(bottom + 0.5);
}

static inline void war_goto_viewport_top(war_env* env) {
    war_cursor_context* cur = env->ctx_cursor;
    war_wayland_context* wl = env->ctx_wayland;
    double total_vis_rows = (double)wl->height / (cur->cell_height * wl->zoom);
    double top = wl->panning[1] + total_vis_rows - 1;
    if (top < 0) top = 0;
    if (top > 127) top = 127;
    cur->instance[0].pos[1] = (uint32_t)(top + 0.5);
}

static inline void war_move_cursor_left_leap(war_env* env) {
    for (int i = 0; i < 13; i++) war_move_cursor_left(env);
}

static inline void war_move_cursor_right_leap(war_env* env) {
    for (int i = 0; i < 13; i++) war_move_cursor_right(env);
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

static inline void war_toggle_playback(war_env* env) {
    war_simple_line_context* line = env->ctx_line;
    if (!line) return;
    float gc = (float)env->ctx_wayland->gutter_cols;
    if (env->play_bar_playing) {
        env->play_bar_playing = 0;
        env->play_bar_position_seconds = 0.0;
        env->play_bar_preview_active = 0;
        line->instance[1].pos[0] = gc;
    } else {
        env->play_bar_playing = 1;
        env->play_bar_position_seconds = 0.0;
        env->play_bar_last_frame_ms = 0;
        env->play_bar_prev_cell_pos = (double)gc;
        env->play_bar_preview_active = 0;
        line->instance[1].pos[0] = gc;
    }
}

static inline void war_capture_audio(war_env* env) {
    if (env->atomics->capture) {
        // second press: stop capture and save to current note/layer
        uint32_t layer = env->ctx_cursor->layer;
        if (layer >= 1 && layer <= 9) {
            uint32_t note = (uint32_t)env->ctx_cursor->instance[0].pos[1];
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
        }
        env->atomics->capture = 0;
        call_king_terry("CAPTURE: saved to note=%u layer=%u",
                        (uint32_t)env->ctx_cursor->instance[0].pos[1],
                        env->ctx_cursor->layer);
    } else {
        // clear existing slot data at current note/layer
        uint32_t layer = env->ctx_cursor->layer;
        if (layer >= 1 && layer <= 9) {
            uint32_t note = (uint32_t)env->ctx_cursor->instance[0].pos[1];
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
                        (uint32_t)env->ctx_cursor->instance[0].pos[1],
                        env->ctx_cursor->layer);
    }
}

static inline void war_preview_toggle(war_env* env) {
    uint32_t note = (uint32_t)env->ctx_cursor->instance[0].pos[1];
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) return;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    if (!env->capture_slots[idx].samples || !env->capture_slots[idx].count)
        return;
    env->preview_note = note;
    env->preview_layer = layer;
    // flush old data in play ring buffer so new preview writes don't get stuck
    env->pc_play->i_from_wr = env->pc_play->i_to_a;
    env->preview_read_pos = 0;
    env->preview_active = 1;
    call_king_terry("PREVIEW: start note=%u layer=%u count=%u",
                    note,
                    layer,
                    env->capture_slots[idx].count);
}

static inline void war_set_width_to_duration(war_env* env) {
    uint32_t note = (uint32_t)env->ctx_cursor->instance[0].pos[1];
    if (note > 127) note = 127;
    uint32_t layer = env->ctx_cursor->layer;
    if (layer < 1 || layer > 9) layer = 1;
    uint32_t idx = note * WAR_CAPTURE_SLOT_LAYERS + (layer - 1);
    float new_w;
    if (env->capture_slots[idx].samples && env->capture_slots[idx].count > 0) {
        double duration = (double)env->capture_slots[idx].count / 48000.0;
        if (duration < 1.0) duration = 1.0;
        new_w = (float)duration;
        call_king_terry(
            "WIDTH: note=%u layer=%u count=%lu duration=%.3f size=%.2f",
            note,
            layer,
            env->capture_slots[idx].count,
            duration,
            new_w);
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
