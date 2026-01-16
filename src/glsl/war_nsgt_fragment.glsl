//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_nsgt_fragment.glsl
//-----------------------------------------------------------------------------

#version 450

layout(binding = 16) uniform sampler2D image; // single-channel red (magnitude)

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
    layout(offset = 72) int frame_offset;
    layout(offset = 76) int frame_count;
    layout(offset = 80) int frame_filled;
} push_constant;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

void main() {
    float fc = float(max(push_constant.frame_capacity, 1));
    int filled_i = push_constant.frame_filled;
    if (filled_i <= 0) {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Direct mapping: show only written frames, no stretch
    float x = frag_uv.x * fc; // frame index
    if (filled_i < push_constant.frame_capacity && x >= float(filled_i)) {
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float u = frag_uv.x;

    vec2 uv = vec2(u, frag_uv.y);
    vec4 tex = texture(image, uv);
    out_color = vec4(tex.r, 0.0, 0.0, 1.0);
}
