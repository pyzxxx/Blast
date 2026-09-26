#version 460
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"

struct CompositeConsts {
    uint src_slot;
    float exposure;
    uint pad[2];
};

layout(buffer_reference, std430) readonly buffer CompositeConstsRef { CompositeConsts value; };

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    CompositeConsts pc = CompositeConstsRef(push.data.pass_addr).value;
    vec3 c = sample_tex(pc.src_slot, v_uv).rgb;
    c = aces(c * pc.exposure);
    c = pow(c, vec3(1.0 / 2.2));
    out_color = vec4(c, 1.0);
}
