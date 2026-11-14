#include "vellum/print_dialog.hh"
#include "vellum/atomic_file.hh"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <wx/dcbuffer.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
namespace vellum {
class print_preview_panel : public wxPanel {
  public:
    print_preview_panel(wxWindow *parent, const print_job &job, sheet_painter &painter,
                        const bool &valid, const std::size_t &side)
        : wxPanel(parent), job(job), painter(painter), valid(valid), side(side) {
        this->SetMinSize(wxSize(380, 400));
        this->SetBackgroundStyle(wxBG_STYLE_PAINT);
    }

  private:
    void on_size(wxSizeEvent &event) {
        event.Skip();
        this->Refresh();
    }
    void on_paint(wxPaintEvent &) {
        try {
            wxAutoBufferedPaintDC dc(this);
            dc.SetBackground(wxBrush(wxColour(95, 100, 108)));
            dc.Clear();
            if (!this->valid) {
                return;
            }
            try {
                const auto size = this->GetClientSize();
                const auto paper = this->job.options.paper;
                const double scale =
                    std::min((size.x - 24) / paper.width, (size.y - 24) / paper.height);
                if (scale <= 0) {
                    return;
                }
                const int width = static_cast<int>(paper.width * scale);
                const int height = static_cast<int>(paper.height * scale);
                this->painter.paint(
                    dc, this->job, this->side,
                    wxRect((size.x - width) / 2, (size.y - height) / 2, width, height), 72, true);
            } catch (const std::exception &error) {
                dc.SetTextForeground(*wxWHITE);
                dc.DrawText("Preview failed: " + wxString::FromUTF8(error.what()), 12, 12);
            }
        } catch (const std::exception &error) {
            try {
                wxLogError("Preview failed: %s", wxString::FromUTF8(error.what()));
            } catch (...) {
                // Reporting can itself fail under resource exhaustion.
            }
        }
    }
    wxDECLARE_EVENT_TABLE();
    const print_job &job;
    sheet_painter &painter;
    const bool &valid;
    const std::size_t &side;
};
// clang-format off
wxBEGIN_EVENT_TABLE(print_preview_panel, wxPanel)
    EVT_SIZE(print_preview_panel::on_size)
    EVT_PAINT(print_preview_panel::on_paint)
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(print_dialog, wxDialog)
    EVT_CHOICE(wxID_ANY, print_dialog::on_change)
    EVT_TEXT(wxID_ANY, print_dialog::on_change)
    EVT_CHECKBOX(wxID_ANY, print_dialog::on_change)
    EVT_SPINCTRL(wxID_ANY, print_dialog::on_spin)
    EVT_SPINCTRLDOUBLE(wxID_ANY, print_dialog::on_decimal_spin)
    EVT_TIMER(wxID_ANY, print_dialog::on_preview_timer)
    EVT_BUTTON(wxID_SETUP, print_dialog::on_properties)
    EVT_BUTTON(wxID_PRINT, print_dialog::on_submit)
    EVT_BUTTON(wxID_SAVEAS, print_dialog::on_save_preview)
    EVT_BUTTON(wxID_BACKWARD, print_dialog::on_previous_side)
    EVT_BUTTON(wxID_FORWARD, print_dialog::on_next_side)
wxEND_EVENT_TABLE()
    // clang-format on

