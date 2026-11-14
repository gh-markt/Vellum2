#include "vellum/main_frame.hh"
#include <cmath>
#include <cstdarg>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <wx/app.h>
#include <wx/modalhook.h>
#include <wx/msgdlg.h>
#include <wx/statbmp.h>
#include <wx/statusbr.h>

namespace {
std::string_view failure;
bool injected = false;
bool status_failure = false;
int page_value_failures = 0;
int status_page_count_failures = 0;
int status_format_failures = 0;
bool fail_runtime_error_construction = false;
int operation_count = 0;
wxScrolledWindow *target_canvas = nullptr;
} // namespace

extern "C" void __real__ZNSt13runtime_errorC1EPKc(std::runtime_error *, const char *);
extern "C" void __wrap__ZNSt13runtime_errorC1EPKc(std::runtime_error *error, const char *text) {
    if (fail_runtime_error_construction) {
        fail_runtime_error_construction = false;
        throw std::bad_alloc();
    }
    __real__ZNSt13runtime_errorC1EPKc(error, text);
}
extern "C" wxString __wrap__ZN8wxString13DoFormatWcharEPKwz(const wchar_t *format, ...) {
    if (status_format_failures > 0) {
        --status_format_failures;
        injected = true;
        throw std::runtime_error("injected page status format failure");
    }
    va_list arguments;
    va_start(arguments, format);
    try {
        auto result = wxString::FormatV(wxString(format), arguments);
        va_end(arguments);
        return result;
    } catch (...) {
        va_end(arguments);
        throw;
    }
}
extern "C" int __real__ZNK6vellum12pdf_document5pagesEv(const void *);
extern "C" int __wrap__ZNK6vellum12pdf_document5pagesEv(const void *document) {
    if (status_page_count_failures > 0) {
        --status_page_count_failures;
        injected = true;
        throw std::runtime_error("injected page status count failure");
    }
    return __real__ZNK6vellum12pdf_document5pagesEv(document);
}
extern "C" void __real__ZN17wxSpinCtrlGTKBase10DoSetValueEd(void *, double);
extern "C" void __wrap__ZN17wxSpinCtrlGTKBase10DoSetValueEd(void *control, double value) {
    if (page_value_failures > 0) {
        --page_value_failures;
        injected = true;
        throw std::runtime_error("injected page status failure");
    }
    __real__ZN17wxSpinCtrlGTKBase10DoSetValueEd(control, value);
}
extern "C" void __real__ZN8wxStringC1EPKc(void *, const char *);
extern "C" void __wrap__ZN8wxStringC1EPKc(void *value, const char *text) {
    if (failure.starts_with("literal-") && operation_count++ == failure.back() - '0') {
        injected = true;
        failure = {};
        throw std::runtime_error("injected zoom literal failure");
    }
    __real__ZN8wxStringC1EPKc(value, text);
}
extern "C" wxString &__real__ZN8wxString4TrimEb(wxString *, bool);
extern "C" wxString &__wrap__ZN8wxString4TrimEb(wxString *text, bool from_right) {
    if (failure.starts_with("trim-") && operation_count++ == (failure == "trim-first" ? 0 : 1)) {
        injected = true;
        failure = {};
        throw std::runtime_error("injected zoom trimming failure");
    }
    return __real__ZN8wxString4TrimEb(text, from_right);
}
extern "C" int __real__ZNK8wxString9CmpNoCaseERKS_(const wxString *, const wxString &);
extern "C" int __wrap__ZNK8wxString9CmpNoCaseERKS_(const wxString *text, const wxString &other) {
    if (failure.starts_with("compare-") &&
        operation_count++ == (failure == "compare-first"    ? 0
                              : failure == "compare-second" ? 1
                                                            : 2)) {
        injected = true;
        failure = {};
        throw std::runtime_error("injected zoom comparison failure");
    }
    return __real__ZNK8wxString9CmpNoCaseERKS_(text, other);
}
extern "C" bool __real__ZNK8wxString8EndsWithERKS_PS_(const wxString *, const wxString &,
                                                      wxString *);
