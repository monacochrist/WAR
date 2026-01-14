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
} push_constant;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 tex = texture(image, frag_uv);
    out_color = vec4(tex.r, tex.g, 0.0, 1.0); // both channels in red
}
