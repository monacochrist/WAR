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
    double vis_cols = (double)wl->width / (cw * z) - wl->gutter_cols;
    double vis_rows = (double)wl->height / (ch * z) - wl->gutter_rows;
    if (vis_cols < 1) vis_cols = 1;
    if (vis_rows < 1) vis_rows = 1;
    double cx = cur->instance[0].pos[0];
    double cy = cur->instance[0].pos[1];
    float* p = wl->panning;
    if (cx < p[0]) p[0] = (float)cx;
    if (cx >= p[0] + vis_cols) p[0] = (float)(cx - vis_cols + 1);
    if (cy < p[1]) p[1] = (float)cy;
    if (cy >= p[1] + vis_rows) p[1] = (float)(cy - vis_rows + 1);
    if (p[0] < 0) p[0] = 0;
    if (p[1] < 0) p[1] = 0;
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
    double bound = env->ctx_wayland->panning[0] + env->ctx_wayland->gutter_cols;
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
    double bound = env->ctx_wayland->panning[1] + env->ctx_wayland->gutter_rows;
    if (cursor->instance_count && cursor->instance[0].pos[1] > bound)
        cursor->instance[0].pos[1] -= 1;
    war_pan_follow(env);
}

static inline void war_zoom_in(war_env* env) {
    env->ctx_wayland->zoom *= 1.25f;
    war_pan_follow(env);
}

static inline void war_zoom_out(war_env* env) {
    env->ctx_wayland->zoom *= 0.80f;
    war_pan_follow(env);
}

static inline void war_zoom_reset(war_env* env) {
    env->ctx_wayland->zoom = 1.0f;
}

#endif // WAR_KEYMAP_FUNCTIONS_H
