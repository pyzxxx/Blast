#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace FS
{
enum class FileError
{
    OK = 0,
    FileNotFound,
    FileCantOpen
};

enum class FileMode
{
    Read = 1,
    Write = 2,
    ReadWrite = 3,
    WriteRead = 4,
    Append = 5
};

bool IsFile(const std::string& path);
bool IsDirectory(const std::string& path);
void CopyFile(const std::string& from, const std::string& to);
void MakeDirectory(const std::string& path, bool recursive = true);
bool RemoveFile(const std::string& path);
bool RemoveDirectory(const std::string& path, bool recursive);

class Path
{
public:
    static std::string Extension(const std::string& path);
    static std::string FileName(const std::string& path);
    static std::string FullFileName(const std::string& path);
    static std::string ParentPath(const std::string& path);

    static void RegisterProtocol(const std::string& proto, const std::string& path);
    static std::string FixPath(const std::string& path);

    template<typename... Args>
    static std::string Join(Args&&... parts)
    {
        std::filesystem::path result;
        ((result /= std::forward<Args>(parts)), ...);
        return result.string();
    }

private:
    static std::map<std::string, std::string> s_protocols;
};

class File
{
public:
    File() = default;
    ~File() { Close(); }

    static File* Open(const std::string& path, FileMode mode, FileError* error = nullptr);
    void Close();

    bool IsOpen() const { return m_stream.is_open(); }
    std::string GetPath() const { return m_path.string(); }
    std::string GetAbsolutePath() const { return std::filesystem::absolute(m_path).string(); }

    void Seek(int64_t position);
    void SeekEnd(int64_t offset = 0);
    size_t GetPosition();
    size_t GetSize();
    bool IsEof() const;

    size_t Read(uint8_t* buffer, size_t size);
    void Write(const uint8_t* buffer, size_t size);
    void Write(const std::string& str);

    void Flush();

private:
    FileError OpenFile(const std::string& path, FileMode mode);
    std::ios::openmode GetIosMode(FileMode mode) const;

    std::fstream m_stream;
    std::filesystem::path m_path;
    FileMode m_mode = FileMode::Read;
    mutable std::vector<char> m_buffer;
};
} // namespace FS