//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_nsgt_compute_image.glsl
//-----------------------------------------------------------------------------

#version 450

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

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
    layout(offset = 0) uint arg_1; // frame_count
    layout(offset = 4) uint arg_2; // base_sample (unused here)
    layout(offset = 8) uint arg_3; // bin_capacity
    layout(offset = 12) uint arg_4; // hop (unused here)
} push_constant;

vec2 complexAdd(vec2 a, vec2 b) { return vec2(a.x + b.x, a.y + b.y); }
vec2 complexMul(vec2 a, vec2 b) { return vec2(a.x*b.x - a.y*b.y, a.x*b.y + a.y*b.x); }

#define WINDOW_LENGTH_CAPACITY 4096
shared vec2 cis_shared[WINDOW_LENGTH_CAPACITY];
shared float window_shared[WINDOW_LENGTH_CAPACITY];

void main() {
    uint frame = gl_GlobalInvocationID.x;
    uint bin   = gl_GlobalInvocationID.y;

    uint frame_count = push_constant.arg_1;
    uint bins = push_constant.arg_3;

    if (frame >= frame_count || bin >= bins) return;

    uint idx = bin * frame_count + frame;
    float mag = magnitude_temp_buffer.data[idx];
    float trans = transient_temp_buffer.data[idx];

    // Linear magnitude with stronger transient lift
    float val = clamp(mag + trans * 1.0, 0.0, 1.0);
    imageStore(image_temp, ivec2(frame, bin), vec4(val, 0.0, 0.0, 1.0));
}
