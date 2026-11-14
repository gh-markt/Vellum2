#include "vellum/atomic_file.hh"
#include "vellum/pdf_document.hh"
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <system_error>
#include <unistd.h>

namespace {
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void exhaust(const std::filesystem::path &path) {
    const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    require(fd >= 0, "cannot create filler");
    const std::string block(4096, 'x');
    for (;;) {
        const auto count = ::write(fd, block.data(), block.size());
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0 && errno == ENOSPC) {
            break;
        }
        if (count <= 0) {
            ::close(fd);
            throw std::runtime_error("unexpected filler write failure");
        }
    }
    ::close(fd);
}
} // namespace

int main() {
    const char *configured = std::getenv("VELLUM_DISK_FULL_TEST_DIR");
    const char *reference_path = std::getenv("VELLUM_DISK_FULL_REFERENCE");
    const char *old_path = std::getenv("VELLUM_DISK_FULL_EXISTING");
    if (!configured || !*configured || !reference_path || !*reference_path || !old_path ||
        !*old_path) {
        std::cerr << "bounded tmpfs and two PDF fixtures are required\n";
        return 2;
    }
    const auto root = std::filesystem::canonical(configured);
    const auto source = root / "source.pdf";
    const auto target = root / "destination.pdf";
    const auto filler = root / "filler";
    int result = 0;
    try {
        struct statfs fs{};
        struct stat current{}, parent{};
        require(::statfs(root.c_str(), &fs) == 0 && ::stat(root.c_str(), &current) == 0 &&
                    ::stat(root.parent_path().c_str(), &parent) == 0 && fs.f_type == 0x01021994 &&
                    current.st_dev != parent.st_dev && fs.f_blocks > 0 && fs.f_bsize > 0 &&
                    static_cast<unsigned long long>(fs.f_blocks) * fs.f_bsize <=
                        16ULL * 1024 * 1024 &&
                    std::filesystem::is_empty(root),
                "test requires an empty dedicated tmpfs no larger than 16 MiB");
        const auto reference = vellum::read_file(reference_path);
        const auto old = vellum::read_file(old_path);
        const auto marker = reference.rfind("startxref\n");
        require(marker != std::string::npos && old != reference, "invalid test fixtures");
        const auto xref_begin = marker + std::string("startxref\n").size();
        const auto xref_end = reference.find('\n', xref_begin);
        require(xref_end != std::string::npos, "fixture lacks startxref value");
        const auto padded = reference + "\n%" + std::string(8192, 'a') + "\nstartxref\n" +
                            reference.substr(xref_begin, xref_end - xref_begin) + "\n%%EOF\n";
        require(padded.size() > static_cast<std::size_t>(fs.f_bsize), "fixture is too small");
        vellum::atomic_write(source, padded);
        vellum::atomic_write(target, old);
        vellum::pdf_document document(source);
        vellum::pdf_document previous(target);
        require(document.pages() == 2 && previous.pages() > 0 && document.snapshot() == padded,
                "initial PDF fixtures did not open");
        exhaust(filler);
        for (const bool partial : {false, true}) {
            if (partial) {
                const auto size = std::filesystem::file_size(filler);
                require(size >= static_cast<std::uintmax_t>(fs.f_bsize), "filler is too small");
                std::filesystem::resize_file(filler,
                                             size - static_cast<std::uintmax_t>(fs.f_bsize));
            }
            bool enospc = false;
            try {
                document.save_as(target);
            } catch (const std::system_error &error) {
                enospc = error.code().value() == ENOSPC;
            }
            require(enospc && document.path() == source && document.snapshot() == padded &&
                        vellum::read_file(source) == padded && vellum::read_file(target) == old,
                    "failed Save As changed document identity or existing bytes");
            for (const auto &entry : std::filesystem::directory_iterator(root)) {
                require(entry.path() == source || entry.path() == target || entry.path() == filler,
                        "failed Save As left a temporary file");
            }
        }
        require(std::filesystem::remove(filler), "cannot release filler");
        document.save_as(target);
        require(document.path() == target && document.snapshot() == padded &&
                    vellum::read_file(source) == padded && vellum::read_file(target) == padded,
                "Save As retry failed to publish the complete PDF");
        vellum::pdf_document reopened(target);
        require(reopened.pages() == 2 && reopened.snapshot() == padded,
                "Save As retry did not produce a readable PDF");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::error_code ignored;
    std::filesystem::remove(filler, ignored);
    std::filesystem::remove(target, ignored);
    std::filesystem::remove(source, ignored);
    return result;
}
