#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
namespace vellum {
enum class commit_point { before_rename, after_rename };
using commit_observer = std::function<void(commit_point)>;
void atomic_write(const std::filesystem::path &target, std::string_view bytes,
                  const commit_observer &observer = {});
std::filesystem::path create_private_directory(const std::filesystem::path &base);
std::string read_file(const std::filesystem::path &path, std::size_t limit = 536870912);
} // namespace vellum
