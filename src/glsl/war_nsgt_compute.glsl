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

layout(std430, binding = 0) buffer buf0 { uint data[]; } offset; 
layout(std430, binding = 1) buffer buf1 { uint data[]; } hop;
layout(std430, binding = 2) buffer buf2 { uint data[]; } length;
layout(std430, binding = 3) buffer buf3 { float data[]; } window;
layout(std430, binding = 4) buffer buf4 { float data[]; } dual_window;
layout(std430, binding = 5) buffer buf5 { float data[]; } frequency;
layout(std430, binding = 6) buffer buf6 { float data[]; } l_buffer;
layout(std430, binding = 7) buffer buf7 { float data[]; } r_buffer;
layout(std430, binding = 8) buffer buf8 { vec2 data[]; } l_nsgt_temp_buffer;
layout(std430, binding = 9) buffer buf9 { vec2 data[]; } r_nsgt_temp_buffer;
layout(std430, binding = 10) buffer buf10 { vec2 data[]; } l_nsgt_buffer;
layout(std430, binding = 11) buffer buf11 { vec2 data[]; } r_nsgt_buffer;
layout(std430, binding = 12) buffer buf12 { float data[]; } l_magnitude_temp_buffer;
layout(std430, binding = 13) buffer buf13 { float data[]; } r_magnitude_temp_buffer;
layout(std430, binding = 14) buffer buf14 { float data[]; } l_magnitude_buffer;
layout(std430, binding = 15) buffer buf15 { float data[]; } r_magnitude_buffer;
layout(r32f, binding = 16) uniform writeonly image2D l_image;
layout(r32f, binding = 17) uniform writeonly image2D r_image;

layout(push_constant) uniform pc {
    layout(offset = 0) int operation_type;
    layout(offset = 4) int channel;
    layout(offset = 8) int bin_start;
    layout(offset = 12) int bin_end;
    layout(offset = 16) int frame_start;
    layout(offset = 20) int frame_end;
    layout(offset = 24) float param1;
    layout(offset = 28) float param2;
} push_constant;

void main() {
}
