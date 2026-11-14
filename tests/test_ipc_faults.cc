#include "vellum/script_process.hh"
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <poll.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace {
enum class failure {
    none,
    socket,
    memfd,
    truncate,
    mapping,
    spawn,
    spawn_actions,
    path_allocation,
    spawn_dup,
    spawn_dup_second,
    send,
    poll,
    poll_timeout,
    poll_deadline,
    poll_disconnect,
    send_short,
    receive_short,
    receive,
    send_interrupted,
    poll_interrupted,
    receive_interrupted,
    wait_interrupted
};
failure active = failure::none;
int injections = 0;
bool fail_allocation = false;
bool fail_error_construction = false;
int actions_initialized = 0;
int actions_destroyed = 0;
bool inject(failure selected) {
    if (active == selected && injections++ == 0) {
        return true;
    }
    return false;
}
} // namespace
void *operator new(std::size_t size) {
    if (fail_allocation) {
        fail_allocation = false;
        ++injections;
        throw std::bad_alloc();
    }
    if (void *result = std::malloc(size ? size : 1)) {
        return result;
    }
    throw std::bad_alloc();
}
void operator delete(void *value) noexcept {
    std::free(value);
}
void operator delete(void *value, std::size_t) noexcept {
    std::free(value);
}
extern "C" void __real__ZNSt13runtime_errorC1EPKc(std::runtime_error *, const char *);
extern "C" void __wrap__ZNSt13runtime_errorC1EPKc(std::runtime_error *error, const char *message) {
    if (fail_error_construction) {
        fail_error_construction = false;
        throw std::bad_alloc();
    }
    __real__ZNSt13runtime_errorC1EPKc(error, message);
}
extern "C" int __real_socketpair(int, int, int, int[2]);
extern "C" int __wrap_socketpair(int domain, int type, int protocol, int sockets[2]) {
    if (inject(failure::socket)) {
        errno = EMFILE;
        return -1;
    }
    return __real_socketpair(domain, type, protocol, sockets);
}
extern "C" int __real_posix_spawn_file_actions_init(posix_spawn_file_actions_t *);
extern "C" int __wrap_posix_spawn_file_actions_init(posix_spawn_file_actions_t *actions) {
    if (inject(failure::spawn_actions)) {
        return ENOMEM;
    }
    const int result = __real_posix_spawn_file_actions_init(actions);
    if (result == 0 && active == failure::path_allocation) {
        ++actions_initialized;
        fail_allocation = true;
    }
    return result;
}
extern "C" int __real_posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *);
extern "C" int __wrap_posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *actions) {
    if (active == failure::path_allocation) {
        ++actions_destroyed;
    }
    return __real_posix_spawn_file_actions_destroy(actions);
}
extern "C" int __real_posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *, int, int);
extern "C" int __wrap_posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *actions, int fd,
                                                       int target) {
    if (inject(failure::spawn_dup)) {
        return EMFILE;
    }
    if (active == failure::spawn_dup_second && ++injections == 2) {
        return EMFILE;
    }
    return __real_posix_spawn_file_actions_adddup2(actions, fd, target);
}
extern "C" int __real_memfd_create(const char *, unsigned int);
extern "C" int __wrap_memfd_create(const char *name, unsigned int flags) {
    if (inject(failure::memfd)) {
        errno = EMFILE;
        return -1;
    }
    return __real_memfd_create(name, flags);
}
#if defined(__GLIBC__)
extern "C" int __real_ftruncate64(int, off64_t);
extern "C" int __wrap_ftruncate64(int fd, off64_t size) {
    if (inject(failure::truncate)) {
        errno = ENOSPC;
        return -1;
    }
    return __real_ftruncate64(fd, size);
}
extern "C" void *__real_mmap64(void *, size_t, int, int, int, off64_t);
extern "C" void *__wrap_mmap64(void *address, size_t length, int protection, int flags, int fd,
                               off64_t offset) {
    if (inject(failure::mapping)) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    return __real_mmap64(address, length, protection, flags, fd, offset);
}
#endif
extern "C" int __real_ftruncate(int, off_t);
extern "C" int __wrap_ftruncate(int fd, off_t size) {
    if (inject(failure::truncate)) {
        errno = ENOSPC;
        return -1;
    }
    return __real_ftruncate(fd, size);
}
extern "C" void *__real_mmap(void *, size_t, int, int, int, off_t);
extern "C" void *__wrap_mmap(void *address, size_t length, int protection, int flags, int fd,
                             off_t offset) {
    if (inject(failure::mapping)) {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    return __real_mmap(address, length, protection, flags, fd, offset);
}
extern "C" int __real_posix_spawn(pid_t *, const char *, const posix_spawn_file_actions_t *,
                                  const posix_spawnattr_t *, char *const[], char *const[]);
extern "C" int __wrap_posix_spawn(pid_t *pid, const char *path,
                                  const posix_spawn_file_actions_t *actions,
                                  const posix_spawnattr_t *attributes, char *const arguments[],
                                  char *const environment[]) {
    if (inject(failure::spawn)) {
        return EAGAIN;
    }
    return __real_posix_spawn(pid, path, actions, attributes, arguments, environment);
}
extern "C" ssize_t __real_send(int, const void *, size_t, int);
extern "C" ssize_t __wrap_send(int fd, const void *data, size_t size, int flags) {
    if (inject(failure::send_short)) {
        return static_cast<ssize_t>(size - 1);
    }
    if (inject(failure::send)) {
        errno = EPIPE;
        return -1;
    }
    if (inject(failure::send_interrupted)) {
        errno = EINTR;
        return -1;
    }
    return __real_send(fd, data, size, flags);
}
extern "C" int __real_poll(struct pollfd *, nfds_t, int);
extern "C" int __wrap_poll(struct pollfd *fds, nfds_t count, int timeout) {
    if (inject(failure::poll_timeout)) {
        return 0;
    }
    if (inject(failure::poll_disconnect)) {
        fds[0].revents = POLLHUP;
        return 1;
    }
    if (inject(failure::poll_deadline)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        errno = EINTR;
        return -1;
    }
    if (inject(failure::poll)) {
        errno = EIO;
        return -1;
    }
    if (inject(failure::poll_interrupted)) {
        errno = EINTR;
        return -1;
    }
    return __real_poll(fds, count, timeout);
}
extern "C" ssize_t __real_recv(int, void *, size_t, int);
extern "C" ssize_t __wrap_recv(int fd, void *data, size_t size, int flags) {
    if (inject(failure::receive_short)) {
        return static_cast<ssize_t>(size - 1);
    }
    if (inject(failure::receive)) {
        errno = EIO;
        return -1;
    }
    if (inject(failure::receive_interrupted)) {
        errno = EINTR;
        return -1;
    }
    return __real_recv(fd, data, size, flags);
}
extern "C" pid_t __real_waitpid(pid_t, int *, int);
extern "C" pid_t __wrap_waitpid(pid_t pid, int *status, int options) {
    if (inject(failure::wait_interrupted)) {
        errno = EINTR;
        return -1;
    }
    return __real_waitpid(pid, status, options);
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
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        const std::filesystem::path worker(argv[1]);
        const auto descriptors_before_allocation_failure = descriptors();
        actions_initialized = actions_destroyed = injections = 0;
        active = failure::path_allocation;
        bool allocation_rejected = false;
        try {
            vellum::run_script_process(worker, "allocation failure");
        } catch (const std::bad_alloc &) {
            allocation_rejected = true;
        }
        active = failure::none;
        fail_allocation = false;
        require(allocation_rejected && injections == 1 && actions_initialized == 1 &&
                    actions_destroyed == 1,
                "spawn setup allocation failure did not clean up its actions");
        require(descriptors() == descriptors_before_allocation_failure,
                "spawn setup allocation failure leaked a descriptor");
        errno = 0;
        require(::waitpid(-1, nullptr, WNOHANG) == -1 && errno == ECHILD,
                "spawn setup allocation failure left a worker");
        require(vellum::run_script_process(worker, "retry").text == "retry",
                "spawn setup allocation failure prevented retry");
        for (const auto selected :
             {failure::socket, failure::memfd, failure::truncate, failure::mapping,
              failure::spawn_actions, failure::spawn_dup, failure::spawn_dup_second, failure::spawn,
              failure::send, failure::poll, failure::receive}) {
            const auto before = descriptors();
            active = selected;
            injections = 0;
            bool failed = false;
            bool poll_error_preserved = false;
            try {
                vellum::run_script_process(worker, "test");
            } catch (const std::system_error &error) {
                failed = true;
                poll_error_preserved = error.code().value() == EIO;
            } catch (const std::exception &) {
                failed = true;
            }
            active = failure::none;
            require(failed && injections > 0, "IPC fault was not injected and rejected");
            require(selected != failure::poll || poll_error_preserved,
                    "poll failure lost its system error");
            require(descriptors() == before, "IPC fault leaked a descriptor");
            errno = 0;
            require(::waitpid(-1, nullptr, WNOHANG) == -1 && errno == ECHILD,
                    "IPC fault left a running or unreaped worker");
            require(vellum::run_script_process(worker, "retry").text == "retry",
                    "IPC failure prevented subsequent requests");
        }
        for (const auto selected :
             {failure::poll_timeout, failure::poll_deadline, failure::poll_disconnect,
              failure::send_short, failure::receive_short}) {
            const auto before = descriptors();
            active = selected;
            injections = 0;
            std::string message;
            try {
                const auto timeout = selected == failure::poll_deadline
                                         ? std::chrono::milliseconds(100)
                                         : std::chrono::seconds(5);
                vellum::run_script_process(worker, "fault", timeout);
            } catch (const std::exception &error) {
                message = error.what();
            }
            active = failure::none;
            const auto expected = selected == failure::poll_disconnect ? "disconnected"
                                  : selected == failure::send_short    ? "send failed"
                                  : selected == failure::receive_short ? "invalid IPC packet"
                                                                       : "timed out";
            require(injections > 0 && message.find(expected) != std::string::npos,
                    "IPC deadline or packet fault did not report the expected error");
            require(descriptors() == before, "IPC deadline or packet fault leaked a descriptor");
            errno = 0;
            require(::waitpid(-1, nullptr, WNOHANG) == -1 && errno == ECHILD,
                    "IPC deadline or packet fault left a worker");
            require(vellum::run_script_process(worker, "retry").text == "retry",
                    "IPC deadline or packet fault prevented retry");
        }
        for (const auto selected :
             {failure::socket, failure::memfd, failure::mapping, failure::spawn_actions,
              failure::spawn, failure::send, failure::receive, failure::poll_timeout,
              failure::poll_disconnect}) {
            const auto before = descriptors();
            active = selected;
            injections = 0;
            fail_error_construction = true;
            bool failed = false;
            try {
                vellum::run_script_process(worker, "fault");
            } catch (const std::bad_alloc &) {
                failed = true;
            }
            active = failure::none;
            const bool constructor_was_reached = !fail_error_construction;
            fail_error_construction = false;
            require(failed && injections > 0 && constructor_was_reached,
                    "IPC error construction fault was not reached");
            require(descriptors() == before, "IPC error construction failure leaked a descriptor");
            errno = 0;
            require(::waitpid(-1, nullptr, WNOHANG) == -1 && errno == ECHILD,
                    "IPC error construction failure left a worker");
            require(vellum::run_script_process(worker, "retry").text == "retry",
                    "IPC error construction failure prevented retry");
        }
        for (const auto selected : {failure::send_interrupted, failure::poll_interrupted,
                                    failure::receive_interrupted, failure::wait_interrupted}) {
            const auto before = descriptors();
            active = selected;
            injections = 0;
            const auto result = vellum::run_script_process(worker, "interrupted");
            active = failure::none;
            require(injections > 0 && result.success && result.text == "interrupted",
                    "interrupted IPC operation failed to retry correctly");
            require(descriptors() == before, "interrupted IPC operation leaked a descriptor");
            errno = 0;
            require(::waitpid(-1, nullptr, WNOHANG) == -1 && errno == ECHILD,
                    "interrupted IPC operation left an unreaped worker");
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
