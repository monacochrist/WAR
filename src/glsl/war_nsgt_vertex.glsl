//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_nsgt_vertex.glsl
//-----------------------------------------------------------------------------

#version 450

// Vertex outputs
layout(location = 0) out vec2 frag_uv;

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

void main() {
    // Fullscreen quad (two triangles)
    vec2 positions[6] = vec2[6](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0, -1.0),
        vec2( 1.0,  1.0)
    );

    vec2 uvs[6] = vec2[6](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0)
    );

    vec2 pos = positions[gl_VertexIndex];
    
    gl_Position = vec4(pos, push_constant.z_layer, 1.0);

    frag_uv = uvs[gl_VertexIndex];
}
