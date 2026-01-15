//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_nsgt_compute_transient.glsl
//-----------------------------------------------------------------------------

#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

layout(std430, binding = 0) buffer buf0 { uint data[]; } offset_buffer; 
layout(std430, binding = 1) buffer buf1 { uint data[]; } hop_buffer;
layout(std430, binding = 2) buffer buf2 { uint data[]; } length_buffer;
layout(std430, binding = 3) buffer buf3 { float data[]; } window_buffer;
layout(std430, binding = 4) buffer buf4 { float data[]; } dual_window_buffer;
layout(std430, binding = 5) buffer buf5 { float data[]; } frequency_buffer;
layout(std430, binding = 6) buffer buf6 { vec2 data[]; } cis_buffer;
layout(std430, binding = 7) buffer buf7 { float data[]; } wav_temp_buffer;
layout(std430, binding = 8) buffer buf8 { float data[]; } wav_buffer;
layout(std430, binding = 9) buffer buf9 { vec2 data[]; } nsgt_temp_buffer;
layout(std430, binding = 10) buffer buf10 { vec2 data[]; } nsgt_buffer;
layout(std430, binding = 11) buffer buf11 { float data[]; } magnitude_temp_buffer;
layout(std430, binding = 12) buffer buf12 { float data[]; } magnitude_buffer;
layout(std430, binding = 13) buffer buf13 { float data[]; } transient_temp_buffer;
layout(std430, binding = 14) buffer buf14 { float data[]; } transient_buffer;
layout(r32f, binding = 15) uniform writeonly image2D image_temp;
layout(r32f, binding = 16) uniform writeonly image2D image;

layout(push_constant) uniform pc {
    layout(offset = 0) uint arg_1;
    layout(offset = 4) uint window_length_max;
    layout(offset = 8) uint bin_capacity;
    layout(offset = 12) uint frame_capacity;
} push_constant;

vec2 complexAdd(vec2 a, vec2 b) { return vec2(a.x + b.x, a.y + b.y); }
vec2 complexMul(vec2 a, vec2 b) { return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x); }

#define WINDOW_LENGTH_CAPACITY 4096
shared vec2 cis_shared[WINDOW_LENGTH_CAPACITY];
shared float window_shared[WINDOW_LENGTH_CAPACITY];

void main() {
    uint tid = gl_LocalInvocationID.x;
    uint group_id = gl_WorkGroupID.x;
    uint threads_per_group = gl_WorkGroupSize.x;

    uint arg_samples = push_constant.arg_1;
    uint bins       = push_constant.bin_capacity;

    uint global_tid = tid + group_id * threads_per_group;

    for(uint s = global_tid; s < arg_samples; s += threads_per_group * gl_NumWorkGroups.x) {
        for(uint b = 0; b < bins; b++) {
            uint off = b * arg_samples; // linearized index

            float mag_current = magnitude_temp_buffer.data[off + s];
            float mag_prev = (s > 0) ? magnitude_temp_buffer.data[off + s - 1] : 0.0;

            // transient = positive increase
            float trans = max(mag_current - mag_prev, 0.0);

            transient_temp_buffer.data[off + s] = trans;
        }
    }
    memoryBarrier();
    barrier();
}
