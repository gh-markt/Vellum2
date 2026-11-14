#include "vellum/main_frame.hh"
#include "vellum/printing.hh"
#include "vellum/version.hh"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <unistd.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/textdlg.h>

namespace vellum {
class viewer_canvas : public wxScrolledWindow {
  public:
    explicit viewer_canvas(main_frame *owner) : wxScrolledWindow(owner), owner(owner) {}

  private:
    void on_size(wxSizeEvent &event) { this->owner->on_resize(event); }
    void on_scroll(wxScrollWinEvent &event) { this->owner->on_scroll(event); }
    void on_wheel(wxMouseEvent &event) { this->owner->on_mouse_wheel(event); }
    wxDECLARE_EVENT_TABLE();
    main_frame *owner;
};

// clang-format off
wxBEGIN_EVENT_TABLE(viewer_canvas, wxScrolledWindow)
    EVT_SIZE(viewer_canvas::on_size)
    EVT_SCROLLWIN_TOP(viewer_canvas::on_scroll)
    EVT_SCROLLWIN_BOTTOM(viewer_canvas::on_scroll)
    EVT_SCROLLWIN_LINEUP(viewer_canvas::on_scroll)
    EVT_SCROLLWIN_LINEDOWN(viewer_canvas::on_scroll)
    EVT_SCROLLWIN_PAGEUP(viewer_canvas::on_scroll)
    EVT_SCROLLWIN_PAGEDOWN(viewer_canvas::on_scroll)
    EVT_SCROLLWIN_THUMBTRACK(viewer_canvas::on_scroll)
    EVT_SCROLLWIN_THUMBRELEASE(viewer_canvas::on_scroll)
    EVT_MOUSEWHEEL(viewer_canvas::on_wheel)
wxEND_EVENT_TABLE()
    // clang-format on

    namespace {

    struct recovery_directory_rollback {
        const std::filesystem::path &directory;
        bool committed = false;
        ~recovery_directory_rollback() {
            if (!this->committed) {
                ::rmdir(this->directory.c_str());
            }
        }
    };

    std::filesystem::path utf8_path(const wxString &value) {
        const auto bytes = value.ToUTF8();
        return std::filesystem::path(std::string(bytes.data(), bytes.length()));
    }

