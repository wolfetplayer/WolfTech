#version 450

// GPU vertex-lerp blending for MD3/MDC entities (vk_entity.c's static per-model buffers, see vkEntityFrameVert_t); pairs with single_texture.frag unchanged, reusing its 128-byte push-constant layout as-is: camPos is repurposed to the entity-local-space camera position (computed per entity on the CPU, exact for a rigid axis/origin transform), and clipPlane.x carries backlerp (entities never clip, so that slot is otherwise always zero) -- see vk_entity.c's BuildLerpPushConstants.
layout(push_constant) uniform Transform {
    mat4 mvp;        // combined camera view-projection * entity model matrix
    vec4 camPos;     // xyz = camera position in this entity's local space
    vec4 fogColor;
    vec4 fogParams;
    vec4 clipPlane;  // x = backlerp (0 = at frame, 1 = at oldframe); yzw unused
};

// Binding 0: current frame's position (vkEntityFrameVert_t). Binding 1: same static buffer, old frame's position (bound at a different byte offset). Binding 2: this frame's dynamic color/uv.
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_position2;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_tex_coord;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord;
layout(location = 2) out float frag_fogFactor;
layout(location = 3) out float frag_clip_dist;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec3 position = mix(in_position, in_position2, clipPlane.x);

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
