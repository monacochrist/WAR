//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Nick Monaco
// 
// This file is part of WAR 1.0 software.
// WAR 1.0 software is licensed under the GNU Affero General Public License
// version 3, with the following modification: attribution to the original
// author is waived.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// 
// For the full license text, see LICENSE-AGPL and LICENSE-CC-BY-SA and LICENSE.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_nsgt_compute.glsl
//-----------------------------------------------------------------------------

#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer buf0 {
    float data[];
} l_buffer;

layout(std430, binding = 1) buffer buf1 {
    float data[];
} r_buffer;

layout(std430, binding = 2) buffer buf2 {
    float data[];
} l_previous_buffer;

layout(std430, binding = 3) buffer buf3 {
    float data[];
} r_previous_buffer;

layout(std430, binding = 4) buffer buf4 {
    uint data[];
} undo_buffer;

layout(std430, binding = 5) buffer buf5 {
    uint data[];
} redo_buffer;

void main() {
    l_buffer.data[13] = 13;
    r_buffer.data[13] = 13;
    l_previous_buffer.data[13] = 13;
    r_previous_buffer.data[13] = 13;
    undo_buffer.data[13] = 13;
    redo_buffer.data[13] = 13;
    l_buffer.data[0] = 13;
    r_buffer.data[0] = 13;
    l_previous_buffer.data[0] = 13;
    r_previous_buffer.data[0] = 13;
    undo_buffer.data[0] = 13;
    redo_buffer.data[0] = 13;
}
