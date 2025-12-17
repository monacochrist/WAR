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
    // For testing: create a detailed spectrogram-like pattern
    vec2 uv = frag_uv;

    // Create multiple frequency bands with different patterns
    float freq1 = sin(uv.x * 50.0 + uv.y * 10.0) * 0.5 + 0.5;
    float freq2 = cos(uv.x * 30.0 + uv.y * 15.0) * 0.5 + 0.5;
    float freq3 = sin(uv.x * 20.0 - uv.y * 8.0) * 0.5 + 0.5;
    float freq4 = cos(uv.x * 70.0 + uv.y * 5.0) * 0.5 + 0.5;

    // Combine frequencies with different weights for different "harmonics"
    float intensity = (freq1 * 0.4 + freq2 * 0.3 + freq3 * 0.2 + freq4 * 0.1);

    // Add some noise-like variation
    intensity += sin(uv.x * 200.0) * cos(uv.y * 150.0) * 0.1;

    // Clamp and create spectrogram color (black to white with yellow tint)
    intensity = clamp(intensity, 0.0, 1.0);
    vec3 spectrogram_color = vec3(intensity, intensity, intensity * 0.7);

    // Blend with instance color
    out_color = vec4(spectrogram_color, 1.0) * frag_color;
    
    // Optional: Add some visual effects
    // out_color.rgb = pow(out_color.rgb, vec3(0.8)); // S-curve for contrast
    // out_color.a *= 0.9; // Slight transparency
}
