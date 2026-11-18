#version 450

// 128 bytes: mvp, then fog state (tr_fog.h). Trimmed from vkQuake3's original (GPLv2-or-later), dropping the clip-plane/mirror support this renderer doesn't use.
layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;     // xyz = camera world position
    vec4 fogColor;   // rgb = fog color
    vec4 fogParams;  // x = start (linear) / density (exp), y = end (linear only), z = mode (0=linear,1=exp), w = enabled
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord;
layout(location = 2) out float frag_fogFactor;

// Off for the 2D UI pipelines, which share this shader but must never be fogged.
layout (constant_id = 3) const int fog_enabled = 0;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    frag_color = in_color;
    frag_tex_coord = in_tex_coord;

    if (fog_enabled != 0 && fogParams.w != 0.0) {
        float dist = distance(in_position, camPos.xyz);
        frag_fogFactor = (fogParams.z != 0.0)
            ? clamp(exp(-fogParams.x * dist), 0.0, 1.0)
            : clamp((fogParams.y - dist) / max(fogParams.y - fogParams.x, 1.0), 0.0, 1.0);
    } else {
        frag_fogFactor = 1.0;
    }
}
