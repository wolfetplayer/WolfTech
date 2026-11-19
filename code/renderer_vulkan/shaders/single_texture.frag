#version 450

// Adapted from vkQuake3's original (GPLv2-or-later). clip_plane is the fragment-discard fallback for mirror/portal world draws -- see single_texture.vert.
layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;
    vec4 fogColor;
    vec4 fogParams;
    vec4 clipPlane;
};

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_tex_coord;
layout(location = 2) in float frag_fogFactor;
layout(location = 3) in float frag_clip_dist;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int alpha_test_func = 0;
layout (constant_id = 2) const int clip_plane = 0;

void main() {
    if (clip_plane != 0 && frag_clip_dist < 0.0) discard;

    out_color = frag_color * texture(texture0, frag_tex_coord);

    if (alpha_test_func == 1) {
        if (out_color.a == 0.0f) discard;
    } else if (alpha_test_func == 2) {
        if (out_color.a >= 0.5f) discard;
    } else if (alpha_test_func == 3) {
        if (out_color.a < 0.5f) discard;
    }

    out_color.rgb = mix(fogColor.rgb, out_color.rgb, frag_fogFactor);
}
