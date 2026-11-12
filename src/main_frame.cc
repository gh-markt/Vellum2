#include "vellum/main_frame.hh"
#include "vellum/printing.hh"
#include "vellum/version.hh"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/textdlg.h>
namespace vellum {
namespace {
std::filesystem::path utf8_path(const wxString &value) {
    const auto bytes = value.ToUTF8();
    return std::filesystem::path(std::string(bytes.data(), bytes.length()));
}
} // namespace
main_frame::main_frame()
    : wxFrame(nullptr, wxID_ANY, "Vellum " VELLUM_VERSION, wxDefaultPosition, wxSize(1000, 800)),
      timer(this), zoom_timer(this) {
    this->build_menus();
    this->CreateStatusBar();
    this->build_toolbar();
    this->canvas = new wxScrolledWindow(this);
    this->canvas->SetScrollRate(16, 16);
    this->canvas->SetName("viewer-canvas");
    this->bitmap = new wxStaticBitmap(this->canvas, wxID_ANY, wxNullBitmap);
    this->bitmap->SetName("viewer-bitmap");
    this->bind_actions();
    this->Bind(wxEVT_TIMER, &main_frame::on_timer, this, this->timer.GetId());
    this->Bind(wxEVT_TIMER, &main_frame::on_zoom_timer, this, this->zoom_timer.GetId());
    this->canvas->Bind(wxEVT_SIZE, &main_frame::on_resize, this);
    for (const auto type :
         {wxEVT_SCROLLWIN_TOP, wxEVT_SCROLLWIN_BOTTOM, wxEVT_SCROLLWIN_LINEUP,
          wxEVT_SCROLLWIN_LINEDOWN, wxEVT_SCROLLWIN_PAGEUP, wxEVT_SCROLLWIN_PAGEDOWN,
          wxEVT_SCROLLWIN_THUMBTRACK, wxEVT_SCROLLWIN_THUMBRELEASE}) {
        this->canvas->Bind(type, &main_frame::on_scroll, this);
    }
    this->canvas->Bind(wxEVT_MOUSEWHEEL, &main_frame::on_mouse_wheel, this);
    this->update_actions();
    this->timer.Start(1000);
    this->SetStatusText(wxString::FromUTF8("Open a PDF — Reader mode never autosaves"));
}
main_frame::~main_frame() {
    this->SetEvtHandlerEnabled(false);
    this->timer.Stop();
    this->zoom_timer.Stop();
    this->canvas->SetEvtHandlerEnabled(false);
    this->canvas->Unbind(wxEVT_SIZE, &main_frame::on_resize, this);
}
void main_frame::show_error(const std::exception &error) {
    wxMessageBox(wxString::FromUTF8(error.what()), "Vellum", wxOK | wxICON_ERROR, this);
}
void main_frame::open_path(const std::filesystem::path &path) {
    this->load_document(path, false);
}
void main_frame::open_document() {
    wxFileDialog dialog(this, "Open PDF", {}, {}, "PDF files (*.pdf)|*.pdf",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    try {
        this->open_path(utf8_path(dialog.GetPath()));
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}
void main_frame::recover_document() {
    wxFileDialog dialog(this, "Recover committed snapshot (temporary files are ignored)", {}, {},
                        "Committed snapshots (*.pdf)|*.pdf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) {
        return;
    }
    try {
        const auto path = utf8_path(dialog.GetPath());
        const snapshot_store store(path);
        // Opening below validates the complete snapshot using its authenticated password.
        this->load_document(path, true);
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}
void main_frame::load_document(const std::filesystem::path &path, bool recovered) {
    std::unique_ptr<pdf_document> candidate;
    try {
        candidate = std::make_unique<pdf_document>(path);
    } catch (const pdf_password_error &) {
        wxPasswordEntryDialog password(this, "Enter the password required to open this PDF.",
                                       "Open protected PDF");
        if (password.ShowModal() != wxID_OK) {
            return;
        }
        const auto text = password.GetValue().ToStdString(wxConvUTF8);
        candidate = std::make_unique<pdf_document>(path, std::string_view(text));
    }
    if (recovered) {
        candidate->validate_recovery_snapshot(candidate->snapshot());
    }
    this->autosave.reset();
    this->document = std::move(candidate);
    this->state = document_state{};
    this->page = 0;
    this->recovery_path.clear();
    if (recovered) {
        this->SetTitle(wxString::FromUTF8("Vellum — recovered snapshot (Reader mode)"));
    } else {
        this->SetTitle(wxString::FromUTF8("Vellum — ") +
                       wxString::FromUTF8(path.filename().string()));
    }
    this->page_control->SetRange(1, this->document->pages());
    this->update_actions();
    this->show_page();
}
void main_frame::show_page() {
    try {
        if (!this->document) {
            return;
        }
        const auto viewport = this->canvas->GetClientSize();
        if (viewport.x < 32 || viewport.y < 32) {
            return;
        }
        this->current_scale = calculate_view_scale(
            this->current_zoom_mode, this->document->dimensions(this->page),
            {static_cast<double>(viewport.x - 24), static_cast<double>(viewport.y - 24)},
            this->percentage);
        const auto extent = this->document->dimensions(this->page);
        const double total_width = std::ceil(extent.width * this->current_scale);
        const double total_height = std::ceil(extent.height * this->current_scale);
        if (!std::isfinite(total_width) || !std::isfinite(total_height) ||
            total_width > 100000000 || total_height > 100000000) {
            throw std::runtime_error("page exceeds viewer scrolling limits");
        }
        const int page_x = std::max(12, (viewport.x - static_cast<int>(total_width)) / 2);
        const int page_y = std::max(12, (viewport.y - static_cast<int>(total_height)) / 2);
        this->canvas->SetVirtualSize(std::max(static_cast<int>(total_width) + 24, viewport.x),
                                     std::max(static_cast<int>(total_height) + 24, viewport.y));
        const auto origin = this->canvas->CalcUnscrolledPosition(wxPoint(0, 0));
        const int left = std::max(page_x, origin.x), top = std::max(page_y, origin.y);
        const int right = std::min(page_x + static_cast<int>(total_width), origin.x + viewport.x);
        const int bottom = std::min(page_y + static_cast<int>(total_height), origin.y + viewport.y);
        if (right > left && bottom > top) {
            const page_rectangle region{
                (left - page_x) / this->current_scale, (top - page_y) / this->current_scale,
                std::min((right - left) / this->current_scale,
                         extent.width - (left - page_x) / this->current_scale),
                std::min((bottom - top) / this->current_scale,
                         extent.height - (top - page_y) / this->current_scale)};
            const auto rendered = this->document->render_region(
                this->page, region, static_cast<float>(this->current_scale));
            wxImage image(rendered.width, rendered.height);
            if (!image.IsOk()) {
                throw std::runtime_error("cannot allocate visible page image");
            }
            std::copy(rendered.rgb.begin(), rendered.rgb.end(), image.GetData());
            this->bitmap->SetBitmap(wxBitmap(image));
            this->bitmap->SetSize(rendered.width, rendered.height);
            const auto position = this->canvas->CalcScrolledPosition(wxPoint(
                page_x +
                    static_cast<int>(std::lround(rendered.raster_area.x * this->current_scale)),
                page_y +
                    static_cast<int>(std::lround(rendered.raster_area.y * this->current_scale))));
            this->bitmap->SetPosition(position);
            this->bitmap->Show();
        } else {
            this->bitmap->Hide();
        }
        this->page_control->SetValue(this->page + 1);
        this->SetStatusText(
            wxString::Format(wxString::FromUTF8("Page %d / %d — %.1f%% — %s"), this->page + 1,
                             this->document->pages(), this->current_scale * 100,
                             this->state.mode() == document_mode::reader ? "Reader" : "Editor"));
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}
void main_frame::enable_editor() {
    if (!this->document || this->state.mode() == document_mode::editor) {
        return;
    }
    try {
        const auto authorization = this->request_authorization(document_action::edit);
        if (!authorization) {
            return;
        }
        // Create an exclusive private directory; each document session has its own snapshot.
        const auto base = utf8_path(wxStandardPaths::Get().GetUserLocalDataDir());
        this->recovery_path = create_private_directory(base) / "snapshot.pdf";
        const auto serialize = [this] { return this->document->snapshot(); };
        const auto validate = [this](const std::string &bytes) {
            this->document->validate_recovery_snapshot(bytes);
        };
        auto controller = std::make_unique<autosave_controller>(
            this->state, snapshot_store(this->recovery_path), serialize, validate,
            autosave_controller::clock::now());
        this->commit_authorization(*authorization);
        this->state.enable_editor();
        this->autosave = std::move(controller);
        this->update_actions();
        this->show_page();
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}
void main_frame::save_copy() {
    if (!this->document) {
        return;
    }
    try {
        const auto authorization = this->request_authorization(document_action::save_as);
        if (!authorization) {
            return;
        }
        this->commit_authorization(*authorization);
        wxFileDialog dialog(this, "Save As", {}, {}, "PDF files (*.pdf)|*.pdf",
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() != wxID_OK) {
            return;
        }
        this->document->save_as(utf8_path(dialog.GetPath()));
        this->SetTitle(wxString::FromUTF8("Vellum — ") +
                       wxString::FromUTF8(this->document->path().filename().string()));
        this->update_actions();
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}
void main_frame::save_document() {
    if (!this->document) {
        return;
    }
    try {
        const auto authorization = this->request_authorization(document_action::save);
        if (!authorization) {
            return;
        }
        this->commit_authorization(*authorization);
        this->document->save();
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}
void main_frame::on_zoom_timer(wxTimerEvent &) {
    this->show_page();
}
void main_frame::on_resize(wxSizeEvent &event) {
    event.Skip();
    if (this->document) {
        this->zoom_timer.StartOnce(40);
    }
}
void main_frame::on_scroll(wxScrollWinEvent &event) {
    event.Skip();
    if (this->document) {
        this->zoom_timer.StartOnce(10);
    }
}
void main_frame::on_mouse_wheel(wxMouseEvent &event) {
    event.Skip();
    if (this->document) {
        this->zoom_timer.StartOnce(10);
    }
}
void main_frame::on_timer(wxTimerEvent &) {
    try {
        if (this->autosave) {
            this->autosave->tick(autosave_controller::clock::now());
        }
    } catch (const std::exception &error) {
        this->SetStatusText("Autosave failed: " + wxString::FromUTF8(error.what()));
    }
}
} // namespace vellum
