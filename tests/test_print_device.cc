#include "vellum/atomic_file.hh"
#include "vellum/print_dialog.hh"
#include "vellum/printing.hh"
#include "vellum/sheet_painter.hh"
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <execinfo.h>
#include <limits>
#include <new>
#endif
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <wx/app.h>
#include <wx/dcmemory.h>
#include <wx/dcps.h>
#include <wx/log.h>
#include <wx/modalhook.h>
#include <wx/print.h>
#include <wx/printdlg.h>
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
namespace {
thread_local bool track_submission_allocations = false;
thread_local bool checking_submission_stack = false;
thread_local std::size_t submission_allocations = 0;
thread_local std::size_t fail_submission_allocation = 0;
bool submission_owned_allocation() {
    if (checking_submission_stack) {
        return false;
    }
    checking_submission_stack = true;
    void *frames[40];
    const int count = ::backtrace(frames, 40);
    bool owned = false;
    for (int index = 2; index < count; ++index) {
        Dl_info info{};
        if (!::dladdr(frames[index], &info)) {
            continue;
        }
        if (info.dli_fname && std::strstr(info.dli_fname, "libwx")) {
            break;
        }
        if (info.dli_sname && std::strncmp(info.dli_sname, "_ZN6vellum", 10) == 0) {
            owned = true;
            break;
        }
    }
    checking_submission_stack = false;
    return owned;
}
} // namespace
[[gnu::noinline]] void *operator new(std::size_t size) {
    if (track_submission_allocations && submission_owned_allocation() &&
        submission_allocations++ == fail_submission_allocation) {
        track_submission_allocations = false;
        throw std::bad_alloc();
    }
    if (void *value = std::malloc(size ? size : 1)) {
        return value;
    }
    throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](std::size_t size) {
    return ::operator new(size);
}
[[gnu::noinline]] void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
[[gnu::noinline]] void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
    return ::operator new(size, std::nothrow);
}
[[gnu::noinline]] void operator delete(void *value) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete(void *value, std::size_t) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value, std::size_t) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete(void *value, const std::nothrow_t &) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value, const std::nothrow_t &) noexcept {
    std::free(value);
}
#endif
class device_test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(device_test_app);
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
bool deny_next_page_permission = false;
int denied_page_permission_checks = 0;
enum class native_printer_fault {
    none,
    dialog_copy,
    dialog_construct,
    dialog_assign,
    printout,
    printer
};
native_printer_fault injected_native_printer_fault = native_printer_fault::none;
int native_printer_faults = 0;
void native_printer_checkpoint(native_printer_fault expected) {
    if (injected_native_printer_fault == expected) {
        ++native_printer_faults;
        throw std::bad_alloc();
    }
}
extern "C" void __real__ZN17wxPrintDialogDataC1ERKS_(void *, const wxPrintDialogData &);
extern "C" void __wrap__ZN17wxPrintDialogDataC1ERKS_(void *data, const wxPrintDialogData &other) {
    native_printer_checkpoint(native_printer_fault::dialog_copy);
    __real__ZN17wxPrintDialogDataC1ERKS_(data, other);
}
extern "C" wxPrintDialogData &__real__ZN17wxPrintDialogDataaSERKS_(void *,
                                                                   const wxPrintDialogData &);
extern "C" wxPrintDialogData &__wrap__ZN17wxPrintDialogDataaSERKS_(void *data,
                                                                   const wxPrintDialogData &other) {
    native_printer_checkpoint(native_printer_fault::dialog_assign);
    return __real__ZN17wxPrintDialogDataaSERKS_(data, other);
}
extern "C" void __real__ZN13wxPrintDialogC1EP8wxWindowP17wxPrintDialogData(void *, wxWindow *,
                                                                           wxPrintDialogData *);
extern "C" void
__wrap__ZN13wxPrintDialogC1EP8wxWindowP17wxPrintDialogData(void *dialog, wxWindow *parent,
                                                           wxPrintDialogData *data) {
    native_printer_checkpoint(native_printer_fault::dialog_construct);
    __real__ZN13wxPrintDialogC1EP8wxWindowP17wxPrintDialogData(dialog, parent, data);
}
extern "C" void __real__ZN10wxPrintoutC2ERK8wxString(void *, const wxString &);
extern "C" void __wrap__ZN10wxPrintoutC2ERK8wxString(void *printout, const wxString &title) {
    native_printer_checkpoint(native_printer_fault::printout);
    __real__ZN10wxPrintoutC2ERK8wxString(printout, title);
}
extern "C" void __real__ZN9wxPrinterC1EP17wxPrintDialogData(void *, wxPrintDialogData *);
extern "C" void __wrap__ZN9wxPrinterC1EP17wxPrintDialogData(void *printer,
                                                            wxPrintDialogData *data) {
    native_printer_checkpoint(native_printer_fault::printer);
    __real__ZN9wxPrinterC1EP17wxPrintDialogData(printer, data);
}
extern "C" bool __real__ZNK6vellum12pdf_document9may_printEv(const void *);
extern "C" bool __wrap__ZNK6vellum12pdf_document9may_printEv(const void *document) {
    if (deny_next_page_permission) {
        deny_next_page_permission = false;
        ++denied_page_permission_checks;
        return false;
    }
    return __real__ZNK6vellum12pdf_document9may_printEv(document);
}
#endif
class accept_print_settings : public wxModalDialogHook {
  public:
    explicit accept_print_settings(int copies = 3, wxDuplexMode duplex = wxDUPLEX_HORIZONTAL)
        : copies(copies), duplex(duplex) {
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
        data.SetNoCopies(this->copies);
        data.SetCollate(false);
        data.GetPrintData().SetOrientation(wxLANDSCAPE);
        data.GetPrintData().SetDuplex(this->duplex);
        data.GetPrintData().SetColour(false);
        return wxID_OK;
    }

