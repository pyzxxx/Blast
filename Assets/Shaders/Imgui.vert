#version 450

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_color;
layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;

layout(push_constant) uniform PushConstants
{
    mat4 projMatrix;
} pc;

void main()
{
    v_uv = a_uv;
    v_color = a_color;
    gl_Position = pc.projMatrix * vec4(a_position, 0.0, 1.0);
}
