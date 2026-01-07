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

// Vertex outputs
layout(location = 0) out vec2 frag_uv;

layout(push_constant) uniform pc {
    layout(offset = 0) int channel;
    layout(offset = 4) int blend;
    layout(offset = 8) int _pad0[2];
    layout(offset = 16) float color_l[4];
    layout(offset = 32) float color_r[4];
    layout(offset = 48) float time_offset;
    layout(offset = 52) float freq_scale;
    layout(offset = 56) float time_scale;
    layout(offset = 60) int bin_capacity;
    layout(offset = 64) int frame_capacity;
    layout(offset = 68) float z_layer;
} push_constant;

void main() {
    // Generate a fullscreen triangle from gl_VertexIndex
    // Vertex indices: 0, 1, 2
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0), // bottom-left
        vec2( 3.0, -1.0), // bottom-right (outside NDC, will cover full screen)
        vec2(-1.0,  3.0)  // top-left
    );

    vec2 pos = positions[gl_VertexIndex];
    
    gl_Position = vec4(pos, push_constant.z_layer, 1.0);

    // Map NDC [-1,1] to UV [0,1]
    frag_uv = pos * 0.5 + 0.5;
}
