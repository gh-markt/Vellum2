#include "vellum/script_process.hh"
#include <cerrno>
#include <cstdarg>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__GLIBC__) && defined(__linux__)
namespace {
enum class failure { none, seal, first_duplicate, second_duplicate };
failure active = failure::none;
int duplicates = 0;
bool injected = false;
} // namespace
extern "C" int __real_fcntl64(int, int, ...);
extern "C" int __real_fcntl(int, int, ...);
namespace {
int intercept_fcntl(int fd, int command, int value, bool large_file) {
    if (command == F_DUPFD_CLOEXEC) {
        ++duplicates;
        if ((active == failure::first_duplicate && duplicates == 1) ||
            (active == failure::second_duplicate && duplicates == 2)) {
            injected = true;
            errno = EMFILE;
            return -1;
        }
    }
    if (command == F_ADD_SEALS && active == failure::seal) {
        injected = true;
        errno = EPERM;
        return -1;
    }
    return large_file ? __real_fcntl64(fd, command, value) : __real_fcntl(fd, command, value);
}
} // namespace
extern "C" int __wrap_fcntl64(int fd, int command, ...) {
    va_list arguments;
    va_start(arguments, command);
    const int value = va_arg(arguments, int);
    va_end(arguments);
    return intercept_fcntl(fd, command, value, true);
}
extern "C" int __wrap_fcntl(int fd, int command, ...) {
    va_list arguments;
    va_start(arguments, command);
    const int value = va_arg(arguments, int);
    va_end(arguments);
    return intercept_fcntl(fd, command, value, false);
}
namespace {
std::size_t descriptors() {
    std::size_t count = 0;
    for (const auto &entry : std::filesystem::directory_iterator("/proc/self/fd")) {
        (void)entry;
        ++count;
    }
    return count;
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        for (const auto selected :
             {failure::seal, failure::first_duplicate, failure::second_duplicate}) {
            const auto before = descriptors();
            active = selected;
            duplicates = 0;
            injected = false;
            bool failed = false;
            try {
                vellum::run_script_process(argv[1], "fault");
            } catch (const std::exception &) {
                failed = true;
            }
            active = failure::none;
            errno = 0;
            const bool no_children = ::waitpid(-1, nullptr, WNOHANG) == -1 && errno == ECHILD;
            if (!failed || !injected || descriptors() != before || !no_children ||
                vellum::run_script_process(argv[1], "retry").text != "retry") {
                throw std::runtime_error("fcntl failure did not clean up and allow retry");
            }
        }
        std::cout << "IPC sealing and descriptor duplication: 3 injected failures\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#else
int main() {
    return 77;
}
#endif
