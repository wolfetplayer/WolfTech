#version 450

layout(push_constant) uniform PushConstants {
	vec2 scale;
	vec2 offset;
} pc;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in uint in_texture;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord;
layout(location = 2) flat out uint frag_texture;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = vec4(in_position.xy * pc.scale + pc.offset, 0.0, 1.0);
    frag_color = in_color;
    frag_tex_coord = in_tex_coord;
    frag_texture = in_texture;
}
