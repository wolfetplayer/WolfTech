#version 450

layout(set = 0, binding = 0) uniform sampler2D diffuseMap;
layout(set = 0, binding = 1) uniform sampler2D lightMap;

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_tex_coord;
layout(location = 2) in vec2 frag_lightmap_coord;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 diffuse = frag_color * texture(diffuseMap, frag_tex_coord);
    vec3 light = texture(lightMap, frag_lightmap_coord).rgb;
    out_color = vec4(diffuse.rgb * light, diffuse.a);
}