    namespace {
    void report_print_error(wxWindow * parent, const char *caption, const std::exception &error) {
        try {
            wxMessageBox(wxString::FromUTF8(error.what()), wxString::FromUTF8(caption),
                         wxOK | wxICON_ERROR, parent);
        } catch (...) {
            std::fprintf(stderr, "%s: %s\n", caption, error.what());
        }
    }
    wxChoice *choice(wxWindow * parent, wxFlexGridSizer * grid, const char *label, const char *name,
                     std::initializer_list<const char *> values, int initial = 0) {
        grid->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto *control = new wxChoice(parent, wxID_ANY);
        for (const auto *text : values) {
            control->Append(text);
        }
        control->SetSelection(initial);
        control->SetName(name);
        grid->Add(control, 1, wxEXPAND);
        return control;
    }
    wxSpinCtrlDouble *decimal(wxWindow * parent, wxFlexGridSizer * grid, const char *label,
                              const char *name, double value, double maximum) {
        grid->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto *control = new wxSpinCtrlDouble(parent, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                                             wxSP_ARROW_KEYS, 0, maximum, value, 1);
        control->SetDigits(1);
        control->SetName(name);
        grid->Add(control, 1, wxEXPAND);
        return control;
    }
    wxSpinCtrl *integer(wxWindow * parent, wxFlexGridSizer * grid, const char *label,
                        const char *name, int value, int maximum) {
        grid->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto *control = new wxSpinCtrl(parent, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                                       wxSP_ARROW_KEYS, 1, maximum, value);
        control->SetName(name);
        grid->Add(control, 1, wxEXPAND);
        return control;
    }
    wxCheckBox *check(wxWindow * parent, wxFlexGridSizer * grid, const char *label,
                      const char *name, bool value = false) {
        auto *control = new wxCheckBox(parent, wxID_ANY, label);
        control->SetValue(value);
        control->SetName(name);
        grid->AddSpacer(1);
        grid->Add(control, 0, wxEXPAND);
        return control;
    }
} // namespace
print_dialog::print_dialog(wxWindow *parent, pdf_document &document, printer_settings &printer,
                           int current_page)
    : wxDialog(parent, wxID_ANY, wxString::FromUTF8("Print — Vellum"), wxDefaultPosition,
               wxSize(1060, 800), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      document(document), printer(printer), current_page(current_page), painter(document),
      timer(this) {
    require_print_permission(document, 72);
    for (int page = 0; page < document.pages(); ++page) {
        this->dimensions.push_back(document.dimensions(page));
    }
    auto root = std::make_unique<wxBoxSizer>(wxVERTICAL);
    auto content = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    auto *controls = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(370, 500));
    controls->SetScrollRate(0, 12);
    controls->SetName("print-settings-panel");
    controls->ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_ALWAYS);
    auto grid = std::make_unique<wxFlexGridSizer>(2, 6, 8);
    grid->AddGrowableCol(1);
    this->build_controls(controls, grid.get());
    auto settings_padding = std::make_unique<wxBoxSizer>(wxVERTICAL);
    settings_padding->Add(grid.get(), 0, wxEXPAND | wxALL, controls->FromDIP(16));
    grid.release();
    auto *padding = settings_padding.get();
    controls->SetSizer(padding);
    settings_padding.release();
    controls->SetMinSize(wxSize(padding->GetMinSize().x + controls->FromDIP(24), -1));
    controls->FitInside();
    content->Add(controls, 0, wxEXPAND | wxALL, 12);
    this->preview = new print_preview_panel(this, this->current_job, this->painter,
                                            this->plan_valid, this->preview_side);
    content->Add(this->preview, 1, wxEXPAND | wxALL, 12);
    root->Add(content.get(), 1, wxEXPAND);
    content.release();
    this->build_footer(root.get());
    this->SetSizer(root.get());
    root.release();
    this->Layout();
    controls->FitInside();
    this->SetMinSize(wxSize(880, 650));
    this->controls_ready = true;
    this->regenerate_preview();
}
print_dialog::~print_dialog() {
    this->timer.Stop();
    this->SetEvtHandlerEnabled(false);
    delete this->preview;
    this->preview = nullptr;
}
void print_dialog::build_controls(wxWindow *controls, wxFlexGridSizer *grid) {
    this->selection = choice(controls, grid, "Pages", "print-selection",
                             {"All pages", "Current page", "Custom ranges"});
    grid->Add(new wxStaticText(controls, wxID_ANY, "Ranges"), 0, wxALIGN_CENTER_VERTICAL);
    this->ranges = new wxTextCtrl(controls, wxID_ANY);
    this->ranges->SetName("print-ranges");
    this->ranges->SetToolTip("Enter pages in print order. Consecutive commas insert blank pages.");
    grid->Add(this->ranges, 1, wxEXPAND);
    this->parity = choice(controls, grid, "Within selection", "print-parity",
                          {"All pages", "Odd pages", "Even pages"});
    this->layout = choice(controls, grid, "Layout", "print-layout",
                          {"Single page", "N-up / multiple", "Booklet", "Poster / tiles"});
    this->scaling = choice(controls, grid, "Page size", "print-scaling",
                           {"Fit", "Shrink oversized", "Actual size", "Custom scale"});
    this->percentage = decimal(controls, grid, "Scale %", "print-percentage", 100, 1000);
    this->paper = choice(controls, grid, "Paper", "print-paper",
                         {"A4", "Letter", "A3", "Legal", "Printer properties"});
    if (this->printer.selected()) {
        this->paper->SetSelection(4);
        this->paper->Disable();
    }
    this->orientation =
        choice(controls, grid, "Orientation", "print-orientation", {"Portrait", "Landscape"});
    this->margin = decimal(controls, grid, "Margins (mm)", "print-margin", 10, 100);
    this->columns = integer(controls, grid, "N-up columns", "print-columns", 2, 10);
    this->rows = integer(controls, grid, "N-up rows", "print-rows", 1, 10);
    this->order = choice(controls, grid, "N-up order", "print-order",
                         {"Across, left to right", "Down, top to bottom", "Across, right to left",
                          "Down, bottom to top"});
    this->gutter = decimal(controls, grid, "Gutter (mm)", "print-gutter", 0, 50);
    this->overlap = decimal(controls, grid, "Tile overlap (mm)", "print-overlap", 5, 100);
    this->duplex =
        choice(controls, grid, "Two-sided", "print-duplex",
               {"One-sided", "Duplex: long edge", "Duplex: short edge", "Manual duplex"});
    this->sides = choice(controls, grid, "Sheet sides", "print-sides",
                         {"All sides", "Fronts only", "Backs only"});
    this->copies = integer(controls, grid, "Copies", "print-copies", 1, 100);
    this->resolution = choice(controls, grid, "Raster DPI", "print-resolution", {"72", "150"}, 1);
    if (this->document.may_print_high_quality()) {
        this->resolution->Append("300");
        this->resolution->Append("600");
    }
    this->grayscale = check(controls, grid, "Grayscale", "print-grayscale");
    this->collate = check(controls, grid, "Collate copies", "print-collate", true);
    this->reverse = check(controls, grid, "Reverse selected pages", "print-reverse");
    this->rotate = check(controls, grid, "Auto-rotate to fit", "print-rotate", true);
    this->binding = check(controls, grid, "Right-edge booklet binding", "print-binding");
    this->borders = check(controls, grid, "Page cell borders", "print-borders");
    this->cut_marks = check(controls, grid, "Poster cut marks", "print-cut-marks");
    this->labels = check(controls, grid, "Poster tile labels", "print-labels", true);
    this->reverse_backs =
        check(controls, grid, "Reverse manual back-side order", "print-reverse-backs");
}
void print_dialog::build_footer(wxBoxSizer *root) {
    auto navigation = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    auto *previous = new wxButton(this, wxID_BACKWARD, "Previous side");
    auto *next = new wxButton(this, wxID_FORWARD, "Next side");
    this->side_label = new wxStaticText(this, wxID_ANY, {});
    navigation->Add(previous, 0, wxRIGHT, 8);
    navigation->Add(this->side_label, 1, wxALIGN_CENTER_VERTICAL);
    navigation->Add(next, 0, wxLEFT, 8);
    root->Add(navigation.get(), 0, wxEXPAND | wxLEFT | wxRIGHT, 12);
    navigation.release();
    this->status = new wxStaticText(this, wxID_ANY, {});
    root->Add(this->status, 0, wxEXPAND | wxALL, 12);
    auto actions = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
    auto *properties = new wxButton(this, wxID_SETUP, "Printer & properties...");
    auto *save = new wxButton(this, wxID_SAVEAS, "Save preview image...");
    this->print_button = new wxButton(this, wxID_PRINT, "Print");
    this->print_button->SetName("print-submit");
    actions->Add(properties, 0, wxRIGHT, 8);
    actions->Add(save, 0, wxRIGHT, 8);
    actions->AddStretchSpacer();
    actions->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxRIGHT, 8);
    actions->Add(this->print_button);
    root->Add(actions.get(), 0, wxEXPAND | wxALL, 12);
    actions.release();
}
void print_dialog::on_change(wxCommandEvent &event) {
    if (!this->controls_ready) {
        event.Skip();
        return;
    }
    if (event.GetEventObject() == this->layout) {
        if (this->layout->GetSelection() == 2) {
            this->orientation->SetSelection(1);
        }
        if (this->layout->GetSelection() == 3) {
            this->duplex->SetSelection(0);
            this->sides->SetSelection(0);
        }
    }
    if (event.GetEventObject() == this->selection) {
        if (this->selection->GetSelection() != 2) {
            this->ranges->ChangeValue({});
        }
        if (this->selection->GetSelection() != 1) {
            this->parity->SetSelection(0);
        }
    }
    const bool booklet = this->layout->GetSelection() == 2;
    const bool multiple = this->layout->GetSelection() == 1;
    if (booklet) {
        this->duplex->SetSelection(2);
    }
    if (booklet || multiple) {
        this->parity->SetSelection(0);
    }
    const bool filtered = this->parity->GetSelection() != 0;
    if (!booklet && filtered) {
        this->duplex->SetSelection(0);
    }
    this->duplex->Enable(!booklet && !filtered);
    this->parity->Enable(!booklet && !multiple);
    this->settings_changed();
}
void print_dialog::on_spin(wxSpinEvent &event) {
    if (!this->controls_ready) {
        event.Skip();
        return;
    }
    this->settings_changed();
}
void print_dialog::on_decimal_spin(wxSpinDoubleEvent &event) {
    if (!this->controls_ready) {
        event.Skip();
        return;
    }
    this->settings_changed();
}
void print_dialog::on_properties(wxCommandEvent &) {
    try {
        this->choose_printer();
    } catch (const std::exception &error) {
        report_print_error(this, "Printer selection failed", error);
    }
}
void print_dialog::on_submit(wxCommandEvent &) {
    this->submit();
}
void print_dialog::on_save_preview(wxCommandEvent &) {
    this->save_preview();
}
void print_dialog::on_previous_side(wxCommandEvent &) {
    this->navigate_preview(false);
}
void print_dialog::on_next_side(wxCommandEvent &) {
    this->navigate_preview(true);
}
void print_dialog::navigate_preview(bool forward) {
    if ((!forward && this->preview_side == 0) ||
        (forward && this->preview_side + 1 >= this->current_job.sides.size())) {
        return;
    }
    const auto previous = this->preview_side;
    this->preview_side = forward ? previous + 1 : previous - 1;
    try {
        this->update_side_label();
        this->preview->Refresh();
    } catch (const std::exception &error) {
        this->preview_side = previous;
        try {
            this->update_side_label();
            this->preview->Refresh();
        } catch (...) {
            this->plan_valid = false;
            try {
                this->print_button->Disable();
            } catch (...) {
                // Keep the plan invalid even if the native control cannot be updated.
            }
        }
        report_print_error(this, "Preview update failed", error);
    }
}
void print_dialog::settings_changed() {
    this->plan_valid = false;
    try {
        this->print_button->Disable();
        this->status->SetLabel("Updating sheet preview...");
        if (this->timer.StartOnce(100)) {
            return;
        }
    } catch (const std::exception &) {
        // Recover immediately if the UI update or timer could not be started.
    }
    this->regenerate_preview_safely();
}
void print_dialog::on_preview_timer(wxTimerEvent &) {
    this->regenerate_preview_safely();
}
void print_dialog::regenerate_preview_safely() {
    try {
        this->regenerate_preview();
    } catch (const std::exception &error) {
        this->plan_valid = false;
        try {
            this->print_button->Disable();
            this->status->SetLabel(wxString::FromUTF8(error.what()));
            this->update_side_label();
            this->preview->Refresh();
        } catch (const std::exception &reporting_error) {
            report_print_error(this, "Preview update failed", reporting_error);
        }
    }
}
print_options print_dialog::read_options() {
    print_options options;
    const int layout = this->layout->GetSelection();
    const int scaling = this->scaling->GetSelection();
    const auto valid_decimal = [](const wxSpinCtrlDouble *control) {
        double value = 0;
        return control->GetTextValue().ToDouble(&value) && std::isfinite(value) &&
               value >= control->GetMin() && value <= control->GetMax();
    };
    const auto valid_integer = [](const wxSpinCtrl *control) {
        long value = 0;
        return control->GetTextValue().ToLong(&value) && value >= control->GetMin() &&
               value <= control->GetMax();
    };
    if (!valid_decimal(this->margin) || !valid_decimal(this->gutter) ||
        !valid_integer(this->copies) ||
        ((scaling == 3 || layout == 3) && !valid_decimal(this->percentage)) ||
        (layout == 1 && (!valid_integer(this->columns) || !valid_integer(this->rows))) ||
        (layout == 3 && !valid_decimal(this->overlap))) {
        throw std::invalid_argument("invalid numeric print setting");
    }
    const page_extent formats[] = {
        {595.2756, 841.8898}, {612, 792}, {841.8898, 1190.5512}, {612, 1008}};
    const int format = this->printer.selected() ? 4 : this->paper->GetSelection();
    if (format < 0 || format > 4) {
        throw std::invalid_argument("invalid paper format");
    }
    options.paper = formats[std::min(format, 3)];
    const int orientation = this->orientation->GetSelection();
    if (orientation < 0 || orientation > 1) {
        throw std::invalid_argument("invalid paper orientation");
    }
    if (orientation == 1) {
        std::swap(options.paper.width, options.paper.height);
    }
    this->printer.set_paper(format, orientation == 1);
    options.printable = {0, 0, options.paper.width, options.paper.height};
    options = this->printer.measure(options);
    const double margin = this->margin->GetValue() * 72 / 25.4;
    const double right =
        std::min(options.printable.x + options.printable.width, options.paper.width - margin);
    const double bottom =
        std::min(options.printable.y + options.printable.height, options.paper.height - margin);
    options.printable.x = std::max(options.printable.x, margin);
    options.printable.y = std::max(options.printable.y, margin);
    options.printable.width = right - options.printable.x;
    options.printable.height = bottom - options.printable.y;
    options.layout = static_cast<print_layout>(layout);
    options.scaling = static_cast<print_scale>(scaling);
    options.percentage = this->percentage->GetValue();
    if (scaling != 3 && layout != 3 &&
        (!std::isfinite(options.percentage) || options.percentage < 1 ||
         options.percentage > 1000)) {
        options.percentage = 100;
    }
    options.columns = this->columns->GetValue();
    options.rows = this->rows->GetValue();
    options.order = static_cast<nup_order>(this->order->GetSelection());
    options.overlap = this->overlap->GetValue() * 72 / 25.4;
    options.gutter = this->gutter->GetValue() * 72 / 25.4;
    options.duplex = static_cast<print_duplex>(this->duplex->GetSelection());
    options.sides = static_cast<sheet_subset>(this->sides->GetSelection());
    options.copies = this->copies->GetValue();
    options.collate = this->collate->GetValue();
    options.grayscale = this->grayscale->GetValue();
    options.reverse = this->reverse->GetValue();
    options.auto_rotate = this->rotate->GetValue();
    options.right_binding = this->binding->GetValue();
    options.borders = this->borders->GetValue();
    options.cut_marks = this->cut_marks->GetValue();
    options.tile_labels = this->labels->GetValue();
    options.reverse_backs = this->reverse_backs->GetValue();
    return options;
}
void print_dialog::regenerate_preview() {
    this->timer.Stop();
    this->plan_valid = false;
    try {
        const auto options = this->read_options();
        const int selection = this->selection->GetSelection();
        if (selection < 0 || selection > 2) {
            throw std::invalid_argument("invalid page selection mode");
        }
        const auto expression = selection == 0   ? "1-" + std::to_string(this->document.pages())
                                : selection == 1 ? std::to_string(this->current_page)
                                                 : this->ranges->GetValue().ToStdString(wxConvUTF8);
        const auto pages =
            select_print_pages(expression, this->document.pages(),
                               static_cast<page_subset>(this->parity->GetSelection()));
        this->current_job = compose_print_job(this->dimensions, pages, options);
        this->preview_side = std::min(this->preview_side, this->current_job.sides.size() - 1);
        this->plan_valid = true;
        this->print_button->Enable();
        this->status->SetLabel(this->printer.selected()
                                   ? "Printer: " + wxString::FromUTF8(this->printer.name())
                                   : "Preview uses nominal paper. Choose Printer & properties "
                                     "before printing. Driver layout should be 1 page per sheet.");
        this->status->Wrap(this->GetClientSize().x - 32);
    } catch (const std::exception &error) {
        this->plan_valid = false;
        this->print_button->Disable();
        this->status->SetLabel(wxString::FromUTF8(error.what()));
    }
    this->percentage->Enable(this->scaling->GetSelection() == 3 ||
                             this->layout->GetSelection() == 3);
    this->columns->Enable(this->layout->GetSelection() == 1);
    this->rows->Enable(this->layout->GetSelection() == 1);
    this->order->Enable(this->layout->GetSelection() == 1);
    this->binding->Enable(this->layout->GetSelection() == 2);
    this->overlap->Enable(this->layout->GetSelection() == 3);
    this->cut_marks->Enable(this->layout->GetSelection() == 3);
    this->labels->Enable(this->layout->GetSelection() == 3);
    this->ranges->Enable(this->selection->GetSelection() == 2);
    this->update_side_label();
    this->preview->Refresh();
    this->Layout();
}
const print_job &print_dialog::job() const {
    return this->current_job;
}
bool print_dialog::valid() const {
    return this->plan_valid;
}
void print_dialog::update_side_label() {
    if (!this->plan_valid) {
        this->side_label->SetLabel("Invalid print settings");
        return;
    }
    const auto &side = this->current_job.sides[this->preview_side];
    this->side_label->SetLabel(
        wxString::Format(wxString::FromUTF8("Side %zu/%zu — sheet %d, %s, copy %d"),
                         this->preview_side + 1, this->current_job.sides.size(), side.sheet + 1,
                         side.back ? "back" : "front", side.copy + 1));
}
void print_dialog::choose_printer() {
    this->regenerate_preview();
    print_options requested;
    requested.duplex = static_cast<print_duplex>(this->duplex->GetSelection());
    requested.copies = this->copies->GetValue();
    requested.collate = this->collate->GetValue();
    requested.grayscale = this->grayscale->GetValue();
    this->printer.set_job_defaults(requested);
    if (!this->printer.select_printer(this)) {
        return;
    }
    const auto settings = this->printer.preferences();
    this->paper->SetSelection(4);
    this->paper->Disable();
    this->orientation->SetSelection(settings.landscape ? 1 : 0);
    this->copies->SetValue(settings.copies);
    this->collate->SetValue(settings.collate);
    this->grayscale->SetValue(settings.grayscale);
    if (this->layout->GetSelection() == 2) {
        this->duplex->SetSelection(2);
    } else if (this->parity->GetSelection() != 0) {
        this->duplex->SetSelection(0);
    } else if (requested.duplex != print_duplex::manual) {
        this->duplex->SetSelection(static_cast<int>(settings.duplex));
    }
    this->regenerate_preview();
}
void print_dialog::submit() {
    try {
        this->regenerate_preview();
        if (!this->valid()) {
            return;
        }
        if (!this->printer.selected()) {
            this->choose_printer();
            return;
        }
        long dpi = 0;
        if (!this->resolution->GetStringSelection().ToLong(&dpi)) {
            throw std::runtime_error("invalid print resolution");
        }
        const wxWindowDisabler disable_existing_windows;
        if (this->current_job.options.duplex == print_duplex::manual &&
            this->current_job.options.sides == sheet_subset::all) {
            auto front = this->current_job;
            auto back = this->current_job;
            std::erase_if(front.sides, [](const print_side &side) { return side.back; });
            std::erase_if(back.sides, [](const print_side &side) { return !side.back; });
            front.options.sides = sheet_subset::fronts;
            back.options.sides = sheet_subset::backs;
            if (!this->printer.submit(this, this->document, front, static_cast<int>(dpi))) {
                return;
            }
            if (wxMessageBox("Front sides submitted. Wait for printing, then reinsert the stack "
                             "for your printer's paper path. Feed direction varies by printer; "
                             "test a small job first. Continue with back sides?",
                             "Manual duplex", wxYES_NO | wxICON_INFORMATION, this) != wxYES) {
                return;
            }
            if (!back.sides.empty() &&
                !this->printer.submit(this, this->document, back, static_cast<int>(dpi))) {
                return;
            }
        } else if (!this->printer.submit(this, this->document, this->current_job,
                                         static_cast<int>(dpi))) {
            return;
        }
        this->EndModal(wxID_OK);
    } catch (const std::exception &error) {
        report_print_error(this, "Printing failed", error);
    }
}
void print_dialog::export_preview(const std::filesystem::path &path) {
    require_print_permission(this->document, 72);
    if (!this->valid()) {
        throw std::runtime_error("print settings are not valid");
    }
    const auto paper = this->current_job.options.paper;
    const double scale = std::min(1200 / paper.width, 1200 / paper.height);
    wxBitmap image(std::max(1, static_cast<int>(paper.width * scale)),
                   std::max(1, static_cast<int>(paper.height * scale)), 24);
    if (!image.IsOk()) {
        throw std::runtime_error("cannot allocate preview image");
    }
    wxMemoryDC dc(image);
    this->painter.paint(dc, this->current_job, this->preview_side,
                        wxRect(0, 0, image.GetWidth(), image.GetHeight()), 72, false);
    dc.SelectObject(wxNullBitmap);
    const auto converted = image.ConvertToImage();
    if (!converted.IsOk()) {
        throw std::runtime_error("cannot convert preview image");
    }
    wxMemoryOutputStream encoded;
    if (!converted.SaveFile(encoded, wxBITMAP_TYPE_PNG) || !encoded.IsOk()) {
        throw std::runtime_error("cannot save preview image");
    }
    std::string bytes(encoded.GetSize(), '\0');
    if (encoded.CopyTo(bytes.data(), bytes.size()) != bytes.size()) {
        throw std::runtime_error("cannot read encoded preview image");
    }
    atomic_write(path, bytes);
}
void print_dialog::save_preview() {
    try {
        wxFileDialog dialog(this, "Save current sheet preview", {}, {}, "PNG images (*.png)|*.png",
                            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dialog.ShowModal() != wxID_OK) {
            return;
        }
        this->export_preview(std::filesystem::path(dialog.GetPath().ToStdString(wxConvUTF8)));
    } catch (const std::exception &error) {
        report_print_error(this, "Preview export failed", error);
    }
}
} // namespace vellum
