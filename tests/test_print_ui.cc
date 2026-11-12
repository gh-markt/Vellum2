#include "vellum/print_dialog.hh"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <wx/app.h>
#include <wx/modalhook.h>
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
void tiling_and_errors(vellum::print_dialog &dialog) {
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
    ranges.SetValue("1-2");
    events();
    require(dialog.valid(), "valid ranges must restore Print");
    cancel_native hook;
    button(dialog, wxID_SETUP);
    events();
    require(hook.calls() == 1 && dialog.valid(), "native printer selection cancellation");
}
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
            tiling_and_errors(dialog);
        }
        require(!printer.selected(), "canceled native selection changed printer state");
        require(document.snapshot() == original, "print settings changed PDF bytes");
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