    void configure_scrollbars(wxScrolledWindow & canvas, zoom_mode mode) {
        switch (mode) {
        case zoom_mode::fit_page:
            canvas.ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_NEVER);
            canvas.Scroll(0, 0);
            break;
        case zoom_mode::fit_width:
            canvas.ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_ALWAYS);
            canvas.Scroll(0, -1);
            break;
        case zoom_mode::fit_height:
            canvas.ShowScrollbars(wxSHOW_SB_ALWAYS, wxSHOW_SB_NEVER);
            canvas.Scroll(-1, 0);
            break;
        case zoom_mode::percentage:
            canvas.ShowScrollbars(wxSHOW_SB_DEFAULT, wxSHOW_SB_DEFAULT);
            break;
        }
    }

    struct page_layout {
        double scale = 1;
        wxSize viewport;
        wxPoint position;
        wxSize pixels;
    };

    page_layout calculate_page_layout(wxScrolledWindow & canvas, zoom_mode mode, page_extent extent,
                                      double percentage) {
        configure_scrollbars(canvas, mode);

        const int margin = canvas.FromDIP(12);
        const int border = margin * 2;
        page_layout result;

        // Recheck after virtual-size changes, including native scrollbar layout.
        // Fit modes have fixed scrollbar visibility, avoiding a visibility/zoom cycle.
        for (int attempt = 0; attempt < 3; ++attempt) {
            result.viewport = canvas.GetClientSize();
            if (result.viewport.x <= border || result.viewport.y <= border) {
                return result;
            }

            result.scale = calculate_view_scale(mode, extent,
                                                {static_cast<double>(result.viewport.x - border),
                                                 static_cast<double>(result.viewport.y - border)},
                                                percentage);

            const double width = std::ceil(extent.width * result.scale);
            const double height = std::ceil(extent.height * result.scale);
            if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 ||
                width > 100000000 || height > 100000000) {
                throw std::runtime_error("page exceeds viewer scrolling limits");
            }

            result.pixels = wxSize(static_cast<int>(width), static_cast<int>(height));
            result.position = wxPoint(std::max(margin, (result.viewport.x - result.pixels.x) / 2),
                                      std::max(margin, (result.viewport.y - result.pixels.y) / 2));

            const wxSize virtual_size(std::max(result.pixels.x + border, result.viewport.x),
                                      std::max(result.pixels.y + border, result.viewport.y));

            if (canvas.GetVirtualSize() != virtual_size) {
                canvas.SetVirtualSize(virtual_size);
            }

            if (canvas.GetClientSize() == result.viewport) {
                break;
            }
        }

        return result;
    }

    void display_visible_region(wxScrolledWindow & canvas, wxStaticBitmap & bitmap,
                                pdf_document & document, int page, page_extent extent,
                                const page_layout &layout) {
        const auto origin = canvas.CalcUnscrolledPosition(wxPoint(0, 0));
        const int left = std::max(layout.position.x, origin.x);
        const int top = std::max(layout.position.y, origin.y);
        const int right =
            std::min(layout.position.x + layout.pixels.x, origin.x + layout.viewport.x);
        const int bottom =
            std::min(layout.position.y + layout.pixels.y, origin.y + layout.viewport.y);

        if (right <= left || bottom <= top) {
            bitmap.Hide();
            return;
        }

        const double source_x = (left - layout.position.x) / layout.scale;
        const double source_y = (top - layout.position.y) / layout.scale;
        const page_rectangle region{
            source_x, source_y, std::min((right - left) / layout.scale, extent.width - source_x),
            std::min((bottom - top) / layout.scale, extent.height - source_y)};

        const auto rendered =
            document.render_region(page, region, static_cast<float>(layout.scale));
        wxImage image(rendered.width, rendered.height);
        if (!image.IsOk()) {
            throw std::runtime_error("cannot allocate visible page image");
        }

        std::copy(rendered.rgb.begin(), rendered.rgb.end(), image.GetData());
        bitmap.SetBitmap(wxBitmap(image));
        bitmap.SetSize(rendered.width, rendered.height);

        const wxPoint virtual_position(
            layout.position.x +
                static_cast<int>(std::lround(rendered.raster_area.x * layout.scale)),
            layout.position.y +
                static_cast<int>(std::lround(rendered.raster_area.y * layout.scale)));

        bitmap.SetPosition(canvas.CalcScrolledPosition(virtual_position));
        bitmap.Show();
    }

} // namespace

main_frame::main_frame()
    : wxFrame(nullptr, wxID_ANY, "Vellum " VELLUM_VERSION, wxDefaultPosition, wxDefaultSize),
      timer(this, autosave_timer_id), zoom_timer(this, zoom_timer_id) {
    this->SetClientSize(this->FromDIP(wxSize(1000, 800)));
    this->build_menus();
    if (this->CreateStatusBar() == nullptr) {
        throw std::runtime_error("cannot create status bar");
    }
    this->build_toolbar();

    this->canvas = new viewer_canvas(this);
    const int scroll_step = this->canvas->FromDIP(16);
    this->canvas->SetScrollRate(scroll_step, scroll_step);
    this->canvas->SetName("viewer-canvas");

    this->bitmap = new wxStaticBitmap(this->canvas, wxID_ANY, wxNullBitmap);
    this->bitmap->SetName("viewer-bitmap");

    this->update_actions();
    this->timer.Start(1000);
    this->SetStatusText(wxString::FromUTF8("Open a PDF — Reader mode never autosaves"));
}

main_frame::~main_frame() {
    this->SetEvtHandlerEnabled(false);
    this->timer.Stop();
    this->zoom_timer.Stop();
    this->canvas->SetEvtHandlerEnabled(false);
}

void main_frame::show_error(const std::exception &error) {
    try {
        wxMessageBox(wxString::FromUTF8(error.what()), "Vellum", wxOK | wxICON_ERROR, this);
    } catch (...) {
        std::fprintf(stderr, "Vellum: %s\n", error.what());
    }
}

void main_frame::open_path(const std::filesystem::path &path) {
    this->load_document(path, false);
}

