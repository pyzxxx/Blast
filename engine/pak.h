#pragma once

#include "file.h"
#include "pak_format.h"

#include <cstdint>
#include <string>
#include <vector>

class Pak {
public:
    bool find(uint64_t id, pak_format::ManifestEntry& out_entry) const;
    bool read_blob(const pak_format::ManifestEntry& entry, std::vector<uint8_t>& out_bytes) const;

    const uint64_t* dependencies(uint64_t id, uint32_t& out_count) const;

    uint32_t entry_count() const;
    const pak_format::ManifestEntry* entry_at(uint32_t index) const;
    const char* entry_path(uint32_t index) const;

    const std::string& path() const { return path_; }
private:
    friend class PakManager;

    bool open(File file);

    File file_;
    std::string path_;
    pak_format::ManifestHeader manifest_ = {};
    std::vector<pak_format::ManifestEntry> entries_;
    std::vector<pak_format::DependencyEntry> dependency_entries_;
    std::vector<uint64_t> dependency_ids_;
    std::vector<std::string> paths_;
    uint64_t manifest_offset_ = 0;
};
