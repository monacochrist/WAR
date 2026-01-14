//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/shaders/war_text_fragment.glsl
//-----------------------------------------------------------------------------

#version 450

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_uv;
layout(location = 2) in float frag_thickness;
layout(location = 3) in float frag_feather;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D sdf_sampler;

void main() {
    float distance = texture(sdf_sampler, frag_uv).r;

    float alpha = smoothstep(0.5 - frag_thickness - frag_feather,
                             0.5 - frag_thickness + frag_feather,
                             distance);

    out_color = vec4(frag_color.rgb, frag_color.a * alpha);

    if (alpha <= 0.01) {
        discard;
    }
}
