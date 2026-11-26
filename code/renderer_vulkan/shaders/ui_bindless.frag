#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform texture2D uTextures[4096];
layout(set = 0, binding = 1) uniform sampler uSamplers[4];

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec2 frag_tex_coord;
layout(location = 2) flat in uint frag_texture;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 texel = texture(nonuniformEXT(sampler2D(uTextures[nonuniformEXT(frag_texture & 4095u)], uSamplers[nonuniformEXT(frag_texture >> 12)])), frag_tex_coord);
    out_color = texel * frag_color;
}
