#pragma once
#include <concepts>
#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

namespace vellum {
template <typename Result, Result Failure, typename Function, typename... Arguments>
    requires std::is_scalar_v<Result> && std::invocable<Function, Arguments...> &&
             std::convertible_to<std::invoke_result_t<Function, Arguments...>, Result>
Result guard_callback(std::exception_ptr &error, Function &&function,
                      Arguments &&...arguments) noexcept {
    if (!error) {
        try {
            return std::invoke(std::forward<Function>(function),
                               std::forward<Arguments>(arguments)...);
        } catch (...) {
            error = std::current_exception();
        }
    }
    return Failure;
}

template <typename Function, typename... Arguments>
    requires std::invocable<Function, Arguments...> &&
             std::is_void_v<std::invoke_result_t<Function, Arguments...>>
void guard_callback(std::exception_ptr &error, Function &&function,
                    Arguments &&...arguments) noexcept {
    if (!error) {
        try {
            std::invoke(std::forward<Function>(function), std::forward<Arguments>(arguments)...);
        } catch (...) {
            error = std::current_exception();
        }
    }
}
template <typename Function, typename... Arguments>
    requires std::invocable<Function, Arguments...> &&
             std::is_void_v<std::invoke_result_t<Function, Arguments...>>
void guard_cleanup_callback(std::exception_ptr &error, Function &&function,
                            Arguments &&...arguments) noexcept {
    try {
        std::invoke(std::forward<Function>(function), std::forward<Arguments>(arguments)...);
    } catch (...) {
        if (!error) {
            error = std::current_exception();
        }
    }
}
} // namespace vellum
