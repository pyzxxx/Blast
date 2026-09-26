#include "file.h"
#include "log.h"

#include <fstream>
#include <string_view>

class File::Impl {
public:
    bool open(const char* path, FileMode mode) {
        std::ios::openmode open_mode = std::ios::binary;
        switch (mode) {
            case FileMode::Read:
                open_mode |= std::ios::in;
                break;
            case FileMode::Write:
                open_mode |= std::ios::out | std::ios::trunc;
                break;
            case FileMode::ReadWrite:
                open_mode |= std::ios::in | std::ios::out;
                break;
            case FileMode::Append:
                open_mode |= std::ios::out | std::ios::app;
                break;
        }

        stream_.open(path, open_mode);
        return stream_.good();
    }

    bool is_open() const {
        return stream_.is_open();
    }

    bool eof() {
        return stream_.eof();
    }

    uint64_t size() {
        const std::streampos cur = stream_.tellg();
        stream_.clear();
        stream_.seekg(0, std::ios::end);
        const std::streampos end = stream_.tellg();
        stream_.clear();
        stream_.seekg(cur);
        return end < 0 ? 0 : static_cast<uint64_t>(end);
    }

    uint64_t position() {
        const std::streampos pos = stream_.tellg();
        return pos < 0 ? 0 : static_cast<uint64_t>(pos);
    }

    void seek(uint64_t position) {
        stream_.clear();
        stream_.seekg(static_cast<std::streamoff>(position));
        stream_.seekp(static_cast<std::streamoff>(position));
    }

    void seek_end(int64_t offset) {
        stream_.clear();
        stream_.seekg(static_cast<std::streamoff>(offset), std::ios::end);
        stream_.seekp(static_cast<std::streamoff>(offset), std::ios::end);
    }

    uint64_t read(void* out_bytes, uint64_t read_size) {
        if (!stream_.read(reinterpret_cast<char*>(out_bytes), static_cast<std::streamsize>(read_size))) {
            const std::streamsize got = stream_.gcount();
            return got > 0 ? static_cast<uint64_t>(got) : 0;
        }

        return read_size;
    }

    bool write(const void* bytes, uint64_t size) {
        return static_cast<bool>(stream_.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(size)));
    }

    void flush() {
        stream_.flush();
    }

private:
    std::fstream stream_;
};

File::File(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {
}

File::File() = default;

File::~File() = default;

File::File(File&&) = default;

File& File::operator=(File&&) = default;

bool File::is_open() const {
    return impl_ != nullptr && impl_->is_open();
}

bool File::eof() {
    return impl_ == nullptr || impl_->eof();
}

uint64_t File::size() {
    return impl_ != nullptr ? impl_->size() : 0;
}

uint64_t File::position() {
    return impl_ != nullptr ? impl_->position() : 0;
}

void File::seek(uint64_t position) const {
    if (impl_ != nullptr) {
        impl_->seek(position);
    }
}

void File::seek_end(int64_t offset) const {
    if (impl_ != nullptr) {
        impl_->seek_end(offset);
    }
}

uint64_t File::read(void* out_bytes, uint64_t read_size) const {
    return impl_ != nullptr ? impl_->read(out_bytes, read_size) : 0;
}

bool File::write(const void* bytes, uint64_t size) {
    return impl_ != nullptr && impl_->write(bytes, size);
}

void File::flush() {
    if (impl_ != nullptr) {
        impl_->flush();
    }
}

File File::open(const char* path, FileMode mode) {
    const std::string_view view(path);
    if (view.find(".zip/") != std::string_view::npos) {
        LOGE("File::open: zip not supported yet ('%s')", path);
        return File();
    }

    auto impl = std::make_unique<Impl>();
    if (!impl->open(path, mode)) {
        return File();
    }

    return File(std::move(impl));
}
