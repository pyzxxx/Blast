#ifndef SHADER_INTEROP_H
#define SHADER_INTEROP_H

#ifdef __cplusplus
#include <glm/glm.hpp>
#include <cstdint>

using sh_vec2 = glm::vec2;
using sh_vec3 = glm::vec3;
using sh_vec4 = glm::vec4;
using sh_uvec2 = glm::uvec2;
using sh_uvec3 = glm::uvec3;
using sh_uvec4 = glm::uvec4;
using sh_mat4 = glm::mat4;
using sh_uint = uint32_t;
using sh_float = float;
using sh_bool = uint32_t;
#else
#define sh_vec2 vec2
#define sh_vec3 vec3
#define sh_vec4 vec4
#define sh_uvec2 uvec2
#define sh_uvec3 uvec3
#define sh_uvec4 uvec4
#define sh_mat4 mat4
#define sh_uint uint
#define sh_float float
#define sh_bool bool
#endif

struct PerFrame
{
    sh_mat4 view;
    sh_mat4 projection;
    sh_mat4 viewProjection;
    sh_mat4 inverseView;
    sh_mat4 inverseProjection;
    sh_vec4 cameraPosition;
    sh_uvec4 lightCount;
    sh_uvec4 clusterSize;
    sh_uvec2 screenSize;
    sh_vec2 zNearFar;
    sh_float exposure;
    sh_float pad0;
    sh_vec2 pad1;
};

struct GpuSceneData
{
    sh_mat4 transform;
    sh_mat4 pad0;
    sh_mat4 pad1;
    sh_mat4 pad2;
};

struct MaterialData
{
    sh_uint albedoTexIndex;
    sh_uint normalTexIndex;
    sh_uint roughnessMetallicTexIndex;
    sh_uint emissiveTexIndex;
    sh_uint albedoSamplerIndex;
    sh_uint normalSamplerIndex;
    sh_uint roughnessMetallicSamplerIndex;
    sh_uint emissiveSamplerIndex;
    sh_uint flags;
    sh_float roughness;
    sh_float metallic;
    sh_float alphaCutoff;
    sh_vec4 baseColor;
    sh_vec4 emissiveColor;
    sh_uint blendMode;
    sh_uint pad0;
    sh_uint pad1;
    sh_uint pad2;
};

struct PunctualLight
{
    sh_vec3 position;
    sh_float invRadius;
    sh_vec3 direction;
    sh_float size;
    sh_vec3 color;
    sh_float intensity;
    sh_float coneAngle;
    sh_float innerAngle;
    sh_float pad0;
    sh_float pad1;
};

struct DirectionLight
{
    sh_vec3 direction;
    sh_float size;
    sh_vec3 color;
    sh_float intensity;
    sh_bool hasShadow;
    sh_uint shadow;
    sh_uint pad0;
    sh_uint pad1;
};

struct Cluster
{
    sh_vec4 aabbMin;
    sh_vec4 aabbMax;
    sh_uvec4 litBits;
};

struct PerRenderPass
{
    sh_uvec2 rtSize;
    sh_uvec2 pad0;
    sh_vec4 pad1[15];
};

struct GPUTriangle
{
    sh_vec4 v0;
    sh_vec4 v1;
    sh_vec4 v2;
    sh_vec4 d0;
};

struct MeshPushConstants
{
    sh_uint sceneIndex;
    sh_uint materialIndex;
};

#ifdef __cplusplus
static_assert(sizeof(GPUTriangle) == 64, "GPUTriangle size mismatch");
#endif

struct ClusterAABBPushConstants
{
    sh_vec2 zNearFar;
    sh_uvec2 screenSize;
    sh_uvec4 clusterSize;
    sh_mat4 invProj;
};

struct ClusteringPushConstants
{
    sh_uvec2 lightCount;
    sh_uvec2 clusterSize;
    sh_mat4 viewMatrix;
};

struct ClusterDebugPushConstants
{
    sh_uvec2 screenSize;
    sh_uint pad0;
    sh_uint pad1;
    sh_uvec4 clusterSize;
};

#ifdef __cplusplus
enum class GpuMaterialBlendMode : uint32_t
{
    Opaque = 0,
    Mask = 1,
    Blend = 2
};
#else
const sh_uint GpuMaterialBlendMode_Opaque = 0;
const sh_uint GpuMaterialBlendMode_Mask = 1;
const sh_uint GpuMaterialBlendMode_Blend = 2;
#endif

const sh_uint MAX_LIGHT_DATA_STRUCTS = 32;
const sh_uint CLUSTER_SIZE_X = 32;
const sh_uint CLUSTER_SIZE_Y = 32;
const sh_uint CLUSTER_SIZE_Z = 12;
const sh_uint CLUSTER_COUNT = CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z;
const sh_uint MAX_INSTANCES_PER_BRICK = 32;

#define SAMPLER_LINEAR_CLAMP 1
#define SAMPLER_LINEAR_REPEAT 2
#define SAMPLER_NEAREST_CLAMP 3
#define SAMPLER_NEAREST_REPEAT 4
#define SAMPLER_LINEAR_MIRROR 5
#define SAMPLER_NEAREST_MIRROR 6
#define SAMPLER_LINEAR_MIPMAP_LINEAR_CLAMP 7
#define SAMPLER_LINEAR_MIPMAP_LINEAR_REPEAT 8
#define SAMPLER_LINEAR_MIPMAP_NEAREST_REPEAT 9
#define SAMPLER_NEAREST_MIPMAP_LINEAR_REPEAT 10
#define SAMPLER_NEAREST_MIPMAP_NEAREST_REPEAT 11

#ifdef __cplusplus
using GpuSceneNode = GpuSceneData;
using GpuMaterialData = MaterialData;
using PerFrameData = PerFrame;
using PerRenderPassData = PerRenderPass;
#endif

#endif
