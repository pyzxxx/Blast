#pragma once

#include "PCH.h"

namespace FS {
enum class FileMode;
class File;
}// namespace FS

namespace VFS {
void Mount(const std::string& virtualPath, const std::string& physicalPath);

FS::File* Open(const std::string& path, FS::FileMode mode);

std::string GetRealPath(const std::string& virtualPath);

bool IsFile(const std::string& path);
bool IsDirectory(const std::string& path);
void MakeDirectory(const std::string& path);
void DuplicateFile(const std::string& from, const std::string& to);

std::string ParentPath(const std::string& path);
std::string FileName(const std::string& path);
std::string FullFileName(const std::string& path);
std::string Extension(const std::string& path);

std::string Join(const std::string& a, const std::string& b);

std::vector<std::string> ListDirectory(const std::string& path);

template<typename... Args>
std::string Join(const std::string& a, const std::string& b, Args&&... rest)
{
    return Join(Join(a, b), std::forward<Args>(rest)...);
}
}// namespace VFS
