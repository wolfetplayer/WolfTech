#version 450

// clip_plane is the fragment-discard fallback for mirror/portal world draws -- see world_lightmap.vert.
layout(push_constant) uniform Transform {
    mat4 mvp;
    vec4 camPos;
    vec4 fogColor;
    vec4 fogParams;
    vec4 clipPlane;
};

layout(set = 0, binding = 0) uniform sampler2D diffuseMap;
layout(set = 0, binding = 1) uniform sampler2D lightMap;

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_tex_coord;
layout(location = 2) in vec2 frag_lightmap_coord;
layout(location = 3) in float frag_fogFactor;
layout(location = 4) in float frag_clip_dist;

layout(location = 0) out vec4 out_color;

layout (constant_id = 0) const int clip_plane = 0;

void main() {
    if (clip_plane != 0 && frag_clip_dist < 0.0) discard;

    vec4 diffuse = frag_color * texture(diffuseMap, frag_tex_coord);
    vec3 light = texture(lightMap, frag_lightmap_coord).rgb;
    vec3 lit = diffuse.rgb * light;
    out_color = vec4(mix(fogColor.rgb, lit, frag_fogFactor), diffuse.a);
}
