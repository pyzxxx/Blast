#pragma once

#include "Foundation/Module.h"
#include "RHI/RHI.h"
#include "ShaderRegistry.h"

class ShaderCache : public Module<ShaderCache>
{
public:
    void Initialize() override;
    void Terminate() override;

    RHI::Shader* GetShader(const std::string& path);
    RHI::Shader* GetShader(const std::string& path, const std::vector<std::string>& macros);
    RHI::Shader* GetShader(ShaderId id);

private:
    static void Compile(const std::string& path, const std::vector<std::string>& macros, void** data, uint32_t& size);

    std::unordered_map<std::size_t, RHI::Shader*> m_shaderDict;
};