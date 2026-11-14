#include "vellum/autosave.hh"
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <unistd.h>

enum class fault {
    none,
    interrupted_write,
    short_write,
    fragmented_write,
    partial_then_full,
    zero_write,
    temporary_file,
    disk_full,
    file_sync,
    rename,
    directory_sync
};
static fault active_fault = fault::none;
static int sync_calls = 0;
static int injected = 0;

extern "C" ssize_t __real_write(int fd, const void *data, size_t size);
extern "C" int __real_fsync(int fd);
extern "C" int __real_rename(const char *old_path, const char *new_path);
extern "C" int __real_mkostemp(char *pattern, int flags);
#if defined(__GLIBC__)
extern "C" int __real_mkostemp64(char *pattern, int flags);
#endif

extern "C" int __wrap_mkostemp(char *pattern, int flags) {
    if (active_fault == fault::temporary_file) {
        ++injected;
        errno = EMFILE;
        return -1;
    }
    return __real_mkostemp(pattern, flags);
}
#if defined(__GLIBC__)
extern "C" int __wrap_mkostemp64(char *pattern, int flags) {
    if (active_fault == fault::temporary_file) {
        ++injected;
        errno = EMFILE;
        return -1;
    }
    return __real_mkostemp64(pattern, flags);
}
#endif

extern "C" ssize_t __wrap_write(int fd, const void *data, size_t size) {
    if (active_fault == fault::fragmented_write) {
        ++injected;
        if (injected % 3 == 0) {
            errno = EINTR;
            return -1;
        }
        return __real_write(fd, data, std::min(size, size_t{2}));
    }
    if (active_fault == fault::partial_then_full) {
        if (injected++ == 0) {
            return __real_write(fd, data, std::min(size, size_t{3}));
        }
        errno = ENOSPC;
        return -1;
    }
    if (active_fault == fault::interrupted_write && injected++ == 0) {
        errno = EINTR;
        return -1;
    }
    if (active_fault == fault::short_write && injected++ == 0) {
        return __real_write(fd, data, std::min(size, size_t{3}));
    }
    if (active_fault == fault::disk_full && injected++ == 0) {
        errno = ENOSPC;
        return -1;
    }
    if (active_fault == fault::zero_write && injected++ == 0) {
        return 0;
    }
    return __real_write(fd, data, size);
}
extern "C" int __wrap_fsync(int fd) {
    ++sync_calls;
    if ((active_fault == fault::file_sync && sync_calls == 1) ||
        (active_fault == fault::directory_sync && sync_calls == 2)) {
        ++injected;
        errno = EIO;
        return -1;
    }
    return __real_fsync(fd);
}
extern "C" int __wrap_rename(const char *old_path, const char *new_path) {
    if (active_fault == fault::rename) {
        ++injected;
        errno = EIO;
        return -1;
    }
    return __real_rename(old_path, new_path);
}

namespace {
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void set_fault(fault next) {
    active_fault = next;
    sync_calls = 0;
    injected = 0;
}
void check_no_temporary_files(const std::filesystem::path &directory) {
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        require(entry.path().filename().string().find(".tmp.") == std::string::npos,
                "temporary file was not cleaned up");
    }
}
void check_retryable_write(const std::filesystem::path &target, fault selected) {
    set_fault(fault::none);
    vellum::atomic_write(target, "old");
    set_fault(selected);
    vellum::atomic_write(target, "replacement");
    require(injected > 0, "write fault was not injected");
    if (selected == fault::fragmented_write) {
        require(injected >= 4, "fragmented write did not exercise multiple retries");
    }
    require(vellum::read_file(target) == "replacement", "write retry lost bytes");
}
void check_failed_commit(const std::filesystem::path &target, fault selected,
                         const std::string &expected) {
    set_fault(fault::none);
    vellum::atomic_write(target, "old");
    set_fault(selected);
    bool failed = false;
    try {
        vellum::atomic_write(target, "replacement");
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed && injected > 0, "commit must report the injected failure");
    require(vellum::read_file(target) == expected, "wrong file visible after failed commit");
    check_no_temporary_files(target.parent_path());
    set_fault(fault::none);
    vellum::atomic_write(target, "retry");
    require(vellum::read_file(target) == "retry", "retry did not publish complete data");
}
void check_autosave_after_directory_sync_failure(const std::filesystem::path &target) {
    vellum::document_state state;
    state.enable_editor();
    state.changed();
    std::string content = "first";
    const auto serialize = [&content] { return content; };
    const auto validate = [](const std::string &bytes) {
        if (bytes.empty()) {
            throw std::runtime_error("empty snapshot");
        }
    };
    const auto now = vellum::autosave_controller::clock::now();
    vellum::autosave_controller controller(state, vellum::snapshot_store(target), serialize,
                                           validate, now);
    set_fault(fault::none);
    require(controller.trigger_now(now), "initial autosave failed");
    state.changed();
    content = "second";
    set_fault(fault::directory_sync);
    bool failed = false;
    try {
        controller.trigger_now(now);
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed && injected == 1, "directory sync failure was not reported");
    require(state.needs_autosave(), "failed durability must retain the dirty revision");
    require(vellum::read_file(target) == "second", "post-rename file must remain visible");
    set_fault(fault::none);
    require(controller.trigger_now(now), "retry must commit the pending revision");
    require(!state.needs_autosave(), "successful retry must clear the captured revision");
}
} // namespace

int main() {
    auto pattern =
        (std::filesystem::temp_directory_path() / "vellum-atomic-faults-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr) {
        return 2;
    }
    const std::filesystem::path directory(pattern);
    try {
        const auto target = directory / "snapshot.pdf";
        check_retryable_write(target, fault::interrupted_write);
        check_retryable_write(target, fault::short_write);
        check_retryable_write(target, fault::fragmented_write);
        check_failed_commit(target, fault::disk_full, "old");
        check_failed_commit(target, fault::partial_then_full, "old");
        check_failed_commit(target, fault::zero_write, "old");
        check_failed_commit(target, fault::temporary_file, "old");
        check_failed_commit(target, fault::file_sync, "old");
        check_failed_commit(target, fault::rename, "old");
        check_failed_commit(target, fault::directory_sync, "replacement");
        check_autosave_after_directory_sync_failure(target);
        std::filesystem::remove_all(directory);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(directory);
        return 1;
    }
}
