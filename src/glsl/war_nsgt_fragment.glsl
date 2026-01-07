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
// src/glsl/war_nsgt_fragment.glsl
//-----------------------------------------------------------------------------

#version 450

layout(binding = 16) uniform sampler2D l_image;
layout(binding = 17) uniform sampler2D r_image;

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

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

void main() {
    // Sample the texture instead of using imageLoad
    float l = texture(l_image, frag_uv).r;
    float r = texture(r_image, frag_uv).r;

    // Choose left or right channel for visualization
    float v = (push_constant.channel == 0) ? l : r;

    out_color = vec4(v, 0.0, 0.0, 1.0);
}
