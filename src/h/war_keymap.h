//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_keymap.h
//-----------------------------------------------------------------------------

#ifndef WAR_KEYMAP_H
#define WAR_KEYMAP_H

#ifndef WAR_KEYMAP_H_VERSION
#define WAR_KEYMAP_H_VERSION 0
#endif // WAR_KEYMAP_H_VERSION

#include "war_build_keymap_functions.h"
#include "war_data.h"
#include "war_functions.h"

__attribute__((noinline)) static void
war_keymap_set(war_keymap_context* keymap,
               war_config_context* config,
               uint32_t mode_count,
               war_mode_id* modes,
               uint32_t sequence_count,
               char** sequences,
               war_function_id function_id,
               void (*function)(war_env* env),
               war_keymap_flags flags) {
    if (!keymap || !modes || !sequences) return;

    for (uint32_t m_idx = 0; m_idx < mode_count; m_idx++) {
        war_mode_id mode = modes[m_idx];

        if (mode >= config->KEYMAP_MODE_CAPACITY) continue;

        for (uint32_t seq_idx = 0; seq_idx < sequence_count; seq_idx++) {
            const char* seq = sequences[seq_idx];
            if (!seq) continue;

            size_t len = strlen(seq);
            if (!len) continue;

            uint32_t current_state = 0; // root per mode
            size_t offset = 0;
            char token_buf[64];

            while (offset < len) {
                size_t token_len = 0;

                if (seq[offset] == '<') {
                    size_t end = offset + 1;
                    while (end < len && seq[end] != '>') end++;
                    token_len =
                        (end < len) ? (end - offset + 1) : (len - offset);
                } else {
                    token_len = 1;
                }

                if (token_len >= sizeof(token_buf))
                    token_len = sizeof(token_buf) - 1;

                memcpy(token_buf, seq + offset, token_len);
                token_buf[token_len] = '\0';

                uint32_t keysym = 0;
                uint8_t mod = 0;

                if (!war_parse_token_to_keysym_mod(token_buf, &keysym, &mod)) {
                    offset += token_len;
                    continue;
                }

                // -------- TRANSITION INDEX --------
                size_t trans_idx =
                    mode * (config->KEYMAP_STATE_CAPACITY *
                            config->KEYMAP_KEYSYM_CAPACITY *
                            config->KEYMAP_MOD_CAPACITY) +
                    current_state * (config->KEYMAP_KEYSYM_CAPACITY *
                                     config->KEYMAP_MOD_CAPACITY) +
                    keysym * config->KEYMAP_MOD_CAPACITY + mod;

                uint64_t next_state = keymap->next_state[trans_idx];

                if (!next_state) {
                    if (keymap->state_count[mode] <
                        config->KEYMAP_STATE_CAPACITY) {
                        next_state = keymap->state_count[mode]++;
                    } else {
                        next_state =
                            config->KEYMAP_STATE_CAPACITY - 1; // overflow
                    }

                    keymap->next_state[trans_idx] = next_state;
                }

                // Mark prefix
                if (offset + token_len < len) {
                    size_t flag_idx =
                        mode * config->KEYMAP_STATE_CAPACITY + current_state;

                    keymap->flags[flag_idx] |= WAR_KEYMAP_PREFIX;
                }

                // -------- TERMINAL --------
                if (offset + token_len >= len) {
                    size_t func_count_idx =
                        mode * config->KEYMAP_STATE_CAPACITY + next_state;

                    size_t func_base =
                        mode * (config->KEYMAP_STATE_CAPACITY *
                                config->KEYMAP_FUNCTION_CAPACITY) +
                        next_state * config->KEYMAP_FUNCTION_CAPACITY;

                    uint8_t* count = &keymap->function_count[func_count_idx];

                    if (flags & WAR_KEYMAP_EXTEND) {
                        if (*count < config->KEYMAP_FUNCTION_CAPACITY) {
                            if (*count > 0) {
                                call_king_terry("war_keymap_set: extending "
                                                "previous mapping for "
                                                "(seq, mode_id): %s, %u",
                                                seq,
                                                mode);
                            }
                            keymap->function[func_base + *count] = function;
                            (*count)++;
                        }
                    } else {
                        if (*count > 0) {
                            call_king_terry("war_keymap_set: replacing "
                                            "previous mapping for "
                                            "(seq, mode_id): %s, %u",
                                            seq,
                                            mode);
                        }

                        if (!function) {
                            keymap->function_id[func_base + 0] = function_id;
                            keymap->function[func_base + 0] = NULL;
                            *count = 1;
                        } else {
                            keymap->function[func_base + 0] = function;
                            keymap->function_id[func_base + 0] =
                                WAR_FUNCTION_ID_NONE;
                            *count = 1;
                        }
                    }

                    keymap->flags[mode * config->KEYMAP_STATE_CAPACITY +
                                  next_state] |= flags;
                }

                current_state = next_state;
                offset += token_len;
            }
        }
    }
}

