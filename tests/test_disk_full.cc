#include "vellum/autosave.hh"
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
void verify_isolated_filesystem(const std::filesystem::path &root) {
    struct statfs filesystem{};
    struct stat current{};
    struct stat parent{};
    require(::statfs(root.c_str(), &filesystem) == 0 && ::stat(root.c_str(), &current) == 0 &&
                ::stat(root.parent_path().c_str(), &parent) == 0,
            "cannot inspect test filesystem");
    constexpr long tmpfs_magic = 0x01021994;
    require(filesystem.f_type == tmpfs_magic && current.st_dev != parent.st_dev &&
                filesystem.f_blocks > 0 && filesystem.f_bsize > 0 &&
                static_cast<unsigned long long>(filesystem.f_blocks) * filesystem.f_bsize <=
                    16ULL * 1024 * 1024 &&
                std::filesystem::is_empty(root),
            "test requires an empty, dedicated tmpfs mount of at most 16 MiB");
}
void exhaust_filesystem(const std::filesystem::path &path) {
    const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    require(fd >= 0, "cannot create bounded filesystem filler");
    const std::string block(4096, 'x');
    bool full = false;
    while (!full) {
        const auto count = ::write(fd, block.data(), block.size());
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0 && errno == ENOSPC) {
            full = true;
        } else if (count <= 0) {
            ::close(fd);
            throw std::runtime_error("unexpected error filling test filesystem");
        }
    }
    ::close(fd);
}
void exercise_disk_full(const std::filesystem::path &root) {
    const auto target = root / "snapshot.pdf";
    const auto filler = root / "filler";
    vellum::document_state state;
    state.enable_editor();
    state.changed();
    std::string content = "previous snapshot";
    const auto serialize = [&content] { return content; };
    const auto validate = [](const std::string &bytes) {
        if (bytes.empty()) {
            throw std::runtime_error("empty snapshot");
        }
    };
    const auto now = vellum::autosave_controller::clock::now();
    vellum::autosave_controller controller(state, vellum::snapshot_store(target), serialize,
                                           validate, now);
    require(controller.trigger_now(now), "initial autosave failed");
    state.changed();
    content = "replacement snapshot";
    exhaust_filesystem(filler);
    bool disk_full = false;
    try {
        controller.trigger_now(now);
    } catch (const std::system_error &error) {
        disk_full = error.code().value() == ENOSPC;
    }
    require(disk_full, "atomic write did not report ENOSPC");
    require(state.needs_autosave(), "disk full cleared the pending revision");
    require(vellum::read_file(target) == "previous snapshot", "disk full changed the snapshot");
    for (const auto &entry : std::filesystem::directory_iterator(root)) {
        require(entry.path() == target || entry.path() == filler, "temporary file was not cleaned");
    }
    require(std::filesystem::remove(filler), "could not release test filesystem space");
    require(controller.trigger_now(now), "retry did not commit pending revision");
    require(!state.needs_autosave(), "successful retry left the revision dirty");
    require(vellum::read_file(target) == "replacement snapshot",
            "retry failed after space release");
}
} // namespace

int main() {
    const char *configured = std::getenv("VELLUM_DISK_FULL_TEST_DIR");
    if (configured == nullptr || *configured == '\0') {
        std::cerr << "Dedicated bounded tmpfs not configured for disk-full integration test\n";
        return 1;
    }
    try {
        const auto root = std::filesystem::canonical(configured);
        verify_isolated_filesystem(root);
        exercise_disk_full(root);
        require(std::filesystem::remove(root / "snapshot.pdf") && std::filesystem::is_empty(root),
                "disk-full test did not clean its bounded filesystem");
        std::cout << "Verified ENOSPC preserves the snapshot and retry commits after space is freed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
