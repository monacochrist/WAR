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
    uint sample_idx = gl_GlobalInvocationID.x; // x-dimension → sample
    uint bin_idx    = gl_GlobalInvocationID.y; // y-dimension → bin

    uint num_samples = push_constant.arg_1;
    uint bins        = push_constant.bin_capacity;

    if(sample_idx >= num_samples || bin_idx >= bins) return;

    // Compute linear index into buffers
    uint off = bin_idx * num_samples + sample_idx;

    float mag = magnitude_temp_buffer.data[off];
    float trans = transient_temp_buffer.data[off];

float val = mag + 0.5 * trans; // boost sudden onsets
val = clamp(val, 0.0, 1.0);
imageStore(image_temp, ivec2(sample_idx, bin_idx), vec4(val,val,val,1.0));
}
