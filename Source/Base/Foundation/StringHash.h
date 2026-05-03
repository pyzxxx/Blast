#pragma once

#include <cstdint>
#include <cstring>

using StringHash = uint32_t;

inline constexpr StringHash Fnv1aHash(const char* str, size_t len)
{
    uint32_t hash = 0x811c9dc5;
    for (size_t i = 0; i < len; ++i)
    {
        hash ^= static_cast<uint8_t>(str[i]);
        hash *= 0x01000193;
    }
    return hash;
}

inline constexpr StringHash Fnv1aHash(const char* str)
{
    uint32_t hash = 0x811c9dc5;
    while (*str)
    {
        hash ^= static_cast<uint8_t>(*str);
        hash *= 0x01000193;
        ++str;
    }
    return hash;
}

inline constexpr StringHash operator""_sh(const char* str, size_t len) { return Fnv1aHash(str, len); }
