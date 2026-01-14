//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/shaders/war_quad_fragment.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec4 frag_color;
layout(location = 1) in float frag_outline_thickness;
layout(location = 2) in vec4 frag_outline_color;
layout(location = 3) in vec2 frag_uv;
layout(location = 4) in vec2 frag_span;

layout(location = 0) out vec4 out_color;

void main() {
    vec2 scaled_pos = frag_uv * frag_span; // position in world units
    float dist_to_left   = scaled_pos.x;
    float dist_to_right  = frag_span.x - scaled_pos.x;
    float dist_to_bottom = scaled_pos.y;
    float dist_to_top    = frag_span.y - scaled_pos.y;
    
    float dist_to_edge = min(min(dist_to_left, dist_to_right),
                             min(dist_to_bottom, dist_to_top));
    
    if (dist_to_edge < frag_outline_thickness) {
        out_color = frag_outline_color;
    } else {
        out_color = frag_color;
    }
}
