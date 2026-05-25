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
    if (cx < p[0]) p[0] = (float)(cx - wl->gutter_cols - 0.5);
    if (cx >= p[0] + vis_cols) p[0] = (float)(cx - vis_cols + 1);
    if (cy < p[1]) p[1] = (float)(cy - wl->gutter_rows - 0.5);
    if (cy >= p[1] + vis_rows) p[1] = (float)(cy - vis_rows + 1);
    if (p[0] < 0) p[0] = 0;
    if (p[1] < 0) p[1] = 0;
}

static inline void war_layer_1(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_1;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
}

static inline void war_layer_2(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_2;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
}

static inline void war_layer_3(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_3;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
}

static inline void war_layer_4(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_4;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
}

static inline void war_layer_5(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_5;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
}

static inline void war_layer_6(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_6;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
}

static inline void war_layer_7(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_7;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
}

static inline void war_layer_8(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_8;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
}

static inline void war_layer_9(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_9;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
}

static inline void war_layer_0(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    uint32_t c = env->ctx_color->layer_none;
    float rgba[4] = {((c>>24)&0xFF)/255.0f, ((c>>16)&0xFF)/255.0f, ((c>>8)&0xFF)/255.0f, (c&0xFF)/255.0f};
    for (uint32_t i = 0; i < ctx_cursor->instance_count; i++)
        memcpy(ctx_cursor->instance[i].color, rgba, sizeof(rgba));
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

#endif // WAR_KEYMAP_FUNCTIONS_H
