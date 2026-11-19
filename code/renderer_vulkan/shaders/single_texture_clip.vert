#version 450

// Hardware-clip-distance twin of single_texture.vert (vk.features.shaderClipDistance): gl_ClipDistance needs its own SPIR-V module, unconditionally. Ported from vkQuake3 (GPLv2-or-later).
layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;
    vec4 fogColor;
    vec4 fogParams;
    vec4 clipPlane;  // xyz = world-space plane normal, w = dist
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coord;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord;
layout(location = 2) out float frag_fogFactor;
layout(location = 3) out float frag_clip_dist; // unused here, just keeps the vs/fs interface matched with single_texture.vert

layout (constant_id = 3) const int fog_enabled = 0;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_ClipDistance[1];
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

    frag_clip_dist = 1.0;
    gl_ClipDistance[0] = dot(in_position, clipPlane.xyz) - clipPlane.w;
}
