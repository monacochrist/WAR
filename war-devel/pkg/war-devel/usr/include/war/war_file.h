//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_file.h
//-----------------------------------------------------------------------------

#ifndef WAR_FILE_H
#define WAR_FILE_H

#ifndef WAR_FILE_H_VERSION
#define WAR_FILE_H_VERSION 0
#endif // WAR_FILE_H_VERSION

#include "stdint.h"
#include "war_data.h"

static inline void war_file_set(war_file_context* file, char* path) {}

static inline void war_file_include(war_file_context* file, char* path) {}

static inline void war_file_default(war_file_context* file) {}

void war_file_override(war_file_context* file);

#endif // WAR_FILE_H
