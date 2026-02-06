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

#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_functions.h"
#include "h/war_new_vulkan.h"
#include "h/war_nsgt.h"

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
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    war_misc_context* ctx_misc = env->ctx_misc;
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        for (uint32_t k = 0; k < ctx_cursor->count[i]; k++) {
            ctx_cursor->x_seconds[i][k] +=
                ctx_misc->seconds_per_cell * ctx_cursor->move_factor;
            if (ctx_cursor->x_seconds[i][k] >=
                ctx_cursor->max_seconds_x - ctx_misc->epsilon) {
                ctx_cursor->x_seconds[i][k] =
                    ctx_cursor->max_seconds_x - ctx_misc->seconds_per_cell;
                ctx_cursor->stage[i][k].pos[0] =
                    (ctx_cursor->max_seconds_x - ctx_misc->seconds_per_cell) /
                    ctx_misc->seconds_per_cell;
                ctx_cursor->dirty[i] = 1;
                goto war_label_pan;
            }
            ctx_cursor->stage[i][k].pos[0] += 1 * ctx_cursor->move_factor;
            ctx_cursor->dirty[i] = 1;
        war_label_pan: {
            double offset_seconds =
                (ctx_cursor->push_constant->cell_offset[0] + 1) *
                ctx_misc->seconds_per_cell;
            if (ctx_cursor->x_seconds[i][k] < ctx_cursor->right_bound_seconds -
                                                  offset_seconds -
                                                  ctx_misc->epsilon) {
                continue;
            }
            double diff_seconds =
                ctx_cursor->x_seconds[i][k] -
                (ctx_cursor->right_bound_seconds - offset_seconds);
            ctx_cursor->push_constant->panning[0] -=
                diff_seconds / ctx_misc->seconds_per_cell;
            ctx_cursor->right_bound_seconds += diff_seconds;
            ctx_cursor->left_bound_seconds += diff_seconds;
        }
        }
    }
}

static inline void war_move_cursor_left(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    war_misc_context* ctx_misc = env->ctx_misc;
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        for (uint32_t k = 0; k < ctx_cursor->count[i]; k++) {
            ctx_cursor->x_seconds[i][k] -=
                ctx_misc->seconds_per_cell * ctx_cursor->move_factor;
            if (ctx_cursor->x_seconds[i][k] <= ctx_misc->epsilon) {
                ctx_cursor->x_seconds[i][k] = 0;
                ctx_cursor->stage[i][k].pos[0] = 0;
                ctx_cursor->dirty[i] = 1;
                goto war_label_pan;
            }
            ctx_cursor->stage[i][k].pos[0] -= 1 * ctx_cursor->move_factor;
            ctx_cursor->dirty[i] = 1;
        war_label_pan: {
            double offset_seconds = 0;
            if (ctx_cursor->x_seconds[i][k] >= ctx_cursor->left_bound_seconds +
                                                   offset_seconds -
                                                   ctx_misc->epsilon) {
                continue;
            }
            double diff_seconds = ctx_cursor->left_bound_seconds +
                                  offset_seconds - ctx_cursor->x_seconds[i][k];
            ctx_cursor->push_constant->panning[0] +=
                diff_seconds / ctx_misc->seconds_per_cell;
            ctx_cursor->right_bound_seconds -= diff_seconds;
            ctx_cursor->left_bound_seconds -= diff_seconds;
        }
        }
    }
}

static inline void war_move_cursor_up(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    war_misc_context* ctx_misc = env->ctx_misc;
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        for (uint32_t k = 0; k < ctx_cursor->count[i]; k++) {
            ctx_cursor->y_cells[i][k] += ctx_cursor->move_factor;
            if (ctx_cursor->y_cells[i][k] >=
                ctx_cursor->max_cells_y - ctx_misc->epsilon) {
                ctx_cursor->y_cells[i][k] = ctx_cursor->max_cells_y - 1;
                ctx_cursor->stage[i][k].pos[1] = ctx_cursor->max_cells_y - 1;
                ctx_cursor->dirty[i] = 1;
                goto war_label_pan;
            }
            ctx_cursor->stage[i][k].pos[1] += ctx_cursor->move_factor;
            ctx_cursor->dirty[i] = 1;
        war_label_pan: {
            double offset_cells = ctx_cursor->push_constant->cell_offset[1] + 1;
            if (ctx_cursor->y_cells[i][k] < ctx_cursor->top_bound_cells -
                                                offset_cells -
                                                ctx_misc->epsilon) {
                continue;
            }
            double diff_cells = ctx_cursor->y_cells[i][k] -
                                (ctx_cursor->top_bound_cells - offset_cells);
            ctx_cursor->push_constant->panning[1] -= diff_cells;
            ctx_cursor->top_bound_cells += diff_cells;
            ctx_cursor->bottom_bound_cells += diff_cells;
        }
        }
    }
}

