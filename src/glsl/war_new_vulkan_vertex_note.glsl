//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_new_vulkan_vertex_note.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec2 in_pos;

layout(location = 1) in vec3 in_instance_pos;
layout(location = 2) in vec2 in_instance_size;
layout(location = 3) in vec4 in_color;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec2 cell_size;
    layout(offset = 8) vec2 panning;
    layout(offset = 16) float zoom;
    layout(offset = 24) vec2 screen_size;
    layout(offset = 32) vec2 cell_offset;
} pc;

layout(location = 0) out vec4 frag_color;

void main() {
    // Quad size in pixels
    vec2 quadSizePx =
        in_instance_size * pc.cell_size * pc.zoom;

    // Quad local vertex position (still pixels)
    vec2 localPosPx =
        in_pos * quadSizePx;

    // Instance position in pixels
    vec2 instancePosPx =
        (in_instance_pos.xy + pc.cell_offset) * pc.cell_size * pc.zoom;

    vec2 finalPos =
        localPosPx + instancePosPx + pc.panning * pc.cell_size;

    // Pixels → NDC
    vec2 ndc = (finalPos / pc.screen_size) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, in_instance_pos.z, 1.0);
    frag_color = in_color;
}