extern "C" bool __wrap__ZNK8wxString8EndsWithERKS_PS_(const wxString *text, const wxString &suffix,
                                                      wxString *rest) {
    if (failure == "suffix") {
        injected = true;
        failure = {};
        throw std::runtime_error("injected zoom suffix failure");
    }
    return __real__ZNK8wxString8EndsWithERKS_PS_(text, suffix, rest);
}
extern "C" bool __real__ZNK8wxString8ToDoubleEPd(const wxString *, double *);
extern "C" bool __wrap__ZNK8wxString8ToDoubleEPd(const wxString *text, double *value) {
    if (failure == "number") {
        injected = true;
        failure = {};
        throw std::runtime_error("injected zoom number failure");
    }
    return __real__ZNK8wxString8ToDoubleEPd(text, value);
}

extern "C" vellum::page_extent
__real__ZNK6vellum12pdf_document10dimensionsEi(const vellum::pdf_document *, int);
extern "C" vellum::page_extent
__wrap__ZNK6vellum12pdf_document10dimensionsEi(const vellum::pdf_document *document, int page) {
    if (failure.starts_with("status-format-")) {
        status_format_failures = failure == "status-format-double" ? 2 : 1;
        failure = {};
    } else if (failure.starts_with("status-count-")) {
        status_page_count_failures = failure == "status-count-double" ? 2 : 1;
        failure = {};
    }
    if (failure == "dimensions" || failure == "double") {
        injected = true;
        status_failure = failure == "double";
        failure = {};
        throw std::runtime_error("injected dimension failure");
    }
    if (failure == "resize-during-layout") {
        failure = {};
        injected = true;
        target_canvas->SetClientSize(wxSize(16, 16));
    } else if (failure == "outside-horizontal") {
        failure = "outside-horizontal-armed";
    } else if (failure == "outside-vertical") {
        failure = "outside-vertical-armed";
    }
    if (failure == "oversize" || failure == "oversize-error-allocation") {
        fail_runtime_error_construction = failure == "oversize-error-allocation";
        injected = true;
        failure = {};
        return {200000000.0, 200.0};
    }
    if (failure == "oversize-height") {
        injected = true;
        failure = {};
        return {200.0, 200000000.0};
    }
    return __real__ZNK6vellum12pdf_document10dimensionsEi(document, page);
}

extern "C" vellum::page_image
__real__ZNK6vellum12pdf_document13render_regionEiNS_14page_rectangleEf(const vellum::pdf_document *,
                                                                       int, vellum::page_rectangle,
                                                                       float);
extern "C" vellum::page_image
__wrap__ZNK6vellum12pdf_document13render_regionEiNS_14page_rectangleEf(
    const vellum::pdf_document *document, int page, vellum::page_rectangle area, float scale) {
    if (failure == "render") {
        injected = true;
        failure = {};
        throw std::runtime_error("injected render failure");
    }
    return __real__ZNK6vellum12pdf_document13render_regionEiNS_14page_rectangleEf(document, page,
                                                                                  area, scale);
}

extern "C" void __real__ZNK18wxScrollHelperBase24DoCalcUnscrolledPositionEiiPiS0_(const void *, int,
                                                                                  int, int *,
                                                                                  int *);
extern "C" void
__wrap__ZNK18wxScrollHelperBase24DoCalcUnscrolledPositionEiiPiS0_(const void *scroll, int x, int y,
                                                                  int *result_x, int *result_y) {
    __real__ZNK18wxScrollHelperBase24DoCalcUnscrolledPositionEiiPiS0_(scroll, x, y, result_x,
                                                                      result_y);
    if (failure == "outside-horizontal-armed") {
        *result_x = 10000000;
        injected = true;
        failure = {};
    } else if (failure == "outside-vertical-armed") {
        *result_y = 10000000;
        injected = true;
        failure = {};
    }
}

