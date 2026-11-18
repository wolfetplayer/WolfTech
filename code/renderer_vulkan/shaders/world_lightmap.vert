#version 450

// 128 bytes to match vk.lightmap_pipeline_layout's push constant range; only the leading mat4 is used.
layout(push_constant) uniform Transform {
    mat4 mvp;
};

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec2 in_lightmap_coord;

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 frag_tex_coord;
layout(location = 2) out vec2 frag_lightmap_coord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    frag_color = in_color;
    frag_tex_coord = in_tex_coord;
    frag_lightmap_coord = in_lightmap_coord;
}
