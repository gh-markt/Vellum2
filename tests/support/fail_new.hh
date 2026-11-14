#pragma once
#include <cstdlib>
#include <new>

// Include in exactly one translation unit of a test executable. Each scenario
// owns the arming and assertions; this only supplies the shared interception.
namespace {
bool armed = false;
std::size_t allocations = 0;
std::size_t fail_at = 0;
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
