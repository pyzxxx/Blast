#version 450
#extension GL_GOOGLE_include_directive : require

#include "ShaderInterop.h"

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 o_fragColor;

layout(binding = 0) uniform sampler2D u_opaqueColor;
layout(binding = 1) uniform sampler2D u_alphaColor;
layout(binding = 2) uniform PerFrameBlock
{
    PerFrame data;
} u_perFrame;

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec4 opaque = texture(u_opaqueColor, v_uv);
    vec4 alpha = texture(u_alphaColor, v_uv);
    vec3 color = mix(opaque.rgb, alpha.rgb, alpha.a);

    color *= u_perFrame.data.exposure;
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    o_fragColor = vec4(color, 1.0);
}
