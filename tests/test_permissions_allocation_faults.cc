#include "vellum/pdf_document.hh"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace {
bool armed = false;
std::size_t allocations = 0, fail_at = 0;
void *allocate(std::size_t size) {
    if (armed && allocations++ == fail_at) {
        armed = false;
        throw std::bad_alloc();
    }
    if (void *result = std::malloc(size ? size : 1)) {
        return result;
    }
    throw std::bad_alloc();
}
} // namespace
void *operator new(std::size_t size) {
    return allocate(size);
}
void *operator new[](std::size_t size) {
    return allocate(size);
}
void operator delete(void *value) noexcept {
    std::free(value);
}
void operator delete[](void *value) noexcept {
    std::free(value);
}
void operator delete(void *value, std::size_t) noexcept {
    std::free(value);
}
void operator delete[](void *value, std::size_t) noexcept {
    std::free(value);
}
namespace {
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        std::size_t total = 0;
        {
            vellum::pdf_document document(argv[1]);
            require(!document.may_edit() && !document.may_print(), "fixture lacks restrictions");
            allocations = 0;
            fail_at = static_cast<std::size_t>(-1);
            armed = true;
            const bool accepted = document.authenticate_owner("vellum-long-owner-password-test");
            armed = false;
            total = allocations;
            require(accepted && document.may_edit() && document.may_print(),
                    "owner authentication failed");
        }
        require(total > 0, "owner authentication did not allocate");
        for (std::size_t index = 0; index < total; ++index) {
            vellum::pdf_document document(argv[1]);
            const auto original = document.snapshot();
            allocations = 0;
            fail_at = index;
            armed = true;
            bool failed = false;
            bool accepted = false;
            try {
                accepted = document.authenticate_owner("vellum-long-owner-password-test");
            } catch (const std::bad_alloc &) {
                failed = true;
            }
            armed = false;
            require(allocations == index + 1, "owner allocation failure was not reached");
            if (failed) {
                require(!document.may_edit() && !document.may_print(),
                        "failed owner authentication granted permissions");
            } else {
                require(accepted && document.may_edit() && document.may_print(),
                        "handled owner authentication failure gave inconsistent permissions");
            }
            require(document.snapshot() == original, "owner authentication changed PDF bytes");
            if (failed) {
                require(document.authenticate_owner("vellum-long-owner-password-test"),
                        "owner retry failed");
            }
            require(document.may_edit() && document.may_print(),
                    "owner retry did not grant permissions");
            document.validate_recovery_snapshot(document.snapshot());
        }
        std::cout << "Owner authentication allocation failures: " << total << " cases\n";
    } catch (const std::exception &error) {
        armed = false;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