  private:
    bool seen = false;
    int copies;
    wxDuplexMode duplex;
};
class inspect_print_settings : public wxModalDialogHook {
  public:
    inspect_print_settings() { this->Register(); }
    bool seen = false;
    int copies = 0;
    bool collate = false;
    bool colour = false;
    wxDuplexMode duplex = wxDUPLEX_SIMPLEX;

  protected:
    int Enter(wxDialog *dialog) override {
        if (auto *native = dynamic_cast<wxPrintDialogBase *>(dialog)) {
            auto &data = native->GetPrintDialogData();
            this->seen = true;
            this->copies = data.GetNoCopies();
            this->collate = data.GetCollate();
            this->colour = data.GetPrintData().GetColour();
            this->duplex = data.GetPrintData().GetDuplex();
        }
        return wxID_CANCEL;
    }
};
class fail_print_settings : public wxModalDialogHook {
  public:
    fail_print_settings() { Register(); }
    bool seen = false;

  protected:
    int Enter(wxDialog *dialog) override {
        if (dynamic_cast<wxPrintDialogBase *>(dialog)) {
            seen = true;
            throw std::runtime_error("injected printer dialog failure");
        }
        return wxID_CANCEL;
    }
};
#if wxUSE_POSTSCRIPT
enum class measurement {
    valid,
    unavailable,
    no_x_ppi,
    no_y_ppi,
    no_width,
    no_height,
    no_printable_width,
    no_printable_height,
    throw_paper,
    throw_ppi,
    throw_size,
    throw_construct
};
struct submission_state {
    wxDuplexMode expected_duplex = wxDUPLEX_SIMPLEX;
    bool expected_colour = true;
    wxPrinterError outcome = wxPRINTER_NO_ERROR;
    bool check_content = true;
    bool throw_on_submit = false;
    bool deny_page_permission = false;
    int outcome_on_call = 0;
    measurement geometry = measurement::valid;
    int calls = 0;
};
class controlled_printer_dc : public wxPostScriptDCImpl {
  public:
    controlled_printer_dc(wxPrinterDC *owner, const wxPrintData &data, submission_state &state)
        : wxPostScriptDCImpl(owner, data), state(state) {}
    bool IsOk() const override {
        return state.geometry != measurement::unavailable && wxPostScriptDCImpl::IsOk();
    }
    wxSize GetPPI() const override {
        if (state.geometry == measurement::throw_ppi) {
            throw std::runtime_error("injected printer PPI failure");
        }
        return {state.geometry == measurement::no_x_ppi ? 0 : 72,
                state.geometry == measurement::no_y_ppi ? 0 : 144};
    }
    wxRect GetPaperRect() const override {
        if (state.geometry == measurement::throw_paper) {
            throw std::runtime_error("injected printer paper failure");
        }
        return {-10, -20, state.geometry == measurement::no_width ? 0 : 612,
                state.geometry == measurement::no_height ? 0 : 792};
    }
    void DoGetSize(int *width, int *height) const override {
        if (state.geometry == measurement::throw_size) {
            throw std::runtime_error("injected printer size failure");
        }
        *width = state.geometry == measurement::no_printable_width ? 0 : 590;
        *height = state.geometry == measurement::no_printable_height ? 0 : 760;
    }

