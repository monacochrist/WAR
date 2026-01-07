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

layout(std430, binding = 0) buffer buf0 { uint data[]; } offset_buffer; 
layout(std430, binding = 1) buffer buf1 { uint data[]; } hop_buffer;
layout(std430, binding = 2) buffer buf2 { uint data[]; } length_buffer;
layout(std430, binding = 3) buffer buf3 { float data[]; } window_buffer;
layout(std430, binding = 4) buffer buf4 { float data[]; } dual_window_buffer;
layout(std430, binding = 5) buffer buf5 { float data[]; } frequency_buffer;
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
    layout(offset = 32) uint bin_capacity;
    layout(offset = 36) uint frame_capacity;
} push_constant;

vec2 complexAdd(vec2 a, vec2 b) { return vec2(a.x + b.x, a.y + b.y); }
vec2 complexMul(vec2 a, vec2 b) { return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x); }
vec2 cis(float theta) { return vec2(cos(theta), sin(theta)); }

void main() {
    uint gid = gl_GlobalInvocationID.x;
    uint total_coeffs = push_constant.bin_capacity * push_constant.frame_capacity;

    if (gid >= total_coeffs) return;

    uint b = gid % push_constant.bin_capacity;          // bin index
    uint n = gid / push_constant.bin_capacity;          // frame index

    int start = int(push_constant.bin_start);
    int end = int(push_constant.bin_end);
    if (b < start || b >= end) return;

    // --- Calculate NSGT coefficient for left channel ---
    int L_b = int(length_buffer.data[b]);
    int a_b = int(hop_buffer.data[b]);
    vec2 coeff_l = vec2(0.0);
    vec2 coeff_r = vec2(0.0);

    for (int t = 0; t < L_b; t++) {
        uint idx = n * a_b + t;
        float sample_l = l_buffer.data[idx];
        float sample_r = r_buffer.data[idx];

        float w = window_buffer.data[offset_buffer.data[b] + t];           // Gaussian window_buffer
        float dw = dual_window_buffer.data[offset_buffer.data[b] + t];     // Dual window_buffer

        float phase = -6.28318530718 * frequency_buffer.data[b] * float(t) / float(L_b); // -2π f t / L_b
        vec2 exp_phase = cis(phase);

        vec2 s_l = vec2(sample_l * w * dw, 0.0);
        vec2 s_r = vec2(sample_r * w * dw, 0.0);

        coeff_l = complexAdd(coeff_l, complexMul(s_l, exp_phase));
        coeff_r = complexAdd(coeff_r, complexMul(s_r, exp_phase));
    }

    // --- Store NSGT coefficient ---
    uint idx_out = b * push_constant.frame_capacity + n;
    l_nsgt_buffer.data[idx_out] = coeff_l;
    r_nsgt_buffer.data[idx_out] = coeff_r;

    // --- Magnitude for visualization ---
    float mag_l = length(coeff_l);
    float mag_r = length(coeff_r);

    imageStore(l_image, ivec2(n, b), vec4(mag_l, mag_l, mag_l, 1.0));
    imageStore(r_image, ivec2(n, b), vec4(mag_r, mag_r, mag_r, 1.0));
}
