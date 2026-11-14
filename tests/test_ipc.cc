#include "vellum/script_process.hh"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
namespace {
void require(bool value) {
    if (!value) {
        throw std::runtime_error("IPC assertion failed");
    }
}
template <class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected);
}
void descriptor_exhaustion(const std::filesystem::path &worker) {
    const auto child = ::fork();
    require(child >= 0);
    if (child == 0) {
        struct rlimit limit{};
        if (::getrlimit(RLIMIT_NOFILE, &limit) != 0) {
            std::_Exit(2);
        }
        const auto original = limit;
        limit.rlim_cur = 3;
        if (::setrlimit(RLIMIT_NOFILE, &limit) != 0) {
            std::_Exit(2);
        }
        bool rejected = false;
        try {
            vellum::run_script_process(worker, "resource exhaustion");
        } catch (const std::exception &) {
            rejected = true;
        }
        if (::setrlimit(RLIMIT_NOFILE, &original) != 0) {
            std::_Exit(2);
        }
        std::_Exit(rejected ? 0 : 1);
    }
    int status = 0;
    require(::waitpid(child, &status, 0) == child);
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        const std::filesystem::path worker(argv[1]);
        const std::string unicode = "héllo 世界";
        require(vellum::run_script_process(worker, unicode).text == unicode);
        require(vellum::run_script_process(worker, "").success);
        const auto failed = vellum::run_script_process(worker, "error");
        require(!failed.success && failed.text == "expected failure");
        for (const auto *bad : {"malformed magic", "malformed version", "malformed length",
                                "malformed status", "malformed short", "malformed long"}) {
            const auto malformed = [&] { vellum::run_script_process(worker, bad); };
            rejects(malformed);
        }
        const std::string maximum(1024 * 1024, 'x');
        require(vellum::run_script_process(worker, maximum).text == maximum);
        require(vellum::run_script_process(worker, "maximum output").text ==
                std::string(1024 * 1024, 'z'));
        const std::string binary("a\0b", 3);
        require(vellum::run_script_process(worker, binary).text == binary);
        rejects([&] { vellum::run_script_process(worker, maximum + "x"); });
        rejects([&] { vellum::run_script_process(worker, "x", std::chrono::milliseconds(0)); });
        rejects([&] { vellum::run_script_process(worker, "x", std::chrono::milliseconds(-1)); });
        rejects([&] {
            vellum::run_script_process(
                worker, "x",
                std::chrono::milliseconds(static_cast<long long>(std::numeric_limits<int>::max()) +
                                          1));
        });
        rejects([&] { vellum::run_script_process("/missing/vellum-worker", "x"); });
        descriptor_exhaustion(worker);
        rejects(
            [&] { vellum::run_script_process(worker, "hang", std::chrono::milliseconds(100)); });
        rejects(
            [&] { vellum::run_script_process(worker, "crash", std::chrono::milliseconds(100)); });
        require(vellum::run_script_process(worker, "after failure").text == "after failure");
        std::cout << "IPC positive, negative, boundary, timeout and crash checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