void main_frame::open_document() {
    try {
        wxFileDialog dialog(this, "Open PDF", {}, {}, "PDF files (*.pdf)|*.pdf",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() != wxID_OK) {
            return;
        }
        this->open_path(utf8_path(dialog.GetPath()));
    } catch (const std::exception &error) {
        this->show_error(error);
    }
}

void main_frame::recover_document() {
    try {
        wxFileDialog dialog(this, "Recover committed snapshot (temporary files are ignored)", {},
                            {}, "Committed snapshots (*.pdf)|*.pdf",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() != wxID_OK) {
            return;
        }
        const auto path = utf8_path(dialog.GetPath());
        const snapshot_store store(path);
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

    const auto title =
        recovered ? wxString::FromUTF8("Vellum — recovered snapshot (Reader mode)")
                  : wxString::FromUTF8("Vellum — ") + wxString::FromUTF8(path.filename().string());
    const auto previous_title = this->GetTitle();
    const int previous_min = this->page_control->GetMin();
    const int previous_max = this->page_control->GetMax();
    const int previous_value = this->page_control->GetValue();
    try {
        this->page_control->SetRange(1, candidate->pages());
        this->SetTitle(title);
    } catch (...) {
        bool rollback_failed = false;
        try {
            this->page_control->SetRange(previous_min, previous_max);
            this->page_control->SetValue(previous_value);
        } catch (...) {
            rollback_failed = true;
        }
        try {
            this->SetTitle(previous_title);
        } catch (...) {
            rollback_failed = true;
        }
        if (rollback_failed) {
            std::throw_with_nested(std::runtime_error("cannot restore previous document controls"));
        }
        throw;
    }
    this->autosave.reset();
    this->document = std::move(candidate);
    this->state = document_state{};
    this->page = 0;
    this->recovery_path.clear();
    this->canvas->Scroll(0, 0);

    this->update_actions();
    this->show_page();
}

void main_frame::show_page() {
    const auto update_status = [this] {
        this->page_control->SetValue(this->page + 1);
        this->SetStatusText(
            wxString::Format(wxString::FromUTF8("Page %d / %d — %.1f%% — %s"), this->page + 1,
                             this->document->pages(), this->current_scale * 100,
                             this->state.mode() == document_mode::reader ? "Reader" : "Editor"));
    };
    try {
        if (!this->document) {
            return;
        }

        const int minimum_viewport = this->canvas->FromDIP(32);
        const auto viewport = this->canvas->GetClientSize();
        if (viewport.x < minimum_viewport || viewport.y < minimum_viewport) {
            return;
        }

        const auto extent = this->document->dimensions(this->page);
        const auto layout =
            calculate_page_layout(*this->canvas, this->current_zoom_mode, extent, this->percentage);
        if (layout.pixels.x <= 0 || layout.pixels.y <= 0) {
            return;
        }

        this->current_scale = layout.scale;
        update_status();
        display_visible_region(*this->canvas, *this->bitmap, *this->document, this->page, extent,
                               layout);
    } catch (const std::exception &error) {
        this->bitmap->Hide();
        try {
            update_status();
        } catch (...) {
            // A second failure can prevent a status update; keep the failed image hidden.
        }
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

        const auto base = utf8_path(wxStandardPaths::Get().GetUserLocalDataDir());
        const auto directory = create_private_directory(base);
        recovery_directory_rollback rollback{directory};
        auto recovery_path = directory / "snapshot.pdf";

        const auto serialize = [this] { return this->document->snapshot(); };
        const auto validate = [this](const std::string &bytes) {
            this->document->validate_recovery_snapshot(bytes);
        };

        auto controller = std::make_unique<autosave_controller>(
            this->state, snapshot_store(recovery_path), serialize, validate,
            autosave_controller::clock::now());

        this->commit_authorization(*authorization);
        this->state.enable_editor();
        this->autosave = std::move(controller);
        this->recovery_path.swap(recovery_path);
        rollback.committed = true;
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
        try {
            this->SetStatusText("Autosave failed: " + wxString::FromUTF8(error.what()));
        } catch (...) {
            this->show_error(error);
        }
    }
}

} // namespace vellum
