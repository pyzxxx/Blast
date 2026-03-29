#include "VFS.h"
#include "FileSystem.h"
#include <algorithm>

namespace VFS
{
static std::map<std::string, std::string> s_mounts;

static std::string Normalize(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    auto it = std::unique(path.begin(), path.end(),
        [](char a, char b) { return a == '/' && b == '/'; });
    path.erase(it, path.end());
    if (!path.empty() && path.back() == '/')
    {
        // Preserve root paths like "/" or "C:/" to avoid ambiguous paths
        if (path != "/" && !(path.size() == 3 && path[1] == ':'))
            path.pop_back();
    }
    return path;
}

static bool FindMount(const std::string& path, std::string& outVirtual, std::string& outPhysical)
{
    std::vector<std::pair<std::string, std::string>> sorted(s_mounts.begin(), s_mounts.end());
    std::sort(sorted.begin(), sorted.end(),
        [](auto& a, auto& b) { return a.first.length() > b.first.length(); });

    for (const auto& m : sorted)
    {
        if (path.length() >= m.first.length() &&
            path.compare(0, m.first.length(), m.first) == 0 &&
            (path.length() == m.first.length() || path[m.first.length()] == '/'))
        {
            outVirtual = m.first;
            outPhysical = m.second;
            return true;
        }
    }
    return false;
}

static std::string Resolve(const std::string& path)
{
    std::string norm = Normalize(path);
    std::string virt, phys;
    if (FindMount(norm, virt, phys))
    {
        std::string rest = (norm.length() > virt.length())
            ? norm.substr(virt.length() + 1) : "";
        return Normalize(phys + "/" + rest);
    }
    return norm;
}

void Mount(const std::string& virtualPath, const std::string& physicalPath)
{
    s_mounts[Normalize(virtualPath)] = Normalize(physicalPath);
}

FS::File* Open(const std::string& path, FS::FileMode mode)
{
    return FS::File::Open(Resolve(path), mode);
}

std::string GetRealPath(const std::string& virtualPath)
{
    return Resolve(virtualPath);
}

bool IsFile(const std::string& path)
{
    return FS::IsFile(Resolve(path));
}

bool IsDirectory(const std::string& path)
{
    return FS::IsDirectory(Resolve(path));
}

void MakeDirectory(const std::string& path)
{
    std::string resolved = Resolve(path);
    if (!resolved.empty())
        FS::MakeDirectory(resolved);
}

void DuplicateFile(const std::string& from, const std::string& to)
{
    FS::DuplicateFile(Resolve(from), Resolve(to));
}

std::string ParentPath(const std::string& path)
{
    std::string real = Resolve(path);
    std::string parent = Normalize(FS::Path::ParentPath(real));

    // Terminate at root to avoid infinite loops
    if (parent == real || parent.empty())
        return "";

    for (const auto& [virt, phys] : s_mounts)
    {
        if (parent == phys || (parent.length() > phys.length() &&
            parent.compare(0, phys.length(), phys) == 0 && parent[phys.length()] == '/'))
        {
            std::string rest = (parent.length() > phys.length()) ? parent.substr(phys.length() + 1) : "";
            return rest.empty() ? virt : virt + "/" + rest;
        }
    }

    // Parent is outside any mount point - return empty to indicate "no parent in VFS"
    return "";
}

std::string FileName(const std::string& path)
{
    return FS::Path::FullFileName(Resolve(path));
}

std::string FullFileName(const std::string& path)
{
    return FS::Path::FullFileName(Resolve(path));
}

std::string Extension(const std::string& path)
{
    return FS::Path::Extension(Resolve(path));
}

std::string Join(const std::string& a, const std::string& b)
{
    return Normalize(a + "/" + b);
}

std::vector<std::string> ListDirectory(const std::string& path)
{
    std::vector<std::string> result;
    std::string realPath = Resolve(path);

    if (!FS::IsDirectory(realPath))
    {
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(realPath))
    {
        std::string name = entry.path().filename().string();
        result.push_back(name);
    }

    return result;
}
} // namespace VFS
