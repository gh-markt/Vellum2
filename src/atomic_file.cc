#include "vellum/atomic_file.hh"
#include <cerrno>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <vector>
namespace vellum {
namespace {
class descriptor {
  public:
    explicit descriptor(int value) : value(value) {
        if (value < 0) {
            throw std::system_error(errno, std::generic_category(), "open file");
        }
    }
    ~descriptor() { ::close(this->value); }
    descriptor(const descriptor &) = delete;
    descriptor &operator=(const descriptor &) = delete;
    int get() const { return this->value; }

  private:
    int value;
};
void synchronize(int descriptor) {
    if (::fsync(descriptor) != 0) {
        throw std::system_error(errno, std::generic_category(), "fsync");
    }
}
void write_all(int descriptor, std::string_view bytes) {
    while (!bytes.empty()) {
        const auto count = ::write(descriptor, bytes.data(), bytes.size());
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            throw std::system_error(errno, std::generic_category(), "write");
        }
        bytes.remove_prefix(static_cast<std::size_t>(count));
    }
}
} // namespace
void atomic_write(const std::filesystem::path &target, std::string_view bytes,
                  const commit_observer &observer) {
    auto parent = target.parent_path();
    if (parent.empty()) {
        parent = ".";
    }
    const descriptor directory(::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    auto pattern = (parent / (target.filename().string() + ".tmp.XXXXXX")).string();
    std::vector<char> temporary(pattern.begin(), pattern.end());
    temporary.push_back('\0');
    const descriptor output(::mkostemp(temporary.data(), O_CLOEXEC));
    try {
        write_all(output.get(), bytes);
        synchronize(output.get());
        if (observer) {
            observer(commit_point::before_rename);
        }
        if (::rename(temporary.data(), target.c_str()) != 0) {
            throw std::system_error(errno, std::generic_category(), "rename");
        }
        if (observer) {
            observer(commit_point::after_rename);
        }
        synchronize(directory.get());
    } catch (...) {
        ::unlink(temporary.data());
        throw;
    }
}
std::filesystem::path create_private_directory(const std::filesystem::path &base) {
    std::filesystem::create_directories(base);
    auto pattern = (base / "recovery-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr) {
        throw std::system_error(errno, std::generic_category(), "create private directory");
    }
    try {
        return std::filesystem::path(pattern);
    } catch (...) {
        ::rmdir(pattern.c_str());
        throw;
    }
}
std::string read_file(const std::filesystem::path &path, std::size_t limit) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    std::string result;
    char block[8192];
    while (input.read(block, sizeof(block)) || input.gcount() > 0) {
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > limit - result.size()) {
            throw std::runtime_error("file exceeds size limit");
        }
        result.append(block, count);
    }
    if (!input.eof()) {
        throw std::runtime_error("file read failed");
    }
    return result;
}
} // namespace vellum
