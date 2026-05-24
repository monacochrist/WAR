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

static inline void war_move_cursor_right(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    if (cursor->instance_count) cursor->instance[0].pos[0] += 1;
}

static inline void war_move_cursor_left(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    if (cursor->instance_count) cursor->instance[0].pos[0] -= 1;
}

static inline void war_move_cursor_up(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    if (cursor->instance_count) cursor->instance[0].pos[1] += 1;
}

static inline void war_move_cursor_down(war_env* env) {
    war_cursor_context* cursor = env->ctx_cursor;
    if (cursor->instance_count) cursor->instance[0].pos[1] -= 1;
}

static inline void war_zoom_in(war_env* env) {
    env->ctx_wayland->zoom *= 1.25f;
}

static inline void war_zoom_out(war_env* env) {
    env->ctx_wayland->zoom *= 0.80f;
}

static inline void war_zoom_reset(war_env* env) {
    env->ctx_wayland->zoom = 1.0f;
}

#endif // WAR_KEYMAP_FUNCTIONS_H
