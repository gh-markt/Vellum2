#pragma once
#include "vellum/atomic_file.hh"
#include "vellum/document_state.hh"
#include <chrono>
#include <functional>
#include <string>
namespace vellum {
using snapshot_validator = std::function<void(const std::string &)>;
class snapshot_store {
  public:
    explicit snapshot_store(std::filesystem::path target);
    void commit(const std::string &bytes, const snapshot_validator &validate,
                const commit_observer &observer = {}) const;
    std::string recover(const snapshot_validator &validate) const;
    const std::filesystem::path &path() const;

  private:
    std::filesystem::path target;
};
class autosave_controller {
  public:
    using clock = std::chrono::steady_clock;
    using serializer = std::function<std::string()>;
    autosave_controller(document_state &state, snapshot_store store, serializer serialize,
                        snapshot_validator validate, clock::time_point now,
                        std::chrono::seconds interval = std::chrono::seconds(60));
    bool tick(clock::time_point now);
    bool trigger_now(clock::time_point now);

  private:
    document_state &state;
    snapshot_store store;
    serializer serialize;
    snapshot_validator validate;
    clock::time_point last_success;
    std::chrono::seconds interval;
};
} // namespace vellum
