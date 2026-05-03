#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "ShaderInterop.h"
#include "Common.glsl"
#include "PBR.glsl"
#include "Lighting.glsl"
#include "Clustering.glsl"

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_worldPos;
layout(location = 3) in vec3 v_tangent;
layout(location = 4) in vec3 v_bitangent;
layout(location = 5) in vec3 v_viewPos;

layout(location = 0) out vec4 o_fragColor;

layout(set = 0, binding = 0) uniform PerFrameBlock
{
    PerFrame data;
} u_perFrame;

layout(set = 0, binding = 1) readonly buffer GpuScene
{
    GpuSceneData data[];
} s_gpuScene;

layout(set = 0, binding = 2) readonly buffer PointLights
{
    PunctualLight data[];
} s_pointLights;

layout(set = 0, binding = 3) readonly buffer SpotLights
{
    PunctualLight data[];
} s_spotLights;

layout(set = 0, binding = 4) readonly buffer DirLights
{
    DirectionLight data[];
} s_dirLights;

layout(set = 0, binding = 5) readonly buffer Clusters
{
    Cluster data[];
} s_clusters;

layout(set = 0, binding = 6) readonly buffer GpuMaterials
{
    MaterialData data[];
} s_gpuMaterials;

layout(set = 0, binding = 7) uniform PerRenderPassBlock
{
    PerRenderPass data;
} u_perRenderPass;

layout(set = 1, binding = 0) uniform texture2D u_textures[];
layout(set = 1, binding = 1) uniform sampler u_samplers[];
layout(set = 1, binding = 4, std430) readonly buffer BindlessStorageBufferBlock
{
    uint data[];
} s_storageBuffers[];

layout(push_constant) uniform PC
{
    MeshPushConstants pc;
};

vec3 GetNormalFromMap(MaterialData mat, vec3 normal, vec3 tangent, vec3 bitangent, vec2 uv)
{
    if (mat.normalTexIndex == 0xFFFFFFFFu)
    {
        return normalize(normal);
    }

    vec3 tangentNormal = texture(sampler2D(u_textures[nonuniformEXT(mat.normalTexIndex)],
                                           u_samplers[nonuniformEXT(mat.normalSamplerIndex)]),
                                 uv).xyz * 2.0 - 1.0;

    mat3 TBN = mat3(normalize(tangent), normalize(bitangent), normalize(normal));
    return normalize(TBN * tangentNormal);
}

vec4 SampleTexture(texture2D tex, sampler samp, vec2 uv)
{
    return texture(sampler2D(tex, samp), uv);
}

vec4 SampleMaterialTexture(uint texIndex, uint samplerIndex, vec2 uv)
{
    if (texIndex == 0xFFFFFFFFu)
    {
        return vec4(1.0);
    }
    return SampleTexture(u_textures[nonuniformEXT(texIndex)], u_samplers[nonuniformEXT(samplerIndex)], uv);
}

void main()
{
    MaterialData material = s_gpuMaterials.data[pc.materialIndex];

    vec4 baseColor = material.baseColor;
    vec4 albedoTex = SampleMaterialTexture(material.albedoTexIndex, material.albedoSamplerIndex, v_uv);
    baseColor *= albedoTex;

#ifdef ALPHA_MASK
    if (baseColor.a < material.alphaCutoff)
        discard;
#endif

    float roughness = material.roughness;
    float metallic = material.metallic;

    vec4 roughnessMetallicTex = SampleMaterialTexture(material.roughnessMetallicTexIndex, material.roughnessMetallicSamplerIndex, v_uv);
    roughness *= roughnessMetallicTex.g;
    metallic *= roughnessMetallicTex.b;

    roughness = clamp(roughness, 0.04, 1.0);

    vec3 N = GetNormalFromMap(material, v_normal, v_tangent, v_bitangent, v_uv);
    vec3 V = normalize(u_perFrame.data.cameraPosition.xyz - v_worldPos);

    vec3 albedo = baseColor.rgb;

    vec3 Lo = vec3(0.0);

    uint dirLightCount = u_perFrame.data.lightCount.z;
    for (uint i = 0u; i < dirLightCount; i++)
    {
        Lo += CalcDirectionLight(s_dirLights.data[i], N, V, albedo, roughness, metallic);
    }

    uvec3 clusterCoord = GetClusterCoord(gl_FragCoord.xy, v_viewPos.z,
                                         u_perFrame.data.clusterSize.xyz,
                                         u_perFrame.data.zNearFar,
                                         u_perFrame.data.screenSize,
                                         u_perRenderPass.data.rtSize);
    uint clusterIndex = GetClusterIndex(clusterCoord, u_perFrame.data.clusterSize.xyz);

    uint pointLightCountTotal = u_perFrame.data.lightCount.x;
    uint spotLightCountTotal = u_perFrame.data.lightCount.y;

    Cluster cluster = s_clusters.data[clusterIndex];

    uint pointLightBits = cluster.litBits.x;
    while (pointLightBits != 0u)
    {
        uint bit = findLSB(pointLightBits);
        pointLightBits &= ~(1u << bit);
        Lo += CalcPunctualLight(s_pointLights.data[bit], v_worldPos, N, V, albedo, roughness, metallic, false);
    }

    uint spotLightBits = cluster.litBits.y;
    while (spotLightBits != 0u)
    {
        uint bit = findLSB(spotLightBits);
        spotLightBits &= ~(1u << bit);
        Lo += CalcPunctualLight(s_spotLights.data[bit], v_worldPos, N, V, albedo, roughness, metallic, true);
    }

    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    vec3 emissive = material.emissiveColor.rgb;
    vec4 emissiveTex = SampleMaterialTexture(material.emissiveTexIndex, material.emissiveSamplerIndex, v_uv);
    color += emissive * emissiveTex.rgb;

#ifdef ALPHA_BLEND
    o_fragColor = vec4(color, baseColor.a);
#else
    o_fragColor = vec4(color, 1.0);
#endif
}
