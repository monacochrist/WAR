//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_plugin.h
//-----------------------------------------------------------------------------

#ifndef WAR_PLUGIN_H
#define WAR_PLUGIN_H

#ifndef WAR_PLUGIN_H_VERSION
#define WAR_PLUGIN_H_VERSION 0
#endif // WAR_PLUGIN_H_VERSION

#include "war_data.h"

static inline void war_plugin_set(war_env* env,
                                  war_mode_flags mode_flags,
                                  war_event_flags event_flags,
                                  uint32_t function_count,
                                  void (**function)(war_env* env)) {
    if (!function_count || !function) { return; }
    war_hook_context* hook = env->ctx_hook;
    war_config_context* config = env->ctx_config;
    if (hook->count >= config->HOOK_CONTEXT_CAPACITY) {
        call_king_terry("max hook capacity reached");
        return;
    }
    for (uint32_t i = 0; i < function_count; i++) {
        if (!function[i]) { continue; }
        hook->mode_flags[hook->count] = mode_flags;
        hook->event_flags[hook->count] = event_flags;
        hook->function[hook->count] = function[i];
        hook->count++;
        if (hook->count >= config->HOOK_CONTEXT_CAPACITY) {
            call_king_terry("max hook capacity reached");
            return;
        }
    }
}

static inline void war_plugin_override_example(war_env* env) {
    void (*print_hello_world)(war_env* env) = NULL;
    void (*print_goodbye_world)(war_env* env) = NULL;
    // function shouldn't be null but here just for example
    war_plugin_set(env,
                   WAR_MODE_ROLL | WAR_MODE_COMMAND,
                   WAR_EVENT_MOVE_CURSOR_LEFT | WAR_EVENT_MOVE_CURSOR_RIGHT,
                   1,
                   (void (*[])(war_env*)){print_hello_world});
    war_plugin_set(
        env,
        WAR_MODE_ROLL | WAR_MODE_COMMAND | WAR_MODE_MIDI,
        WAR_EVENT_MOVE_CURSOR_LEFT_LEAP | WAR_EVENT_MOVE_CURSOR_RIGHT_LEAP,
        2,
        (void (*[])(war_env*)){print_goodbye_world, print_hello_world});
}

void war_plugin_override(war_env* env);

#endif // WAR_PLUGIN_H
