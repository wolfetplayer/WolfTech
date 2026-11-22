#version 450

// Multitex twin of entity_skin.vert, for the collapsed opaque-stage0+modulate/add-stage1 path (vk_getPipelineEntitySkinMultiTex); pairs with multi_texture.frag unchanged, same push-constant bytes as multi_texture.vert (camPos.xyz repurposed to the entity-local-space camera, same trick as entity_skin.vert's; camPos.w/fogColor.a/clipPlane.yz carry CGEN_LIGHTING_DIFFUSE's packed ambient/directed/light-direction).
layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;     // xyz = camera position in this entity's local space; w = packed ambient color (unorm4x8 bits)
    vec4 fogColor;   // a = packed directed color (unorm4x8 bits)
    vec4 fogParams;
    vec4 clipPlane;  // y/z = light direction as (lat, long) radians, entity-local space; x/w unused
};

layout (constant_id = 2) const int lighting_diffuse = 0;

layout(set = 1, binding = 0) uniform BoneMatrices {
    mat4 u_BoneMatrix[128];
};

layout(location = 0) in vec3 in_offset0;
layout(location = 1) in vec3 in_offset1;
layout(location = 2) in vec3 in_offset2;
layout(location = 3) in vec3 in_offset3;
layout(location = 4) in uvec4 in_boneIndex;
layout(location = 5) in vec4 in_boneWeight;
layout(location = 6) in vec3 in_normal; // bind-pose normal, skinned below the same weighted way as the offsets
layout(location = 7) in vec4 in_color;
layout(location = 8) in vec2 in_tex_coord0;
layout(location = 9) in vec2 in_tex_coord1;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 2) out vec2 frag_tex_coord1;
layout(location = 3) out float frag_fogFactor;

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
    frag_tex_coord0 = in_tex_coord0;
    frag_tex_coord1 = in_tex_coord1;

    if (lighting_diffuse != 0) {
        vec3 normal = normalize(
            in_boneWeight.x * (mat3(m0) * in_normal) +
            in_boneWeight.y * (mat3(m1) * in_normal) +
            in_boneWeight.z * (mat3(m2) * in_normal) +
            in_boneWeight.w * (mat3(m3) * in_normal));
        vec3 ambient = unpackUnorm4x8(floatBitsToUint(camPos.w)).rgb * 255.0;
        vec3 directed = unpackUnorm4x8(floatBitsToUint(fogColor.a)).rgb * 255.0;
        vec3 lightDir = vec3(cos(clipPlane.y) * sin(clipPlane.z), sin(clipPlane.y) * sin(clipPlane.z), cos(clipPlane.z));
        float incoming = max(dot(normal, lightDir), 0.0);
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
