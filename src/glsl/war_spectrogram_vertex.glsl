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
// src/glsl/war_spectrogram_vertex.glsl
//-----------------------------------------------------------------------------

#version 450 core

// Vertex inputs
layout(location = 0) in vec2 corner;
layout(location = 1) in vec3 pos;
layout(location = 2) in vec2 uv;

// Instance inputs
layout(location = 3) in uvec2 instance_xy;       // x, y
layout(location = 4) in uvec2 instance_wh;       // width, height  
layout(location = 5) in uvec4 instance_color;   // color
layout(location = 6) in float instance_time_offset;
layout(location = 7) in float instance_frequency_scale;

// Push constants
layout(push_constant) uniform PushConstants {
    vec2 bottom_left;
    vec2 physical_size;
    vec2 cell_size;
    float zoom;
    vec2 cell_offsets;
    vec2 scroll_margin;
    vec2 anchor_cell;
    vec2 top_right;
    float time_scale;
    float frequency_scale;
    float time_offset;
    uint fft_size;
} pc;

// Outputs
layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out float frag_time_offset;
layout(location = 3) out float frag_frequency_scale;

void main() {
    // Calculate instance position in screen space
    vec2 instance_pos = vec2(instance_xy) * pc.cell_size + pc.bottom_left;
    
    // Apply transforms
    vec2 screen_pos = instance_pos + 
                     (pos.xy + corner * pos.z) * pc.cell_size +
                     pc.cell_offsets;
    
    // Scale by zoom and center
    screen_pos = (screen_pos - pc.anchor_cell) * pc.zoom + pc.anchor_cell;
    
    // Calculate final screen position (normalized)
    vec2 final_pos = (screen_pos - pc.scroll_margin) / pc.physical_size;
    
    gl_Position = vec4(final_pos * 2.0 - 1.0, 0.0, 1.0);
    
    // Pass through to fragment shader
    frag_uv = uv;
    // For testing without instances, use a fixed color
    frag_color = vec4(0.0, 1.0, 0.0, 1.0); // Green
    frag_time_offset = instance_time_offset + pc.time_offset;
    frag_frequency_scale = instance_frequency_scale * pc.frequency_scale;
}
