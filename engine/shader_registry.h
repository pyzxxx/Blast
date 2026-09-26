#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

#define REGISTER_SHADER(Name, Path) Name,
#define REGISTER_VARIANT(BaseName, VariantName, ...) BaseName##_##VariantName,
enum class ShaderId : uint32_t {
#include "shader_list.h"
    Count
};
#undef REGISTER_SHADER
#undef REGISTER_VARIANT

#define REGISTER_SHADER(Name, Path) Path,
#define REGISTER_VARIANT(BaseName, VariantName, ...) nullptr,
inline const char* k_shader_src_path[] = {
#include "shader_list.h"
};
#undef REGISTER_SHADER
#undef REGISTER_VARIANT

#define REGISTER_SHADER(Name, Path) ShaderId::Name,
#define REGISTER_VARIANT(BaseName, VariantName, ...) ShaderId::BaseName,
inline ShaderId k_shader_base[] = {
#include "shader_list.h"
};
#undef REGISTER_SHADER
#undef REGISTER_VARIANT

#define REGISTER_SHADER(Name, Path) nullptr,
#define REGISTER_VARIANT(BaseName, VariantName, ...) #VariantName,
inline const char* k_shader_variant[] = {
#include "shader_list.h"
};
#undef REGISTER_SHADER
#undef REGISTER_VARIANT

inline bool shader_spv_path(ShaderId id, char* out, uint32_t cap) {
    uint32_t i = static_cast<uint32_t>(id);
    if (i >= static_cast<uint32_t>(ShaderId::Count) || out == nullptr || cap == 0) {
        return false;
    }

    const char* src = k_shader_src_path[static_cast<uint32_t>(k_shader_base[i])];
    const char* slash = strrchr(src, '/');
    const char* dot = strrchr(src, '.');
    if (slash == nullptr || dot == nullptr || dot < slash) {
        return false;
    }

    const char* var = k_shader_variant[i];
    int n = snprintf(out, cap, "%.*s/bin/%.*s%s%s%s.spv", static_cast<int>(slash - src), src,
                     static_cast<int>(dot - slash - 1), slash + 1, var != nullptr ? "_" : "", var != nullptr ? var : "",
                     dot);
    return n > 0 && static_cast<uint32_t>(n) < cap;
}
