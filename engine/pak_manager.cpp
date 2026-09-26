#include "pak_manager.h"
#include "log.h"
#include "services.h"
#include "vfs.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* k_pak_dir = "assets";
constexpr const char* k_pak_extension = ".pak";

} // namespace

bool PakManager::init(const Services& services) {
    std::vector<std::string> pak_paths;
    services.vfs->list(k_pak_dir, k_pak_extension, pak_paths);
    for (const std::string& path : pak_paths) {
        Pak pak;
        if (pak.open(services.vfs->open(path.c_str()))) {
            pak.path_ = services.vfs->resolve(path.c_str());
            paks_.push_back(std::move(pak));
        }
    }

    if (paks_.empty()) {
        LOGW("PakManager: no pak files found under '%s'", k_pak_dir);
    } else {
        LOGI("PakManager: mounted %u pak(s)", static_cast<uint32_t>(paks_.size()));
    }

    return true;
}

void PakManager::shutdown() {
    paks_.clear();
}

Pak* PakManager::find(uint64_t id, pak_format::ManifestEntry& out_entry) {
    for (Pak& pak : paks_) {
        if (pak.find(id, out_entry)) {
            return &pak;
        }
    }

    return nullptr;
}

uint32_t PakManager::enumerate(pak_format::AssetType type, std::vector<PakEntryInfo>& out_entries) const {
    out_entries.clear();
    std::unordered_set<uint64_t> seen;
    for (const Pak& pak : paks_) {
        const uint32_t count = pak.entry_count();
        for (uint32_t i = 0; i < count; ++i) {
            const pak_format::ManifestEntry* entry = pak.entry_at(i);
            if (entry->type != static_cast<uint8_t>(type) || !seen.insert(entry->id).second) {
                continue;
            }

            PakEntryInfo info;
            info.id = entry->id;
            info.type = type;
            info.path = pak.entry_path(i);
            out_entries.push_back(info);
        }
    }

    return static_cast<uint32_t>(out_entries.size());
}
