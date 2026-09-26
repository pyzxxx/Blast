#include "pak.h"
#include "log.h"

#include <algorithm>
#include <cstring>

namespace {

uint32_t read_u32(const uint8_t* p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

uint64_t read_u64(const uint8_t* p) {
    uint64_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

template<typename T>
bool read_table(File& file, uint64_t offset, uint64_t count, std::vector<T>& out_table) {
    out_table.resize(static_cast<size_t>(count));
    if (count == 0) {
        return true;
    }

    file.seek(offset);
    return file.read(out_table.data(), count * sizeof(T)) == count * sizeof(T);
}

} // namespace

bool Pak::open(File file) {
    if (!file.is_open()) {
        return false;
    }

    const uint64_t file_size = file.size();
    if (file_size < pak_format::k_footer_size + sizeof(pak_format::ManifestHeader)) {
        LOGE("Pak::open: file too small");
        return false;
    }

    uint8_t footer[pak_format::k_footer_size];
    file.seek(file_size - pak_format::k_footer_size);
    if (file.read(footer, pak_format::k_footer_size) != pak_format::k_footer_size) {
        LOGE("Pak::open: cannot read footer");
        return false;
    }

    const uint64_t manifest_size = read_u64(footer);
    if (read_u32(footer + 8) != pak_format::k_package_magic) {
        LOGE("Pak::open: bad package magic");
        return false;
    }

    if (manifest_size < sizeof(pak_format::ManifestHeader) || manifest_size > file_size - pak_format::k_footer_size) {
        LOGE("Pak::open: bad manifest size %llu", static_cast<uint64_t>(manifest_size));
        return false;
    }

    const uint64_t manifest_offset = file_size - pak_format::k_footer_size - manifest_size;

    pak_format::ManifestHeader manifest = {};
    file.seek(manifest_offset);
    if (file.read(&manifest, sizeof(manifest)) != sizeof(manifest)) {
        LOGE("Pak::open: cannot read manifest header");
        return false;
    }

    if (manifest.magic != pak_format::k_manifest_magic) {
        LOGE("Pak::open: bad manifest magic");
        return false;
    }

    if (manifest.version != pak_format::k_package_version) {
        LOGE("Pak::open: version %u, expect %u", manifest.version, pak_format::k_package_version);
        return false;
    }

    const uint64_t entries_end = sizeof(pak_format::ManifestHeader) + (uint64_t)manifest.entry_count * sizeof(pak_format::ManifestEntry);
    const uint64_t deps_end = entries_end + (uint64_t)manifest.dependency_count * sizeof(pak_format::DependencyEntry);
    if (deps_end > manifest_size || manifest.dependency_id_table_offset < deps_end || manifest.dependency_id_table_offset > manifest_size) {
        LOGE("Pak::open: manifest tables out of bounds");
        return false;
    }

    if (manifest.path_table_offset < manifest.dependency_id_table_offset || manifest.path_table_offset > manifest_size ||
        manifest.entry_count > (manifest_size - manifest.path_table_offset) / sizeof(uint64_t)) {
        LOGE("Pak::open: path table out of bounds");
        return false;
    }

    std::vector<pak_format::ManifestEntry> entries;
    std::vector<pak_format::DependencyEntry> dependency_entries;
    std::vector<uint64_t> dependency_ids;
    std::vector<uint64_t> path_offsets;
    const uint64_t entries_offset = manifest_offset + sizeof(pak_format::ManifestHeader);
    if (!read_table(file, entries_offset, manifest.entry_count, entries) ||
        !read_table(file, entries_offset + (uint64_t)manifest.entry_count * sizeof(pak_format::ManifestEntry), manifest.dependency_count, dependency_entries) ||
        !read_table(file, manifest_offset + manifest.dependency_id_table_offset, (manifest.path_table_offset - manifest.dependency_id_table_offset) / sizeof(uint64_t), dependency_ids) ||
        !read_table(file, manifest_offset + manifest.path_table_offset, manifest.entry_count, path_offsets)) {
        LOGE("Pak::open: cannot read manifest tables");
        return false;
    }

    const uint64_t strings_offset = manifest.path_table_offset + (uint64_t)manifest.entry_count * sizeof(uint64_t);
    std::vector<char> path_blob;
    if (!read_table(file, manifest_offset + strings_offset, manifest_size - strings_offset, path_blob)) {
        LOGE("Pak::open: cannot read path strings");
        return false;
    }

    std::vector<std::string> paths(manifest.entry_count);
    for (uint32_t i = 0; i < manifest.entry_count; ++i) {
        const uint64_t off = path_offsets[i];
        if (off < strings_offset || off >= manifest_size) {
            LOGE("Pak::open: path %u offset out of bounds", i);
            return false;
        }

        const char* begin = path_blob.data() + (off - strings_offset);
        const size_t avail = path_blob.size() - static_cast<size_t>(off - strings_offset);
        if (!memchr(begin, '\0', avail)) {
            LOGE("Pak::open: path %u not terminated", i);
            return false;
        }

        paths[i] = begin;
    }

    file_ = std::move(file);
    manifest_ = manifest;
    entries_ = std::move(entries);
    dependency_entries_ = std::move(dependency_entries);
    dependency_ids_ = std::move(dependency_ids);
    paths_ = std::move(paths);
    manifest_offset_ = manifest_offset;
    return true;
}

bool Pak::find(uint64_t id, pak_format::ManifestEntry& out_entry) const {
    const pak_format::ManifestEntry* begin = entries_.data();
    const pak_format::ManifestEntry* it = std::lower_bound(begin, begin + entries_.size(), id, [](const pak_format::ManifestEntry& e, uint64_t v) { return e.id < v; });
    if (it == begin + entries_.size() || it->id != id) {
        return false;
    }

    out_entry = *it;
    return true;
}

bool Pak::read_blob(const pak_format::ManifestEntry& entry, std::vector<uint8_t>& out_bytes) const {
    out_bytes.clear();
    if (!file_.is_open() || entry.offset > manifest_offset_ || entry.size > manifest_offset_ - entry.offset) {
        return false;
    }

    out_bytes.resize(static_cast<size_t>(entry.size));
    file_.seek(entry.offset);
    if (entry.size > 0 && file_.read(out_bytes.data(), entry.size) != entry.size) {
        out_bytes.clear();
        return false;
    }

    return true;
}

const uint64_t* Pak::dependencies(uint64_t id, uint32_t& out_count) const {
    out_count = 0;

    const pak_format::DependencyEntry* begin = dependency_entries_.data();
    const pak_format::DependencyEntry* it = std::lower_bound(begin, begin + dependency_entries_.size(), id, [](const pak_format::DependencyEntry& e, uint64_t v) { return e.asset_id < v; });
    if (it == begin + dependency_entries_.size() || it->asset_id != id) {
        return nullptr;
    }

    const size_t first = it->first_dependency_index;
    if (first > dependency_ids_.size() || it->dependency_count > dependency_ids_.size() - first) {
        return nullptr;
    }

    out_count = it->dependency_count;
    return dependency_ids_.data() + first;
}

uint32_t Pak::entry_count() const {
    return manifest_.entry_count;
}

const pak_format::ManifestEntry* Pak::entry_at(uint32_t index) const {
    if (index >= entries_.size()) {
        return nullptr;
    }

    return entries_.data() + index;
}

const char* Pak::entry_path(uint32_t index) const {
    if (index >= paths_.size()) {
        return "";
    }

    return paths_[index].c_str();
}
