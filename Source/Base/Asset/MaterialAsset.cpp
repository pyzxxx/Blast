#include "MaterialAsset.h"
#include "AssetManager.h"
#include "Foundation/JsonIO.h"
#include "RHI/RHI.h"
#include "Rendering/RenderScene.h"
#include "Rendering/Renderer.h"
#include "TextureAsset.h"

static uint32_t GetTextureBindlessIndex(TextureAsset* texture)
{
    if (!texture)
    {
        return 0xFFFFFFFF;
    }

    return RHI::GetBindlessIndex(texture->GetTexture());
}

static uint32_t GetSamplerBindlessIndex(const std::string& name)
{
    RenderContext* ctx = Renderer::Get()->GetContext();
    RHI::Sampler* sampler = ctx->GetSampler(name);
    if (sampler)
    {
        return RHI::GetBindlessIndex(sampler);
    }
    return 0xFFFFFFFF;
}

MaterialAsset::MaterialAsset(const std::string& assetPath) : Asset(assetPath)
{
    JsonReader reader(assetPath);

    reader.Object("albedo", [&]() {
        std::string texturePath;
        if (reader.Field("texture", texturePath))
        {
            m_albedo.texture = AssetManager::Get()->Load<TextureAsset>(texturePath);
        }
        reader.Field("sampler", m_albedo.samplerName);
    });

    reader.Object("normal", [&]() {
        std::string texturePath;
        if (reader.Field("texture", texturePath))
        {
            m_normal.texture = AssetManager::Get()->Load<TextureAsset>(texturePath);
        }
        reader.Field("sampler", m_normal.samplerName);
    });

    reader.Object("roughnessMetallic", [&]() {
        std::string texturePath;
        if (reader.Field("texture", texturePath))
        {
            m_roughnessMetallic.texture = AssetManager::Get()->Load<TextureAsset>(texturePath);
        }
        reader.Field("sampler", m_roughnessMetallic.samplerName);
    });

    reader.Object("emissive", [&]() {
        std::string texturePath;
        if (reader.Field("texture", texturePath))
        {
            m_emissive.texture = AssetManager::Get()->Load<TextureAsset>(texturePath);
        }
        reader.Field("sampler", m_emissive.samplerName);
    });

    glm::vec4 baseColor = glm::vec4(1.0f);
    reader.Field("baseColor", baseColor);
    m_baseColor = baseColor;

    glm::vec4 emissiveColor = glm::vec4(0.0f);
    reader.Field("emissiveColor", emissiveColor);
    m_emissiveColor = emissiveColor;

    float roughness = 0.5f;
    reader.Field("roughness", roughness);
    m_roughness = roughness;

    float metallic = 0.0f;
    reader.Field("metallic", metallic);
    m_metallic = metallic;

    reader.Field("flags", m_flags);

    std::string blendModeStr;
    if (reader.Field("blendMode", blendModeStr))
    {
        if (blendModeStr == "mask")
        {
            m_blendMode = MaterialBlendMode::Mask;
        }
        else if (blendModeStr == "blend")
        {
            m_blendMode = MaterialBlendMode::Blend;
        }
        else
        {
            m_blendMode = MaterialBlendMode::Opaque;
        }
    }

    reader.Field("alphaCutoff", m_alphaCutoff);

    RenderScene* scene = Renderer::Get()->GetScene();
    m_gpuMaterialHandle = scene->gpuMaterials.Add();
    m_gpuMaterialIndex = scene->gpuMaterials.GetIndex(m_gpuMaterialHandle);

    auto updateBinding = [&](const MaterialTextureBinding& binding, uint32_t& outTexIndex, uint32_t& outSamplerIndex) {
        if (!binding.IsComplete())
        {
            outTexIndex = 0xFFFFFFFF;
            outSamplerIndex = 0xFFFFFFFF;
            return;
        }

        uint32_t samplerIndex = GetSamplerBindlessIndex(binding.samplerName);
        if (samplerIndex == 0xFFFFFFFF)
        {
            outTexIndex = 0xFFFFFFFF;
            outSamplerIndex = 0xFFFFFFFF;
            return;
        }

        outTexIndex = GetTextureBindlessIndex(binding.texture);
        outSamplerIndex = samplerIndex;
    };

    GpuMaterialData* gpuMaterial = scene->gpuMaterials.Get(m_gpuMaterialHandle);
    gpuMaterial->baseColor = m_baseColor;
    gpuMaterial->emissiveColor = m_emissiveColor;
    gpuMaterial->roughness = m_roughness;
    gpuMaterial->metallic = m_metallic;
    gpuMaterial->flags = m_flags;
    gpuMaterial->alphaCutoff = m_alphaCutoff;
    gpuMaterial->blendMode = static_cast<uint32_t>(m_blendMode);

    updateBinding(m_albedo, gpuMaterial->albedoTexIndex, gpuMaterial->albedoSamplerIndex);
    updateBinding(m_normal, gpuMaterial->normalTexIndex, gpuMaterial->normalSamplerIndex);
    updateBinding(m_roughnessMetallic, gpuMaterial->roughnessMetallicTexIndex, gpuMaterial->roughnessMetallicSamplerIndex);
    updateBinding(m_emissive, gpuMaterial->emissiveTexIndex, gpuMaterial->emissiveSamplerIndex);
}

MaterialAsset::~MaterialAsset()
{
    RenderScene* scene = Renderer::Get()->GetScene();
    scene->gpuMaterials.Remove(m_gpuMaterialHandle);
}
