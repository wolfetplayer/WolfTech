#version 450

// 128 bytes to match vk.lightmap_pipeline_layout's push constant range: mvp, then the fog state (see tr_fog.h).
layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;     // xyz = camera world position
    vec4 fogColor;   // rgb = fog color
    vec4 fogParams;  // x = start (linear) / density (exp), y = end (linear only), z = mode (0=linear,1=exp), w = enabled
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coord0;
layout(location = 3) in vec2 in_tex_coord1;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 2) out vec2 frag_tex_coord1;
layout(location = 3) out float frag_fogFactor;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    frag_color = in_color;
    frag_tex_coord0 = in_tex_coord0;
    frag_tex_coord1 = in_tex_coord1;

    if (fogParams.w != 0.0) {
        float dist = distance(in_position, camPos.xyz);
        frag_fogFactor = (fogParams.z != 0.0)
            ? clamp(exp(-fogParams.x * dist), 0.0, 1.0)
            : clamp((fogParams.y - dist) / max(fogParams.y - fogParams.x, 1.0), 0.0, 1.0);
    } else {
        frag_fogFactor = 1.0;
    }
}
