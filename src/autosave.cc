#include "vellum/autosave.hh"
#include <stdexcept>
#include <utility>
namespace vellum {
snapshot_store::snapshot_store(std::filesystem::path target) : target(std::move(target)) {
    if (this->target.extension() != ".pdf") {
        throw std::invalid_argument("committed recovery snapshot must end in .pdf");
    }
}
void snapshot_store::commit(const std::string &bytes, const snapshot_validator &validate,
                            const commit_observer &observer) const {
    if (!validate) {
        throw std::invalid_argument("snapshot validator required");
    }
    validate(bytes);
    atomic_write(this->target, bytes, observer);
}
std::string snapshot_store::recover(const snapshot_validator &validate) const {
    if (!validate) {
        throw std::invalid_argument("snapshot validator required");
    }
    const auto bytes = read_file(this->target);
    validate(bytes);
    return bytes;
}
const std::filesystem::path &snapshot_store::path() const {
    return this->target;
}
autosave_controller::autosave_controller(document_state &state, snapshot_store store,
                                         serializer serialize, snapshot_validator validate,
                                         clock::time_point now, std::chrono::seconds interval)
    : state(state), store(std::move(store)), serialize(std::move(serialize)),
      validate(std::move(validate)), last_success(now), interval(interval) {
    if (interval.count() <= 0 || !this->serialize || !this->validate) {
        throw std::invalid_argument("invalid autosave configuration");
    }
}
bool autosave_controller::tick(clock::time_point now) {
    if (now - this->last_success < this->interval) {
        return false;
    }
    return this->trigger_now(now);
}
bool autosave_controller::trigger_now(clock::time_point now) {
    if (!this->state.needs_autosave()) {
        return false;
    }
    const auto revision = this->state.revision();
    this->store.commit(this->serialize(), this->validate);
    this->state.autosaved(revision);
    this->last_success = now;
    return true;
}
} // namespace vellum
