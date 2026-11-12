#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
namespace vellum {
struct script_result {
    bool success;
    std::string text;
};
using script_handler = std::function<script_result(std::string_view)>;
script_result run_script_process(const std::filesystem::path &worker, std::string_view source,
                                 std::chrono::milliseconds timeout = std::chrono::seconds(5));
int serve_script_request(const script_handler &execute);
} // namespace vellum
