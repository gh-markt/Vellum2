#include "vellum/print_dialog.hh"
#include <cmath>
#include <iostream>
#include <stdexcept>
#if defined(__WXGTK__)
#include <dlfcn.h>
#endif
#include <wx/app.h>
#include <wx/modalhook.h>
#include <wx/printdlg.h>
class print_test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        this->SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(print_test_app);
namespace {
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void events() {
    for (int step = 0; step < 50; ++step) {
        wxYield();
        wxMilliSleep(5);
    }
}
template <class Control> Control &control(vellum::print_dialog &dialog, const char *name) {
    auto *result = dynamic_cast<Control *>(wxWindow::FindWindowByName(name, &dialog));
    require(result != nullptr, "print control missing");
    return *result;
}
void choose(vellum::print_dialog &dialog, const char *name, int selection) {
    auto &value = control<wxChoice>(dialog, name);
    value.SetSelection(selection);
    wxCommandEvent event(wxEVT_CHOICE, value.GetId());
    event.SetEventObject(&value);
    value.GetEventHandler()->ProcessEvent(event);
}
void decimal(vellum::print_dialog &dialog, const char *name, double number) {
    auto &value = control<wxSpinCtrlDouble>(dialog, name);
    value.SetValue(number);
    wxSpinDoubleEvent event(wxEVT_SPINCTRLDOUBLE, value.GetId());
    event.SetEventObject(&value);
    value.GetEventHandler()->ProcessEvent(event);
}
void integer(vellum::print_dialog &dialog, const char *name, int number) {
    auto &value = control<wxSpinCtrl>(dialog, name);
    value.SetValue(number);
    wxSpinEvent event(wxEVT_SPINCTRL, value.GetId());
    event.SetEventObject(&value);
    value.GetEventHandler()->ProcessEvent(event);
}
void button(vellum::print_dialog &dialog, int id) {
    wxCommandEvent event(wxEVT_BUTTON, id);
    dialog.GetEventHandler()->ProcessEvent(event);
}
void toggle(vellum::print_dialog &dialog, const char *name, bool checked) {
    auto &value = control<wxCheckBox>(dialog, name);
    value.SetValue(checked);
    wxCommandEvent event(wxEVT_CHECKBOX, value.GetId());
    event.SetEventObject(&value);
    value.GetEventHandler()->ProcessEvent(event);
}
class cancel_native : public wxModalDialogHook {
  public:
    cancel_native() { this->Register(); }
    int calls() const { return this->count; }

  protected:
    int Enter(wxDialog *) override {
        ++this->count;
        return wxID_CANCEL;
    }

  private:
    int count = 0;
};
class empty_preview_path : public wxModalDialogHook {
  public:
    empty_preview_path() { Register(); }
    int files = 0;
    int errors = 0;
    bool unexpected = false;

  protected:
    int Enter(wxDialog *dialog) override {
        if (dynamic_cast<wxFileDialog *>(dialog)) {
            ++files;
            return wxID_OK;
        }
        if (dialog->GetTitle() == "Preview export failed") {
            ++errors;
            return wxID_OK;
        }
        unexpected = true;
        return wxID_CANCEL;
    }
};
class accept_native : public wxModalDialogHook {
  public:
    explicit accept_native(wxDuplexMode duplex = wxDUPLEX_HORIZONTAL) : duplex(duplex) {
        this->Register();
    }
    bool observed() const { return this->seen; }

  protected:
    int Enter(wxDialog *dialog) override {
        auto *native = dynamic_cast<wxPrintDialogBase *>(dialog);
        if (native == nullptr) {
            return wxID_CANCEL;
        }
        this->seen = true;
        auto &data = native->GetPrintDialogData();
        data.SetNoCopies(3);
        data.SetCollate(false);
        data.GetPrintData().SetOrientation(wxLANDSCAPE);
        data.GetPrintData().SetDuplex(this->duplex);
        data.GetPrintData().SetColour(false);
        return wxID_OK;
    }

  private:
    bool seen = false;
    wxDuplexMode duplex;
};
void assert_page_color(const wxImage &image, const vellum::print_job &job, int slot, bool red) {
    const auto cell = job.sides[0].placements[static_cast<std::size_t>(slot)].clip;
    const int x =
        static_cast<int>((cell.x + cell.width / 2) * image.GetWidth() / job.options.paper.width);
    const int y =
        static_cast<int>((cell.y + cell.height / 2) * image.GetHeight() / job.options.paper.height);
    require(image.GetRed(x, y) > 240, "red channel mismatch");
    require(red ? image.GetGreen(x, y) < 15 : image.GetGreen(x, y) > 240,
            "sheet page placement/color mismatch");
}
void layout_controls(vellum::print_dialog &dialog, const std::filesystem::path &output) {
    require(dialog.valid() && dialog.job().sides.size() == 2, "initial print preview plan");
    require(control<wxChoice>(dialog, "print-resolution").GetCount() == 4,
            "high-quality print permission must expose high-resolution choices");
    choose(dialog, "print-layout", 1);
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "stale plan must not be printable");
    events();
    require(dialog.valid() && dialog.job().sides.size() == 1 &&
                dialog.job().sides[0].placements.size() == 2,
            "N-up automatic preview");
    wxScreenDC screen;
    const auto size = dialog.GetClientSize();
    const auto origin = dialog.ClientToScreen(wxPoint(0, 0));
    wxBitmap capture(size.x, size.y);
    wxMemoryDC captured(capture);
    captured.Blit(0, 0, size.x, size.y, &screen, origin.x, origin.y);
    captured.SelectObject(wxNullBitmap);
    capture.SaveFile(wxString::FromUTF8((output.parent_path() / "print-dialog.png").string()),
                     wxBITMAP_TYPE_PNG);