// sets defaults, no need to call during override since it's called at init
static inline void war_keymap_default(war_keymap_context* keymap,
                                      war_config_context* config) {
    keymap->version = WAR_KEYMAP_H_VERSION;
    // move
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"j", "<Down>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_DOWN,
                   war_move_cursor_down,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"h", "<Left>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_LEFT,
                   war_move_cursor_left,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"k", "<Up>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_UP,
                   war_move_cursor_up,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"l", "<Right>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_RIGHT,
                   war_move_cursor_right,
                   0);
    // move leap
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"<A-j>", "<A-Down>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_DOWN_LEAP,
                   war_move_cursor_down_leap,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"<A-h>", "<A-Left>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_LEFT_LEAP,
                   war_move_cursor_left_leap,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"<A-k>", "<A-Up>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_UP_LEAP,
                   war_move_cursor_up_leap,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"<A-l>", "<A-Right>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_RIGHT_LEAP,
                   war_move_cursor_right_leap,
                   0);
    // move cursor to next/prev note on same row
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"w"},
                   WAR_FUNCTION_ID_NONE,
                   war_next_note_same_row,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"b"},
                   WAR_FUNCTION_ID_NONE,
                    war_prev_note_same_row,
                    0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"e"},
                   WAR_FUNCTION_ID_NONE,
                    war_go_to_note_end,
                    0);
    // loop markers (n = end, N = start)
    war_keymap_set(keymap, config, 2,
                    (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                    1, (char*[]){"n"}, WAR_FUNCTION_ID_NONE,
                    war_set_loop_end, 0);
    war_keymap_set(keymap, config, 2,
                    (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                    1, (char*[]){"N"}, WAR_FUNCTION_ID_NONE,
                    war_set_loop_start, 0);
    // visual mode toggle (v) — bound in both ROLL and VISUAL modes
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"v"},
                   WAR_FUNCTION_ID_NONE,
                   war_visual_mode,
                   0);
    // visual mode selection move (shift+hjkl)
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                    (char*[]){"<S-l>", "<S-Right>"},
                    WAR_FUNCTION_ID_NONE,
                    war_visual_move_right,
                    0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"<S-h>", "<S-Left>"},
                    WAR_FUNCTION_ID_NONE,
                    war_visual_move_left,
                    0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"<S-k>", "<S-Up>"},
                    WAR_FUNCTION_ID_NONE,
                    war_visual_move_up,
                    0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   2,
                   (char*[]){"<S-j>", "<S-Down>"},
                    WAR_FUNCTION_ID_NONE,
                    war_visual_move_down,
                    0);
    // visual stretch toggle (shift+s)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"<S-s>"},
                   WAR_FUNCTION_ID_NONE,
                   war_visual_stretch_toggle,
                   0);
    // snap cursor to nearest cell (s) — in both ROLL and VISUAL
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"s"},
                   WAR_FUNCTION_ID_NONE,
                   war_reset_step,
                   0);
    // zoom ('=' zoom in, '-' zoom out)
    war_keymap_set(keymap, config, 4,
                    (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI,
                                    WAR_MODE_ID_VISUAL, WAR_MODE_ID_WAV},
                    1, (char*[]){"-"}, WAR_FUNCTION_ID_NONE, war_zoom_out, 0);
    war_keymap_set(keymap, config, 4,
                    (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI,
                                    WAR_MODE_ID_VISUAL, WAR_MODE_ID_WAV},
                    1, (char*[]){"="}, WAR_FUNCTION_ID_NONE, war_zoom_in, 0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){")"},
                   WAR_FUNCTION_ID_NONE,
                   war_zoom_reset,
                   0);
    // $ — go to column (prefix + 3), or right bound without prefix
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"$"},
                   WAR_FUNCTION_ID_NONE,
                   war_roll_cursor_goto_right_bound_or_prefix_horizontal,
                   0);
    // playback
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-Space>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_playback,
                   0);
    // reset playback bar to beginning (shift+d)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-d>"},
                   WAR_FUNCTION_ID_NONE,
                    war_playbar_goto_start,
                    0);
    // toggle playbar loop (shift+L)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-l>"},
                   WAR_FUNCTION_ID_NONE,
                     war_toggle_playbar_loop,
                     0);
    // toggle playback loop in midi mode (shift+l)
    war_keymap_set(keymap, config, 1,
                    (war_mode_id[]){WAR_MODE_ID_MIDI}, 1,
                    (char*[]){"<S-l>"}, WAR_FUNCTION_ID_NONE,
                    war_toggle_playbar_loop, 0);
    // tap tempo (shift+B)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-b>"},
                   WAR_FUNCTION_ID_NONE,
                   war_tap_tempo,
                   0);
    // layers
    war_keymap_set(keymap,
                   config,
               2,
               (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
               1,
               (char*[]){"<A-1>"},
               WAR_FUNCTION_ID_NONE,
               war_layer_1,
               0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-2>"},
                   WAR_FUNCTION_ID_NONE,
                   war_layer_2,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-3>"},
                   WAR_FUNCTION_ID_NONE,
                   war_layer_3,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-4>"},
                   WAR_FUNCTION_ID_NONE,
                   war_layer_4,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-5>"},
                   WAR_FUNCTION_ID_NONE,
                   war_layer_5,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-6>"},
                   WAR_FUNCTION_ID_NONE,
                   war_layer_6,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-7>"},
                   WAR_FUNCTION_ID_NONE,
                   war_layer_7,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-8>"},
                   WAR_FUNCTION_ID_NONE,
                   war_layer_8,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-9>"},
                   WAR_FUNCTION_ID_NONE,
                   war_layer_9,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-0>"},
                   WAR_FUNCTION_ID_NONE,
                    war_layer_0,
                    0);
    // toggle layer visibility (alt+shift+1..9)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-1>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_layer_1,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-2>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_layer_2,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-3>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_layer_3,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-4>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_layer_4,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-5>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_layer_5,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-6>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_layer_6,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-7>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_layer_7,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-8>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_layer_8,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-9>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_layer_9,
                    0);
    // toggle all layers (alt+shift+0)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-S-0>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_all_layers,
                   0);
    // capture audio toggle (Q = shift+q)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-q>", "Q"},
                   WAR_FUNCTION_ID_NONE,
                    war_capture_audio,
                    0);
    // capture and advance (q during capture — saves and moves to next row)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"q"},
                   WAR_FUNCTION_ID_NONE,
                   war_capture_advance,
                   0);
    // preview toggle (space / p / P)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   3,
                   (char*[]){"<Space>", " ", "P"},
                   WAR_FUNCTION_ID_NONE,
                   war_preview_toggle,
                   0);
    // set cursor width to slot duration (a)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"a"},
                   WAR_FUNCTION_ID_NONE,
                   war_set_width_to_duration,
                   0);
    // place note at cursor (z)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"z"},
                   WAR_FUNCTION_ID_NONE,
                   war_place_note,
                   0);
    // trim note under cursor to cursor position (t / r)
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"t"},
                   WAR_FUNCTION_ID_NONE,
                   war_trim_note_under_cursor,
                   0);
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"r"},
                   WAR_FUNCTION_ID_NONE,
                   war_trim_note_under_cursor,
                   0);
    // undo (u)
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"u"},
                   WAR_FUNCTION_ID_NONE,
                   war_undo,
                   0);
    // redo (ctrl+r)
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"<C-r>"},
                   WAR_FUNCTION_ID_NONE,
                   war_redo,
                   0);
    // yank (y in visual mode)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"y"},
                   WAR_FUNCTION_ID_NONE,
                   war_yank,
                   0);
    // swap anchor (o in visual mode — like neovim)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"o"},
                   WAR_FUNCTION_ID_NONE,
                   war_visual_swap_anchor,
                   0);
    // paste (p in roll mode)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"p"},
                   WAR_FUNCTION_ID_NONE,
                   war_paste,
                   0);
    // across mode toggle (i)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"i"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_across,
                   0);
    // resample toggle (alt+r)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-r>"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_resample,
                   0);
    // crop mode toggle (c)
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_ROLL}, 1, (char*[]){"c"}, WAR_FUNCTION_ID_NONE, war_toggle_crop, 0);
    // delete note under cursor (x)
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_VISUAL},
                   1,
                   (char*[]){"x"},
                   WAR_FUNCTION_ID_NONE,
                   war_delete_note_under_cursor,
                   0);
    // goto bottom of viewport (G)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"G"},
                   WAR_FUNCTION_ID_GOTO_VIEWPORT_BOTTOM,
                   war_goto_viewport_bottom,
                   0);
    // goto top of viewport (gg)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"gg"},
                   WAR_FUNCTION_ID_GOTO_VIEWPORT_TOP,
                   war_goto_viewport_top,
                   WAR_KEYMAP_UNIQUE_PREFIX);
    // waveform view — go to definition (gd)
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_ROLL}, 1, (char*[]){"gd"}, WAR_FUNCTION_ID_NONE, war_wave_view, 0);
    // absolute row jumps
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"gt"},
                   WAR_FUNCTION_ID_NONE,
                   war_goto_row_127,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"gm"},
                   WAR_FUNCTION_ID_NONE,
                   war_goto_row_60,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"gb"},
                   WAR_FUNCTION_ID_NONE,
                   war_goto_row_0,
                   0);
    // octaves
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"-"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_minus_1,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-0>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_0,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-1>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_1,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-2>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_2,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-3>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_3,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-4>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_4,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-5>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_5,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-6>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_6,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-7>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_7,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-8>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_8,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<S-9>"},
                   WAR_FUNCTION_ID_NONE,
                   war_octave_9,
                   0);
    // octave in midi mode (numbers)
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"0"}, WAR_FUNCTION_ID_NONE, war_octave_0, 0);
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"1"}, WAR_FUNCTION_ID_NONE, war_octave_1, 0);
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"2"}, WAR_FUNCTION_ID_NONE, war_octave_2, 0);
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"3"}, WAR_FUNCTION_ID_NONE, war_octave_3, 0);
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"4"}, WAR_FUNCTION_ID_NONE, war_octave_4, 0);
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"5"}, WAR_FUNCTION_ID_NONE, war_octave_5, 0);
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"6"}, WAR_FUNCTION_ID_NONE, war_octave_6, 0);
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"7"}, WAR_FUNCTION_ID_NONE, war_octave_7, 0);
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"8"}, WAR_FUNCTION_ID_NONE, war_octave_8, 0);
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"9"}, WAR_FUNCTION_ID_NONE, war_octave_9, 0);

    // playback (midi mode)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"q"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_q,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"w"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_w,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"e"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_e,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"r"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_r,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"t"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_t,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"y"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_y,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"u"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_u,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"i"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_i,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"o"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_o,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"p"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_p,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"["},
                   WAR_FUNCTION_ID_NONE,
                   war_play_left_bracket,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"]"},
                   WAR_FUNCTION_ID_NONE,
                   war_play_right_bracket,
                    0);
    // toggle midi mode (m)
    war_keymap_set(keymap,
                   config,
                   2,
                   (war_mode_id[]){WAR_MODE_ID_ROLL, WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"m"},
                   WAR_FUNCTION_ID_NONE,
                   war_midi_mode,
                    0);
    // move playback bar to cursor (alt+a)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"<A-a>"},
                   WAR_FUNCTION_ID_NONE,
                   war_playbar_goto_cursor,
                   0);
    // toggle midi recording (a)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"a"},
                   WAR_FUNCTION_ID_NONE,
                   war_record_midi,
                   0);
    // toggle loop mode (l)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"l"},
                   WAR_FUNCTION_ID_NONE,
                   war_toggle_loop,
                   0);
    // midi toggle (g)
    war_keymap_set(keymap, config, 1, (war_mode_id[]){WAR_MODE_ID_MIDI}, 1, (char*[]){"g"}, WAR_FUNCTION_ID_NONE, war_midi_toggle, 0);
    // move playback bar to cursor (alt+a)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<A-a>"},
                   WAR_FUNCTION_ID_NONE,
                   war_playbar_goto_cursor,
                   0);
    // reset playback bar to beginning (shift+d)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_MIDI},
                   1,
                   (char*[]){"<S-d>"},
                   WAR_FUNCTION_ID_NONE,
                   war_playbar_goto_start,
                   0);
    // fat and thin
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"f"},
                   WAR_FUNCTION_ID_NONE,
                   war_fat,
                   WAR_KEYMAP_UNIQUE_PREFIX);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"t"},
                   WAR_FUNCTION_ID_NONE,
                   war_thin,
                   WAR_KEYMAP_UNIQUE_PREFIX);
    // step mode fat (shift+f) and thin (shift+t)
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"F"},
                   WAR_FUNCTION_ID_NONE,
                   war_step_mode_fat,
                   WAR_KEYMAP_UNIQUE_PREFIX);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"T"},
                   WAR_FUNCTION_ID_NONE,
                   war_step_mode_thin,
                   WAR_KEYMAP_UNIQUE_PREFIX);
    // goto col
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"$"},
                   WAR_FUNCTION_ID_NONE,
                   war_goto_col,
                   WAR_KEYMAP_UNIQUE_PREFIX);
    // left visible bound
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   1,
                   (char*[]){"0"},
                   WAR_FUNCTION_ID_NONE,
                   war_goto_left_visible_bound,
                   0);
}

void war_keymap_override(war_keymap_context* keymap,
                         war_config_context* config);

#endif // WAR_KEYMAP_H