extern "C" bool __real__ZN7wxImage6CreateEiib(void *, int, int, bool);
extern "C" bool __wrap__ZN7wxImage6CreateEiib(void *image, int width, int height, bool clear) {
    if (failure == "image" || failure == "image-error-allocation") {
        fail_runtime_error_construction = failure == "image-error-allocation";
        injected = true;
        failure = {};
        return false;
    }
    return __real__ZN7wxImage6CreateEiib(image, width, height, clear);
}

extern "C" void __real__ZN8wxBitmapC1ERK7wxImageid(void *, const wxImage &, int, double);
extern "C" void __wrap__ZN8wxBitmapC1ERK7wxImageid(void *bitmap, const wxImage &image, int depth,
                                                   double scale) {
    if (failure == "bitmap") {
        injected = true;
        failure = {};
        throw std::runtime_error("injected bitmap construction failure");
    }
    __real__ZN8wxBitmapC1ERK7wxImageid(bitmap, image, depth, scale);
}
extern "C" void __real__ZN14wxBitmapBundleC1ERK8wxBitmap(void *, const wxBitmap &);
extern "C" void __wrap__ZN14wxBitmapBundleC1ERK8wxBitmap(void *bundle, const wxBitmap &bitmap) {
    if (failure == "bitmap-bundle") {
        injected = true;
        failure = {};
        throw std::runtime_error("injected bitmap bundle failure");
    }
    __real__ZN14wxBitmapBundleC1ERK8wxBitmap(bundle, bitmap);
}
extern "C" void __real__ZN14wxScrollHelper16DoShowScrollbarsE21wxScrollbarVisibilityS0_(
    void *, wxScrollbarVisibility, wxScrollbarVisibility);
extern "C" void __wrap__ZN14wxScrollHelper16DoShowScrollbarsE21wxScrollbarVisibilityS0_(
    void *helper, wxScrollbarVisibility horizontal, wxScrollbarVisibility vertical) {
    if (failure == "scrollbars") {
        injected = true;
        failure = {};
        throw std::runtime_error("injected scrollbar configuration failure");
    }
    __real__ZN14wxScrollHelper16DoShowScrollbarsE21wxScrollbarVisibilityS0_(helper, horizontal,
                                                                            vertical);
}
extern "C" void __real__ZNK18wxScrollHelperBase22DoCalcScrolledPositionEiiPiS0_(const void *, int,
                                                                                int, int *, int *);
extern "C" void __wrap__ZNK18wxScrollHelperBase22DoCalcScrolledPositionEiiPiS0_(const void *scroll,
                                                                                int x, int y,
                                                                                int *result_x,
                                                                                int *result_y) {
    if (failure == "scrolled-position") {
        injected = true;
        failure = {};
        throw std::runtime_error("injected scrolled position failure");
    }
    __real__ZNK18wxScrollHelperBase22DoCalcScrolledPositionEiiPiS0_(scroll, x, y, result_x,
                                                                    result_y);
}

extern "C" void __real__ZN11wxFrameBase13SetStatusTextERK8wxStringi(void *, const wxString &, int);
extern "C" void
__wrap__ZN11wxFrameBase13SetStatusTextERK8wxStringi(void *frame, const wxString &text, int field) {
    if (status_failure || failure == "status") {
        status_failure = false;
        injected = true;
        failure = {};
        throw std::runtime_error("injected status failure");
    }
    __real__ZN11wxFrameBase13SetStatusTextERK8wxStringi(frame, text, field);
}

extern "C" bool
__real__ZN6vellum19autosave_controller4tickENSt6chrono10time_pointINS1_3_V212steady_clockENS1_8durationIlSt5ratioILl1ELl1000000000EEEEEE(
    vellum::autosave_controller *, vellum::autosave_controller::clock::time_point);
