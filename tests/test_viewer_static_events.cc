#include "vellum/main_frame.hh"
#include "vellum/print_dialog.hh"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <wx/app.h>
#include <wx/statusbr.h>

namespace {
bool watching = false;
int dynamic_bindings = 0;
wxDialog *constructing_dialog = nullptr;
int construction_events = 0;
} // namespace

extern "C" void __real__ZN12wxEvtHandler6DoBindEiiiP14wxEventFunctorP8wxObject(void *, int, int,
                                                                               int,
                                                                               wxEventFunctor *,
                                                                               wxObject *);
extern "C" void __wrap__ZN12wxEvtHandler6DoBindEiiiP14wxEventFunctorP8wxObject(
    void *handler, int id, int last, int type, wxEventFunctor *functor, wxObject *user_data) {
    if (watching) {
        ++dynamic_bindings;
    }
    __real__ZN12wxEvtHandler6DoBindEiiiP14wxEventFunctorP8wxObject(handler, id, last, type, functor,
                                                                   user_data);
}

extern "C" void __real__ZN8wxDialogC2EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(
    void *, wxWindow *, int, const wxString &, const wxPoint &, const wxSize &, long,
    const wxString &);
extern "C" void __wrap__ZN8wxDialogC2EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(
    void *dialog, wxWindow *parent, int id, const wxString &title, const wxPoint &position,
    const wxSize &size, long style, const wxString &name) {
    __real__ZN8wxDialogC2EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(
        dialog, parent, id, title, position, size, style, name);
    if (watching) {
        constructing_dialog = static_cast<wxDialog *>(dialog);
    }
}

extern "C" void __real__ZN15wxFlexGridSizer14AddGrowableColEmi(void *, unsigned long, int);
extern "C" void __wrap__ZN15wxFlexGridSizer14AddGrowableColEmi(void *grid, unsigned long column,
                                                               int proportion) {
    __real__ZN15wxFlexGridSizer14AddGrowableColEmi(grid, column, proportion);
    if (watching && constructing_dialog) {
        wxCommandEvent change(wxEVT_CHOICE, wxID_ANY);
        wxSpinEvent spin(wxEVT_SPINCTRL, wxID_ANY);
        wxSpinDoubleEvent decimal(wxEVT_SPINCTRLDOUBLE, wxID_ANY);
        constructing_dialog->GetEventHandler()->ProcessEvent(change);
        constructing_dialog->GetEventHandler()->ProcessEvent(spin);
        constructing_dialog->GetEventHandler()->ProcessEvent(decimal);
        construction_events += 3;
        constructing_dialog = nullptr;
    }
}

class static_event_test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        this->SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(static_event_test_app);

void send_menu(vellum::main_frame &frame, int menu, const wxString &label) {
    const int id = frame.GetMenuBar()->GetMenu(menu)->FindItem(label);
    if (id == wxNOT_FOUND) {
        throw std::runtime_error("viewer menu item missing");
    }
    wxCommandEvent event(wxEVT_MENU, id);
    if (!frame.GetEventHandler()->ProcessEvent(event)) {
        throw std::runtime_error("viewer menu event was not handled");
    }
}

int main(int argc, char **argv) {
    if (argc != 2 || !wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        watching = true;
        vellum::main_frame frame;
        watching = false;
        if (dynamic_bindings != 0) {
            throw std::runtime_error("viewer construction used unsafe dynamic event registration");
        }
        frame.Show();
        frame.open_path(argv[1]);
        wxYield();
        send_menu(frame, 1, "Next page");
        if (frame.current_page() != 2) {
            throw std::runtime_error("static next-page event did not navigate");
        }
        send_menu(frame, 1, "Previous page");
        send_menu(frame, 2, "100%");
        if (frame.current_page() != 1 || std::abs(frame.zoom_scale() - 1.0) > .0001 ||
            !frame.GetStatusBar()->GetStatusText().Contains("Page 1 / 2")) {
            throw std::runtime_error("static viewer events did not preserve navigation or zoom");
        }
        vellum::pdf_document document(argv[1]);
        vellum::printer_settings printer;
        dynamic_bindings = 0;
        watching = true;
        vellum::print_dialog dialog(&frame, document, printer, 1);
        watching = false;
        if (dynamic_bindings != 0 || construction_events != 3) {
            throw std::runtime_error("print dialog used unsafe dynamic event registration");
        }
        auto *layout = dynamic_cast<wxChoice *>(dialog.FindWindowByName("print-layout"));
        auto *orientation = dynamic_cast<wxChoice *>(dialog.FindWindowByName("print-orientation"));
        auto *duplex = dynamic_cast<wxChoice *>(dialog.FindWindowByName("print-duplex"));
        if (!layout || !orientation || !duplex) {
            throw std::runtime_error("print controls missing");
        }
        layout->SetSelection(2);
        wxCommandEvent change(wxEVT_CHOICE, layout->GetId());
        change.SetEventObject(layout);
        if (!dialog.GetEventHandler()->ProcessEvent(change) || orientation->GetSelection() != 1 ||
            duplex->GetSelection() != 2) {
            throw std::runtime_error("static print-layout event did not configure booklet options");
        }
        dialog.regenerate_preview();
        if (!dialog.valid()) {
            throw std::runtime_error("static print event did not regenerate a valid preview");
        }
    } catch (const std::exception &error) {
        watching = false;
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