    dialog.export_preview(output);
    wxImage image(wxString::FromUTF8(output.string()), wxBITMAP_TYPE_PNG);
    require(image.IsOk(), "preview image output");
    assert_page_color(image, dialog.job(), 0, true);
    assert_page_color(image, dialog.job(), 1, false);
    auto &gray = control<wxCheckBox>(dialog, "print-grayscale");
    gray.SetValue(true);
    wxCommandEvent gray_event(wxEVT_CHECKBOX, gray.GetId());
    gray_event.SetEventObject(&gray);
    gray.GetEventHandler()->ProcessEvent(gray_event);
    events();
    require(dialog.valid() && dialog.job().options.grayscale, "grayscale live preview option");
    dialog.export_preview(output);
    image.LoadFile(wxString::FromUTF8(output.string()), wxBITMAP_TYPE_PNG);
    const auto cell = dialog.job().sides[0].placements[0].clip;
    const int center_x = static_cast<int>((cell.x + cell.width / 2) * image.GetWidth() /
                                          dialog.job().options.paper.width);
    const int center_y = static_cast<int>((cell.y + cell.height / 2) * image.GetHeight() /
                                          dialog.job().options.paper.height);
    require(image.GetRed(center_x, center_y) == image.GetGreen(center_x, center_y) &&
                image.GetGreen(center_x, center_y) == image.GetBlue(center_x, center_y),
            "grayscale output pixel mismatch");
    gray.SetValue(false);
    gray.GetEventHandler()->ProcessEvent(gray_event);
    events();

