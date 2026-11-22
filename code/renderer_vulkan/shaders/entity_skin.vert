#version 450

// GPU bone skinning for MDS/MDM entities (vk_entity.c's static per-model buffers, see vkEntitySkinnedVert_t). Pairs with single_texture.frag unchanged, reusing its 128-byte push-constant layout as-is: camPos is repurposed to the entity-local-space camera position (same trick as entity_lerp.vert), clipPlane stays unused/zero (no backlerp needed here).
layout(push_constant) uniform Transform {
    mat4 mvp;        // combined camera view-projection * entity model matrix
    vec4 camPos;     // xyz = camera position in this entity's local space
    vec4 fogColor;
    vec4 fogParams;
    vec4 clipPlane;  // unused
};

layout(set = 1, binding = 0) uniform BoneMatrices {
    mat4 u_BoneMatrix[128];
};

// Binding 0: the model's static bind-pose buffer -- each weight slot stores the vertex's position in that bone's own bind-pose-relative local frame (not a single shared bind-pose position an IQM-style blended matrix could transform), so each of the 4 slots needs its own full matrix-vector transform, weighted-summed; see vkEntitySkinnedVert_t. Binding 1: this frame's dynamic color/uv.
layout(location = 0) in vec3 in_offset0;
layout(location = 1) in vec3 in_offset1;
layout(location = 2) in vec3 in_offset2;
layout(location = 3) in vec3 in_offset3;
layout(location = 4) in uvec4 in_boneIndex;
layout(location = 5) in vec4 in_boneWeight;
layout(location = 6) in vec4 in_color;
layout(location = 7) in vec2 in_tex_coord;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord;
layout(location = 2) out float frag_fogFactor;
layout(location = 3) out float frag_clip_dist;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    mat4 m0 = u_BoneMatrix[in_boneIndex.x];
    mat4 m1 = u_BoneMatrix[in_boneIndex.y];
    mat4 m2 = u_BoneMatrix[in_boneIndex.z];
    mat4 m3 = u_BoneMatrix[in_boneIndex.w];

    vec3 position =
        in_boneWeight.x * (mat3(m0) * in_offset0 + m0[3].xyz) +
        in_boneWeight.y * (mat3(m1) * in_offset1 + m1[3].xyz) +
        in_boneWeight.z * (mat3(m2) * in_offset2 + m2[3].xyz) +
        in_boneWeight.w * (mat3(m3) * in_offset3 + m3[3].xyz);

    gl_Position = mvp * vec4(position, 1.0);
    frag_color = in_color;
    frag_tex_coord = in_tex_coord;

    if (fogParams.w != 0.0) {
        float dist = distance(position, camPos.xyz);
        frag_fogFactor = (fogParams.z != 0.0)
            ? clamp(exp(-fogParams.x * dist), 0.0, 1.0)
            : clamp((fogParams.y - dist) / max(fogParams.y - fogParams.x, 1.0), 0.0, 1.0);
    } else {
        frag_fogFactor = 1.0;
    }

    frag_clip_dist = 1.0;
}
