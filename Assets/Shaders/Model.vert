#version 450
#extension GL_GOOGLE_include_directive : require

#include "ShaderInterop.h"

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec3 a_tangent;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec3 v_worldPos;
layout(location = 3) out vec3 v_tangent;
layout(location = 4) out vec3 v_bitangent;
layout(location = 5) out vec3 v_viewPos;

layout(set = 0, binding = 0) uniform PerFrameBlock
{
    PerFrame data;
} u_perFrame;

layout(set = 0, binding = 1) readonly buffer GpuScene
{
    GpuSceneData data[];
} s_gpuScene;

layout(push_constant) uniform PC
{
    MeshPushConstants pc;
};

void main()
{
    mat4 modelMatrix = s_gpuScene.data[pc.sceneIndex].transform;
    mat3 normalMatrix = mat3(transpose(inverse(modelMatrix)));

    vec4 worldPos = modelMatrix * vec4(a_position, 1.0);
    v_worldPos = worldPos.xyz;

    vec4 viewPos = u_perFrame.data.view * worldPos;
    v_viewPos = viewPos.xyz;

    v_normal = normalize(normalMatrix * a_normal);
    v_tangent = normalize(normalMatrix * a_tangent);
    v_bitangent = cross(v_normal, v_tangent);

    v_uv = a_uv;

    gl_Position = u_perFrame.data.projection * viewPos;
}
