#pragma once

#include "RHI/RHI.h"

class ShaderCache
{
public:
    static void Initialize();

    static void Terminate();

    static RHI::Shader* Get(const std::string& path);

    static RHI::Shader* Get(const std::string& path, const std::vector<std::string>& macros);

private:
    static void Compile(const std::string& path, const std::vector<std::string>& macros, void** data, uint32_t& size);

    static std::unordered_map<std::size_t, RHI::Shader*> m_shaderDict;
};