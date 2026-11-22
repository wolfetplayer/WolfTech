#version 450

// Multitex twin of entity_lerp.vert, for the collapsed opaque-stage0+modulate/add-stage1 path (vk_getPipelineEntityLerpMultiTex); pairs with multi_texture.frag unchanged. Same push-constant bytes as multi_texture.vert plus one extra field (lerpSlot) at the same offset entity_lerp.vert repurposes clipPlane for -- multi_texture.frag never reads that far into the block, so it's unaffected. camPos.w/fogColor.a/lerpSlot.yz carry CGEN_LIGHTING_DIFFUSE's packed ambient/directed/light-direction, same as entity_lerp.vert -- see vk_entity.c's BuildLerpPushConstants/ComputeEntityLighting.
layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;     // xyz = camera position in this entity's local space -- see entity_lerp.vert; w = packed ambient color (unorm4x8 bits)
    vec4 fogColor;   // a = packed directed color (unorm4x8 bits)
    vec4 fogParams;
    vec4 lerpSlot;   // x = backlerp; y/z = light direction as (lat, long) radians, entity-local space; w unused
};

layout (constant_id = 2) const int lighting_diffuse = 0;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_position2;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_tex_coord0;
layout(location = 4) in vec2 in_tex_coord1;
layout(location = 5) in vec3 in_normal; // current frame only -- see entity_lerp.vert

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

    if (lighting_diffuse != 0) {
        vec3 ambient = unpackUnorm4x8(floatBitsToUint(camPos.w)).rgb * 255.0;
        vec3 directed = unpackUnorm4x8(floatBitsToUint(fogColor.a)).rgb * 255.0;
        vec3 lightDir = vec3(cos(lerpSlot.y) * sin(lerpSlot.z), sin(lerpSlot.y) * sin(lerpSlot.z), cos(lerpSlot.z));
        float incoming = max(dot(normalize(in_normal), lightDir), 0.0);
        vec3 diffuse = clamp(ambient + incoming * directed, 0.0, 255.0) * (1.0 / 255.0);
        frag_color.rgb *= diffuse;
    }

    if (fogParams.w != 0.0) {
        float dist = distance(position, camPos.xyz);
        frag_fogFactor = (fogParams.z != 0.0)
            ? clamp(exp(-fogParams.x * dist), 0.0, 1.0)
            : clamp((fogParams.y - dist) / max(fogParams.y - fogParams.x, 1.0), 0.0, 1.0);
    } else {
        frag_fogFactor = 1.0;
    }
}
