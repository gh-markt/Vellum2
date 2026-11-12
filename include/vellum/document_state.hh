#pragma once
#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace vellum {
enum class document_mode { reader, editor };
class document_state {
  public:
    document_mode mode() const;
    bool enable_editor();
    void changed();
    bool needs_autosave() const;
    std::uint64_t revision() const;
    void autosaved(std::uint64_t revision);

  private:
    document_mode current_mode = document_mode::reader;
    std::uint64_t current_revision = 0;
    std::uint64_t autosave_revision = 0;
};
} // namespace vellum
