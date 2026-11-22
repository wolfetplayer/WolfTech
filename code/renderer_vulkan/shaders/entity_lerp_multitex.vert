#version 450

// Multitex twin of entity_lerp.vert, for the collapsed opaque-stage0+modulate/add-stage1 path (vk_getPipelineEntityLerpMultiTex); pairs with multi_texture.frag unchanged. Same push-constant bytes as multi_texture.vert plus one extra field (lerpSlot) at the same offset entity_lerp.vert repurposes clipPlane for -- multi_texture.frag never reads that far into the block, so it's unaffected.
layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;     // xyz = camera position in this entity's local space -- see entity_lerp.vert
    vec4 fogColor;
    vec4 fogParams;
    vec4 lerpSlot;   // x = backlerp; yzw unused
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_position2;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_tex_coord0;
layout(location = 4) in vec2 in_tex_coord1;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 2) out vec2 frag_tex_coord1;
layout(location = 3) out float frag_fogFactor;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec3 position = mix(in_position, in_position2, lerpSlot.x);

    gl_Position = mvp * vec4(position, 1.0);
    frag_color = in_color;
    frag_tex_coord0 = in_tex_coord0;
    frag_tex_coord1 = in_tex_coord1;

    if (fogParams.w != 0.0) {
        float dist = distance(position, camPos.xyz);
        frag_fogFactor = (fogParams.z != 0.0)
            ? clamp(exp(-fogParams.x * dist), 0.0, 1.0)
            : clamp((fogParams.y - dist) / max(fogParams.y - fogParams.x, 1.0), 0.0, 1.0);
    } else {
        frag_fogFactor = 1.0;
    }
}
