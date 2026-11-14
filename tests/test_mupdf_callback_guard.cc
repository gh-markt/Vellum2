#include "vellum/callback_guard.hh"
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mupdf/fitz.h>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
struct conversion_result {
    bool fail;
    operator int() const {
        if (this->fail) {
            throw std::domain_error("conversion failed");
        }
        return 17;
    }
};
void scalar_tests() {
    std::exception_ptr error = nullptr;
    int calls = 0;
    bool should_fail = false;
    const auto consume = [&calls, &should_fail](std::unique_ptr<int> value) -> int {
        ++calls;
        if (should_fail) {
            throw std::runtime_error("first callback failure");
        }
        return *value;
    };
    auto first = std::make_unique<int>(5);
    require(vellum::guard_callback<int, -1>(error, consume, std::move(first)) == 5 && !first &&
                !error && calls == 1,
            "successful scalar callback did not forward its argument and return its result");
    should_fail = true;
    auto second = std::make_unique<int>(3);
    require(vellum::guard_callback<int, -1>(error, consume, std::move(second)) == -1 && error &&
                calls == 2,
            "scalar exception was not captured");
    auto owned = std::make_unique<int>(9);
    require(vellum::guard_callback<int, -1>(error, consume, std::move(owned)) == -1 && owned &&
                calls == 2,
            "pending exception did not suppress later callbacks and argument consumption");
    bool original = false;
    try {
        std::rethrow_exception(error);
    } catch (const std::runtime_error &caught) {
        original = std::string(caught.what()) == "first callback failure";
    }
    require(original, "first callback exception was replaced");
    error = nullptr;
    should_fail = false;
    require(vellum::guard_callback<int, -1>(error, consume, std::move(owned)) == 9 && !owned &&
                !error,
            "guard did not forward move-only arguments after reset");
    bool fail_conversion = false;
    bool fail_invocation = false;
    const auto conversion = [&fail_conversion, &fail_invocation] {
        if (fail_invocation) {
            throw std::logic_error("invocation failed");
        }
        return conversion_result{fail_conversion};
    };
    require(vellum::guard_callback<int, -7>(error, conversion) == 17 && !error,
            "successful result conversion failed");
    fail_conversion = true;
    require(vellum::guard_callback<int, -7>(error, conversion) == -7 && error,
            "result conversion exception was not contained");
    bool converted = false;
    try {
        std::rethrow_exception(error);
    } catch (const std::domain_error &caught) {
        converted = std::string(caught.what()) == "conversion failed";
    }
    require(converted, "conversion exception was not retained");
    fail_conversion = false;
    require(vellum::guard_callback<int, -7>(error, conversion) == -7 && error,
            "pending conversion exception was not retained");
    error = nullptr;
    fail_invocation = true;
    require(vellum::guard_callback<int, -7>(error, conversion) == -7 && error,
            "conversion callback invocation exception was not contained");
    bool invocation_failed = false;
    try {
        std::rethrow_exception(error);
    } catch (const std::logic_error &caught) {
        invocation_failed = std::string(caught.what()) == "invocation failed";
    }
    require(invocation_failed, "conversion callback invocation exception was not retained");
    error = nullptr;
    int value = 4;
    bool fail_pointer = false;
    const auto pointer = [&]() -> int * {
        if (fail_pointer) {
            throw std::runtime_error("pointer failure");
        }
        return &value;
    };
    require(vellum::guard_callback<int *, nullptr>(error, pointer) == &value && !error,
            "successful pointer callback returned the wrong value");
    fail_pointer = true;
    require(vellum::guard_callback<int *, nullptr>(error, pointer) == nullptr && error,
            "pointer callback did not return its failure sentinel");
    fail_pointer = false;
    require(vellum::guard_callback<int *, nullptr>(error, pointer) == nullptr && error,
            "pending pointer exception was not retained");
}
struct warning_state {
    std::exception_ptr error = nullptr;
    int invocations = 0;
    bool fail = false;
    std::string last_message;
};
extern "C" void guarded_warning(void *user, const char *message) noexcept {
    auto &state = *static_cast<warning_state *>(user);
    const auto handle = [&state, message] {
        ++state.invocations;
        state.last_message = message;
        if (state.fail) {
            throw std::runtime_error("warning callback failed");
        }
    };
    vellum::guard_callback(state.error, handle);
}
void mupdf_warning_tests() {
    const std::unique_ptr<fz_context, decltype(&fz_drop_context)> context(
        fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT), &fz_drop_context);
    require(static_cast<bool>(context), "cannot create MuPDF context");
    warning_state state;
    fz_set_warning_callback(context.get(), guarded_warning, &state);
    fz_warn(context.get(), "%s", "first warning");
    fz_flush_warnings(context.get());
    require(state.invocations == 1 && state.last_message == "first warning" && !state.error,
            "MuPDF warning callback did not receive the expected message");
    state.fail = true;
    fz_warn(context.get(), "%s", "failing warning");
    fz_flush_warnings(context.get());
    require(state.invocations == 2 && state.error, "MuPDF callback exception escaped or vanished");
    fz_warn(context.get(), "%s", "suppressed warning");
    fz_flush_warnings(context.get());
    require(state.invocations == 2 && state.last_message == "failing warning",
            "MuPDF invoked user logic while the first exception was pending");
    bool original = false;
    try {
        std::rethrow_exception(state.error);
    } catch (const std::runtime_error &caught) {
        original = std::string(caught.what()) == "warning callback failed";
    }
    require(original, "MuPDF callback exception could not be reported to C++");
    state.error = nullptr;
    state.fail = false;
    fz_warn(context.get(), "%s", "recovered warning");
    fz_flush_warnings(context.get());
    require(state.invocations == 3 && state.last_message == "recovered warning" && !state.error,
            "MuPDF callback did not recover after clearing the error");
    fz_set_warning_callback(context.get(), nullptr, nullptr);
}
struct allocator_state {
    std::exception_ptr error = nullptr;
    bool fail_allocation = false;
    bool fail_cleanup = false;
    int allocations = 0;
    int cleanups = 0;
};
struct allocate_action {
    allocator_state &state;
    void *previous;
    std::size_t size;
    void *operator()() const {
        ++this->state.allocations;
        if (this->state.fail_allocation) {
            throw std::bad_alloc();
        }
        return this->previous ? std::realloc(this->previous, this->size) : std::malloc(this->size);
    }
};
struct free_action {
    allocator_state &state;
    void *value;
    void operator()() const {
        ++this->state.cleanups;
        std::free(this->value);
        if (this->state.fail_cleanup) {
            throw std::runtime_error("cleanup callback failed");
        }
    }
};
extern "C" void *guarded_malloc(void *user, std::size_t size) noexcept {
    auto &state = *static_cast<allocator_state *>(user);
    const allocate_action operation{state, nullptr, size};
    return vellum::guard_callback<void *, nullptr>(state.error, operation);
}
extern "C" void *guarded_realloc(void *user, void *value, std::size_t size) noexcept {
    auto &state = *static_cast<allocator_state *>(user);
    const allocate_action operation{state, value, size};
    return vellum::guard_callback<void *, nullptr>(state.error, operation);
}
extern "C" void guarded_free(void *user, void *value) noexcept {
    auto &state = *static_cast<allocator_state *>(user);
    const free_action operation{state, value};
    vellum::guard_cleanup_callback(state.error, operation);
}
void mupdf_allocator_tests() {
    allocator_state state;
    fz_alloc_context allocator{&state, guarded_malloc, guarded_realloc, guarded_free};
    const std::unique_ptr<fz_context, decltype(&fz_drop_context)> context(
        fz_new_context(&allocator, nullptr, FZ_STORE_DEFAULT), &fz_drop_context);
    require(static_cast<bool>(context) && !state.error, "custom MuPDF allocator failed to start");
    void *existing = fz_malloc(context.get(), 64);
    require(existing != nullptr, "initial MuPDF allocation failed");
    state.fail_allocation = true;
    const int before_attempts = state.allocations;
    int mupdf_failed = 0;
    fz_var(mupdf_failed);
    fz_try(context.get()) {
        (void)fz_malloc(context.get(), 128);
    }
    fz_catch(context.get()) {
        int code = 0;
        fz_convert_error(context.get(), &code);
        mupdf_failed = 1;
    }
    require(mupdf_failed && state.error && state.allocations == before_attempts + 1,
            "allocator failure was not contained or retried user work after an exception");
    require(guarded_malloc(&state, 1) == nullptr && state.allocations == before_attempts + 1,
            "pending allocator error did not suppress additional allocation work");
    const int before_free = state.cleanups;
    fz_free(context.get(), existing);
    require(state.cleanups > before_free && state.error,
            "pending callback error prevented required MuPDF cleanup");
    bool original = false;
    try {
        std::rethrow_exception(state.error);
    } catch (const std::bad_alloc &) {
        original = true;
    }
    require(original, "allocator exception was not retained");
    state.error = nullptr;
    state.fail_allocation = false;
    void *retry = fz_malloc(context.get(), 64);
    retry = fz_realloc(context.get(), retry, 128);
    require(retry && !state.error, "MuPDF allocation did not recover after clearing the error");
    fz_free(context.get(), retry);

    state.fail_cleanup = true;
    guarded_free(&state, std::malloc(1));
    require(static_cast<bool>(state.error), "cleanup exception was not contained");
    const auto first = state.error;
    const int before_retry = state.cleanups;
    guarded_free(&state, std::malloc(1));
    require(state.cleanups == before_retry + 1 && state.error == first,
            "cleanup did not run or replaced the first exception");
    state.fail_cleanup = false;
    guarded_free(&state, std::malloc(1));
    require(state.cleanups == before_retry + 2 && state.error == first,
            "successful cleanup did not run with an exception pending");
    state.error = nullptr;
}
} // namespace
int main() {
    try {
        scalar_tests();
        mupdf_warning_tests();
        mupdf_allocator_tests();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