  private:
    submission_state &state;
};
class controlled_printer : public wxPrinterBase {
  public:
    controlled_printer(wxPrintDialogData *data, submission_state &state)
        : wxPrinterBase(data), state(state) {}
    bool Setup(wxWindow *) override { return false; }
    wxDC *PrintDialog(wxWindow *) override { return nullptr; }
    bool Print(wxWindow *, wxPrintout *output, bool prompt) override {
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
        track_submission_allocations = false;
#endif
        ++this->state.calls;
        if (this->state.throw_on_submit) {
            throw std::runtime_error("injected printer submission failure");
        }
        auto &settings = this->m_printDialogData.GetPrintData();
        if (prompt || settings.GetDuplex() != this->state.expected_duplex ||
            settings.GetColour() != this->state.expected_colour || settings.GetNoCopies() != 1 ||
            this->m_printDialogData.GetNoCopies() != 1 || settings.GetCollate()) {
            throw std::runtime_error("native submission settings were translated incorrectly");
        }
        int minimum = 0, maximum = 0, first = 0, last = 0;
        output->GetPageInfo(&minimum, &maximum, &first, &last);
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
        if (this->state.deny_page_permission) {
            wxLogBuffer messages;
            struct restore_log {
                wxLog *previous;
                ~restore_log() { wxLog::SetActiveTarget(previous); }
            } restore{wxLog::SetActiveTarget(&messages)};
            wxBitmap target(200, 200, 24);
            wxMemoryDC denied_dc(target);
            denied_dc.SetBackground(*wxBLUE_BRUSH);
            denied_dc.Clear();
            output->SetDC(&denied_dc);
            output->SetPaperRectPixels(wxRect(0, 0, 200, 200));
            deny_next_page_permission = true;
            const bool printed = output->OnPrintPage(1);
            output->SetDC(nullptr);
            denied_dc.SelectObject(wxNullBitmap);
            const auto denied_image = target.ConvertToImage();
            if (printed || denied_page_permission_checks != 1 || !denied_image.IsOk() ||
                !messages.GetBuffer().Contains("PDF permissions prohibit printing")) {
                throw std::runtime_error("printout did not enforce a denied page permission");
            }
            for (int y = 0; y < denied_image.GetHeight(); ++y) {
                for (int x = 0; x < denied_image.GetWidth(); ++x) {
                    if (denied_image.GetRed(x, y) != 0 || denied_image.GetGreen(x, y) != 0 ||
                        denied_image.GetBlue(x, y) != 255) {
                        throw std::runtime_error("denied printout page drew on its device");
                    }
                }
            }
        }
#endif
        if (minimum != 1 || maximum < 1 || first != 1 || last != maximum || output->HasPage(0) ||
            !output->HasPage(1) || output->HasPage(maximum + 1) || output->OnPrintPage(1)) {
            throw std::runtime_error("printout page bounds or missing-DC handling is incorrect");
        }
        wxBitmap bitmap(600, 850, 24);
        wxMemoryDC dc(bitmap);
        output->SetDC(&dc);
        output->SetPaperRectPixels(wxRect(0, 0, 0, 850));
        {
            wxLogNull silence_expected_error;
            if (output->OnPrintPage(1)) {
                throw std::runtime_error("printout accepted unusable paper geometry");
            }
        }
        {
            wxLogBuffer messages;
            struct restore_log {
                wxLog *previous;
                ~restore_log() { wxLog::SetActiveTarget(previous); }
            } restore{wxLog::SetActiveTarget(&messages)};
            if (output->OnPrintPage(1) ||
                !messages.GetBuffer().Contains("invalid print render request")) {
                throw std::runtime_error("printout failed to report invalid paper geometry");
            }
        }
        output->SetPaperRectPixels(wxRect(0, 0, 600, 850));
        if (output->OnPrintPage(0) || !output->OnPrintPage(1)) {
            throw std::runtime_error("printout failed to render the expected page");
        }
        output->SetDC(nullptr);
        dc.SelectObject(wxNullBitmap);
        const auto image = bitmap.ConvertToImage();
        bool content = false;
        for (int y = 0; y < image.GetHeight() && !content; ++y) {
            for (int x = 0; x < image.GetWidth(); ++x) {
                if (this->state.expected_colour
                        ? image.GetRed(x, y) > 200 && image.GetGreen(x, y) < 50 &&
                              image.GetBlue(x, y) < 50
                        : image.GetRed(x, y) < 240 && image.GetRed(x, y) == image.GetGreen(x, y) &&
                              image.GetGreen(x, y) == image.GetBlue(x, y)) {
                    content = true;
                    break;
                }
            }
        }
        if (this->state.check_content && !content) {
            throw std::runtime_error("submitted page did not contain the expected image");
        }
        const auto outcome =
            this->state.outcome_on_call == 0 || this->state.calls == this->state.outcome_on_call
                ? this->state.outcome
                : wxPRINTER_NO_ERROR;
        sm_lastError = outcome;
        return outcome == wxPRINTER_NO_ERROR;
    }

  private:
    submission_state &state;
};
class controlled_print_factory : public wxNativePrintFactory {
  public:
    explicit controlled_print_factory(submission_state &state) : state(state) {}
    wxPrinterBase *CreatePrinter(wxPrintDialogData *data) override {
        return new controlled_printer(data, this->state);
    }
    wxDCImpl *CreatePrinterDCImpl(wxPrinterDC *owner, const wxPrintData &data) override {
        if (this->state.geometry == measurement::throw_construct) {
            throw std::runtime_error("injected printer initialization failure");
        }
        return new controlled_printer_dc(owner, data, this->state);
    }

