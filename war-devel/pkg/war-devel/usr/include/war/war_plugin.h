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

#include "stdint.h"
#include "war_data.h"
#include "war_keymap.h"

static inline void war_hook_set(war_hook_context* hook,
                                war_mode_flags mode_flags,
                                war_event_flags event_flags,
                                void (*function)(war_env* env)) {}

static inline void war_plugin_init_example(war_env* env) {
    war_hook_context* hook_context_example = NULL;
    void (*print_hello_world)(war_env* env) = NULL;
    war_hook_set(hook_context_example,
                 WAR_MODE_ROLL | WAR_MODE_COMMAND,
                 WAR_EVENT_MOVE_CURSOR_LEFT | WAR_EVENT_MOVE_CURSOR_RIGHT,
                 print_hello_world);
}

void war_plugin_init(war_env* env);

#endif // WAR_PLUGIN_H