static inline void war_move_cursor_down(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    war_misc_context* ctx_misc = env->ctx_misc;
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        for (uint32_t k = 0; k < ctx_cursor->count[i]; k++) {
            ctx_cursor->y_cells[i][k] -= ctx_cursor->move_factor;
            if (ctx_cursor->y_cells[i][k] <= ctx_misc->epsilon) {
                ctx_cursor->y_cells[i][k] = 0;
                ctx_cursor->stage[i][k].pos[1] = 0;
                ctx_cursor->dirty[i] = 1;
                goto war_label_pan;
            }
            ctx_cursor->stage[i][k].pos[1] -= ctx_cursor->move_factor;
            ctx_cursor->dirty[i] = 1;
        war_label_pan: {
            double offset_cells = 0;
            if (ctx_cursor->y_cells[i][k] >= ctx_cursor->bottom_bound_cells +
                                                 offset_cells +
                                                 ctx_misc->epsilon) {
                continue;
            }
            double diff_cells = ctx_cursor->bottom_bound_cells + offset_cells -
                                ctx_cursor->y_cells[i][k];
            ctx_cursor->push_constant->panning[1] += diff_cells;
            ctx_cursor->top_bound_cells -= diff_cells;
            ctx_cursor->bottom_bound_cells -= diff_cells;
        }
        }
    }
}

static inline void war_move_cursor_right_leap(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    war_misc_context* ctx_misc = env->ctx_misc;
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        for (uint32_t k = 0; k < ctx_cursor->count[i]; k++) {
            ctx_cursor->x_seconds[i][k] += ctx_misc->seconds_per_cell *
                                           ctx_cursor->move_factor *
                                           ctx_cursor->leap_cells;
            if (ctx_cursor->x_seconds[i][k] >=
                ctx_cursor->max_seconds_x - ctx_misc->epsilon) {
                ctx_cursor->x_seconds[i][k] =
                    ctx_cursor->max_seconds_x - ctx_misc->seconds_per_cell;
                ctx_cursor->stage[i][k].pos[0] =
                    (ctx_cursor->max_seconds_x - ctx_misc->seconds_per_cell) /
                    ctx_misc->seconds_per_cell;
                ctx_cursor->dirty[i] = 1;
                goto war_label_pan;
            }
            ctx_cursor->stage[i][k].pos[0] +=
                ctx_cursor->leap_cells * ctx_cursor->move_factor;
            ctx_cursor->dirty[i] = 1;
        war_label_pan: {
            double offset_seconds =
                (ctx_cursor->push_constant->cell_offset[0] + 1) *
                ctx_misc->seconds_per_cell;
            if (ctx_cursor->x_seconds[i][k] < ctx_cursor->right_bound_seconds -
                                                  offset_seconds -
                                                  ctx_misc->epsilon) {
                continue;
            }
            double diff_seconds =
                ctx_cursor->x_seconds[i][k] -
                (ctx_cursor->right_bound_seconds - offset_seconds);
            ctx_cursor->push_constant->panning[0] -=
                diff_seconds / ctx_misc->seconds_per_cell;
            ctx_cursor->right_bound_seconds += diff_seconds;
            ctx_cursor->left_bound_seconds += diff_seconds;
        }
        }
    }
}

