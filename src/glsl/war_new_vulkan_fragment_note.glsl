#version 450

layout(location = 0) in vec4 frag_color;
layout(location = 0) out vec4 out_color;

flat layout(location = 1) in uint frag_flags;
layout(location = 2) in vec2 frag_uv;
flat layout(location = 3) in vec4 frag_outline_color;

void main() {
    if ((frag_flags & 1u) != 0u) discard;
    vec2 edge = min(frag_uv, 1.0 - frag_uv);
    float d = min(edge.x, edge.y);
    float threshold = 0.04;
    out_color = d < threshold ? frag_outline_color : frag_color;
}
