#include "vellum/document_state.hh"
#include <limits>
namespace vellum {
document_mode document_state::mode() const {
    return this->current_mode;
}
bool document_state::enable_editor() {
    if (this->current_mode == document_mode::editor) {
        return false;
    }
    this->current_mode = document_mode::editor;
    return true;
}
void document_state::changed() {
    if (this->current_revision == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("document revision exhausted");
    }
    ++this->current_revision;
}
bool document_state::needs_autosave() const {
    return this->current_mode == document_mode::editor &&
           this->current_revision != this->autosave_revision;
}
std::uint64_t document_state::revision() const {
    return this->current_revision;
}
void document_state::autosaved(std::uint64_t revision) {
    if (revision > this->current_revision || revision < this->autosave_revision) {
        throw std::logic_error("invalid autosave revision");
    }
    this->autosave_revision = revision;
}
} // namespace vellum