  private:
    submission_state &state;
};
void verify_measurement(vellum::printer_settings &printer, vellum::pdf_document &document) {
    submission_state state;
    wxPrintFactory::SetPrintFactory(new controlled_print_factory(state));
    struct restore_factory {
        ~restore_factory() { wxPrintFactory::SetPrintFactory(new wxNativePrintFactory); }
    } restore;
    const auto measured = printer.measure({});
    if (measured.paper.width != 612 || measured.paper.height != 396 || measured.printable.x != 10 ||
        measured.printable.y != 10 || measured.printable.width != 590 ||
        measured.printable.height != 380) {
        throw std::runtime_error("printer geometry did not convert from pixels and PPI correctly");
    }
    vellum::print_dialog dialog(nullptr, document, printer, 1);
    if (!dialog.valid()) {
        throw std::runtime_error("valid printer geometry did not produce a print plan");
    }
    for (const auto problem :
         {measurement::unavailable, measurement::no_x_ppi, measurement::no_y_ppi,
          measurement::no_width, measurement::no_height, measurement::no_printable_width,
          measurement::no_printable_height, measurement::throw_paper, measurement::throw_ppi,
          measurement::throw_size, measurement::throw_construct}) {
        state.geometry = problem;
        bool rejected = false;
        try {
            printer.measure({});
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        dialog.regenerate_preview();
        auto *submit = wxWindow::FindWindowByName("print-submit", &dialog);
        if (!rejected || dialog.valid() || submit == nullptr || submit->IsEnabled()) {
            throw std::runtime_error("unusable printer geometry did not disable submission");
        }
        state.geometry = measurement::valid;
        dialog.regenerate_preview();
        if (!dialog.valid() || !submit->IsEnabled()) {
            throw std::runtime_error(
                "print plan did not recover after printer geometry was restored");
        }
    }
}
void verify_submission(vellum::printer_settings &printer, vellum::pdf_document &document) {
    submission_state state;
    wxPrintFactory::SetPrintFactory(new controlled_print_factory(state));
    struct restore_factory {
        ~restore_factory() { wxPrintFactory::SetPrintFactory(new wxNativePrintFactory); }
    } restore;
    auto options = vellum::print_options{};
    auto job = vellum::compose_print_job({document.dimensions(0)}, {1}, options);
    bool invalid_dpi_rejected = false;
    try {
        printer.submit(nullptr, document, job, 601);
    } catch (const std::runtime_error &) {
        invalid_dpi_rejected = true;
    }
    if (!invalid_dpi_rejected || state.calls != 0) {
        throw std::runtime_error("invalid resolution reached the printer");
    }
    for (const auto &[duplex, sides, native] :
         {std::tuple{vellum::print_duplex::simplex, vellum::sheet_subset::all, wxDUPLEX_SIMPLEX},
          std::tuple{vellum::print_duplex::long_edge, vellum::sheet_subset::all, wxDUPLEX_VERTICAL},
          std::tuple{vellum::print_duplex::short_edge, vellum::sheet_subset::all,
                     wxDUPLEX_HORIZONTAL},
          std::tuple{vellum::print_duplex::manual, vellum::sheet_subset::all, wxDUPLEX_SIMPLEX},
          std::tuple{vellum::print_duplex::long_edge, vellum::sheet_subset::fronts,
                     wxDUPLEX_SIMPLEX}}) {
        job.options.duplex = duplex;
        job.options.sides = sides;
        state.expected_duplex = native;
        if (!printer.submit(nullptr, document, job, 72)) {
            throw std::runtime_error("successful virtual printer submission was cancelled");
        }
    }
    job.options.grayscale = true;
    state.expected_colour = false;
    if (!printer.submit(nullptr, document, job, 72)) {
        throw std::runtime_error("grayscale virtual printer submission was cancelled");
    }
    state.outcome = wxPRINTER_CANCELLED;
    if (printer.submit(nullptr, document, job, 72)) {
        throw std::runtime_error("cancelled printer submission reported success");
    }
    state.outcome = wxPRINTER_ERROR;
    bool failed = false;
    try {
        printer.submit(nullptr, document, job, 72);
    } catch (const std::runtime_error &) {
        failed = true;
    }
    if (!failed || state.calls != 8) {
        throw std::runtime_error("printer failure was not reported correctly");
    }
    const auto original = document.snapshot();
    state.outcome = wxPRINTER_NO_ERROR;
    state.throw_on_submit = true;
    failed = false;
    try {
        printer.submit(nullptr, document, job, 72);
    } catch (const std::runtime_error &error) {
        failed = std::string(error.what()) == "injected printer submission failure";
    }
    state.throw_on_submit = false;
    if (!failed || document.snapshot() != original || !printer.submit(nullptr, document, job, 72)) {
        throw std::runtime_error("printer submission did not recover after device exception");
    }
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
    state.deny_page_permission = true;
    denied_page_permission_checks = 0;
    if (!printer.submit(nullptr, document, job, 72) || denied_page_permission_checks != 1 ||
        document.snapshot() != original) {
        throw std::runtime_error("printout did not recover after a denied page permission");
    }
#endif
}
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
void verify_submission_allocations(vellum::printer_settings &printer,
                                   vellum::pdf_document &document) {
    submission_state state;
    wxPrintFactory::SetPrintFactory(new controlled_print_factory(state));
    struct restore_factory {
        ~restore_factory() { wxPrintFactory::SetPrintFactory(new wxNativePrintFactory); }
    } restore;
    const auto original = document.snapshot();
    const auto job = vellum::compose_print_job({document.dimensions(0)}, {1}, {});
    submission_allocations = 0;
    fail_submission_allocation = std::numeric_limits<std::size_t>::max();
    track_submission_allocations = true;
    const bool baseline = printer.submit(nullptr, document, job, 72);
    track_submission_allocations = false;
    const auto total = submission_allocations;
    if (!baseline || total == 0 || state.calls != 1) {
        throw std::runtime_error("printer submission allocation baseline was not observed");
    }
    for (std::size_t failure = 0; failure < total; ++failure) {
        const int calls_before = state.calls;
        submission_allocations = 0;
        fail_submission_allocation = failure;
        track_submission_allocations = true;
        bool rejected = false;
        try {
            printer.submit(nullptr, document, job, 72);
        } catch (const std::bad_alloc &) {
            rejected = true;
        }
        track_submission_allocations = false;
        if (!rejected || submission_allocations != failure + 1 || state.calls != calls_before ||
            document.snapshot() != original || job.sides.size() != 1 ||
            job.sides[0].placements.size() != 1 || job.sides[0].placements[0].page != 1) {
            throw std::runtime_error(
                "submission allocation failure changed inputs or reached printer");
        }
        if (!printer.submit(nullptr, document, job, 72) || state.calls != calls_before + 1 ||
            document.snapshot() != original) {
            throw std::runtime_error("printer submission did not recover after allocation failure");
        }
    }
    for (const auto fault : {native_printer_fault::dialog_copy, native_printer_fault::printout,
                             native_printer_fault::printer}) {
        const int calls_before = state.calls;
        native_printer_faults = 0;
        injected_native_printer_fault = fault;
        bool rejected = false;
        try {
            printer.submit(nullptr, document, job, 72);
        } catch (const std::bad_alloc &) {
            rejected = true;
        }
        injected_native_printer_fault = native_printer_fault::none;
        if (!rejected || native_printer_faults != 1 || state.calls != calls_before ||
            document.snapshot() != original || job.sides.size() != 1) {
            throw std::runtime_error(
                "native printer construction failure changed submission state");
        }
        if (!printer.submit(nullptr, document, job, 72) || state.calls != calls_before + 1 ||
            document.snapshot() != original) {
            throw std::runtime_error("submission did not recover from native construction failure");
        }
    }
    std::cout << "Printer submission allocation failures: " << total << " cases\n";
}
#endif
class submission_messages : public wxModalDialogHook {
  public:
    explicit submission_messages(bool continue_duplex, bool accept_printer = false)
        : continue_duplex(continue_duplex), accept_printer(accept_printer) {
        Register();
    }
    int duplex_prompts = 0;
    int errors = 0;
    int printer_prompts = 0;
    bool unexpected = false;

  protected:
    int Enter(wxDialog *dialog) override {
        if (dynamic_cast<vellum::print_dialog *>(dialog)) {
            return wxID_NONE;
        }
        if (auto *native = dynamic_cast<wxPrintDialogBase *>(dialog); native && accept_printer) {
            ++printer_prompts;
            auto &data = native->GetPrintDialogData();
            data.SetNoCopies(1);
            data.GetPrintData().SetDuplex(wxDUPLEX_SIMPLEX);
            data.GetPrintData().SetColour(true);
            return wxID_OK;
        }
        if (dialog->GetTitle() == "Manual duplex") {
            ++duplex_prompts;
            return continue_duplex ? wxID_YES : wxID_NO;
        }
        if (dialog->GetTitle() == "Printing failed") {
            ++errors;
            return wxID_OK;
        }
        unexpected = true;
        return wxID_CANCEL;
    }

  private:
    bool continue_duplex;
    bool accept_printer;
};
void verify_dialog_submissions(vellum::printer_settings &printer, vellum::pdf_document &document) {
    submission_state state;
    state.check_content = false;
    wxPrintFactory::SetPrintFactory(new controlled_print_factory(state));
    struct restore_factory {
        ~restore_factory() { wxPrintFactory::SetPrintFactory(new wxNativePrintFactory); }
    } restore;
    {
        vellum::printer_settings unselected;
        submission_messages messages(true, true);
        vellum::print_dialog dialog(nullptr, document, unselected, 1);
        bool first_step_valid = false;
        state.calls = 0;
        wxTheApp->CallAfter([&] {
            wxCommandEvent first(wxEVT_BUTTON, wxID_PRINT);
            dialog.GetEventHandler()->ProcessEvent(first);
            first_step_valid = unselected.selected() && dialog.valid() && state.calls == 0 &&
                               messages.printer_prompts == 1;
            if (first_step_valid) {
                wxCommandEvent second(wxEVT_BUTTON, wxID_PRINT);
                dialog.GetEventHandler()->ProcessEvent(second);
            }
            if (dialog.IsModal()) {
                dialog.EndModal(wxID_CANCEL);
            }
        });
        const auto result = dialog.ShowModal();
        if (!first_step_valid || result != wxID_OK || state.calls != 1 || messages.unexpected) {
            throw std::runtime_error(
                "selecting a printer during submission did not preserve the job");
        }
    }
    {
        submission_messages messages(true);
        vellum::print_dialog dialog(nullptr, document, printer, 1);
        auto *resolution =
            dynamic_cast<wxChoice *>(wxWindow::FindWindowByName("print-resolution", &dialog));
        if (resolution == nullptr || resolution->GetCount() == 0) {
            throw std::runtime_error("print resolution control is missing");
        }
        resolution->SetString(0, "invalid");
        resolution->SetSelection(0);
        state.calls = 0;
        bool rejected = false;
        wxTheApp->CallAfter([&] {
            wxCommandEvent first(wxEVT_BUTTON, wxID_PRINT);
            dialog.GetEventHandler()->ProcessEvent(first);
            rejected = state.calls == 0 && messages.errors == 1 && dialog.IsModal();
            resolution->SetString(0, "72");
            if (rejected) {
                wxCommandEvent second(wxEVT_BUTTON, wxID_PRINT);
                dialog.GetEventHandler()->ProcessEvent(second);
            }
            if (dialog.IsModal()) {
                dialog.EndModal(wxID_CANCEL);
            }
        });
        const auto result = dialog.ShowModal();
        if (!rejected || result != wxID_OK || state.calls != 1 || messages.unexpected) {
            throw std::runtime_error("invalid print resolution was not rejected and recovered");
        }
    }
    struct scenario {
        bool manual;
        bool continue_duplex;
        wxPrinterError outcome;
        int outcome_on_call;
        int expected_calls;
        int expected_prompts;
        int expected_errors;
        int expected_result;
    };
    for (const auto &test : {scenario{false, true, wxPRINTER_NO_ERROR, 0, 1, 0, 0, wxID_OK},
                             scenario{false, true, wxPRINTER_CANCELLED, 0, 1, 0, 0, wxID_CANCEL},
                             scenario{false, true, wxPRINTER_ERROR, 0, 1, 0, 1, wxID_CANCEL},
                             scenario{true, true, wxPRINTER_NO_ERROR, 0, 2, 1, 0, wxID_OK},
                             scenario{true, false, wxPRINTER_NO_ERROR, 0, 1, 1, 0, wxID_CANCEL},
                             scenario{true, true, wxPRINTER_CANCELLED, 1, 1, 0, 0, wxID_CANCEL},
                             scenario{true, true, wxPRINTER_CANCELLED, 2, 2, 1, 0, wxID_CANCEL},
                             scenario{true, true, wxPRINTER_ERROR, 2, 2, 1, 1, wxID_CANCEL}}) {
        state.calls = 0;
        state.outcome = test.outcome;
        state.outcome_on_call = test.outcome_on_call;
        state.expected_duplex = wxDUPLEX_SIMPLEX;
        state.expected_colour = true;
        submission_messages messages(test.continue_duplex);
        vellum::print_dialog dialog(nullptr, document, printer, 1);
        auto *duplex =
            dynamic_cast<wxChoice *>(wxWindow::FindWindowByName("print-duplex", &dialog));
        if (duplex == nullptr) {
            throw std::runtime_error("duplex control is missing");
        }
        duplex->SetSelection(test.manual ? 3 : 0);
        dialog.regenerate_preview();
        if (!dialog.valid()) {
            throw std::runtime_error("controlled printer geometry did not produce a valid plan");
        }
        wxTheApp->CallAfter([&] {
            wxCommandEvent event(wxEVT_BUTTON, wxID_PRINT);
            dialog.GetEventHandler()->ProcessEvent(event);
            if (dialog.IsModal()) {
                dialog.EndModal(wxID_CANCEL);
            }
        });
        const auto result = dialog.ShowModal();
        if (result != test.expected_result || state.calls != test.expected_calls ||
            messages.duplex_prompts != test.expected_prompts ||
            messages.errors != test.expected_errors || messages.unexpected) {
            throw std::runtime_error("print dialog did not handle submission outcome correctly");
        }
    }
}
void verify_painter_boundaries(vellum::pdf_document &document) {
    auto job = vellum::compose_print_job({document.dimensions(0)}, {1}, {});
    vellum::sheet_painter painter(document);
    wxBitmap bitmap(200, 200, 24);
    wxMemoryDC dc(bitmap);
    const auto rejected = [&](std::size_t side, wxRect paper, int dpi) {
        try {
            painter.paint(dc, job, side, paper, dpi, false);
        } catch (const std::invalid_argument &) {
            return true;
        }
        return false;
    };
    if (!rejected(job.sides.size(), wxRect(0, 0, 200, 200), 72) ||
        !rejected(0, wxRect(0, 0, 0, 200), 72) || !rejected(0, wxRect(0, 0, 200, 0), 72) ||
        !rejected(0, wxRect(0, 0, 200, 200), 35) || !rejected(0, wxRect(0, 0, 200, 200), 601)) {
        throw std::runtime_error("invalid painter request was accepted");
    }
    auto blank = vellum::compose_print_job({document.dimensions(0)}, {0}, {});
    for (const int dpi : {36, 600}) {
        painter.paint(dc, blank, 0, wxRect(0, 0, 200, 200), dpi, false);
    }
    auto invalid_poster = job;
    invalid_poster.options.layout = vellum::print_layout::poster;
    invalid_poster.sides[0].placements.clear();
    for (const bool labels : {false, true}) {
        invalid_poster.options.tile_labels = labels;
        bool poster_rejected = false;
        try {
            painter.paint(dc, invalid_poster, 0, wxRect(0, 0, 200, 200), 72, false);
        } catch (const std::invalid_argument &) {
            poster_rejected = true;
        }
        if (!poster_rejected) {
            throw std::runtime_error("empty poster side was not rejected");
        }
    }
    for (const auto destination :
         {vellum::page_rectangle{1000, 0, 50, 50}, vellum::page_rectangle{0, 1000, 50, 50}}) {
        job.sides[0].placements[0].destination = destination;
        painter.paint(dc, job, 0, wxRect(0, 0, 200, 200), 72, false);
        dc.SelectObject(wxNullBitmap);
        const auto image = bitmap.ConvertToImage();
        for (int y = 0; y < image.GetHeight(); ++y) {
            for (int x = 0; x < image.GetWidth(); ++x) {
                if (image.GetRed(x, y) != 255 || image.GetGreen(x, y) != 255 ||
                    image.GetBlue(x, y) != 255) {
                    throw std::runtime_error("placement outside its clip painted on the sheet");
                }
            }
        }
        dc.SelectObject(bitmap);
    }
}
void verify_painter_permissions(const std::filesystem::path &fixtures) {
    for (const auto &[name, dpi] :
         {std::pair{"restricted.pdf", 72}, std::pair{"low-quality-print.pdf", 151}}) {
        vellum::pdf_document document(fixtures / name);
        const auto original = document.snapshot();
        const auto job = vellum::compose_print_job({document.dimensions(0)}, {1}, {});
        vellum::sheet_painter painter(document);
        wxBitmap bitmap(200, 200, 24);
        wxMemoryDC dc(bitmap);
        dc.SetBackground(*wxBLUE_BRUSH);
        dc.Clear();
        bool rejected = false;
        try {
            painter.paint(dc, job, 0, wxRect(0, 0, 200, 200), dpi, false);
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        dc.SelectObject(wxNullBitmap);
        auto image = bitmap.ConvertToImage();
        if (!rejected || document.snapshot() != original || !image.IsOk()) {
            throw std::runtime_error("denied print changed document or drawing device");
        }
        for (int y = 0; y < image.GetHeight(); ++y) {
            for (int x = 0; x < image.GetWidth(); ++x) {
                if (image.GetRed(x, y) != 0 || image.GetGreen(x, y) != 0 ||
                    image.GetBlue(x, y) != 255) {
                    throw std::runtime_error("denied print drew on the device");
                }
            }
        }
        if (dpi == 151) {
            dc.SelectObject(bitmap);
            painter.paint(dc, job, 0, wxRect(0, 0, 200, 200), 150, false);
            dc.SelectObject(wxNullBitmap);
            image = bitmap.ConvertToImage();
            if (!image.IsOk() || image.GetRed(0, 0) != 255 || image.GetGreen(0, 0) != 255 ||
                image.GetBlue(0, 0) != 255 || document.snapshot() != original) {
                throw std::runtime_error("allowed low-resolution print did not render safely");
            }
        }
    }
}
void verify_blank_output(vellum::pdf_document &document, const std::filesystem::path &output) {
    std::vector<vellum::page_extent> dimensions;
    for (int page = 0; page < document.pages(); ++page) {
        dimensions.push_back(document.dimensions(page));
    }
    const auto selected = vellum::select_print_pages("1,,2", document.pages());
    const auto job = vellum::compose_print_job(dimensions, selected, {});
    wxPrintData settings;
    settings.SetPrintMode(wxPRINT_MODE_FILE);
    settings.SetFilename(wxString::FromUTF8(output.string()));
    settings.SetPaperId(wxPAPER_A4);
    settings.SetColour(true);
    wxPostScriptDC dc(settings);
    if (!dc.IsOk() || !dc.StartDoc("Vellum blank page test")) {
        throw std::runtime_error("blank-page virtual printer unavailable");
    }
    vellum::sheet_painter painter(document);
    const auto size = dc.GetSize();
    for (std::size_t side = 0; side < job.sides.size(); ++side) {
        dc.StartPage();
        painter.paint(dc, job, side, wxRect(0, 0, size.x, size.y), 72);
        dc.EndPage();
    }
    dc.EndDoc();
    const auto result = vellum::read_file(output);
    const auto first = result.find("%%Page: 1");
    const auto second = result.find("%%Page: 2", first);
    const auto third = result.find("%%Page: 3", second);
    if (first == std::string::npos || second == std::string::npos || third == std::string::npos ||
        result.find("%%Page: 4", third) != std::string::npos ||
        result.substr(first, second - first).find("colorimage") == std::string::npos ||
        result.substr(second, third - second).find("colorimage") != std::string::npos ||
        result.substr(third).find("colorimage") == std::string::npos) {
        throw std::runtime_error("virtual printer did not emit a blank middle sheet");
    }
}
#endif
int main(int argc, char **argv) {
#if wxUSE_POSTSCRIPT
    if (argc != 4) {
        return 2;
    }
    const std::filesystem::path fixture(argv[1]), output(argv[2]);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        vellum::pdf_document document(fixture);
        verify_painter_boundaries(document);
        verify_painter_permissions(argv[3]);
        vellum::require_print_permission(document, 72);
        std::vector<vellum::page_extent> dimensions;
        for (int page = 0; page < document.pages(); ++page) {
            dimensions.push_back(document.dimensions(page));
        }
        vellum::print_options options;
        options.layout = vellum::print_layout::multiple;
        options.copies = 2;
        const auto job = vellum::compose_print_job(dimensions, {1, 2, 1}, options);
        if (job.sides.size() != 4) {
            throw std::runtime_error("unexpected virtual printer page count");
        }
        vellum::printer_settings printer;
        if (printer.selected()) {
            throw std::runtime_error("new printer settings must not select a device");
        }
        const auto nominal = printer.measure(options);
        if (nominal.paper.width != options.paper.width ||
            nominal.printable.width != options.printable.width) {
            throw std::runtime_error("unselected printer changed nominal geometry");
        }
        for (int format = 0; format <= 4; ++format) {
            printer.set_paper(format, false);
            printer.set_paper(format, true);
        }
        for (const int format : {-1, 5}) {
            bool rejected = false;
            try {
                printer.set_paper(format, false);
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            if (!rejected) {
                throw std::runtime_error("invalid paper format accepted");
            }
        }
        printer.set_job_defaults(options);
        for (const auto &[setting, native] :
             {std::pair{vellum::print_duplex::simplex, wxDUPLEX_SIMPLEX},
              std::pair{vellum::print_duplex::long_edge, wxDUPLEX_VERTICAL},
              std::pair{vellum::print_duplex::short_edge, wxDUPLEX_HORIZONTAL}}) {
            vellum::printer_settings candidate;
            auto defaults = options;
            defaults.duplex = setting;
            defaults.copies = 4;
            defaults.collate = false;
            defaults.grayscale = true;
            candidate.set_job_defaults(defaults);
            inspect_print_settings hook;
            if (candidate.select_printer(nullptr) || !hook.seen || candidate.selected() ||
                hook.copies != 4 || hook.collate || hook.colour || hook.duplex != native) {
                throw std::runtime_error(
                    "printer defaults did not reach the cancelled native dialog");
            }
        }
        bool rejected = false;
        try {
            printer.submit(nullptr, document, job, 72);
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        if (!rejected || printer.selected()) {
            throw std::runtime_error("submission without a selected printer was accepted");
        }
        {
            accept_print_settings hook;
            if (!printer.select_printer(nullptr) || !hook.observed() || !printer.selected()) {
                throw std::runtime_error("accepted printer settings were not committed");
            }
            const auto accepted = printer.preferences();
            if (!accepted.landscape || accepted.duplex != vellum::print_duplex::short_edge ||
                accepted.copies != 3 || accepted.collate || !accepted.grayscale) {
                throw std::runtime_error("accepted printer properties did not round trip");
            }
        }
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
        for (const auto fault :
             {native_printer_fault::dialog_copy, native_printer_fault::dialog_construct,
              native_printer_fault::dialog_assign}) {
            {
                accept_print_settings baseline;
                if (!printer.select_printer(nullptr)) {
                    throw std::runtime_error("could not establish previous printer selection");
                }
            }
            const auto before = printer.preferences();
            accept_print_settings hook(1, wxDUPLEX_VERTICAL);
            native_printer_faults = 0;
            injected_native_printer_fault = fault;
            bool failed = false;
            try {
                printer.select_printer(nullptr);
            } catch (const std::bad_alloc &) {
                failed = true;
            }
            injected_native_printer_fault = native_printer_fault::none;
            const auto after = printer.preferences();
            if (!failed || native_printer_faults != 1 || !printer.selected() ||
                after.landscape != before.landscape || after.duplex != before.duplex ||
                after.copies != before.copies || after.collate != before.collate ||
                after.grayscale != before.grayscale) {
                throw std::runtime_error("native printer failure changed the previous selection");
            }
            if (!printer.select_printer(nullptr) || printer.preferences().copies != 1 ||
                printer.preferences().duplex != vellum::print_duplex::long_edge) {
                throw std::runtime_error("printer selection did not recover after native failure");
            }
        }
#endif
        for (const auto &[native_duplex, expected] :
             {std::pair{wxDUPLEX_SIMPLEX, vellum::print_duplex::simplex},
              std::pair{wxDUPLEX_VERTICAL, vellum::print_duplex::long_edge},
              std::pair{wxDUPLEX_HORIZONTAL, vellum::print_duplex::short_edge}}) {
            vellum::printer_settings selected;
            accept_print_settings hook(1, native_duplex);
            if (!selected.select_printer(nullptr) || !hook.observed() ||
                selected.preferences().duplex != expected) {
                throw std::runtime_error("native duplex setting was translated incorrectly");
            }
        }
        {
            auto empty_job = job;
            empty_job.sides.clear();
            bool empty_rejected = false;
            try {
                printer.submit(nullptr, document, empty_job, 72);
            } catch (const std::runtime_error &) {
                empty_rejected = true;
            }
            if (!empty_rejected || !printer.selected()) {
                throw std::runtime_error("empty job was accepted or changed the selected printer");
            }
        }
        verify_submission(printer, document);
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
        verify_submission_allocations(printer, document);
#endif
        verify_measurement(printer, document);
        verify_dialog_submissions(printer, document);
        for (const int copies : {0, 101}) {
            vellum::printer_settings invalid;
            accept_print_settings hook(copies);
            bool invalid_rejected = false;
            try {
                invalid.select_printer(nullptr);
            } catch (const std::runtime_error &) {
                invalid_rejected = true;
            }
            if (!hook.observed() || !invalid_rejected || invalid.selected()) {
                throw std::runtime_error("invalid native copy count was accepted");
            }
        }
        {
            const auto before = printer.preferences();
            accept_print_settings hook(0);
            bool invalid_rejected = false;
            try {
                printer.select_printer(nullptr);
            } catch (const std::runtime_error &) {
                invalid_rejected = true;
            }
            const auto after = printer.preferences();
            if (!invalid_rejected || !printer.selected() || after.landscape != before.landscape ||
                after.duplex != before.duplex || after.copies != before.copies ||
                after.collate != before.collate || after.grayscale != before.grayscale) {
                throw std::runtime_error(
                    "invalid printer properties changed the previous selection");
            }
        }
        {
            const auto before = printer.preferences();
            fail_print_settings hook;
            bool failed = false;
            try {
                printer.select_printer(nullptr);
            } catch (const std::runtime_error &error) {
                failed = std::string(error.what()) == "injected printer dialog failure";
            }
            const auto after = printer.preferences();
            if (!hook.seen || !failed || !printer.selected() ||
                after.landscape != before.landscape || after.duplex != before.duplex ||
                after.copies != before.copies || after.collate != before.collate ||
                after.grayscale != before.grayscale) {
                throw std::runtime_error("printer dialog failure changed the previous selection");
            }
        }
        wxPrintData settings;
        settings.SetPrintMode(wxPRINT_MODE_FILE);
        settings.SetFilename(wxString::FromUTF8(output.string()));
        settings.SetPaperId(wxPAPER_A4);
        settings.SetColour(true);
        settings.SetNoCopies(1);
        wxPostScriptDC dc(settings);
        if (!dc.IsOk() || !dc.StartDoc("Vellum virtual print test")) {
            throw std::runtime_error("virtual print device unavailable");
        }
        vellum::sheet_painter painter(document);
        const auto size = dc.GetSize();
        for (std::size_t side = 0; side < job.sides.size(); ++side) {
            dc.StartPage();
            painter.paint(dc, job, side, wxRect(0, 0, size.x, size.y), 72);
            dc.EndPage();
        }
        dc.EndDoc();
        const auto postscript = vellum::read_file(output);
        std::size_t count = 0, position = 0;
        while ((position = postscript.find("%%Page:", position)) != std::string::npos) {
            ++count;
            position += 7;
        }
        if (!postscript.starts_with("%!PS") || count != 4 || postscript.size() < 10000 ||
            postscript.size() > 20000000 ||
            (postscript.find("ff0000") == std::string::npos &&
             postscript.find("FF0000") == std::string::npos)) {
            throw std::runtime_error(
                "virtual print output missing pages or rendered red image data");
        }
        verify_blank_output(document, output.parent_path() / "printed-blank.ps");
    } catch (const std::exception &error) {
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
        track_submission_allocations = false;
#endif
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
