//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_new_vulkan_vertex_hud_line.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec2 in_pos;              // vertex of quad [0,1]

layout(location = 1) in vec3 in_instance_pos;    // starting point
layout(location = 2) in vec2 in_instance_size;   // span in X/Y
layout(location = 3) in float in_width;          // thickness
layout(location = 4) in vec4 in_color;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec2 cell_size;
    layout(offset = 8) vec2 panning;
    layout(offset = 16) float zoom;
    layout(offset = 24) vec2 screen_size;
    layout(offset = 32) vec2 cell_offset;
} pc;

layout(location = 0) out vec4 frag_color;

void main() {
    vec2 quadSizePx = in_instance_size * pc.cell_size * pc.zoom;
    vec2 perpSizePx; // perpendicular thickness

    // Determine orientation and thickness
    if (in_instance_size.x == 0.0) {
        // vertical line
        perpSizePx = vec2(in_width * pc.cell_size.x * pc.zoom, 0.0);
        quadSizePx.x = perpSizePx.x; // width along X
        // height along Y = span, no change
    } else if (in_instance_size.y == 0.0) {
        // horizontal line
        perpSizePx = vec2(0.0, in_width * pc.cell_size.y * pc.zoom);
        quadSizePx.y = perpSizePx.y; // width along Y
        // length along X = span, no change
    }

    // Local quad vertex
    // in_pos ranges from [0,1]
    // shift perpendicular axis to center, main axis starts at 0
    vec2 localPosPx = in_pos * quadSizePx;
    if (in_instance_size.x == 0.0) {
        localPosPx.x -= 0.5 * quadSizePx.x; // vertical line: center X
    } else if (in_instance_size.y == 0.0) {
        localPosPx.y -= 0.5 * quadSizePx.y; // horizontal line: center Y
    }

    // Instance position with cell offset
    vec2 instancePosPx = (in_instance_pos.xy + pc.cell_offset) * pc.cell_size * pc.zoom;

    vec2 finalPos = localPosPx + instancePosPx + pc.panning * pc.cell_size;

    // Convert to NDC
    vec2 ndc = (finalPos / pc.screen_size) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, in_instance_pos.z, 1.0);
    frag_color = in_color;
}
