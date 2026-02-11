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

static inline void war_keymap_set(war_keymap_context* keymap,
                                  war_mode_flags mode_flags,
                                  uint32_t sequence_count,
                                  char** sequences,
                                  void (*function)(war_env* env),
                                  war_keymap_flags flags) {
    if (!keymap || !sequences || !function) return;

    for (uint32_t seq_idx = 0; seq_idx < sequence_count; seq_idx++) {
        const char* seq = sequences[seq_idx];
        if (!seq) continue;

        size_t len = strlen(seq);
        if (len == 0) continue;

        uint32_t current_state = 0; // start at root
        size_t offset = 0;
        char token_buf[64];

        while (offset < len) {
            size_t token_len = 0;

            // Detect token: <...> or single char
            if (seq[offset] == '<') {
                size_t end = offset + 1;
                while (end < len && seq[end] != '>') end++;
                token_len = (end < len) ? (end - offset + 1) : (len - offset);
            } else {
                token_len = 1;
            }

            if (token_len >= sizeof(token_buf))
                token_len = sizeof(token_buf) - 1;
            memcpy(token_buf, seq + offset, token_len);
            token_buf[token_len] = '\0';

            // Parse token into keysym + mod
            uint32_t keysym = 0;
            uint8_t mod = 0;
            if (!war_parse_token_to_keysym_mod(token_buf, &keysym, &mod)) {
                offset += token_len;
                continue; // skip invalid token
            }

            // Index into next_state table
            size_t idx3d = KEYMAP_3D_INDEX(current_state, keysym, mod, keymap);
            uint32_t next_state = keymap->next_state[idx3d];

            if (next_state == 0) {
                if (keymap->state_count < keymap->state_capacity) {
                    next_state = keymap->state_count++;
                } else {
                    next_state =
                        keymap->state_capacity - 1; // overflow fallback
                }
                keymap->next_state[idx3d] = next_state;
            }

            if (offset + token_len < len) {
                keymap->flags[current_state] |= WAR_KEYMAP_PREFIX;
            }

            // If this is the last token, assign function
            if (offset + token_len >= len) {
                uint8_t func_idx = keymap->function_count[next_state];

                if (flags & WAR_KEYMAP_EXTEND) {
                    if (func_idx < keymap->function_capacity) {
                        keymap
                            ->function[next_state * keymap->function_capacity +
                                       func_idx] = function;
                        keymap->function_count[next_state]++;
                    } else {
                        // optional: log overflow
                    }
                } else {
                    keymap
                        ->function[next_state * keymap->function_capacity + 0] =
                        function;
                    keymap->function_count[next_state] = 1;
                }

                // Set any extra flags (extend, release, etc.)
                keymap->flags[next_state] |= flags;
            }

            current_state = next_state;
            offset += token_len;
        }
    }
}

// sets defaults, no need to call during override since it's called at init
static inline void war_keymap_default(war_keymap_context* keymap) {
    keymap->version = WAR_KEYMAP_H_VERSION;
    // move
    war_keymap_set(keymap,
                   WAR_MODE_ROLL,
                   2,
                   (char* [2]){"j", "<Down>"},
                   war_move_cursor_down,
                   0);
    war_keymap_set(keymap,
                   WAR_MODE_ROLL,
                   2,
                   (char* [2]){"h", "<Left>"},
                   war_move_cursor_left,
                   0);
    war_keymap_set(keymap,
                   WAR_MODE_ROLL,
                   2,
                   (char* [2]){"k", "<Up>"},
                   war_move_cursor_up,
                   0);
    war_keymap_set(keymap,
                   WAR_MODE_ROLL,
                   2,
                   (char* [2]){"l", "<Right>"},
                   war_move_cursor_right,
                   0);
    // move leap
    war_keymap_set(keymap,
                   WAR_MODE_ROLL,
                   2,
                   (char* [2]){"<A-j>", "<A-Down>"},
                   war_move_cursor_down_leap,
                   0);
    war_keymap_set(keymap,
                   WAR_MODE_ROLL,
                   2,
                   (char* [2]){"<A-h>", "<A-Left>"},
                   war_move_cursor_left_leap,
                   0);
    war_keymap_set(keymap,
                   WAR_MODE_ROLL,
                   2,
                   (char* [2]){"<A-k>", "<A-Up>"},
                   war_move_cursor_up_leap,
                   0);
    war_keymap_set(keymap,
                   WAR_MODE_ROLL,
                   2,
                   (char* [2]){"<A-l>", "<A-Right>"},
                   war_move_cursor_right_leap,
                   0);
}

void war_keymap_override(war_keymap_context* keymap);

#endif // WAR_KEYMAP_H
