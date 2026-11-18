#version 450

// Trimmed from vkQuake3's original (GPLv2-or-later): dropped unused clip-plane/mirror support, added the fog blend (frag_fogFactor is 1.0, a no-op mix, when fog_enabled was off in the vert shader).
layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;
    vec4 fogColor;
    vec4 fogParams;
};

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_tex_coord;
layout(location = 2) in float frag_fogFactor;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int alpha_test_func = 0;

void main() {
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
