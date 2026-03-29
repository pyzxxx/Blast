#version 450

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;

layout(set = 0, binding = 0) uniform PerFrameBuffer
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 inverseView;
    mat4 inverseProjection;
    vec4 cameraPosition;
    vec4 pad[3];
} perFrame;

struct GpuSceneData
{
    mat4 transform;
    mat4 pad0;
    mat4 pad1;
    mat4 pad2;
};

layout(set = 0, binding = 1) readonly buffer GpuSceneBuffer
{
    GpuSceneData data[];
} gpuScene;

layout(push_constant) uniform PushConstants
{
    uint sceneIndex;
    uint materialIndex;
} pc;

void main()
{
    mat4 modelMatrix = gpuScene.data[pc.sceneIndex].transform;
    v_normal = mat3(transpose(inverse(modelMatrix))) * a_normal;
    v_uv = a_uv;
    
    vec4 worldPos = modelMatrix * vec4(a_position, 1.0);
    gl_Position = perFrame.projection * perFrame.view * worldPos;
}