//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_new_vulkan_fragment_text.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D u_font_atlas;

flat layout(location = 2) in uint frag_flags;

void main() {
    if ((frag_flags & 1u) != 0u) discard;
    float coverage = texture(u_font_atlas, frag_uv).r;
    if (coverage < 0.5) discard;
    out_color = vec4(frag_color.rgb, frag_color.a);
}
