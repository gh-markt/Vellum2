#include "vellum/script_process.hh"
#include <iostream>
#include <stdexcept>
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
        rejects([&] { vellum::run_script_process(worker, maximum + "x"); });
        rejects([&] { vellum::run_script_process(worker, "x", std::chrono::milliseconds(0)); });
        rejects([&] { vellum::run_script_process("/missing/vellum-worker", "x"); });
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
