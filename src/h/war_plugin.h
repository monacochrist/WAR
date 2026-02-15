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
#include "war_keymap.h"

#include <stdint.h>

static inline void war_plugin_set(war_env* env,
                                  war_mode_flags mode_flags,
                                  war_event_flags event_flags,
                                  void (*function)(war_env* env)) {
    // use hook context
}

static inline void war_plugin_init_example(war_env* env) {
    void (*print_hello_world)(war_env* env) = NULL;
    war_plugin_set(env,
                   WAR_MODE_ROLL | WAR_MODE_COMMAND,
                   WAR_EVENT_MOVE_CURSOR_LEFT | WAR_EVENT_MOVE_CURSOR_RIGHT,
                   print_hello_world);
}

void war_plugin_override(war_env* env);

#endif // WAR_PLUGIN_H
