#version 460
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"

layout(buffer_reference, std430) readonly buffer UiConstsRef { UiConsts value; };

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_col;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_col;

void main() {
    UiConsts pc = UiConstsRef(push.data.pass_addr).value;
    v_uv = a_uv;
    v_col = a_col;
    gl_Position = vec4(a_pos * pc.scale_translate.xy + pc.scale_translate.zw, 0.0, 1.0);
}
