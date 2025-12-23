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
// src/glsl/war_nsgt_vertex.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec2 in_corner; // Quad corner (0..1)
layout(location = 1) in vec3 in_pos;    // Vertex position

layout(push_constant) uniform PushConstants {
    int channel;         // selected channel (0=L, 1=R, 2=both)
    int blend;           // blend mode (0=off, 1=additive)
    float color_l[4];
    float color_r[4];
    float time_offset;
    float freq_scale;
    float time_scale;
} pc;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) flat out int frag_channel;

void main() {
    // Pass UV coordinates
    frag_uv = in_corner;

    // Pass channel as flat to avoid interpolation
    frag_channel = pc.channel;

    // Position in clip space (optionally scale with push constants)
    gl_Position = vec4(in_pos, 1.0);
}
