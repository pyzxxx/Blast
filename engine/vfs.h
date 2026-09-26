#pragma once

#include "file.h"

#include <cstdint>
#include <string>
#include <vector>

class Vfs {
public:
    bool mount(const char* virtual_prefix, const char* physical_dir);

    bool exists(const char* virtual_path) const;

    File open(const char* virtual_path) const;

    uint32_t list(const char* virtual_dir, const char* extension, std::vector<std::string>& out_paths) const;

    std::string resolve(const char* virtual_path) const;

private:
    static constexpr uint32_t k_max_mounts = 8;

    struct Mount {
        std::string virtual_prefix;
        std::string physical_dir;
    };
    Mount mounts_[k_max_mounts];
    uint32_t mount_count_ = 0;

    Mount* find_mount(const std::string& path) const;
};
