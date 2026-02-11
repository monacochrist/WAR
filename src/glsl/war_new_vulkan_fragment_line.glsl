//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/glsl/war_new_vulkan_fragment_line.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec4 frag_color;
layout(location = 0) out vec4 out_color;

flat layout(location = 1) in uint frag_flags;

void main() {
    if ((frag_flags & 1u) != 0u) discard;
    out_color = frag_color;
}
