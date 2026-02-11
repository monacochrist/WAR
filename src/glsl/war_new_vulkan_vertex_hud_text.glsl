//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_new_vulkan_vertex_hud_text.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec2 in_pos;               // quad [0..1]

layout(location = 1) in vec3 in_instance_pos;      // cell position (x,y,z)
layout(location = 2) in vec2 in_instance_size;     // cell allocation (in cells)
layout(location = 3) in vec4 in_instance_uv;       // {u0, v0, u1, v1}
layout(location = 4) in vec2 in_instance_glyph_scale; // glyph norm size
layout(location = 5) in float in_instance_baseline;   // glyph baseline (norm)
layout(location = 6) in float in_instance_ascent;
layout(location = 7) in float in_instance_descent;
layout(location = 8) in vec4 in_color;

layout(location = 12) in uint in_flags;

layout(push_constant) uniform PushConstants {
    vec2  cell_size;
    vec2  panning;
    float zoom;
    vec2  screen_size;
    vec2  cell_offset;
} pc;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;
flat layout(location = 2) out uint frag_flags;

// Full main for per-glyph alignment
void main() {
    frag_flags = in_flags;
    vec2 cellSizePx = in_instance_size * pc.cell_size * pc.zoom;
    vec2 glyphSizePx = cellSizePx * in_instance_glyph_scale;

    // Horizontal center
    float horizontalOffsetPx = (cellSizePx.x - glyphSizePx.x) * 0.5;

    // Vertical offset: per-glyph ascent/descent
    float verticalOffsetPx = (in_instance_ascent) * pc.cell_size.y * pc.zoom
                             - glyphSizePx.y;

    // You can tweak slightly
    float verticalTweak = 15.0;

    vec2 localPosPx = vec2(
        in_pos.x * glyphSizePx.x + horizontalOffsetPx,
        in_pos.y * glyphSizePx.y + verticalOffsetPx + verticalTweak
    );

    vec2 instancePosPx = (in_instance_pos.xy + pc.cell_offset) * pc.cell_size * pc.zoom;
    vec2 finalPos = localPosPx + instancePosPx + pc.panning * pc.cell_size;

    vec2 ndc = (finalPos / pc.screen_size) * 2.0 - 1.0;
    ndc.y = -ndc.y;

    gl_Position = vec4(ndc, in_instance_pos.z, 1.0);

    frag_uv.x = mix(in_instance_uv.x, in_instance_uv.z, in_pos.x);
    frag_uv.y = mix(in_instance_uv.w, in_instance_uv.y, in_pos.y);

    frag_color = in_color;
}
