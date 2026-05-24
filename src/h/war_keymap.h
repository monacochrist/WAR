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
#include "war_functions.h"
#include "war_data.h"

static inline void war_keymap_set(war_keymap_context* keymap,
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
    war_key();
    keymap->version = WAR_KEYMAP_H_VERSION;
    // move
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   2,
                   (char*[]){"j", "<Down>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_DOWN,
                   war_move_cursor_down,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   2,
                   (char*[]){"h", "<Left>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_LEFT,
                   war_move_cursor_left,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   2,
                   (char*[]){"k", "<Up>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_UP,
                   war_move_cursor_up,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   2,
                   (char*[]){"l", "<Right>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_RIGHT,
                   war_move_cursor_right,
                   0);
    // move leap
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   2,
                   (char*[]){"<A-j>", "<A-Down>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_DOWN_LEAP,
                   0,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   2,
                   (char*[]){"<A-h>", "<A-Left>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_LEFT_LEAP,
                   0,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   2,
                   (char*[]){"<A-k>", "<A-Up>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_UP_LEAP,
                   0,
                   0);
    war_keymap_set(keymap,
                   config,
                   1,
                   (war_mode_id[]){WAR_MODE_ID_ROLL},
                   2,
                   (char*[]){"<A-l>", "<A-Right>"},
                   WAR_FUNCTION_ID_MOVE_CURSOR_RIGHT_LEAP,
                   0,
                   0);
}

void war_keymap_override(war_keymap_context* keymap,
                         war_config_context* config);

#endif // WAR_KEYMAP_H
