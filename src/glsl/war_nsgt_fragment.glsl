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

layout(location = 0) in vec2 frag_uv;
layout(location = 1) flat in int frag_channel;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
    int channel;         // selected channel (0=L, 1=R, 2=both)
    int blend;           // blend mode (0=off, 1=additive)
    float color_l[4];    // RGBA
    float color_r[4];    // RGBA
    float time_offset;
    float freq_scale;
    float time_scale;
} pc;

void main() {
    vec4 final_color;

    if(frag_channel == 0) { 
        final_color = vec4(pc.color_l[0], pc.color_l[1], pc.color_l[2], pc.color_l[3]); 
    } else if(frag_channel == 1) { 
        final_color = vec4(pc.color_r[0], pc.color_r[1], pc.color_r[2], pc.color_r[3]); 
    } else if(frag_channel == 2) { 
        final_color = vec4(
            (pc.color_l[0] + pc.color_r[0]) * 0.5,
            (pc.color_l[1] + pc.color_r[1]) * 0.5,
            (pc.color_l[2] + pc.color_r[2]) * 0.5,
            (pc.color_l[3] + pc.color_r[3]) * 0.5
        );
    }

    // optional blending toggle
    if(pc.blend == 0) {
        final_color.a = 1.0; // opaque if blending is off
    }

    out_color = final_color;
}