extern "C" bool
__wrap__ZN6vellum19autosave_controller4tickENSt6chrono10time_pointINS1_3_V212steady_clockENS1_8durationIlSt5ratioILl1ELl1000000000EEEEEE(
    vellum::autosave_controller *controller, vellum::autosave_controller::clock::time_point now) {
    if (failure == "timer-error" || failure == "timer-status-error") {
        injected = true;
        status_failure = failure == "timer-status-error";
        failure = {};
        throw std::runtime_error("injected autosave failure");
    }
    if (failure == "timer-ok") {
        injected = true;
        failure = {};
    }
    return __real__ZN6vellum19autosave_controller4tickENSt6chrono10time_pointINS1_3_V212steady_clockENS1_8durationIlSt5ratioILl1ELl1000000000EEEEEE(
        controller, now);
}

class viewer_fault_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        this->SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(viewer_fault_app);

class error_hook : public wxModalDialogHook {
  public:
    error_hook() { this->Register(); }
    int errors = 0;
    bool unexpected = false;
    wxString message;

  protected:
    int Enter(wxDialog *dialog) override {
        if (dialog->GetTitle() == "Vellum") {
            ++this->errors;
            if (auto *message_dialog = dynamic_cast<wxMessageDialog *>(dialog)) {
                this->message = message_dialog->GetMessage();
            }
            return wxID_OK;
        }
        this->unexpected = true;
        return wxID_CANCEL;
    }
};

void test_timer(vellum::main_frame &frame, std::string_view mode) {
    error_hook errors;
    const int editor =
        frame.GetMenuBar()->GetMenu(0)->FindItem("Enable editor mode for this document");
    if (editor == wxNOT_FOUND) {
        throw std::runtime_error("editor command missing");
    }
    wxCommandEvent event(wxEVT_MENU, editor);
    frame.GetEventHandler()->ProcessEvent(event);
    if (!frame.GetStatusBar()->GetStatusText().Contains("Editor")) {
        throw std::runtime_error("could not enable editor");
    }
    failure = mode;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!injected && std::chrono::steady_clock::now() < deadline) {
        wxYield();
        wxMilliSleep(10);
    }
    const auto status = frame.GetStatusBar()->GetStatusText();
    const bool expected = mode == "timer-error"
                              ? status.Contains("Autosave failed: injected autosave failure")
                              : status.Contains("Editor");
    const int expected_errors = mode == "timer-status-error" ? 1 : 0;
    if (!injected || status_failure || !expected || errors.errors != expected_errors ||
        errors.unexpected || frame.current_page() != 1 ||
        (expected_errors && !errors.message.Contains("injected autosave failure"))) {
        throw std::runtime_error(
            "autosave timer did not preserve the session and report its outcome");
    }
}

