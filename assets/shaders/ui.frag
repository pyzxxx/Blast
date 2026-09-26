#version 460
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"

layout(buffer_reference, std430) readonly buffer UiConstsRef { UiConsts value; };

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_col;
layout(location = 0) out vec4 out_color;

void main() {
    UiConsts pc = UiConstsRef(push.data.pass_addr).value;
    out_color = v_col * sample_tex(pc.tex_slot, v_uv);
}
