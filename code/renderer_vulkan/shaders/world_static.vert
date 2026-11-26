#version 450

layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;
    vec4 fogColor;
    vec4 fogParams;
    vec4 clipPlane;
    vec4 colorMul;
    vec4 colorAdd;
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec2 in_lightmap_coord;
layout(location = 4) in vec4 in_normal;
layout(location = 5) in vec4 in_tangent;
layout(location = 6) in vec4 in_lightdir;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord;
layout(location = 2) out vec2 frag_lightmap_coord;
layout(location = 3) out float frag_fogFactor;
layout(location = 4) out float frag_clip_dist;

layout (constant_id = 0) const int clip_plane = 0;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    frag_color = in_color * colorMul + colorAdd;
    frag_tex_coord = in_tex_coord;
    frag_lightmap_coord = in_lightmap_coord;

    if (fogParams.w != 0.0) {
        float dist = distance(in_position, camPos.xyz);
        frag_fogFactor = (fogParams.z != 0.0)
            ? clamp(exp(-fogParams.x * dist), 0.0, 1.0)
            : clamp((fogParams.y - dist) / max(fogParams.y - fogParams.x, 1.0), 0.0, 1.0);
    } else {
        frag_fogFactor = 1.0;
    }

    frag_clip_dist = (clip_plane != 0) ? (dot(in_position, clipPlane.xyz) - clipPlane.w) : 1.0;
}
