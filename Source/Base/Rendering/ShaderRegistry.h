#pragma once

#include "Foundation/StringHash.h"

#include <cstdint>
#include <string>
#include <vector>

#define REGISTER_SHADER(Name, Path) Name,
#define REGISTER_VARIANT(BaseName, VariantName, ...) BaseName##_##VariantName,
enum class ShaderId : uint32_t
{
#include "ShaderList.h"
    Count
};
#undef REGISTER_SHADER
#undef REGISTER_VARIANT

#define REGISTER_SHADER(Name, Path) Path,
#define REGISTER_VARIANT(BaseName, VariantName, ...) nullptr,
inline const char* kShaderPaths[] = {
#include "ShaderList.h"
};
#undef REGISTER_SHADER
#undef REGISTER_VARIANT

inline std::vector<std::string> GetShaderMacros(ShaderId id)
{
    std::vector<std::string> macros;
    switch (id)
    {
#define REGISTER_SHADER(Name, Path)
#define REGISTER_VARIANT(BaseName, VariantName, ...) \
    case ShaderId::BaseName##_##VariantName:         \
        for (const char* m : {__VA_ARGS__})          \
            macros.push_back(m);                     \
        break;
#include "ShaderList.h"
#undef REGISTER_SHADER
#undef REGISTER_VARIANT
        default: break;
    }
    return macros;
}

inline uint32_t GetShaderBaseIndex(ShaderId id)
{
    switch (id)
    {
#define REGISTER_SHADER(Name, Path) \
    case ShaderId::Name:            \
        return static_cast<uint32_t>(ShaderId::Name);
#define REGISTER_VARIANT(BaseName, VariantName, ...) \
    case ShaderId::BaseName##_##VariantName:         \
        return static_cast<uint32_t>(ShaderId::BaseName);
#include "ShaderList.h"
#undef REGISTER_SHADER
#undef REGISTER_VARIANT
        default: return 0;
    }
}