    integer(dialog, "print-rows", 2);
    choose(dialog, "print-order", 1);
    events();
    require(dialog.job().sides[0].placements[1].clip.x ==
                dialog.job().sides[0].placements[0].clip.x,
            "N-up down-first order");
    require(dialog.job().sides[0].placements[1].clip.y > dialog.job().sides[0].placements[0].clip.y,
            "N-up row placement");
    choose(dialog, "print-layout", 2);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 2, "booklet preview sides");
    require(dialog.job().options.paper.width > dialog.job().options.paper.height,
            "booklet orientation");
    require(dialog.job().sides[0].placements[0].page == 0 &&
                dialog.job().sides[0].placements[1].page == 1,
            "booklet imposition/padding");
    dialog.export_preview(output);
    image.LoadFile(wxString::FromUTF8(output.string()), wxBITMAP_TYPE_PNG);
    assert_page_color(image, dialog.job(), 0, false);
    assert_page_color(image, dialog.job(), 1, true);
}
void tiling_and_errors(vellum::print_dialog &dialog, const std::filesystem::path &output) {
    choose(dialog, "print-layout", 3);
    choose(dialog, "print-orientation", 0);
    choose(dialog, "print-selection", 1);
    decimal(dialog, "print-percentage", 500);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 4, "poster live scale/tile count");
    require(dialog.job().sides[1].placements[0].destination.x < 0, "poster tile source offset");
    choose(dialog, "print-duplex", 1);
    events();
    require(!dialog.valid(), "poster must not silently duplex tiles");
    choose(dialog, "print-duplex", 0);
    events();
    require(dialog.valid(), "recover from invalid settings");
    choose(dialog, "print-selection", 2);
    auto &ranges = control<wxTextCtrl>(dialog, "print-ranges");
    ranges.SetValue("0-3");
    events();
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "invalid ranges must disable Print");
    {
        cancel_native hook;
        button(dialog, wxID_PRINT);
        require(hook.calls() == 0 && !dialog.valid(),
                "an invalid plan must not open a printer dialog or submit");
    }
    bool rejected = false;
    try {
        dialog.export_preview(output);
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    require(rejected, "invalid print plan must not export a stale preview");
    ranges.SetValue("1-2");
    events();
    require(dialog.valid(), "valid ranges must restore Print");
    {
        cancel_native hook;
        button(dialog, wxID_SAVEAS);
        require(hook.calls() == 1 && dialog.valid(),
                "cancelling preview export must preserve the valid plan");
    }
    {
        wxLogNull silence_expected_error;
        empty_preview_path hook;
        button(dialog, wxID_SAVEAS);
        require(hook.files == 1 && hook.errors == 1 && !hook.unexpected && dialog.valid(),
                "an empty preview path must report an error and preserve the plan");
    }
    {
        wxLogNull silence_expected_error;
        rejected = false;
        try {
            dialog.export_preview(output / "missing" / "preview.png");
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        require(rejected && dialog.valid(), "preview export failure must preserve the print plan");
    }
    cancel_native hook;
    button(dialog, wxID_SETUP);
    events();
    require(hook.calls() == 1 && dialog.valid(), "native printer selection cancellation");
    button(dialog, wxID_PRINT);
    events();
    require(hook.calls() == 2 && dialog.valid(),
            "cancelling printer selection during submission must preserve the preview");
}
void selection_and_copy_controls(vellum::print_dialog &dialog) {
    auto &ranges = control<wxTextCtrl>(dialog, "print-ranges");
    require(ranges.IsEmpty(), "non-custom selection must start with an empty range");
    for (const int selection : {0, 1}) {
        choose(dialog, "print-selection", 2);
        ranges.SetValue("2,,1");
        choose(dialog, "print-selection", selection);
        require(ranges.IsEmpty(), "leaving custom selection must clear the range");
        choose(dialog, "print-selection", 2);
        events();
        require(ranges.IsEmpty() && !dialog.valid(),
                "returning to custom selection must require a new range");
    }
    choose(dialog, "print-selection", 0);
    choose(dialog, "print-parity", 2);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 1 &&
                dialog.job().sides[0].placements[0].page == 2,
            "even selection must print only document page two");
    choose(dialog, "print-selection", 1);
    events();
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "even filter on odd current page must reject empty selection");
    choose(dialog, "print-parity", 1);
    integer(dialog, "print-copies", 2);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 2 &&
                dialog.job().sides[0].placements[0].page == 1 &&
                dialog.job().sides[1].placements[0].page == 1,
            "odd current page must produce both copies");
    choose(dialog, "print-selection", 0);
    choose(dialog, "print-parity", 0);
    integer(dialog, "print-copies", 1);
    toggle(dialog, "print-reverse", true);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 2 &&
                dialog.job().sides[0].placements[0].page == 2 &&
                dialog.job().sides[1].placements[0].page == 1,
            "reverse must change page order in the shared preview plan");
    toggle(dialog, "print-reverse", false);
    events();
    require(dialog.valid() && dialog.job().sides[0].placements[0].page == 1 &&
                dialog.job().sides[1].placements[0].page == 2,
            "turning reverse off must restore document order");
}
void scaling_and_paper_controls(vellum::print_dialog &dialog) {
    choose(dialog, "print-paper", 1);
    choose(dialog, "print-orientation", 1);
    choose(dialog, "print-scaling", 3);
    decimal(dialog, "print-percentage", 0);
    events();
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "zero custom scale must disable printing");
    decimal(dialog, "print-percentage", 125);
    events();
    require(dialog.valid() && std::abs(dialog.job().options.paper.width - 792) < 0.001 &&
                std::abs(dialog.job().options.paper.height - 612) < 0.001 &&
                dialog.job().options.scaling == vellum::print_scale::custom &&
                dialog.job().options.percentage == 125,
            "landscape Letter and custom scale must reach the preview plan");
    choose(dialog, "print-scaling", 2);
    events();
    require(dialog.valid() && !control<wxSpinCtrlDouble>(dialog, "print-percentage").IsEnabled() &&
                dialog.job().options.scaling == vellum::print_scale::actual,
            "actual-size mode must ignore and disable custom scale");
}
void settings_round_trip(vellum::print_dialog &dialog) {
    const auto initial = dialog.job();
    toggle(dialog, "print-grayscale", true);
    toggle(dialog, "print-borders", true);
    toggle(dialog, "print-rotate", false);
    integer(dialog, "print-copies", 2);
    choose(dialog, "print-paper", 1);
    choose(dialog, "print-orientation", 1);
    choose(dialog, "print-scaling", 3);
    decimal(dialog, "print-percentage", 125);
    events();
    const auto &changed = dialog.job();
    require(dialog.valid() && changed.options.grayscale && changed.options.borders &&
                !changed.options.auto_rotate && changed.options.copies == 2 &&
                changed.options.scaling == vellum::print_scale::custom &&
                changed.options.percentage == 125 && changed.sides.size() == 4 &&
                std::abs(changed.options.paper.width - 792) < 0.001 &&
                std::abs(changed.options.paper.height - 612) < 0.001,
            "combined settings must reach the generated plan");

    toggle(dialog, "print-grayscale", false);
    toggle(dialog, "print-borders", false);
    toggle(dialog, "print-rotate", true);
    integer(dialog, "print-copies", 1);
    choose(dialog, "print-paper", 0);
    choose(dialog, "print-orientation", 0);
    choose(dialog, "print-scaling", 0);
    decimal(dialog, "print-percentage", 100);
    events();
    const auto &restored = dialog.job();
    require(dialog.valid() && !restored.options.grayscale && !restored.options.borders &&
                restored.options.auto_rotate && restored.options.copies == 1 &&
                restored.options.scaling == vellum::print_scale::fit &&
                restored.sides.size() == initial.sides.size() &&
                std::abs(restored.options.paper.width - initial.options.paper.width) < 0.001 &&
                std::abs(restored.options.paper.height - initial.options.paper.height) < 0.001 &&
                restored.sides[0].placements[0].page == 1 &&
                restored.sides[1].placements[0].page == 2,
            "reverting combined settings must restore the initial page plan");

    choose(dialog, "print-layout", 2);
    events();
    require(dialog.valid() && dialog.job().options.layout == vellum::print_layout::booklet &&
                dialog.job().sides.size() == 2,
            "booklet must produce its two imposed sides");
    choose(dialog, "print-layout", 0);
    choose(dialog, "print-orientation", 0);
    choose(dialog, "print-duplex", 0);
    events();
    require(dialog.valid() && dialog.job().options.layout == vellum::print_layout::single &&
                dialog.job().sides.size() == 2 && dialog.job().sides[0].placements[0].page == 1 &&
                dialog.job().sides[1].placements[0].page == 2,
            "leaving booklet and restoring orientation must recover the single-page plan");

    choose(dialog, "print-selection", 2);
    auto &ranges = control<wxTextCtrl>(dialog, "print-ranges");
    ranges.SetValue("2-1");
    events();
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "descending custom range must make the plan unprintable");
    ranges.SetValue("1-2");
    events();
    require(dialog.valid() && dialog.job().sides.size() == 2,
            "correcting a custom range must restore the preview");
    choose(dialog, "print-selection", 0);
    events();
    require(dialog.valid() && !ranges.IsEnabled() && dialog.job().sides.size() == 2,
            "returning to all pages must disable custom ranges and restore the plan");
}
void copy_duplex_and_layout_sequences(vellum::print_dialog &dialog) {
    integer(dialog, "print-copies", 2);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 4 &&
                dialog.job().sides[0].placements[0].page == 1 &&
                dialog.job().sides[1].placements[0].page == 2 &&
                dialog.job().sides[2].placements[0].page == 1 &&
                dialog.job().sides[3].placements[0].page == 2,
            "collated copies must retain document order");
    toggle(dialog, "print-collate", false);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 4 &&
                dialog.job().sides[0].placements[0].page == 1 &&
                dialog.job().sides[1].placements[0].page == 1 &&
                dialog.job().sides[2].placements[0].page == 2 &&
                dialog.job().sides[3].placements[0].page == 2,
            "uncollated copies must group identical pages");
    toggle(dialog, "print-collate", true);
    integer(dialog, "print-copies", 1);
    choose(dialog, "print-duplex", 1);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 2 && !dialog.job().sides[0].back &&
                dialog.job().sides[1].back &&
                dialog.job().sides[0].sheet == dialog.job().sides[1].sheet,
            "duplex must pair front and back of one sheet");
    choose(dialog, "print-sides", 1);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 1 && !dialog.job().sides[0].back &&
                dialog.job().sides[0].placements[0].page == 1,
            "fronts-only must select the first page");
    choose(dialog, "print-sides", 2);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 1 && dialog.job().sides[0].back &&
                dialog.job().sides[0].placements[0].page == 2,
            "backs-only must select the second page");
    choose(dialog, "print-sides", 0);
    choose(dialog, "print-duplex", 0);
    choose(dialog, "print-layout", 1);
    integer(dialog, "print-columns", 2);
    integer(dialog, "print-rows", 1);
    decimal(dialog, "print-gutter", 10);
    choose(dialog, "print-order", 2);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 1 &&
                dialog.job().sides[0].placements.size() == 2 &&
                dialog.job().sides[0].placements[0].clip.x >
                    dialog.job().sides[0].placements[1].clip.x &&
                std::abs(dialog.job().options.gutter - 10 * 72.0 / 25.4) < 0.001,
            "reverse N-up order and gutter must affect the actual cell geometry");
    integer(dialog, "print-rows", 2);
    choose(dialog, "print-order", 3);
    events();
    require(dialog.valid() &&
                dialog.job().sides[0].placements[0].clip.x ==
                    dialog.job().sides[0].placements[1].clip.x &&
                dialog.job().sides[0].placements[0].clip.y >
                    dialog.job().sides[0].placements[1].clip.y,
            "reverse vertical N-up order must put the first page below the second");
    integer(dialog, "print-rows", 1);
    choose(dialog, "print-order", 0);
    decimal(dialog, "print-gutter", 0);
    choose(dialog, "print-layout", 2);
    toggle(dialog, "print-binding", true);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 2 &&
                dialog.job().sides[0].placements[0].page == 1 &&
                dialog.job().sides[0].placements[1].page == 0,
            "right binding must swap the booklet front placements");
    toggle(dialog, "print-binding", false);
    events();
    require(dialog.valid() && dialog.job().sides[0].placements[0].page == 0 &&
                dialog.job().sides[0].placements[1].page == 1,
            "restoring left binding must restore booklet front placements");
}
void navigation_and_paper_sequence(vellum::print_dialog &dialog,
                                   const std::filesystem::path &output) {
    button(dialog, wxID_FORWARD);
    button(dialog, wxID_FORWARD);
    dialog.export_preview(output);
    wxImage page(wxString::FromUTF8(output.string()), wxBITMAP_TYPE_PNG);
    require(page.IsOk() && page.GetGreen(page.GetWidth() / 2, page.GetHeight() / 2) > 240,
            "next-side navigation must stop on the white second page");
    button(dialog, wxID_BACKWARD);
    button(dialog, wxID_BACKWARD);
    dialog.export_preview(output);
    page.LoadFile(wxString::FromUTF8(output.string()), wxBITMAP_TYPE_PNG);
    require(page.IsOk() && page.GetRed(page.GetWidth() / 2, page.GetHeight() / 2) > 240 &&
                page.GetGreen(page.GetWidth() / 2, page.GetHeight() / 2) < 15,
            "previous-side navigation must stop on the red first page");
    choose(dialog, "print-paper", 3);
    decimal(dialog, "print-margin", 5);
    choose(dialog, "print-scaling", 1);
    events();
    require(dialog.valid() && std::abs(dialog.job().options.paper.width - 612) < 0.001 &&
                std::abs(dialog.job().options.paper.height - 1008) < 0.001 &&
                std::abs(dialog.job().options.printable.x - 5 * 72.0 / 25.4) < 0.001 &&
                dialog.job().options.scaling == vellum::print_scale::shrink,
            "legal paper, margin and shrink selection must reach the plan");
    choose(dialog, "print-paper", 2);
    events();
    require(dialog.valid() && std::abs(dialog.job().options.paper.width - 841.8898) < 0.001 &&
                std::abs(dialog.job().options.paper.height - 1190.5512) < 0.001,
            "A3 paper selection must reach the plan");
    choose(dialog, "print-paper", 4);
    events();
    require(dialog.valid() && control<wxChoice>(dialog, "print-paper").GetSelection() == 4,
            "printer-properties paper selection must retain a valid nominal preview");
    choose(dialog, "print-paper", 0);
    decimal(dialog, "print-margin", 10);
    choose(dialog, "print-scaling", 0);
    events();
    require(dialog.valid() && std::abs(dialog.job().options.paper.width - 595.2756) < 0.001 &&
                dialog.job().options.scaling == vellum::print_scale::fit,
            "restoring A4 and fit must restore valid defaults");
    for (const int selection : {2, 3, 1}) {
        choose(dialog, "print-resolution", selection);
        events();
        require(dialog.valid() &&
                    control<wxChoice>(dialog, "print-resolution").GetSelection() == selection,
                "resolution changes must preserve a valid preview");
    }
}
void manual_duplex_and_poster_sequence(vellum::print_dialog &dialog) {
    choose(dialog, "print-duplex", 3);
    toggle(dialog, "print-reverse-backs", true);
    events();
    require(dialog.valid() && dialog.job().options.duplex == vellum::print_duplex::manual &&
                dialog.job().options.reverse_backs && !dialog.job().sides[0].back &&
                dialog.job().sides[1].back,
            "manual duplex and reverse-back option must reach the plan");
    toggle(dialog, "print-reverse-backs", false);
    choose(dialog, "print-duplex", 0);
    choose(dialog, "print-layout", 3);
    choose(dialog, "print-selection", 1);
    decimal(dialog, "print-percentage", 500);
    decimal(dialog, "print-overlap", 0);
    toggle(dialog, "print-labels", false);
    toggle(dialog, "print-cut-marks", true);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 4 && !dialog.job().options.tile_labels &&
                dialog.job().options.cut_marks,
            "poster options must produce four tiles with requested marks");
    decimal(dialog, "print-overlap", 100);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 6,
            "large valid overlap must increase poster tile count");
    decimal(dialog, "print-overlap", 0);
    toggle(dialog, "print-labels", true);
    toggle(dialog, "print-cut-marks", false);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 4 && dialog.job().options.tile_labels &&
                !dialog.job().options.cut_marks,
            "restoring poster options must restore four tiles");
}
void blank_page_ranges(vellum::print_dialog &dialog, const std::filesystem::path &output) {
    choose(dialog, "print-selection", 2);
    auto &ranges = control<wxTextCtrl>(dialog, "print-ranges");
    ranges.SetValue("2,,1");
    events();
    require(dialog.valid() && dialog.job().sides.size() == 3 &&
                dialog.job().sides[0].placements[0].page == 2 &&
                dialog.job().sides[1].placements[0].page == 0 &&
                dialog.job().sides[2].placements[0].page == 1,
            "custom range must preserve typed pages and inserted blank");
    button(dialog, wxID_FORWARD);
    dialog.export_preview(output);
    wxImage blank(wxString::FromUTF8(output.string()), wxBITMAP_TYPE_PNG);
    require(blank.IsOk(), "blank preview image missing");
    const auto pixels = static_cast<std::size_t>(blank.GetWidth()) * blank.GetHeight() * 3;
    for (std::size_t index = 0; index < pixels; ++index) {
        require(blank.GetData()[index] == 255, "blank page preview contains printed content");
    }
    choose(dialog, "print-parity", 1);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 2 &&
                dialog.job().sides[0].placements[0].page == 2 &&
                dialog.job().sides[1].placements[0].page == 1,
            "odd-only must select odd positions in the requested sequence");
    choose(dialog, "print-parity", 2);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 1 &&
                dialog.job().sides[0].placements[0].page == 0,
            "even-only must select the blank in the second position");
    choose(dialog, "print-parity", 0);
    ranges.SetValue("1,,1");
    events();
    require(dialog.valid() && dialog.job().sides.size() == 3 &&
                dialog.job().sides[0].placements[0].page == 1 &&
                dialog.job().sides[1].placements[0].page == 0 &&
                dialog.job().sides[2].placements[0].page == 1,
            "repeated pages around a blank must both print");
    ranges.SetValue("2,,1");
    choose(dialog, "print-layout", 3);
    toggle(dialog, "print-cut-marks", true);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 3 &&
                dialog.job().sides[1].placements[0].page == 0,
            "poster layout must retain an explicit blank sheet");
    button(dialog, wxID_FORWARD);
    dialog.export_preview(output);
    blank.LoadFile(wxString::FromUTF8(output.string()), wxBITMAP_TYPE_PNG);
    require(blank.IsOk(), "blank poster preview missing");
    const auto poster_pixels = static_cast<std::size_t>(blank.GetWidth()) * blank.GetHeight() * 3;
    for (std::size_t index = 0; index < poster_pixels; ++index) {
        require(blank.GetData()[index] == 255, "blank poster sheet contains print marks or labels");
    }
    choose(dialog, "print-layout", 0);
    ranges.SetValue(",,");
    events();
    require(dialog.valid() && dialog.job().sides.size() == 1 &&
                dialog.job().sides[0].placements[0].page == 0,
            "an explicit blank-only range must make one blank sheet");
    choose(dialog, "print-parity", 1);
    events();
    require(dialog.valid() && dialog.job().sides[0].placements[0].page == 0,
            "odd-only must retain a blank in the first position");
    choose(dialog, "print-parity", 0);
    events();
    require(dialog.valid() && dialog.job().sides[0].placements[0].page == 0,
            "removing the parity filter must restore the blank sheet");
    std::string long_range;
    for (int index = 0; index < 128; ++index) {
        if (index != 0) {
            long_range += ',';
        }
        long_range += index % 2 == 0 ? '2' : '1';
    }
    ranges.SetValue(wxString::FromUTF8(long_range));
    dialog.regenerate_preview();
    require(dialog.valid() && dialog.job().sides.size() == 128,
            "long ordered range must preserve every selected page");
    for (std::size_t index = 0; index < dialog.job().sides.size(); ++index) {
        require(dialog.job().sides[index].placements.size() == 1 &&
                    dialog.job().sides[index].placements[0].page == (index % 2 == 0 ? 2 : 1),
                "long range changed page order or dropped a duplicate");
    }
    ranges.SetValue(wxString::FromUTF8(long_range + ",3"));
    dialog.regenerate_preview();
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "late out-of-range page must reject the entire selection");
    ranges.SetValue(wxString::FromUTF8(long_range));
    dialog.regenerate_preview();
    require(dialog.valid() && dialog.job().sides.size() == 128,
            "correcting a long range must restore the preview");
}
void booklet_control_sequence(vellum::pdf_document &document) {
    vellum::printer_settings printer;
    vellum::print_dialog dialog(nullptr, document, printer, 1);
    dialog.Show();
    events();
    choose(dialog, "print-selection", 2);
    control<wxTextCtrl>(dialog, "print-ranges").SetValue("2");
    choose(dialog, "print-parity", 2);
    choose(dialog, "print-duplex", 1);
    choose(dialog, "print-layout", 2);
    events();
    auto &parity = control<wxChoice>(dialog, "print-parity");
    auto &duplex = control<wxChoice>(dialog, "print-duplex");
    require(!parity.IsEnabled() && parity.GetSelection() == 0 && !duplex.IsEnabled() &&
                duplex.GetSelection() == 2 && dialog.valid() &&
                dialog.job().options.duplex == vellum::print_duplex::short_edge,
            "booklet must lock all pages and short-edge duplex");
    events();
    require(control<wxChoice>(dialog, "print-selection").GetSelection() == 2 &&
                control<wxTextCtrl>(dialog, "print-ranges").GetValue() == "2" && dialog.valid() &&
                !parity.IsEnabled() && duplex.GetSelection() == 2 && dialog.job().sides.size() == 2,
            "booklet must accept custom ranges while retaining its locked settings");
    choose(dialog, "print-parity", 1);
    choose(dialog, "print-duplex", 0);
    events();
    require(parity.GetSelection() == 0 && duplex.GetSelection() == 2,
            "booklet must restore locked settings after synthetic changes");
    {
        accept_native hook(wxDUPLEX_SIMPLEX);
        button(dialog, wxID_SETUP);
        events();
        require(hook.observed() && printer.selected() && !parity.IsEnabled() &&
                    parity.GetSelection() == 0 && !duplex.IsEnabled() && duplex.GetSelection() == 2,
                "printer properties must not override booklet duplex or parity");
        require(!control<wxChoice>(dialog, "print-paper").IsEnabled(),
                "selected printer must control its paper through properties");
    }
    choose(dialog, "print-layout", 0);
    events();
    require(parity.IsEnabled() && duplex.IsEnabled(),
            "leaving booklet must restore control availability");
    choose(dialog, "print-parity", 1);
    choose(dialog, "print-duplex", 0);
    events();
    require(parity.GetSelection() == 1 && duplex.GetSelection() == 0 && !duplex.IsEnabled(),
            "odd-only filtering must disable duplex after leaving booklet");
    vellum::print_dialog reopened(nullptr, document, printer, 1);
    auto &paper = control<wxChoice>(reopened, "print-paper");
    require(!paper.IsEnabled() && paper.GetSelection() == 4,
            "reopening with a selected printer must retain printer-controlled paper");
}
void parity_duplex_sequence(vellum::pdf_document &document) {
    vellum::printer_settings printer;
    vellum::print_dialog dialog(nullptr, document, printer, 1);
    dialog.Show();
    events();
    auto &duplex = control<wxChoice>(dialog, "print-duplex");
    auto &parity = control<wxChoice>(dialog, "print-parity");
    choose(dialog, "print-duplex", 1);
    for (const int subset : {1, 2}) {
        choose(dialog, "print-parity", subset);
        events();
        require(!duplex.IsEnabled() && duplex.GetSelection() == 0 && dialog.valid() &&
                    dialog.job().options.duplex == vellum::print_duplex::simplex,
                "odd/even selection must force and lock simplex");
        choose(dialog, "print-duplex", 2);
        require(duplex.GetSelection() == 0, "disabled duplex must resist synthetic changes");
        choose(dialog, "print-parity", 0);
        require(duplex.IsEnabled(), "all pages must reenable duplex");
        choose(dialog, "print-duplex", 1);
    }
    choose(dialog, "print-parity", 1);
    choose(dialog, "print-selection", 2);
    require(parity.GetSelection() == 0 && duplex.IsEnabled(),
            "custom page selection must reenable duplex");
    choose(dialog, "print-parity", 2);
    choose(dialog, "print-selection", 0);
    require(parity.GetSelection() == 0 && duplex.IsEnabled(),
            "all-page selection must reenable duplex");
    choose(dialog, "print-selection", 2);
    control<wxTextCtrl>(dialog, "print-ranges").SetValue("2");
    choose(dialog, "print-parity", 1);
    choose(dialog, "print-layout", 1);
    require(!parity.IsEnabled() && parity.GetSelection() == 0 && duplex.IsEnabled(),
            "N-up must disable parity filters while allowing duplex");
    choose(dialog, "print-parity", 2);
    require(parity.GetSelection() == 0 && duplex.IsEnabled(),
            "N-up must resist synthetic parity changes");
    choose(dialog, "print-duplex", 2);
    require(duplex.GetSelection() == 2, "N-up must permit duplex selection");
    events();
    require(control<wxChoice>(dialog, "print-selection").GetSelection() == 2 &&
                control<wxTextCtrl>(dialog, "print-ranges").GetValue() == "2" && dialog.valid() &&
                dialog.job().sides.size() == 2 && !parity.IsEnabled(),
            "N-up must accept a custom range with parity disabled");
    choose(dialog, "print-layout", 0);
    require(parity.IsEnabled(), "leaving N-up must reenable parity filters");
    choose(dialog, "print-parity", 1);
    {
        accept_native hook(wxDUPLEX_HORIZONTAL);
        button(dialog, wxID_SETUP);
        require(hook.observed() && !duplex.IsEnabled() && duplex.GetSelection() == 0,
                "printer properties must not override simplex for odd/even output");
    }
}
void accepted_settings_sequence(vellum::pdf_document &document, bool manual) {
    vellum::printer_settings printer;
    vellum::print_dialog dialog(nullptr, document, printer, 1);
    dialog.Show();
    events();
    if (manual) {
        choose(dialog, "print-duplex", 3);
        events();
    }
    accept_native hook;
    button(dialog, wxID_SETUP);
    events();
    require(hook.observed() && printer.selected(), "accepted printer settings were not retained");
    require(control<wxChoice>(dialog, "print-paper").GetSelection() == 4 &&
                !control<wxChoice>(dialog, "print-paper").IsEnabled() &&
                control<wxChoice>(dialog, "print-orientation").GetSelection() == 1 &&
                control<wxSpinCtrl>(dialog, "print-copies").GetValue() == 3 &&
                !control<wxCheckBox>(dialog, "print-collate").GetValue() &&
                control<wxCheckBox>(dialog, "print-grayscale").GetValue() &&
                control<wxChoice>(dialog, "print-duplex").GetSelection() == (manual ? 3 : 2),
            "accepted settings must update controls without replacing manual duplex");
}
void invalid_current_page_sequence(vellum::pdf_document &document) {
    vellum::printer_settings printer;
    vellum::print_dialog dialog(nullptr, document, printer, 0);
    dialog.Show();
    events();
    require(dialog.valid(), "all-pages selection should not require a current page");
    choose(dialog, "print-selection", 1);
    events();
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "invalid current page must not produce a printable job");
    choose(dialog, "print-selection", 0);
    events();
    require(dialog.valid() && dialog.job().sides.size() == 2,
            "returning to all pages must recover from an invalid current page");
}
void invalid_choice_selection_sequence(vellum::pdf_document &document) {
    vellum::printer_settings printer;
    vellum::print_dialog dialog(nullptr, document, printer, 1);
    auto &paper = control<wxChoice>(dialog, "print-paper");
    require(dialog.valid(), "initial paper selection must produce a valid preview");
    paper.SetSelection(wxNOT_FOUND);
    dialog.regenerate_preview();
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "missing paper selection must reject the preview");
    const auto unexpected = paper.Append("Unexpected paper format");
    paper.SetSelection(static_cast<int>(unexpected));
    dialog.regenerate_preview();
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "unknown paper selection must reject the preview");
    paper.SetSelection(0);
    dialog.regenerate_preview();
    require(dialog.valid() && control<wxButton>(dialog, "print-submit").IsEnabled(),
            "restoring paper selection must recover the preview");
    for (const char *name : {"print-orientation", "print-selection"}) {
        auto &choice = control<wxChoice>(dialog, name);
        choice.SetSelection(wxNOT_FOUND);
        dialog.regenerate_preview();
        require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
                "missing print choice must reject the preview");
        const auto invalid = choice.Append("Unexpected choice");
        choice.SetSelection(static_cast<int>(invalid));
        dialog.regenerate_preview();
        require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
                "unknown print choice must reject the preview");
        choice.SetSelection(0);
        dialog.regenerate_preview();
        require(dialog.valid() && control<wxButton>(dialog, "print-submit").IsEnabled(),
                "restoring print choice must recover the preview");
    }
}
void adversarial_numeric_input_sequence(vellum::pdf_document &document) {
    vellum::printer_settings printer;
    vellum::print_dialog dialog(nullptr, document, printer, 1);
    for (const char *name : {"print-percentage", "print-margin", "print-gutter", "print-overlap"}) {
        auto &value = control<wxSpinCtrlDouble>(dialog, name);
        if (value.GetTextValue().empty()) {
            throw std::runtime_error(std::string("hidden decimal text is empty: ") + name);
        }
    }
    for (const char *name : {"print-columns", "print-rows", "print-copies"}) {
        auto &value = control<wxSpinCtrl>(dialog, name);
        if (value.GetTextValue().empty()) {
            throw std::runtime_error(std::string("hidden integer text is empty: ") + name);
        }
    }
    auto &hidden_margin = control<wxSpinCtrlDouble>(dialog, "print-margin");
    hidden_margin.SetValue(wxString());
    dialog.regenerate_preview();
    require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
            "blank hidden margin must not produce a valid preview");
    hidden_margin.SetValue(10.0);
    dialog.regenerate_preview();
    require(dialog.valid(), "correcting a hidden margin must restore the preview");
    dialog.Show();
    events();
    for (const char *name : {"print-percentage", "print-margin", "print-gutter", "print-overlap"}) {
        auto &value = control<wxSpinCtrlDouble>(dialog, name);
        if (value.GetTextValue().empty()) {
            throw std::runtime_error(std::string("initial decimal text is empty: ") + name);
        }
    }
    for (const char *name : {"print-columns", "print-rows", "print-copies"}) {
        auto &value = control<wxSpinCtrl>(dialog, name);
        if (value.GetTextValue().empty()) {
            throw std::runtime_error(std::string("initial integer text is empty: ") + name);
        }
    }
    auto &margin = control<wxSpinCtrlDouble>(dialog, "print-margin");
    margin.SetValue(wxString::FromUTF8("broken"));
    dialog.regenerate_preview();
    const auto displayed_margin = margin.GetTextValue();
    if (displayed_margin.empty() || displayed_margin == "broken") {
        require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
                "visible invalid margin text must not silently print as a numeric minimum");
    }
    margin.SetValue(10.0);
    dialog.regenerate_preview();
    require(dialog.valid(), "correcting invalid margin text must recover the preview");
    for (const char *text :
         {"nan", "inf", "-inf", "1e9999", "broken", "-10", "999999999999999999999999999999"}) {
        margin.SetValue(wxString::FromUTF8(text));
        dialog.regenerate_preview();
        if (dialog.valid()) {
            const auto &area = dialog.job().options.printable;
            require(std::isfinite(area.x) && std::isfinite(area.y) && std::isfinite(area.width) &&
                        std::isfinite(area.height) && area.width > 0 && area.height > 0,
                    "malformed numeric input must not produce invalid printable geometry");
        } else {
            require(!control<wxButton>(dialog, "print-submit").IsEnabled(),
                    "invalid numeric input must disable printing");
        }
        margin.SetValue(10.0);
        dialog.regenerate_preview();
        require(dialog.valid(), "valid numeric input must recover the preview");
    }
    for (const char *name : {"print-columns", "print-rows", "print-copies"}) {
        auto &value = control<wxSpinCtrl>(dialog, name);
        for (const char *text :
             {"nan", "inf", "-inf", "1e9999", "broken", "-10", "999999999999999999999999999999"}) {
            value.SetValue(wxString::FromUTF8(text));
            dialog.regenerate_preview();
            if (dialog.valid()) {
                const auto &options = dialog.job().options;
                require(options.columns >= 1 && options.columns <= 10 && options.rows >= 1 &&
                            options.rows <= 10 && options.copies >= 1 && options.copies <= 100,
                        "malformed integer input must not produce out-of-range print options");
            } else {
                require(!control<wxButton>(dialog, "print-submit").IsEnabled(),
                        "invalid integer input must disable printing");
            }
            value.SetValue(1);
            dialog.regenerate_preview();
            require(dialog.valid(), "valid integer input must recover the preview");
        }
    }
}
void destroy_with_pending_preview(vellum::pdf_document &document) {
    vellum::printer_settings printer;
    {
        vellum::print_dialog dialog(nullptr, document, printer, 1);
        toggle(dialog, "print-reverse", true);
        require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
                "changed settings must invalidate the pending preview");
    }
    events();
}
void blank_numeric_controls(vellum::pdf_document &document) {
    vellum::printer_settings printer;
    vellum::print_dialog dialog(nullptr, document, printer, 1);
    dialog.Show();
    events();
    const auto reject_blank_decimal = [&](const char *name, double restore) {
        auto &value = control<wxSpinCtrlDouble>(dialog, name);
        value.SetValue(wxString());
        require(value.GetTextValue().empty(), "decimal field did not retain blank text");
        dialog.regenerate_preview();
        require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
                "blank active decimal field must disable printing");
        value.SetValue(restore);
        dialog.regenerate_preview();
        require(dialog.valid(), "corrected decimal field must restore the preview");
    };
    const auto reject_blank_integer = [&](const char *name, int restore) {
        auto &value = control<wxSpinCtrl>(dialog, name);
        value.SetValue(wxString());
        require(value.GetTextValue().empty(), "integer field did not retain blank text");
        dialog.regenerate_preview();
        require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
                "blank active integer field must disable printing");
        value.SetValue(restore);
        dialog.regenerate_preview();
        require(dialog.valid(), "corrected integer field must restore the preview");
    };
    reject_blank_decimal("print-margin", 10);
    reject_blank_decimal("print-gutter", 0);
    reject_blank_integer("print-copies", 1);
    choose(dialog, "print-scaling", 3);
    dialog.regenerate_preview();
    reject_blank_decimal("print-percentage", 100);
    auto &percentage = control<wxSpinCtrlDouble>(dialog, "print-percentage");
    percentage.SetValue(wxString());
    dialog.regenerate_preview();
    require(!dialog.valid(), "blank custom scale must invalidate the preview");
    choose(dialog, "print-scaling", 0);
    dialog.regenerate_preview();
    require(dialog.valid(), "inactive blank custom scale must not block fit-to-page printing");
    choose(dialog, "print-scaling", 3);
    dialog.regenerate_preview();
    require(!dialog.valid(), "returning to blank custom scale must invalidate the preview");
    percentage.SetValue(100);
    dialog.regenerate_preview();
    require(dialog.valid(), "correcting custom scale must restore the preview");
    choose(dialog, "print-scaling", 0);
    choose(dialog, "print-layout", 1);
    dialog.regenerate_preview();
    reject_blank_integer("print-columns", 2);
    reject_blank_integer("print-rows", 1);
    auto &columns = control<wxSpinCtrl>(dialog, "print-columns");
    columns.SetValue(wxString());
    dialog.regenerate_preview();
    require(!dialog.valid(), "blank N-up columns must invalidate the preview");
    choose(dialog, "print-layout", 0);
    dialog.regenerate_preview();
    require(dialog.valid(), "inactive blank N-up columns must not block single-page printing");
    choose(dialog, "print-layout", 1);
    dialog.regenerate_preview();
    require(!dialog.valid(), "returning to blank N-up columns must invalidate the preview");
    columns.SetValue(2);
    dialog.regenerate_preview();
    require(dialog.valid(), "correcting N-up columns must restore the preview");
    choose(dialog, "print-layout", 3);
    dialog.regenerate_preview();
    reject_blank_decimal("print-overlap", 5);
    auto &overlap = control<wxSpinCtrlDouble>(dialog, "print-overlap");
    overlap.SetValue(wxString());
    dialog.regenerate_preview();
    require(!dialog.valid(), "blank poster overlap must invalidate the preview");
    choose(dialog, "print-layout", 0);
    dialog.regenerate_preview();
    require(dialog.valid(), "inactive blank poster overlap must not block single-page printing");
    choose(dialog, "print-layout", 3);
    dialog.regenerate_preview();
    require(!dialog.valid(), "returning to blank poster overlap must invalidate the preview");
    overlap.SetValue(5.0);
    dialog.regenerate_preview();
    require(dialog.valid(), "correcting poster overlap must restore the preview");
}
#if defined(__WXGTK__)
void raw_numeric_text_boundaries(vellum::pdf_document &document) {
    using get_buffer_function = void *(*)(void *);
    using set_text_function = void (*)(void *, const char *, int);
    const auto get_buffer =
        reinterpret_cast<get_buffer_function>(dlsym(RTLD_DEFAULT, "gtk_entry_get_buffer"));
    const auto set_text =
        reinterpret_cast<set_text_function>(dlsym(RTLD_DEFAULT, "gtk_entry_buffer_set_text"));
    require(get_buffer && set_text, "GTK text-buffer operations are unavailable");
    vellum::printer_settings printer;
    vellum::print_dialog dialog(nullptr, document, printer, 1);
    dialog.Show();
    events();
    auto &margin = control<wxSpinCtrlDouble>(dialog, "print-margin");
    auto &copies = control<wxSpinCtrl>(dialog, "print-copies");
    for (const char *text : {"0", "100"}) {
        set_text(get_buffer(margin.GetHandle()), text, -1);
        require(margin.GetTextValue() == wxString::FromUTF8(text),
                "raw decimal boundary was not retained");
        dialog.regenerate_preview();
        require(dialog.valid(), "valid decimal boundary must produce a preview");
    }
    margin.SetValue(10.0);
    for (const char *text : {"1", "100"}) {
        set_text(get_buffer(copies.GetHandle()), text, -1);
        require(copies.GetTextValue() == wxString::FromUTF8(text),
                "raw integer boundary was not retained");
        dialog.regenerate_preview();
        require(dialog.valid() && dialog.job().options.copies == std::stoi(text),
                "valid integer boundary must produce the requested number of copies");
    }
    copies.SetValue(1);
    for (const char *text : {"-1", "101", "nan", "inf"}) {
        set_text(get_buffer(margin.GetHandle()), text, -1);
        require(margin.GetTextValue() == wxString::FromUTF8(text),
                "raw decimal input was not retained");
        dialog.regenerate_preview();
        require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
                "out-of-range or non-finite decimal text must reject the preview");
        margin.SetValue(10.0);
        dialog.regenerate_preview();
        require(dialog.valid(), "correcting decimal text must restore the preview");
    }
    for (const char *text : {"0", "101", "nan", "999999999999999999999999"}) {
        set_text(get_buffer(copies.GetHandle()), text, -1);
        require(copies.GetTextValue() == wxString::FromUTF8(text),
                "raw integer input was not retained");
        dialog.regenerate_preview();
        require(!dialog.valid() && !control<wxButton>(dialog, "print-submit").IsEnabled(),
                "out-of-range or malformed integer text must reject the preview");
        copies.SetValue(1);
        dialog.regenerate_preview();
        require(dialog.valid(), "correcting integer text must restore the preview");
    }
}
#endif
} // namespace
int main(int argc, char **argv) {
    if (argc != 4) {
        return 2;
    }
    const std::filesystem::path fixture(argv[1]), restricted(argv[2]), output(argv[3]);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        vellum::pdf_document document(fixture);
        const auto original = document.snapshot();
        vellum::printer_settings printer;
        {
            vellum::print_dialog dialog(nullptr, document, printer, 1);
            dialog.Show();
            events();
            layout_controls(dialog, output);
            tiling_and_errors(dialog, output);
        }
        {
            vellum::print_dialog dialog(nullptr, document, printer, 1);
            dialog.Show();
            events();
            selection_and_copy_controls(dialog);
        }
        {
            vellum::print_dialog dialog(nullptr, document, printer, 1);
            dialog.Show();
            events();
            scaling_and_paper_controls(dialog);
        }
        {
            vellum::print_dialog dialog(nullptr, document, printer, 1);
            dialog.Show();
            events();
            settings_round_trip(dialog);
        }
        {
            vellum::print_dialog dialog(nullptr, document, printer, 1);
            dialog.Show();
            events();
            copy_duplex_and_layout_sequences(dialog);
        }
        {
            vellum::print_dialog dialog(nullptr, document, printer, 1);
            dialog.Show();
            events();
            navigation_and_paper_sequence(dialog, output);
            manual_duplex_and_poster_sequence(dialog);
        }
        {
            vellum::print_dialog dialog(nullptr, document, printer, 1);
            dialog.Show();
            events();
            blank_page_ranges(dialog, output);
        }
        require(!printer.selected(), "canceled native selection changed printer state");
        require(document.snapshot() == original, "print settings changed PDF bytes");
        accepted_settings_sequence(document, false);
        accepted_settings_sequence(document, true);
        booklet_control_sequence(document);
        parity_duplex_sequence(document);
        invalid_current_page_sequence(document);
        invalid_choice_selection_sequence(document);
        adversarial_numeric_input_sequence(document);
        destroy_with_pending_preview(document);
        blank_numeric_controls(document);
#if defined(__WXGTK__)
        raw_numeric_text_boundaries(document);
#endif
        vellum::pdf_document low_quality(restricted.parent_path() / "low-quality-print.pdf");
        {
            vellum::print_dialog dialog(nullptr, low_quality, printer, 1);
            dialog.Show();
            events();
            auto &resolution = control<wxChoice>(dialog, "print-resolution");
            require(resolution.GetCount() == 2 && resolution.GetString(0) == "72" &&
                        resolution.GetString(1) == "150",
                    "low-quality permission must not expose high-resolution choices");
            choose(dialog, "print-resolution", 0);
            events();
            require(dialog.valid(), "allowed low-resolution preview must remain available");
        }
        vellum::pdf_document denied(restricted);
        bool rejected = false;
        try {
            vellum::print_dialog dialog(nullptr, denied, printer, 1);
        } catch (const std::exception &) {
            rejected = true;
        }
        require(rejected, "forbidden printing must reject alternate entry point");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
