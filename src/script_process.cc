#include "vellum/script_process.hh"
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <poll.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
extern char **environ;
namespace vellum {
namespace {
constexpr std::size_t capacity = 1024 * 1024;
constexpr std::uint32_t magic = 0x564c4d31;
using packet = std::array<std::uint32_t, 4>;
class descriptor {
  public:
    explicit descriptor(int fd) : fd(fd) {
        if (fd < 0) {
            throw std::runtime_error("IPC descriptor creation failed");
        }
    }
    ~descriptor() { this->close(); }
    descriptor(const descriptor &) = delete;
    descriptor &operator=(const descriptor &) = delete;
    int get() const { return this->fd; }
    void close() {
        if (this->fd >= 0) {
            ::close(this->fd);
            this->fd = -1;
        }
    }

  private:
    int fd;
};
class mapping {
  public:
    explicit mapping(int fd)
        : memory(::mmap(nullptr, capacity, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) {
        if (this->memory == MAP_FAILED) {
            throw std::runtime_error("IPC mapping failed");
        }
    }
    ~mapping() { ::munmap(this->memory, capacity); }
    mapping(const mapping &) = delete;
    mapping &operator=(const mapping &) = delete;
    char *data() const { return static_cast<char *>(this->memory); }

  private:
    void *memory;
};
class child_process {
  public:
    child_process(const std::filesystem::path &executable, int socket, int memory) {
        const descriptor high_socket(::fcntl(socket, F_DUPFD_CLOEXEC, 10));
        const descriptor high_memory(::fcntl(memory, F_DUPFD_CLOEXEC, 10));
        posix_spawn_file_actions_t actions;
        if (::posix_spawn_file_actions_init(&actions) != 0) {
            throw std::runtime_error("spawn init");
        }
        struct actions_cleanup {
            posix_spawn_file_actions_t *value;
            ~actions_cleanup() { ::posix_spawn_file_actions_destroy(this->value); }
        } cleanup{&actions};
        const int first = ::posix_spawn_file_actions_adddup2(&actions, high_socket.get(), 3);
        const int second = ::posix_spawn_file_actions_adddup2(&actions, high_memory.get(), 4);
        std::string path = executable.string();
        char *arguments[] = {path.data(), nullptr};
        const int status = first != 0    ? first
                           : second != 0 ? second
                                         : ::posix_spawn(&this->pid, path.c_str(), &actions,
                                                         nullptr, arguments, environ);
        if (status != 0) {
            throw std::runtime_error("cannot start JavaScript worker");
        }
    }
    ~child_process() {
        if (this->pid > 0) {
            ::kill(this->pid, SIGKILL);
            while (::waitpid(this->pid, nullptr, 0) < 0 && errno == EINTR) {
            }
        }
    }
    child_process(const child_process &) = delete;
    child_process &operator=(const child_process &) = delete;

  private:
    pid_t pid = -1;
};
void send_packet(int socket, const packet &message) {
    ssize_t count;
    do {
        count = ::send(socket, message.data(), sizeof(message), MSG_NOSIGNAL);
    } while (count < 0 && errno == EINTR);
    if (count != static_cast<ssize_t>(sizeof(message))) {
        throw std::runtime_error("IPC send failed");
    }
}
packet receive_packet(int socket) {
    packet message{};
    ssize_t count;
    do {
        count = ::recv(socket, message.data(), sizeof(message), MSG_TRUNC);
    } while (count < 0 && errno == EINTR);
    if (count != static_cast<ssize_t>(sizeof(message)) || message[0] != magic || message[1] != 1 ||
        message[2] > capacity || message[3] > 1) {
        throw std::runtime_error("invalid IPC packet");
    }
    return message;
}
void wait_for_reply(int socket, std::chrono::milliseconds timeout) {
    if (timeout.count() <= 0 || timeout.count() > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("invalid worker timeout");
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    pollfd event{socket, POLLIN, 0};
    for (;;) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   deadline - std::chrono::steady_clock::now())
                                   .count();
        if (remaining <= 0) {
            throw std::runtime_error("JavaScript worker timed out");
        }
        const int ready = ::poll(&event, 1, static_cast<int>(remaining));
        if (ready < 0 && errno == EINTR) {
            continue;
        }
        if (ready < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "JavaScript worker poll failed");
        }
        if (ready == 0) {
            throw std::runtime_error("JavaScript worker timed out");
        }
        if ((event.revents & POLLIN) == 0) {
            throw std::runtime_error("JavaScript worker disconnected");
        }
        return;
    }
}
} // namespace
script_result run_script_process(const std::filesystem::path &worker, std::string_view source,
                                 std::chrono::milliseconds timeout) {
    if (source.size() > capacity || timeout.count() <= 0 ||
        timeout.count() > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("invalid JavaScript request size or timeout");
    }
    int sockets[2];
    if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0, sockets) != 0) {
        throw std::runtime_error("IPC socketpair failed");
    }
    const descriptor parent(sockets[0]);
    descriptor child(sockets[1]);
    const descriptor shared(::memfd_create("vellum-script", MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (::ftruncate(shared.get(), capacity) != 0 ||
        ::fcntl(shared.get(), F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0) {
        throw std::runtime_error("cannot size/seal IPC memory");
    }
    const mapping memory(shared.get());
    std::memcpy(memory.data(), source.data(), source.size());
    const child_process process(worker, child.get(), shared.get());
    child.close();
    send_packet(parent.get(), {magic, 1, static_cast<std::uint32_t>(source.size()), 0});
    wait_for_reply(parent.get(), timeout);
    const auto response = receive_packet(parent.get());
    return {response[3] == 0, std::string(memory.data(), response[2])};
}
int serve_script_request(const script_handler &execute) {
    try {
        const auto request = receive_packet(3);
        if (request[3] != 0 || !execute) {
            return 2;
        }
        const mapping memory(4);
        const std::string source(memory.data(), request[2]);
        const auto result = execute(source);
        if (result.text.size() > capacity) {
            return 2;
        }
        std::memcpy(memory.data(), result.text.data(), result.text.size());
        send_packet(3, {magic, 1, static_cast<std::uint32_t>(result.text.size()),
                        result.success ? 0U : 1U});
        return 0;
    } catch (...) {
        return 2;
    }
}
} // namespace vellum
