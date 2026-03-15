#version 450

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;
layout(location = 0) out vec4 o_fragColor;

layout(binding = 1) uniform sampler2D u_texture;

void main()
{
    o_fragColor = v_color * texture(u_texture, v_uv);
}
