#version 450

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 o_fragColor;

layout(binding = 0) uniform sampler2D u_inputTexture;

void main()
{
    o_fragColor = texture(u_inputTexture, v_uv);
}