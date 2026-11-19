#version 450

// Collapses an opaque stage 0 + a modulate/add-blended stage 1 into one pass -- see vk_pipelines.c's vk_getPipelineMultiTexture.
layout(set = 0, binding = 0) uniform sampler2D texture0;
layout(set = 0, binding = 1) uniform sampler2D texture1;

layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;
    vec4 fogColor;
    vec4 fogParams;
};

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_tex_coord0;
layout(location = 2) in vec2 frag_tex_coord1;
layout(location = 3) in float frag_fogFactor;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int combine_mode = 0;
layout (constant_id = 1) const int alpha_test_func = 0;

void main() {
    vec4 base = frag_color * texture(texture0, frag_tex_coord0);
    vec4 over = texture(texture1, frag_tex_coord1);

    if (alpha_test_func == 1) {
        if (base.a == 0.0f) discard;
    } else if (alpha_test_func == 2) {
        if (base.a >= 0.5f) discard;
    } else if (alpha_test_func == 3) {
        if (base.a < 0.5f) discard;
    }

    if (combine_mode == 1) {
        out_color = vec4(base.rgb + over.rgb, base.a);
    } else {
        out_color = vec4(base.rgb * over.rgb, base.a);
    }

    out_color.rgb = mix(fogColor.rgb, out_color.rgb, frag_fogFactor);
}