int main(int argc, char **argv) {
    if (argc != 3 || !wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        vellum::main_frame frame;
        frame.Show();
        frame.open_path(argv[1]);
        wxYield();
        frame.set_zoom(vellum::zoom_mode::percentage, 100);
        auto *bitmap = dynamic_cast<wxStaticBitmap *>(frame.FindWindowByName("viewer-bitmap"));
        if (!bitmap || !bitmap->IsShown()) {
            throw std::runtime_error("initial page was not rendered");
        }
        const auto title = frame.GetTitle();
        target_canvas = dynamic_cast<wxScrolledWindow *>(frame.FindWindowByName("viewer-canvas"));
        if (!target_canvas) {
            throw std::runtime_error("viewer canvas missing");
        }
        const auto original_canvas_size = target_canvas->GetSize();
        if (std::string_view(argv[2]) == "zoom-input-failure") {
            error_hook errors;
            auto *control = dynamic_cast<wxComboBox *>(frame.FindWindowByName("viewer-zoom"));
            if (!control) {
                throw std::runtime_error("zoom control missing");
            }
            const auto original_policy = frame.zoom_policy();
            const auto original_scale = frame.zoom_scale();
            for (const auto *point :
                 {"trim-first", "trim-second", "compare-first", "compare-second", "compare-third",
                  "suffix", "number", "literal-0", "literal-1", "literal-2", "literal-3"}) {
                control->SetValue("125%");
                const int previous_errors = errors.errors;
                failure = point;
                operation_count = 0;
                injected = false;
                wxCommandEvent event(wxEVT_TEXT_ENTER, control->GetId());
                frame.GetEventHandler()->ProcessEvent(event);
                if (!injected || errors.errors != previous_errors + 1 || errors.unexpected ||
                    frame.zoom_policy() != original_policy ||
                    frame.zoom_scale() != original_scale || frame.current_page() != 1 ||
                    frame.GetTitle() != title || !bitmap->IsShown()) {
                    throw std::runtime_error("zoom input failure changed the active view");
                }
            }
            for (const auto *text : {"Fit page", "Fit width", "Fit height", "125%"}) {
                control->SetValue(text);
                const int previous_errors = errors.errors;
                failure = "dimensions";
                injected = false;
                wxCommandEvent event(wxEVT_TEXT_ENTER, control->GetId());
                frame.GetEventHandler()->ProcessEvent(event);
                if (!injected || errors.errors != previous_errors + 1 || errors.unexpected ||
                    frame.zoom_policy() != original_policy ||
                    frame.zoom_scale() != original_scale || frame.current_page() != 1 ||
                    frame.GetTitle() != title || !bitmap->IsShown()) {
                    throw std::runtime_error(
                        "zoom input dimension failure changed the active view");
                }
            }
        } else if (std::string_view(argv[2]) == "menu-zoom-failure") {
            error_hook errors;
            const auto original_policy = frame.zoom_policy();
            const auto original_scale = frame.zoom_scale();
            for (const auto *label :
                 {"Fit page", "Fit width", "Fit height", "100%", "Zoom in", "Zoom out"}) {
                const int id = frame.GetMenuBar()->GetMenu(2)->FindItem(label);
                if (id == wxNOT_FOUND) {
                    throw std::runtime_error("zoom menu item missing");
                }
                const int previous_errors = errors.errors;
                failure = "dimensions";
                injected = false;
                wxCommandEvent event(wxEVT_MENU, id);
                frame.GetEventHandler()->ProcessEvent(event);
                if (!injected || errors.errors != previous_errors + 1 || errors.unexpected ||
                    frame.zoom_policy() != original_policy ||
                    frame.zoom_scale() != original_scale || frame.current_page() != 1 ||
                    frame.GetTitle() != title || !bitmap->IsShown()) {
                    throw std::runtime_error("zoom menu failure changed the active view");
                }
            }
        } else if (std::string_view(argv[2]).starts_with("timer-")) {
            test_timer(frame, argv[2]);
        } else {
            error_hook errors;
            const bool outside = std::string_view(argv[2]).starts_with("outside-");
            const bool resize = std::string_view(argv[2]) == "resize-during-layout";
            const bool page_status = std::string_view(argv[2]).starts_with("page-status-");
            if (page_status) {
                page_value_failures = std::string_view(argv[2]) == "page-status-double" ? 2 : 1;
            }
            failure = argv[2];
            frame.go_to_page(1);
            failure = {};
            if (!injected || status_failure || fail_runtime_error_construction ||
                page_value_failures != 0 || status_page_count_failures != 0 ||
                status_format_failures != 0 || errors.errors != (outside || resize ? 0 : 1) ||
                errors.unexpected || (!resize && bitmap->IsShown()) || frame.current_page() != 1 ||
                frame.GetTitle() != title) {
                std::cerr << "injected=" << injected << " status_failure=" << status_failure
                          << " errors=" << errors.errors << " unexpected=" << errors.unexpected
                          << " bitmap=" << bitmap->IsShown() << " page=" << frame.current_page()
                          << " title_matches=" << (frame.GetTitle() == title) << '\n';
                throw std::runtime_error(
                    "render failure did not preserve session and hide stale image");
            }
            if (resize) {
                target_canvas->SetSize(original_canvas_size);
            }
            frame.go_to_page(1);
            if (!bitmap->IsShown() || frame.current_page() != 1 ||
                errors.errors != (outside || resize ? 0 : 1)) {
                throw std::runtime_error("viewer did not recover after render failure");
            }
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
