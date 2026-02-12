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

static inline void war_command_set(war_command_context* command, ...) {}

static inline void war_command_default(war_command_context* command) {
    war_command_set(command, "");
}

void war_command_override(war_command_context* command) {}

#endif // WAR_COMMAND_H
