#include "vellum/main_frame.hh"
#include "vellum/print_dialog.hh"
#include <algorithm>
#include <stdexcept>
#include <wx/artprov.h>
#include <wx/textdlg.h>
namespace vellum {
void main_frame::build_menus() {
    auto file = std::make_unique<wxMenu>();
    file->Append(open_id, "&Open PDF...\tCtrl+O");
    file->Append(recover_id, "&Recover snapshot...");
    file->Append(wxID_SAVE, "&Save\tCtrl+S");
    file->Append(wxID_SAVEAS, "Save &As...\tCtrl+Shift+S");
    file->Append(editor_id, "Enable &editor mode for this document");
    file->Append(unlock_id, "Unlock PDF permissions...");
    file->Append(wxID_PRINT, "&Print...\tCtrl+P");
    file->Append(wxID_EXIT, "E&xit");
    auto navigation = std::make_unique<wxMenu>();
    navigation->Append(previous_id, "Previous page\tAlt+Left");
    navigation->Append(next_id, "Next page\tAlt+Right");
    auto view = std::make_unique<wxMenu>();
    view->Append(zoom_in_id, "Zoom in\tCtrl++");
    view->Append(zoom_out_id, "Zoom out\tCtrl+-");
    view->Append(actual_id, "100%\tCtrl+1");
    view->Append(fit_page_id, "Fit page\tCtrl+0");
    view->Append(fit_width_id, "Fit width\tCtrl+2");
    view->Append(fit_height_id, "Fit height\tCtrl+3");
    auto menus = std::make_unique<wxMenuBar>();
    if (!menus->Append(file.get(), "&File")) {
        throw std::runtime_error("cannot add File menu");
    }
    file.release();
    if (!menus->Append(navigation.get(), "&Navigate")) {
        throw std::runtime_error("cannot add Navigate menu");
    }
    navigation.release();
    if (!menus->Append(view.get(), "&View")) {
        throw std::runtime_error("cannot add View menu");
    }
    view.release();
    this->SetMenuBar(menus.get());
    menus.release();
}
void main_frame::build_toolbar() {
    auto *tools = this->CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT);
    if (tools == nullptr) {
        throw std::runtime_error("cannot create toolbar");
    }
    tools->AddTool(open_id, "Open", wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR));
    tools->AddTool(previous_id, "Previous", wxArtProvider::GetBitmap(wxART_GO_BACK, wxART_TOOLBAR));
    tools->AddTool(next_id, "Next", wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR));
    this->page_control = new wxSpinCtrl(tools, page_control_id, {}, wxDefaultPosition,
                                        wxSize(80, -1), wxSP_ARROW_KEYS, 1, 1, 1);
    this->page_control->SetName("viewer-page");
    tools->AddControl(this->page_control);
    this->zoom_control =
        new wxComboBox(tools, zoom_control_id, "Fit page", wxDefaultPosition, wxSize(125, -1), 0,
                       nullptr, wxCB_DROPDOWN | wxTE_PROCESS_ENTER);
    this->zoom_control->SetName("viewer-zoom");
    for (const auto *label : {"Fit page", "Fit width", "Fit height", "25%", "50%", "75%", "100%",
                              "125%", "150%", "200%", "400%", "800%"}) {
        this->zoom_control->Append(label);
    }
    tools->AddControl(this->zoom_control);
    tools->AddTool(wxID_SAVE, "Save", wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR));
    tools->AddTool(wxID_PRINT, "Print", wxArtProvider::GetBitmap(wxART_PRINT, wxART_TOOLBAR));
    tools->Realize();
}
// clang-format off
wxBEGIN_EVENT_TABLE(main_frame, wxFrame)
    EVT_MENU(open_id, main_frame::on_open)
    EVT_MENU(recover_id, main_frame::on_recover)
    EVT_MENU(wxID_SAVE, main_frame::on_save)
    EVT_MENU(wxID_SAVEAS, main_frame::on_save_as)
    EVT_MENU(editor_id, main_frame::on_editor)
    EVT_MENU(previous_id, main_frame::on_previous)
    EVT_MENU(next_id, main_frame::on_next)
    EVT_MENU(wxID_PRINT, main_frame::on_print)
    EVT_MENU(wxID_EXIT, main_frame::on_exit)
    EVT_MENU(unlock_id, main_frame::on_unlock)
    EVT_MENU(fit_page_id, main_frame::on_fit_page)
    EVT_MENU(fit_width_id, main_frame::on_fit_width)
    EVT_MENU(fit_height_id, main_frame::on_fit_height)
    EVT_MENU(actual_id, main_frame::on_actual_size)
    EVT_MENU(zoom_in_id, main_frame::on_zoom_in)
    EVT_MENU(zoom_out_id, main_frame::on_zoom_out)
    EVT_COMBOBOX(zoom_control_id, main_frame::on_zoom_control)
    EVT_TEXT_ENTER(zoom_control_id, main_frame::on_zoom_control)
    EVT_SPINCTRL(page_control_id, main_frame::on_page_control)
    EVT_TIMER(autosave_timer_id, main_frame::on_timer)
    EVT_TIMER(zoom_timer_id, main_frame::on_zoom_timer)
