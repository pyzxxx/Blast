#pragma once

#include <cstdint>
#include <memory>

enum class FileMode {
    Read,
    Write,
    ReadWrite,
    Append,
};

class File {
public:
    File();
    ~File();
    File(File&&);
    File& operator=(File&&);
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    bool is_open() const;
    bool eof();
    uint64_t size();
    uint64_t position();

    void seek(uint64_t position) const;
    void seek_end(int64_t offset = 0) const;

    uint64_t read(void* out_bytes, uint64_t read_size) const;
    bool write(const void* bytes, uint64_t size);
    void flush();

    static File open(const char* path, FileMode mode = FileMode::Read);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    explicit File(std::unique_ptr<Impl> impl);
};
