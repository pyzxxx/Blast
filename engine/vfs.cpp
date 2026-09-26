#include "vfs.h"
#include "log.h"

#include <algorithm>
#include <filesystem>

namespace {

std::string normalize(const char* path) {
    if (path == nullptr) {
        return "";
    }

    std::string result(path);
    std::replace(result.begin(), result.end(), '\\', '/');
    while (result.size() > 1 && result.back() == '/' && result[result.size() - 2] != ':') {
        result.pop_back();
    }

    return result;
}

} // namespace

bool Vfs::mount(const char* virtual_prefix, const char* physical_dir) {
    Mount m;
    m.virtual_prefix = normalize(virtual_prefix);
    m.physical_dir = normalize(physical_dir);
    if (m.virtual_prefix.empty() || m.physical_dir.empty()) {
        LOGE("Vfs::mount: empty prefix or dir");
        return false;
    }

    for (uint32_t i = 0; i < mount_count_; ++i) {
        if (mounts_[i].virtual_prefix == m.virtual_prefix) {
            mounts_[i].physical_dir = m.physical_dir;
            return true;
        }
    }

    if (mount_count_ >= k_max_mounts) {
        LOGE("Vfs::mount: mount table full (%u)", k_max_mounts);
        return false;
    }

    mounts_[mount_count_++] = m;
    return true;
}

bool Vfs::exists(const char* virtual_path) const {
    return std::filesystem::is_regular_file(resolve(virtual_path));
}

File Vfs::open(const char* virtual_path) const {
    File file = File::open(resolve(virtual_path).c_str());
    if (!file.is_open()) {
        LOGE("Vfs::open: cannot open '%s'", virtual_path);
    }

    return file;
}

uint32_t Vfs::list(const char* virtual_dir, const char* extension, std::vector<std::string>& out_paths) const {
    const std::string dir = resolve(virtual_dir);
    const std::string vdir = normalize(virtual_dir);
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return 0;
    }

    const uint32_t before = static_cast<uint32_t>(out_paths.size());
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (!e.is_regular_file() || e.path().extension() != extension) {
            continue;
        }

        out_paths.push_back(vdir + "/" + e.path().filename().generic_string());
    }

    return static_cast<uint32_t>(out_paths.size()) - before;
}

Vfs::Mount* Vfs::find_mount(const std::string& path) const {
    const Mount* best = nullptr;
    for (uint32_t i = 0; i < mount_count_; ++i) {
        const Mount& m = mounts_[i];
        if (path.compare(0, m.virtual_prefix.size(), m.virtual_prefix) != 0) {
            continue;
        }

        if (best == nullptr || m.virtual_prefix.size() > best->virtual_prefix.size()) {
            best = &m;
        }
    }

    return const_cast<Mount*>(best);
}

std::string Vfs::resolve(const char* virtual_path) const {
    const std::string path = normalize(virtual_path);
    const Mount* m = find_mount(path);
    if (m == nullptr) {
        return path;
    }

    std::string rest = path.substr(m->virtual_prefix.size());
    if (rest.empty()) {
        return m->physical_dir;
    }

    if (rest.front() == '/') {
        rest.erase(0, 1);
    }

    return (std::filesystem::path(m->physical_dir) / rest).generic_string();
}
