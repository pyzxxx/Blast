#pragma once

#include "Asset.h"
#include <string>

class TextureAsset;
class RenderScene;
#include "Rendering/ShaderSchema.h"

enum class MaterialBlendMode
{
    Opaque,
    Mask,
    Blend
};

struct MaterialTextureBinding
{
    TextureAsset* texture = nullptr;
    std::string samplerName = "linearClamp";

    bool IsComplete() const { return texture != nullptr && !samplerName.empty(); }
};

class MaterialAsset : public Asset
{
public:
    MaterialAsset(const std::string& assetPath);
    virtual ~MaterialAsset();

    uint32_t GetGpuMaterialIndex() const { return m_gpuMaterialIndex; }

    const MaterialTextureBinding& GetAlbedo() const { return m_albedo; }
    const MaterialTextureBinding& GetNormal() const { return m_normal; }
    const MaterialTextureBinding& GetRoughnessMetallic() const { return m_roughnessMetallic; }
    const MaterialTextureBinding& GetEmissive() const { return m_emissive; }

    TextureAsset* GetAlbedoTexture() const { return m_albedo.texture; }
    TextureAsset* GetNormalTexture() const { return m_normal.texture; }
    TextureAsset* GetRoughnessMetallicTexture() const { return m_roughnessMetallic.texture; }
    TextureAsset* GetEmissiveTexture() const { return m_emissive.texture; }

    const std::string& GetAlbedoSampler() const { return m_albedo.samplerName; }
    const std::string& GetNormalSampler() const { return m_normal.samplerName; }
    const std::string& GetRoughnessMetallicSampler() const { return m_roughnessMetallic.samplerName; }
    const std::string& GetEmissiveSampler() const { return m_emissive.samplerName; }

    const glm::vec4& GetBaseColor() const { return m_baseColor; }
    const glm::vec4& GetEmissiveColor() const { return m_emissiveColor; }
    float GetRoughness() const { return m_roughness; }
    float GetMetallic() const { return m_metallic; }
    uint32_t GetFlags() const { return m_flags; }
    MaterialBlendMode GetBlendMode() const { return m_blendMode; }
    float GetAlphaCutoff() const { return m_alphaCutoff; }

private:
    MaterialTextureBinding m_albedo;
    MaterialTextureBinding m_normal;
    MaterialTextureBinding m_roughnessMetallic;
    MaterialTextureBinding m_emissive;

    glm::vec4 m_baseColor = glm::vec4(1.0f);
    glm::vec4 m_emissiveColor = glm::vec4(0.0f);
    float m_roughness = 0.5f;
    float m_metallic = 0.0f;
    float m_alphaCutoff = 0.5f;
    uint32_t m_flags = 0;
    MaterialBlendMode m_blendMode = MaterialBlendMode::Opaque;

    uint32_t m_gpuMaterialHandle = 0;
    uint32_t m_gpuMaterialIndex = 0;
};