static inline void war_move_cursor_left_leap(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    war_misc_context* ctx_misc = env->ctx_misc;
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        for (uint32_t k = 0; k < ctx_cursor->count[i]; k++) {
            ctx_cursor->x_seconds[i][k] -= ctx_misc->seconds_per_cell *
                                           ctx_cursor->move_factor *
                                           ctx_cursor->leap_cells;
            if (ctx_cursor->x_seconds[i][k] <= ctx_misc->epsilon) {
                ctx_cursor->x_seconds[i][k] = 0;
                ctx_cursor->stage[i][k].pos[0] = 0;
                ctx_cursor->dirty[i] = 1;
                goto war_label_pan;
            }
            ctx_cursor->stage[i][k].pos[0] -=
                ctx_cursor->leap_cells * ctx_cursor->move_factor;
            ctx_cursor->dirty[i] = 1;
        war_label_pan: {
            double offset_seconds = 0;
            if (ctx_cursor->x_seconds[i][k] >= ctx_cursor->left_bound_seconds +
                                                   offset_seconds -
                                                   ctx_misc->epsilon) {
                continue;
            }
            double diff_seconds = ctx_cursor->left_bound_seconds +
                                  offset_seconds - ctx_cursor->x_seconds[i][k];
            ctx_cursor->push_constant->panning[0] +=
                diff_seconds / ctx_misc->seconds_per_cell;
            ctx_cursor->right_bound_seconds -= diff_seconds;
            ctx_cursor->left_bound_seconds -= diff_seconds;
        }
        }
    }
}

static inline void war_move_cursor_up_leap(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    war_misc_context* ctx_misc = env->ctx_misc;
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        for (uint32_t k = 0; k < ctx_cursor->count[i]; k++) {
            ctx_cursor->y_cells[i][k] +=
                ctx_cursor->move_factor * ctx_cursor->leap_cells;
            if (ctx_cursor->y_cells[i][k] >=
                ctx_cursor->max_cells_y - ctx_misc->epsilon) {
                ctx_cursor->y_cells[i][k] = ctx_cursor->max_cells_y - 1;
                ctx_cursor->stage[i][k].pos[1] = ctx_cursor->max_cells_y - 1;
                ctx_cursor->dirty[i] = 1;
                goto war_label_pan;
            }
            ctx_cursor->stage[i][k].pos[1] +=
                ctx_cursor->move_factor * ctx_cursor->leap_cells;
            ctx_cursor->dirty[i] = 1;
        war_label_pan: {
            double offset_cells = ctx_cursor->push_constant->cell_offset[1] + 1;
            if (ctx_cursor->y_cells[i][k] < ctx_cursor->top_bound_cells -
                                                offset_cells -
                                                ctx_misc->epsilon) {
                continue;
            }
            double diff_cells = ctx_cursor->y_cells[i][k] -
                                (ctx_cursor->top_bound_cells - offset_cells);
            ctx_cursor->push_constant->panning[1] -= diff_cells;
            ctx_cursor->top_bound_cells += diff_cells;
            ctx_cursor->bottom_bound_cells += diff_cells;
        }
        }
    }
}

static inline void war_move_cursor_down_leap(war_env* env) {
    war_cursor_context* ctx_cursor = env->ctx_cursor;
    war_misc_context* ctx_misc = env->ctx_misc;
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        for (uint32_t k = 0; k < ctx_cursor->count[i]; k++) {
            ctx_cursor->y_cells[i][k] -=
                ctx_cursor->move_factor * ctx_cursor->leap_cells;
            if (ctx_cursor->y_cells[i][k] <= ctx_misc->epsilon) {
                ctx_cursor->y_cells[i][k] = 0;
                ctx_cursor->stage[i][k].pos[1] = 0;
                ctx_cursor->dirty[i] = 1;
                goto war_label_pan;
            }
            ctx_cursor->stage[i][k].pos[1] -=
                ctx_cursor->move_factor * ctx_cursor->leap_cells;
            ctx_cursor->dirty[i] = 1;
        war_label_pan: {
            double offset_cells = 0;
            if (ctx_cursor->y_cells[i][k] >= ctx_cursor->bottom_bound_cells +
                                                 offset_cells +
                                                 ctx_misc->epsilon) {
                continue;
            }
            double diff_cells = ctx_cursor->bottom_bound_cells + offset_cells -
                                ctx_cursor->y_cells[i][k];
            ctx_cursor->push_constant->panning[1] += diff_cells;
            ctx_cursor->top_bound_cells -= diff_cells;
            ctx_cursor->bottom_bound_cells -= diff_cells;
        }
        }
    }
}

#endif // WAR_KEYMAP_FUNCTIONS_H
