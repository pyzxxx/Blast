#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#include "ShaderInterop.h"
#include "RayTracingInterop.h"

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer BVHNodeBuffer {
    GpuBVHNode data[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer TriangleBuffer {
    GPUTriangle data[];
};

layout(binding = 0) uniform PerFrameBlock
{
    PerFrame data;
} u_perFrame;

layout(binding = 8) readonly buffer TLASNodes
{
    TLASNode data[];
} s_tlasNodes;

layout(binding = 9) readonly buffer TLASInstances
{
    TLASInstance data[];
} s_tlasInstances;

#define g_tlasNodes s_tlasNodes.data
#define g_tlasInstances s_tlasInstances.data

#include "BVHTrace.glsl"

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_fragColor;

vec3 ComputeRayDirFromUV(vec2 uv, mat4 invProj, mat4 invView)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    vec4 viewPos = invProj * clip;
    vec3 viewDir = viewPos.xyz / viewPos.w;
    return normalize((invView * vec4(viewDir, 0.0)).xyz);
}

void main()
{
    vec3 rayOrigin = u_perFrame.data.cameraPosition.xyz;
    vec2 uv = v_uv;
    vec3 rayDir = ComputeRayDirFromUV(uv, u_perFrame.data.inverseProjection, u_perFrame.data.inverseView);

    RayHit hit = TraceTLAS(rayOrigin, rayDir);
    if (hit.valid)
    {
        o_fragColor = vec4(hit.normal * 0.5 + 0.5, 1.0);
    }
    else
    {
        o_fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
