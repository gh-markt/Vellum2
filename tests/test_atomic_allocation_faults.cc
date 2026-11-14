#include "vellum/atomic_file.hh"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>

#include "support/fail_new.hh"

int main() {
    auto pattern =
        (std::filesystem::temp_directory_path() / "vellum-atomic-allocation-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr) {
        return 2;
    }
    const std::filesystem::path directory(pattern);
    try {
        const auto target = directory / "long-snapshot-name-for-allocation-testing.pdf";
        vellum::atomic_write(target, "old");
        allocations = 0;
        fail_at = static_cast<std::size_t>(-1);
        armed = true;
        vellum::atomic_write(target, "complete replacement");
        armed = false;
        const auto count = allocations;
        if (count == 0) {
            throw std::runtime_error("atomic write had no allocations");
        }
        for (std::size_t index = 0; index < count; ++index) {
            vellum::atomic_write(target, "old");
            allocations = 0;
            fail_at = index;
            armed = true;
            bool failed = false;
            try {
                vellum::atomic_write(target, "complete replacement");
            } catch (const std::bad_alloc &) {
                failed = true;
            }
            armed = false;
            if (!failed || allocations != index + 1) {
                throw std::runtime_error("allocation failure did not reach expected position " +
                                         std::to_string(index));
            }
            const auto visible = vellum::read_file(target);
            if (visible != "old" && visible != "complete replacement") {
                throw std::runtime_error("allocation failure exposed partial file content");
            }
            for (const auto &entry : std::filesystem::directory_iterator(directory)) {
                if (entry.path() != target) {
                    throw std::runtime_error("allocation " + std::to_string(index) +
                                             " left a temporary file");
                }
            }
            vellum::atomic_write(target, "retry");
            if (vellum::read_file(target) != "retry") {
                throw std::runtime_error("retry failed after allocation failure");
            }
        }
        std::cout << "Atomic-write allocation failures: " << count << " cases\n";
        const auto base = directory / "private-recovery";
        std::filesystem::create_directory(base);
        allocations = 0;
        fail_at = static_cast<std::size_t>(-1);
        armed = true;
        const auto created = vellum::create_private_directory(base);
        armed = false;
        const auto directory_count = allocations;
        std::filesystem::remove(created);
        if (directory_count == 0) {
            throw std::runtime_error("private-directory creation had no allocations");
        }
        for (std::size_t index = 0; index < directory_count; ++index) {
            allocations = 0;
            fail_at = index;
            armed = true;
            bool failed = false;
            try {
                (void)vellum::create_private_directory(base);
            } catch (const std::bad_alloc &) {
                failed = true;
            }
            armed = false;
            if (!failed || allocations != index + 1) {
                throw std::runtime_error("directory allocation failure did not reach position " +
                                         std::to_string(index));
            }
            if (!std::filesystem::is_empty(base)) {
                throw std::runtime_error("allocation " + std::to_string(index) +
                                         " left an unused recovery directory");
            }
            const auto retry = vellum::create_private_directory(base);
            if (!std::filesystem::is_directory(retry)) {
                throw std::runtime_error("private-directory retry failed");
            }
            std::filesystem::remove(retry);
        }
        std::cout << "Private-directory allocation failures: " << directory_count << " cases\n";
        std::filesystem::remove_all(directory);
    } catch (const std::exception &error) {
        armed = false;
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(directory);
        return 1;
    }
}
