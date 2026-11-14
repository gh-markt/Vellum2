// Loaded only into the test worker; record security setup and inject failures.
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>

#if defined(__GLIBC__)
namespace {
void record(char event) {
    const char *descriptor = std::getenv("VELLUM_TEST_TRACE_FD");
    if (descriptor != nullptr) {
        const int fd = std::atoi(descriptor);
        (void)::write(fd, &event, 1);
    }
}
int limit_result(bool valid) {
    record(valid ? 'R' : 'r');
    const char *mode = std::getenv("VELLUM_TEST_SECURITY_FAILURE");
    if (mode != nullptr && std::strcmp(mode, "limit") == 0) {
        errno = EPERM;
        return -1;
    }
    return 0;
}
} // namespace

extern "C" int setrlimit(__rlimit_resource_t resource, const struct rlimit *limit) noexcept {
    return limit_result(resource == RLIMIT_CORE && limit->rlim_cur == 0 && limit->rlim_max == 0);
}

extern "C" int setrlimit64(__rlimit_resource_t resource, const struct rlimit64 *limit) noexcept {
    return limit_result(resource == RLIMIT_CORE && limit->rlim_cur == 0 && limit->rlim_max == 0);
}

extern "C" int prctl(int option, ...) noexcept {
    va_list arguments;
    va_start(arguments, option);
    const int value = va_arg(arguments, int);
    va_end(arguments);
    record(option == PR_SET_NO_NEW_PRIVS && value == 1 ? 'P' : 'p');
    errno = EPERM;
    return -1;
}
#endif
