//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_command.h
//-----------------------------------------------------------------------------

#ifndef WAR_COMMAND_H
#define WAR_COMMAND_H

#ifndef WAR_COMMAND_H_VERSION
#define WAR_COMMAND_H_VERSION 0
#endif // WAR_COMMAND_H_VERSION

// similar to keymap fsm

#include "war_data.h"
#include "war_functions.h"

static inline void war_command_set(war_command_context* command,
                                   war_config_context* config,
                                   uint32_t sequence_count,
                                   char** sequences,
                                   war_function_id function_id,
                                   void (*function)(war_env* env),
                                   war_command_flags flags) {
    if (!command || !sequences || !function) return;

    for (uint32_t seq_idx = 0; seq_idx < sequence_count; seq_idx++) {
        const char* seq = sequences[seq_idx];
        if (!seq) continue;

        size_t len = strlen(seq);
        if (len == 0) continue;

        uint64_t current_state = 0; // root state
        size_t offset = 0;

        while (offset < len) {
            char ch = seq[offset]; // current character
            // index into next_state table
            size_t idx =
                current_state * 256 + (unsigned char)ch; // 256 = CHAR_CAPACITY
            uint64_t next_state = command->next_state[idx];

            if (next_state == 0) {
                if (command->state_count <
                    config->COMMAND_CONTEXT_STATE_CAPACITY) {
                    next_state = command->state_count++;
                } else {
                    next_state = config->COMMAND_CONTEXT_STATE_CAPACITY -
                                 1; // fallback on overflow
                }
                command->next_state[idx] = next_state;
            }

            // mark prefix if not the last char
            if (offset + 1 < len) {
                command->flags[next_state] |= WAR_COMMAND_PREFIX;
            }

            // assign function if terminal state
            if (offset + 1 >= len) {
                uint8_t func_idx = command->function_count[next_state];

                if (flags & WAR_COMMAND_EXTEND) {
                    if (func_idx < config->COMMAND_CONTEXT_FUNCTION_CAPACITY) {
                        call_king_terry("war_command_set: extending "
                                        "previous mapping for "
                                        "(seq): %s",
                                        seq);
                        command->function
                            [next_state *
                                 config->COMMAND_CONTEXT_FUNCTION_CAPACITY +
                             func_idx] = function;
                        command->function_count[next_state]++;
                    } // else overflow ignored
                } else {
                    call_king_terry("war_command_set: replacing "
                                    "previous mapping for "
                                    "(seq): %s",
                                    seq);
                    if (!function) {
                        command->function_id
                            [next_state *
                                 config->COMMAND_CONTEXT_FUNCTION_CAPACITY +
                             0] = function_id;
                        command->function
                            [next_state *
                                 config->COMMAND_CONTEXT_FUNCTION_CAPACITY +
                             0] = NULL;
                        command->function_count[next_state] = 1;
                    } else {
                        command->function
                            [next_state *
                                 config->COMMAND_CONTEXT_FUNCTION_CAPACITY +
                             0] = function;
                        command->function_id
                            [next_state *
                                 config->COMMAND_CONTEXT_FUNCTION_CAPACITY +
                             0] = WAR_FUNCTION_ID_NONE;
                        command->function_count[next_state] = 1;
                    }
                }

                // set extra flags
                command->flags[next_state] |= flags;
            }

            current_state = next_state;
            offset++;
        }
    }
}

static inline void war_command_default(war_command_context* command,
                                       war_config_context* config) {
    war_key();
    //war_command_set(command,
    //                config,
    //                3,
    //                (char*[]){"a", "b", "aa"},
    //                WAR_FUNCTION_ID_NONE,
    //                0,
    //                0);
}

void war_command_override(war_command_context* command,
                          war_config_context* config);

#endif // WAR_COMMAND_H