wxEND_EVENT_TABLE()
    // clang-format on

    void main_frame::on_open(wxCommandEvent &) {
    this->open_document();
}
void main_frame::on_recover(wxCommandEvent &) {
    this->recover_document();
}
void main_frame::on_save(wxCommandEvent &) {
    this->save_document();
}
void main_frame::on_save_as(wxCommandEvent &) {
    this->save_copy();
}
void main_frame::on_editor(wxCommandEvent &) {
    this->enable_editor();
}
void main_frame::on_previous(wxCommandEvent &) {
    if (this->document && this->page > 0) {
        this->go_to_page(this->page);
    }
}
void main_frame::on_next(wxCommandEvent &) {
    if (this->document && this->page + 1 < this->document->pages()) {
        this->go_to_page(this->page + 2);
    }
}
void main_frame::on_print(wxCommandEvent &) {
    this->print_document();
}
void main_frame::on_exit(wxCommandEvent &) {
    this->Close();
}
void main_frame::on_unlock(wxCommandEvent &) {
    this->unlock_permissions();
}
void main_frame::on_fit_page(wxCommandEvent &) {
    this->apply_zoom_menu(zoom_mode::fit_page);
}
void main_frame::on_fit_width(wxCommandEvent &) {
    this->apply_zoom_menu(zoom_mode::fit_width);
}
void main_frame::on_fit_height(wxCommandEvent &) {
    this->apply_zoom_menu(zoom_mode::fit_height);
}
void main_frame::on_actual_size(wxCommandEvent &) {
    this->apply_zoom_menu(zoom_mode::percentage, 100);
}
void main_frame::on_zoom_in(wxCommandEvent &) {
    this->apply_zoom_menu(zoom_mode::percentage, std::min(800.0, this->current_scale * 125));
}
void main_frame::on_zoom_out(wxCommandEvent &) {
    this->apply_zoom_menu(zoom_mode::percentage, std::max(1.0, this->current_scale * 80));
}
void main_frame::on_zoom_control(wxCommandEvent &) {
    this->apply_zoom_control();
}
void main_frame::on_page_control(wxSpinEvent &) {
    this->go_to_page(this->page_control->GetValue());
}
void main_frame::update_actions() {
    const bool opened = static_cast<bool>(this->document);
    this->GetMenuBar()->Enable(wxID_PRINT, opened);
    this->GetToolBar()->EnableTool(wxID_PRINT, opened);
    this->GetMenuBar()->Enable(wxID_SAVE, opened);
    this->GetToolBar()->EnableTool(wxID_SAVE, opened);
    this->GetMenuBar()->Enable(wxID_SAVEAS, opened);
    this->GetMenuBar()->Enable(editor_id, opened && this->state.mode() == document_mode::reader);
    this->GetMenuBar()->Enable(unlock_id, opened);
    this->page_control->Enable(opened);
    this->zoom_control->Enable(opened);
}
void main_frame::set_zoom(zoom_mode mode, double percentage) {
    if (!this->document) {
        return;
    }
    calculate_view_scale(mode, this->document->dimensions(this->page), {100, 100}, percentage);
    const auto label = mode == zoom_mode::fit_page     ? "Fit page"
                       : mode == zoom_mode::fit_width  ? "Fit width"
                       : mode == zoom_mode::fit_height ? "Fit height"
                                                       : wxString::Format("%.1f%%", percentage);
    this->zoom_control->SetValue(label);
    this->current_zoom_mode = mode;
    this->percentage = percentage;
    this->show_page();
}
void main_frame::apply_zoom_menu(zoom_mode mode, double percentage) {
    try {
        this->set_zoom(mode, percentage);
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}
void main_frame::apply_zoom_control() {
    try {
        auto text = this->zoom_control->GetValue().Trim().Trim(false);
        if (text.CmpNoCase("Fit page") == 0) {
            this->set_zoom(zoom_mode::fit_page);
            return;
        }
        if (text.CmpNoCase("Fit width") == 0) {
            this->set_zoom(zoom_mode::fit_width);
            return;
        }
        if (text.CmpNoCase("Fit height") == 0) {
            this->set_zoom(zoom_mode::fit_height);
            return;
        }
        if (text.EndsWith("%")) {
            text.RemoveLast();
        }
        double value = 0;
        if (!text.ToDouble(&value)) {
            throw std::invalid_argument("enter a zoom percentage from 1 to 800");
        }
        this->set_zoom(zoom_mode::percentage, value);
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}
void main_frame::go_to_page(int page) {
    if (!this->document || page < 1 || page > this->document->pages()) {
        return;
    }
    this->page = page - 1;
    this->canvas->Scroll(0, 0);
    this->show_page();
}
double main_frame::zoom_scale() const {
    return this->current_scale;
}
zoom_mode main_frame::zoom_policy() const {
    return this->current_zoom_mode;
}
int main_frame::current_page() const {
    return this->page + 1;
}
void main_frame::print_document() {
    if (!this->document) {
        return;
    }
    try {
        const auto authorization = this->request_authorization(document_action::print);
        if (!authorization) {
            return;
        }
        this->commit_authorization(*authorization);
        print_dialog dialog(this, *this->document, this->printer, this->page + 1);
        dialog.ShowModal();
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}

} // namespace vellum

namespace vellum {
void main_frame::unlock_permissions() {
    if (!this->document) {
        return;
    }
    try {
        wxPasswordEntryDialog dialog(this,
                                     "Enter the PDF owner password to unlock its permissions.",
                                     "Unlock PDF permissions");
        if (dialog.ShowModal() != wxID_OK) {
            return;
        }
        const auto password = dialog.GetValue().ToStdString(wxConvUTF8);
        if (!this->document->authenticate_owner(std::string_view(password))) {
            throw std::runtime_error("This password does not grant owner privileges");
        }
        this->update_actions();
        this->SetStatusText("PDF owner password authenticated; permissions updated");
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}
} // namespace vellum
