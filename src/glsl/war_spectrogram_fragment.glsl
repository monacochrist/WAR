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
// src/glsl/war_spectrogram_fragment.glsl
//-----------------------------------------------------------------------------

#version 450 core

// Inputs from vertex shader
layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;
layout(location = 2) in float frag_time_offset;
layout(location = 3) in float frag_frequency_scale;

// Descriptor set
layout(binding = 0) uniform sampler2D spectrogram_texture;

// Output
layout(location = 0) out vec4 out_color;

void main() {
    // Sample spectrogram texture with time/frequency scaling
    vec2 uv_adjusted = frag_uv;
    uv_adjusted.x = uv_adjusted.x * frag_time_offset;    // Time dimension
    uv_adjusted.y = uv_adjusted.y * frag_frequency_scale; // Frequency dimension
    
    vec4 texel = texture(spectrogram_texture, uv_adjusted);
    
    // Blend texture color with instance color
    out_color = texel * frag_color;
    
    // Optional: Add some visual effects
    // out_color.rgb = pow(out_color.rgb, vec3(0.8)); // S-curve for contrast
    // out_color.a *= 0.9; // Slight transparency
}
