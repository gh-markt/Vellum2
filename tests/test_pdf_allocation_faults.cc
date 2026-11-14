#include "vellum/atomic_file.hh"
#include "vellum/pdf_document.hh"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <new>
#include <stdexcept>

#include "support/fail_new.hh"
namespace {
template <class Operation, class Check> std::size_t exercise(Operation operation, Check check) {
    allocations = 0;
    fail_at = static_cast<std::size_t>(-1);
    armed = true;
    operation();
    armed = false;
    const auto count = allocations;
    if (count == 0) {
        throw std::runtime_error("PDF operation made no observed C++ allocations");
    }
    check();
    for (std::size_t index = 0; index < count; ++index) {
        allocations = 0;
        fail_at = index;
        armed = true;
        bool failed = false;
        try {
            operation();
        } catch (const std::bad_alloc &) {
            failed = true;
        }
        armed = false;
        if (allocations != index + 1) {
            throw std::runtime_error("PDF allocation failure was not observed at " +
                                     std::to_string(index) + " (observed " +
                                     std::to_string(allocations) + ")");
        }
        (void)failed; // A callee may handle the failure; the same postconditions still apply.
        check();
        operation();
        check();
    }
    return count;
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    auto pattern =
        (std::filesystem::temp_directory_path() / "vellum-pdf-allocation-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr) {
        return 2;
    }
    const std::filesystem::path directory(pattern);
    try {
        const vellum::pdf_document document(argv[1]);
        const auto original = document.snapshot();
        const auto check = [&] {
            if (document.snapshot() != original || document.pages() != 2) {
                throw std::runtime_error("failed PDF operation changed the live document");
            }
        };
        const auto render = [&] {
            const auto image = document.render(0, 1);
            if (image.width != 200 || image.height != 200 || image.rgb[0] != 255 ||
                image.rgb[1] != 0) {
                throw std::runtime_error("full render differs from fixture");
            }
        };
        const auto region = [&] {
            const auto image = document.render_region(0, {10, 20, 30, 40}, 1);
            if (image.width != 30 || image.height != 40 || image.rgb[0] != 255 ||
                image.rgb[1] != 0) {
                throw std::runtime_error("region render differs from fixture");
            }
        };
        const auto validate = [&] { document.validate_recovery_snapshot(original); };
        const auto saved = directory / "long-name-for-pdf-copy-allocation-testing.pdf";
        const auto copy = [&] { document.save_copy(saved); };
        const auto check_copy = [&] {
            check();
            if (vellum::read_file(saved) != original) {
                throw std::runtime_error("failed save-copy changed the previously committed PDF");
            }
            for (const auto &entry : std::filesystem::directory_iterator(directory)) {
                if (entry.path() != saved) {
                    throw std::runtime_error("failed save-copy left a temporary file");
                }
            }
        };
        std::cout << "PDF full-render allocation failures: " << exercise(render, check)
                  << " cases\n";
        std::cout << "PDF region-render allocation failures: " << exercise(region, check)
                  << " cases\n";
        std::cout << "PDF recovery-validation allocation failures: " << exercise(validate, check)
                  << " cases\n";
        copy();
        std::cout << "PDF save-copy allocation failures: " << exercise(copy, check_copy)
                  << " cases\n";
        std::filesystem::remove_all(directory);
    } catch (const std::exception &error) {
        armed = false;
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(directory);
        return 1;
    }
}
