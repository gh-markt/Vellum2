#include "vellum/main_frame.hh"
#include <stdexcept>
#include <wx/textdlg.h>
namespace vellum {
std::optional<main_frame::authorization> main_frame::request_authorization(document_action action) {
    if (!this->document) {
        return std::nullopt;
    }
    bool allowed = false;
    const char *operation = "";
    switch (action) {
    case document_action::edit:
        allowed = this->document->may_edit();
        operation = "enable Editor mode";
        break;
    case document_action::print:
        allowed = this->document->may_print();
        operation = "print";
        break;
    case document_action::save:
        allowed = this->document->may_save();
        operation = "save";
        break;
    case document_action::save_as:
        allowed = this->document->may_save_as();
        operation = "Save As";
        break;
    }
    if (allowed) {
        return authorization{};
    }
    const auto message = "PDF permissions prohibit this operation. Enter the owner password to " +
                         wxString::FromUTF8(operation) + ".";
    wxPasswordEntryDialog dialog(this, message, "PDF owner authorization");
    if (dialog.ShowModal() != wxID_OK) {
        return std::nullopt;
    }
    auto password = dialog.GetValue().ToStdString(wxConvUTF8);
    if (!this->document->owner_password_valid(password)) {
        wxMessageBox("Incorrect owner password. The document has not been changed.",
                     "Authorization failed", wxOK | wxICON_ERROR, this);
        return std::nullopt;
    }
    return authorization{true, std::move(password)};
}
void main_frame::commit_authorization(const authorization &authorization) {
    if (!authorization.owner_required) {
        return;
    }
    if (!this->document->authenticate_owner(authorization.password)) {
        throw std::runtime_error("owner authentication failed; operation was not performed");
    }
    this->update_actions();
}
} // namespace vellum
