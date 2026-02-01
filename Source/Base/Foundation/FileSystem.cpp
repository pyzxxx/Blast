#include "FileSystem.h"

namespace FS
{
bool IsFile(const std::string& path)
{
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool IsDirectory(const std::string& path)
{
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

void CopyFile(const std::string& from, const std::string& to)
{
    std::filesystem::create_directories(std::filesystem::path(to).parent_path());
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing);
}

void MakeDirectory(const std::string& path, bool recursive)
{
    if (recursive)
    {
        std::filesystem::create_directories(path);
    }
    else
    {
        std::filesystem::create_directory(path);
    }
}

bool RemoveFile(const std::string& path)
{
    return std::filesystem::remove(path);
}

bool RemoveDirectory(const std::string& path, bool recursive)
{
    if (recursive)
    {
        return std::filesystem::remove_all(path) > 0;
    }
    else
    {
        return std::filesystem::remove(path);
    }
}

std::string Path::Extension(const std::string& path)
{
    return std::filesystem::path(path).extension().string();
}

std::string Path::FileName(const std::string& path)
{
    return std::filesystem::path(path).filename().string();
}

std::string Path::ParentPath(const std::string& path)
{
    return std::filesystem::path(path).parent_path().string();
}

File* File::Open(const std::string& path, FileMode mode, FileError* error)
{
    auto file = new File();

    FileError err = file->OpenFile(path, mode);

    if (error)
    {
        *error = err;
    }

    if (err != FileError::OK)
    {
        return nullptr;
    }

    return file;
}

FileError File::OpenFile(const std::string& path, FileMode mode)
{
    m_path = path;
    m_mode = mode;

    std::ios::openmode iosMode = GetIosMode(m_mode);

    if (m_mode == FileMode::Write || m_mode == FileMode::WriteRead || m_mode == FileMode::Append)
    {
        std::filesystem::create_directories(m_path.parent_path());
    }

    m_stream.open(m_path, iosMode);

    if (!m_stream.is_open())
    {
        if (!std::filesystem::exists(m_path))
        {
            return FileError::FileNotFound;
        }
        return FileError::FileCantOpen;
    }

    if (m_buffer.empty())
    {
        m_buffer.reserve(8192);
    }

    return FileError::OK;
}

void File::Close()
{
    if (m_stream.is_open())
    {
        m_stream.close();
    }
}

std::ios::openmode File::GetIosMode(FileMode mode) const
{
    switch (mode)
    {
        case FileMode::Read:
            return std::ios::in | std::ios::binary;
        case FileMode::Write:
            return std::ios::out | std::ios::binary | std::ios::trunc;
        case FileMode::ReadWrite:
            return std::ios::in | std::ios::out | std::ios::binary;
        case FileMode::WriteRead:
            return std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc;
        case FileMode::Append:
            return std::ios::out | std::ios::binary | std::ios::app;
        default:
            return std::ios::in | std::ios::binary;
    }
}

void File::Seek(int64_t position)
{
    if (!IsOpen()) return;

    m_stream.seekg(position, std::ios::beg);
    m_stream.seekp(position, std::ios::beg);
}

void File::SeekEnd(int64_t offset)
{
    if (!IsOpen()) return;

    m_stream.seekg(offset, std::ios::end);
    m_stream.seekp(offset, std::ios::end);
}

size_t File::GetPosition()
{
    if (!IsOpen()) return 0;

    return static_cast<size_t>(m_stream.tellg());
}

size_t File::GetSize()
{
    if (!IsOpen()) return 0;

    auto current = m_stream.tellg();
    m_stream.seekg(0, std::ios::end);
    auto size = m_stream.tellg();
    m_stream.seekg(current, std::ios::beg);
    return static_cast<size_t>(size);
}

bool File::IsEof() const
{
    return m_stream.eof();
}

size_t File::Read(uint8_t* buffer, size_t size)
{
    if (!IsOpen() || !buffer || size == 0) return 0;

    m_stream.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(size));
    return static_cast<size_t>(m_stream.gcount());
}

void File::Write(const uint8_t* buffer, size_t size)
{
    if (!IsOpen() || !buffer) return;

    m_stream.write(reinterpret_cast<const char*>(buffer), static_cast<std::streamsize>(size));
}

void File::Write(const std::string& text)
{
    if (!IsOpen()) return;

    m_stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void File::Flush()
{
    if (IsOpen())
    {
        m_stream.flush();
    }
}
} // namespace FS