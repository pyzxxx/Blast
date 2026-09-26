#pragma once

#include "pak.h"

#include <cstdint>
#include <vector>

struct Services;

struct PakEntryInfo {
    uint64_t id;
    pak_format::AssetType type;
    const char* path;
};

class PakManager {
public:
    bool init(const Services& services);
    void shutdown();

    Pak* find(uint64_t id, pak_format::ManifestEntry& out_entry);
    uint32_t enumerate(pak_format::AssetType type, std::vector<PakEntryInfo>& out_entries) const;

private:
    std::vector<Pak> paks_;
};
