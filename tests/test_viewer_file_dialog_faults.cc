#include "vellum/main_frame.hh"
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <wx/app.h>
#include <wx/statusbr.h>
#include <wx/textdlg.h>

namespace {
enum class fault {
    none,
    construct,
    show,
    path,
    path_conversion,
    password_construct,
    password_show,
    password_conversion,
    permission_failure,
    owner_rejection,
    denial_display_failure,
    authorization_prefix_failure,
    authorization_suffix_failure,
    replacement_title_concat,
    replacement_page_count
};
fault injected = fault::none;
int failures = 0, messages = 0, cancellations = 0;
wxString accepted_path;
bool opening_protected = false, password_dialog_accepted = false;
bool fail_title = false;
bool fail_title_rollback = false;
int title_failures = 0;
bool fail_page_range = false;
bool fail_range_rollback = false;
int page_range_failures = 0;
int fail_page_query = 0;
bool arm_page_value_on_title_failure = false;
bool fail_rollback_value = false;
bool owner_rejected = false;
bool fail_error_display = false;
int error_display_failures = 0;
bool watching_fallback = false, fail_fallback = false;
int fallback_calls = 0;
bool fallback_matches = false;
} // namespace
extern "C" int __real__ZNK6vellum12pdf_document5pagesEv(const void *);
extern "C" int __wrap__ZNK6vellum12pdf_document5pagesEv(const void *document) {
    if (injected == fault::replacement_page_count) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected replacement page count failure");
    }
    return __real__ZNK6vellum12pdf_document5pagesEv(document);
}
extern "C" wxString __real__ZplRK8wxStringS1_(const wxString &, const wxString &);
extern "C" wxString __wrap__ZplRK8wxStringS1_(const wxString &left, const wxString &right) {
    if (injected == fault::replacement_title_concat) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected replacement title concatenation failure");
    }
    return __real__ZplRK8wxStringS1_(left, right);
}
extern "C" double __real__ZNK17wxSpinCtrlGTKBase8DoGetMinEv(const void *);
extern "C" double __real__ZNK17wxSpinCtrlGTKBase8DoGetMaxEv(const void *);
extern "C" double __real__ZNK17wxSpinCtrlGTKBase10DoGetValueEv(const void *);
extern "C" void __real__ZN17wxSpinCtrlGTKBase10DoSetValueEd(void *, double);
extern "C" void __wrap__ZN17wxSpinCtrlGTKBase10DoSetValueEd(void *control, double value) {
    __real__ZN17wxSpinCtrlGTKBase10DoSetValueEd(control, value);
    if (fail_rollback_value) {
        fail_rollback_value = false;
        throw std::runtime_error("injected rollback page-value failure");
    }
}
extern "C" double __wrap__ZNK17wxSpinCtrlGTKBase8DoGetMinEv(const void *control) {
    if (fail_page_query == 1) {
        fail_page_query = 0;
        throw std::runtime_error("injected page minimum failure");
    }
    return __real__ZNK17wxSpinCtrlGTKBase8DoGetMinEv(control);
}
extern "C" double __wrap__ZNK17wxSpinCtrlGTKBase8DoGetMaxEv(const void *control) {
    if (fail_page_query == 2) {
        fail_page_query = 0;
        throw std::runtime_error("injected page maximum failure");
    }
    return __real__ZNK17wxSpinCtrlGTKBase8DoGetMaxEv(control);
}
extern "C" double __wrap__ZNK17wxSpinCtrlGTKBase10DoGetValueEv(const void *control) {
    if (fail_page_query == 3) {
        fail_page_query = 0;
        throw std::runtime_error("injected page value failure");
    }
    return __real__ZNK17wxSpinCtrlGTKBase10DoGetValueEv(control);
}
extern "C" wxString __real__ZplPKcRK8wxString(const char *, const wxString &);
extern "C" wxString __wrap__ZplPKcRK8wxString(const char *prefix, const wxString &text) {
    if (injected == fault::authorization_prefix_failure) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected authorization message failure");
    }
    return __real__ZplPKcRK8wxString(prefix, text);
}
extern "C" wxString __real__ZplRK8wxStringPKc(const wxString &, const char *);
extern "C" wxString __wrap__ZplRK8wxStringPKc(const wxString &text, const char *suffix) {
    if (injected == fault::authorization_suffix_failure) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected authorization message failure");
    }
    return __real__ZplRK8wxStringPKc(text, suffix);
}
extern "C" int __wrap_fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    if (watching_fallback) {
        ++fallback_calls;
        char text[512];
        const int result = std::vsnprintf(text, sizeof(text), format, args);
        va_end(args);
        fallback_matches = stream == stderr &&
                           std::string(text).find("injected file chooser") != std::string::npos;
        return fail_fallback ? -1 : result;
    }
    const int result = std::vfprintf(stream, format, args);
    va_end(args);
    return result;
}
extern "C" bool __real__ZNK6vellum12pdf_document8may_editEv(const void *);
extern "C" bool __real__ZNK6vellum12pdf_document9may_printEv(const void *);
extern "C" bool __real__ZNK6vellum12pdf_document8may_saveEv(const void *);
extern "C" bool __real__ZNK6vellum12pdf_document11may_save_asEv(const void *);
void check_permission_failure() {
    if (injected == fault::permission_failure) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected permission inspection failure");
    }
}
extern "C" bool __wrap__ZNK6vellum12pdf_document8may_editEv(const void *document) {
    check_permission_failure();
    return __real__ZNK6vellum12pdf_document8may_editEv(document);
}
extern "C" bool __wrap__ZNK6vellum12pdf_document9may_printEv(const void *document) {
    check_permission_failure();
    return __real__ZNK6vellum12pdf_document9may_printEv(document);
}
extern "C" bool __wrap__ZNK6vellum12pdf_document8may_saveEv(const void *document) {
    check_permission_failure();
    return __real__ZNK6vellum12pdf_document8may_saveEv(document);
}
extern "C" bool __wrap__ZNK6vellum12pdf_document11may_save_asEv(const void *document) {
    check_permission_failure();
    return __real__ZNK6vellum12pdf_document11may_save_asEv(document);
}
extern "C" bool
__real__ZN6vellum12pdf_document18authenticate_ownerESt17basic_string_viewIcSt11char_traitsIcEE(
    void *, std::string_view);
extern "C" bool
__wrap__ZN6vellum12pdf_document18authenticate_ownerESt17basic_string_viewIcSt11char_traitsIcEE(
    void *document, std::string_view password) {
    if (injected == fault::owner_rejection) {
        injected = fault::none;
        ++failures;
        owner_rejected = true;
        return false;
    }
    return __real__ZN6vellum12pdf_document18authenticate_ownerESt17basic_string_viewIcSt11char_traitsIcEE(
        document, password);
}
extern "C" void __real__ZN19wxTopLevelWindowGTK8SetTitleERK8wxString(void *, const wxString &);
extern "C" void __wrap__ZN19wxTopLevelWindowGTK8SetTitleERK8wxString(void *window,
                                                                     const wxString &title) {
    __real__ZN19wxTopLevelWindowGTK8SetTitleERK8wxString(window, title);
    if (fail_title || fail_title_rollback) {
        if (fail_title && arm_page_value_on_title_failure) {
            fail_rollback_value = true;
        }
        if (fail_title)
            fail_title = false;
        else
            fail_title_rollback = false;
        ++title_failures;
        throw std::runtime_error("injected window title failure");
    }
}
extern "C" void __real__ZN17wxSpinCtrlGTKBase10DoSetRangeEdd(void *, double, double);
extern "C" void __wrap__ZN17wxSpinCtrlGTKBase10DoSetRangeEdd(void *control, double minimum,
                                                             double maximum) {
    __real__ZN17wxSpinCtrlGTKBase10DoSetRangeEdd(control, minimum, maximum);
    if (fail_page_range || fail_range_rollback) {
        if (fail_page_range)
            fail_page_range = false;
        else
            fail_range_rollback = false;
        ++page_range_failures;
        throw std::runtime_error("injected page range failure");
    }
}
extern "C" void __real__ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_(
    void *, wxWindow *, const wxString &, const wxString &, const wxString &, const wxString &,
    long, const wxPoint &, const wxSize &, const wxString &);
extern "C" void __wrap__ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_(
    void *object, wxWindow *parent, const wxString &message, const wxString &directory,
    const wxString &filename, const wxString &wildcard, long style, const wxPoint &position,
    const wxSize &size, const wxString &name) {
    if (injected == fault::construct) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected file chooser construction failure");
    }
    __real__ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_(
        object, parent, message, directory, filename, wildcard, style, position, size, name);
}
extern "C" int __wrap__ZN12wxFileDialog9ShowModalEv(void *) {
    if (injected == fault::show) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected file chooser display failure");
    }
    if (injected == fault::path || injected == fault::path_conversion || opening_protected)
        return wxID_OK;
    ++cancellations;
    return wxID_CANCEL;
}
extern "C" wxString __real__ZNK12wxFileDialog7GetPathEv(const void *);
extern "C" wxString __wrap__ZNK12wxFileDialog7GetPathEv(const void *dialog) {
    if (injected == fault::path) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected file chooser path failure");
    }
    if (injected == fault::path_conversion || opening_protected)
        return accepted_path;
    return __real__ZNK12wxFileDialog7GetPathEv(dialog);
}
extern "C" const char *__real__ZNK8wxString6AsCharERK8wxMBConv(const void *, const wxMBConv &);
extern "C" const char *__wrap__ZNK8wxString6AsCharERK8wxMBConv(const void *string,
                                                               const wxMBConv &conversion) {
    if (injected == fault::path_conversion || (injected == fault::password_conversion &&
                                               (!opening_protected || password_dialog_accepted))) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected value conversion failure");
    }
    return __real__ZNK8wxString6AsCharERK8wxMBConv(string, conversion);
}
extern "C" bool __real__ZN21wxPasswordEntryDialog6CreateEP8wxWindowRK8wxStringS4_S4_lRK7wxPoint(
    void *, wxWindow *, const wxString &, const wxString &, const wxString &, long,
    const wxPoint &);
extern "C" bool __wrap__ZN21wxPasswordEntryDialog6CreateEP8wxWindowRK8wxStringS4_S4_lRK7wxPoint(
    void *dialog, wxWindow *parent, const wxString &message, const wxString &caption,
    const wxString &value, long style, const wxPoint &position) {
    if (injected == fault::password_construct) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected password chooser construction failure");
    }
    return __real__ZN21wxPasswordEntryDialog6CreateEP8wxWindowRK8wxStringS4_S4_lRK7wxPoint(
        dialog, parent, message, caption, value, style, position);
}
extern "C" int __wrap__ZN8wxDialog9ShowModalEv(void *dialog) {
    if (injected == fault::password_show) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected password chooser display failure");
    }
    if (injected == fault::password_conversion || injected == fault::owner_rejection ||
        injected == fault::denial_display_failure) {
        static_cast<wxPasswordEntryDialog *>(static_cast<wxDialog *>(dialog))
            ->SetValue(injected == fault::denial_display_failure ? "wrong" : "test-owner");
        password_dialog_accepted = true;
        return wxID_OK;
    }
    ++cancellations;
    return wxID_CANCEL;
}
extern "C" int __wrap__Z12wxMessageBoxRK8wxStringS1_lP8wxWindowii(const wxString &message,
                                                                  const wxString &caption, long,
                                                                  wxWindow *, int, int) {
    if (caption == "Authorization failed" && injected == fault::denial_display_failure) {
        injected = fault::none;
        ++failures;
        throw std::runtime_error("injected authorization error display failure");
    }
    if (caption != "Vellum" ||
        (!message.Contains("injected ") &&
         !(owner_rejected && message.Contains("owner authentication failed"))))
        throw std::runtime_error("unexpected application error");
    if (fail_error_display) {
        fail_error_display = false;
        ++error_display_failures;
        throw std::runtime_error("injected error display failure");
    }
    ++messages;
    return wxID_OK;
}
class test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(test_app);
int main(int argc, char **argv) {
    if (argc != 4 || !wxEntryStart(argc, argv) || !wxTheApp->CallOnInit())
        return 2;
    int result = 0;
    try {
        vellum::main_frame frame;
        frame.Show();
        frame.open_path(argv[1]);
        wxYield();
        frame.go_to_page(2);
        accepted_path = wxString::FromUTF8(argv[1]);
        const auto title = frame.GetTitle();
        for (const auto *label : {"Open PDF...", "Recover snapshot...", "Save As..."}) {
            const int id = frame.GetMenuBar()->GetMenu(0)->FindItem(label);
            if (id == wxNOT_FOUND)
                throw std::runtime_error("file menu item missing");
            for (const auto mode :
                 {fault::construct, fault::show, fault::path, fault::path_conversion}) {
                failures = messages = cancellations = 0;
                injected = mode;
                wxCommandEvent command(wxEVT_MENU, id);
                bool escaped = false;
                try {
                    frame.GetEventHandler()->ProcessEvent(command);
                } catch (const std::exception &) {
                    escaped = true;
                }
                injected = fault::none;
                if (escaped || failures != 1 || messages != 1 || cancellations != 0 ||
                    frame.GetTitle() != title || frame.current_page() != 2)
                    throw std::runtime_error(
                        std::string(label) +
                        " chooser failure escaped or changed the active document");
                frame.GetEventHandler()->ProcessEvent(command);
                if (cancellations != 1 || frame.GetTitle() != title || frame.current_page() != 2)
                    throw std::runtime_error("cancelled chooser retry changed the active document");
            }
        }
        for (const bool fail_log : {false, true}) {
            const int id = frame.GetMenuBar()->GetMenu(0)->FindItem("Open PDF...");
            failures = messages = cancellations = error_display_failures = fallback_calls = 0;
            fallback_matches = false;
            injected = fault::construct;
            fail_error_display = true;
            watching_fallback = true;
            fail_fallback = fail_log;
            wxCommandEvent command(wxEVT_MENU, id);
            bool escaped = false;
            try {
                frame.GetEventHandler()->ProcessEvent(command);
            } catch (const std::exception &) {
                escaped = true;
            }
            fail_error_display = false;
            watching_fallback = false;
            fail_fallback = false;
            injected = fault::none;
            if (escaped || failures != 1 || error_display_failures != 1 || fallback_calls != 1 ||
                !fallback_matches || frame.GetTitle() != title || frame.current_page() != 2)
                throw std::runtime_error("error display failure escaped or changed the session");
            frame.GetEventHandler()->ProcessEvent(command);
            if (cancellations != 1 || frame.current_page() != 2)
                throw std::runtime_error("file chooser did not recover after display failure");
        }
        accepted_path = wxString::FromUTF8(argv[3]);
        for (const auto *label : {"Open PDF...", "Recover snapshot..."}) {
            const int id = frame.GetMenuBar()->GetMenu(0)->FindItem(label);
            for (const auto mode :
                 {fault::password_construct, fault::password_show, fault::password_conversion}) {
                failures = messages = cancellations = 0;
                opening_protected = true;
                password_dialog_accepted = false;
                injected = mode;
                wxCommandEvent command(wxEVT_MENU, id);
                frame.GetEventHandler()->ProcessEvent(command);
                opening_protected = false;
                injected = fault::none;
                if (failures != 1 || messages != 1 || frame.GetTitle() != title ||
                    frame.current_page() != 2)
                    throw std::runtime_error("protected open failure changed the prior session");
                frame.GetEventHandler()->ProcessEvent(command);
                if (cancellations != 1 || frame.current_page() != 2)
                    throw std::runtime_error("protected open failure did not recover");
            }
        }
        const int unlock = frame.GetMenuBar()->GetMenu(0)->FindItem("Unlock PDF permissions...");
        if (unlock == wxNOT_FOUND)
            throw std::runtime_error("unlock menu item missing");
        for (const auto mode :
             {fault::password_construct, fault::password_show, fault::password_conversion}) {
            failures = messages = cancellations = 0;
            injected = mode;
            wxCommandEvent command(wxEVT_MENU, unlock);
            bool escaped = false;
            try {
                frame.GetEventHandler()->ProcessEvent(command);
            } catch (const std::exception &) {
                escaped = true;
            }
            injected = fault::none;
            if (escaped || failures != 1 || messages != 1 || frame.GetTitle() != title ||
                frame.current_page() != 2)
                throw std::runtime_error(
                    "password chooser failure escaped or changed the document");
            frame.GetEventHandler()->ProcessEvent(command);
            if (cancellations != 1 || frame.GetTitle() != title || frame.current_page() != 2)
                throw std::runtime_error("cancelled password retry changed the active document");
        }
        for (int point = 1; point <= 3; ++point) {
            fail_page_query = point;
            bool rejected = false;
            try {
                frame.open_path(argv[2]);
            } catch (const std::runtime_error &error) {
                rejected = std::string(error.what()).find("injected page ") == 0;
            }
            fail_page_query = 0;
            auto *control = dynamic_cast<wxSpinCtrl *>(frame.FindWindowByName("viewer-page"));
            if (!rejected || frame.GetTitle() != title || frame.current_page() != 2 || !control ||
                control->GetMin() != 1 || control->GetMax() != 2 || control->GetValue() != 2 ||
                !frame.GetStatusBar()->GetStatusText().Contains("Page 2 / 2")) {
                throw std::runtime_error("failed page-control query changed the previous session");
            }
        }
        for (const auto point : {fault::replacement_title_concat, fault::replacement_page_count}) {
            failures = 0;
            injected = point;
            bool rejected = false;
            try {
                frame.open_path(argv[2]);
            } catch (const std::runtime_error &) {
                rejected = true;
            }
            injected = fault::none;
            auto *control = dynamic_cast<wxSpinCtrl *>(frame.FindWindowByName("viewer-page"));
            if (!rejected || failures != 1 || frame.GetTitle() != title ||
                frame.current_page() != 2 || !control || control->GetMin() != 1 ||
                control->GetMax() != 2 || control->GetValue() != 2 ||
                !frame.GetStatusBar()->GetStatusText().Contains("Page 2 / 2")) {
                std::cerr << "point=" << static_cast<int>(point) << " rejected=" << rejected
                          << " injections=" << failures << " page=" << frame.current_page()
                          << " title=" << (frame.GetTitle() == title) << '\n';
                throw std::runtime_error("failed title preparation changed the previous session");
            }
        }
        fail_title = true;
        bool open_failed = false;
        try {
            frame.open_path(argv[2]);
        } catch (const std::runtime_error &) {
            open_failed = true;
        }
        fail_title = false;
        if (!open_failed || title_failures != 1 || frame.GetTitle() != title ||
            frame.current_page() != 2 ||
            !frame.GetStatusBar()->GetStatusText().Contains("Page 2 / 2"))
            throw std::runtime_error("failed replacement changed the previous document session");
        auto *page_control = dynamic_cast<wxSpinCtrl *>(frame.FindWindowByName("viewer-page"));
        if (!page_control || page_control->GetMin() != 1 || page_control->GetMax() != 2 ||
            page_control->GetValue() != 2)
            throw std::runtime_error("title failure changed the previous page control");
        fail_title = true;
        arm_page_value_on_title_failure = true;
        bool rollback_value_reported = false;
        try {
            frame.open_path(argv[2]);
        } catch (const std::runtime_error &error) {
            rollback_value_reported =
                std::string(error.what()) == "cannot restore previous document controls";
        }
        fail_title = arm_page_value_on_title_failure = fail_rollback_value = false;
        if (!rollback_value_reported || frame.GetTitle() != title || frame.current_page() != 2 ||
            page_control->GetMin() != 1 || page_control->GetMax() != 2 ||
            page_control->GetValue() != 2)
            throw std::runtime_error("rollback page-value failure changed the previous session");
        fail_page_range = true;
        open_failed = false;
        try {
            frame.open_path(argv[2]);
        } catch (const std::runtime_error &) {
            open_failed = true;
        }
        fail_page_range = false;
        if (!open_failed || page_range_failures != 1 || frame.GetTitle() != title ||
            frame.current_page() != 2 || page_control->GetMin() != 1 ||
            page_control->GetMax() != 2 || page_control->GetValue() != 2)
            throw std::runtime_error("page-range failure changed the previous document session");
        for (const bool title_case : {false, true}) {
            title_failures = page_range_failures = 0;
            fail_title = fail_title_rollback = title_case;
            fail_page_range = fail_range_rollback = !title_case;
            open_failed = false;
            try {
                frame.open_path(argv[2]);
            } catch (const std::runtime_error &error) {
                if (std::string(error.what()) == "cannot restore previous document controls") {
                    try {
                        std::rethrow_if_nested(error);
                    } catch (const std::runtime_error &cause) {
                        open_failed = std::string(cause.what()).find("injected ") == 0;
                    }
                }
            }
            fail_title = fail_title_rollback = fail_page_range = fail_range_rollback = false;
            if (!open_failed || (title_case ? title_failures : page_range_failures) != 2 ||
                frame.GetTitle() != title || frame.current_page() != 2 ||
                page_control->GetMin() != 1 || page_control->GetMax() != 2 ||
                page_control->GetValue() != 2)
                throw std::runtime_error("rollback failure changed the previous document session");
        }
        frame.open_path(argv[2]);
        const auto restricted_title = frame.GetTitle();
        for (const auto *label :
             {"Print...", "Enable editor mode for this document", "Save", "Save As..."}) {
            const int id = frame.GetMenuBar()->GetMenu(0)->FindItem(label);
            if (id == wxNOT_FOUND)
                throw std::runtime_error("permission menu item missing");
            failures = messages = cancellations = 0;
            injected = fault::permission_failure;
            wxCommandEvent command(wxEVT_MENU, id);
            frame.GetEventHandler()->ProcessEvent(command);
            if (failures != 1 || messages != 1 || cancellations != 0 ||
                frame.GetTitle() != restricted_title || frame.current_page() != 1 ||
                !frame.GetStatusBar()->GetStatusText().Contains("Reader"))
                throw std::runtime_error("permission failure did not stop the operation");
        }
        for (const auto *label : {"Print...", "Enable editor mode for this document"}) {
            const int id = frame.GetMenuBar()->GetMenu(0)->FindItem(label);
            if (id == wxNOT_FOUND)
                throw std::runtime_error("authorization menu item missing");
            for (const auto mode :
                 {fault::authorization_prefix_failure, fault::authorization_suffix_failure}) {
                failures = messages = cancellations = 0;
                injected = mode;
                wxCommandEvent command(wxEVT_MENU, id);
                frame.GetEventHandler()->ProcessEvent(command);
                if (failures != 1 || messages != 1 || cancellations != 0 ||
                    frame.GetTitle() != restricted_title || frame.current_page() != 1 ||
                    !frame.GetStatusBar()->GetStatusText().Contains("Reader"))
                    throw std::runtime_error("authorization message failure changed the session");
            }
            for (const auto mode :
                 {fault::password_construct, fault::password_show, fault::password_conversion,
                  fault::owner_rejection, fault::denial_display_failure}) {
                failures = messages = cancellations = 0;
                owner_rejected = false;
                injected = mode;
                wxCommandEvent command(wxEVT_MENU, id);
                bool escaped = false;
                try {
                    frame.GetEventHandler()->ProcessEvent(command);
                } catch (const std::exception &) {
                    escaped = true;
                }
                injected = fault::none;
                if (escaped || failures != 1 || messages != 1 ||
                    frame.GetTitle() != restricted_title || frame.current_page() != 1 ||
                    !frame.GetStatusBar()->GetStatusText().Contains("Reader"))
                    throw std::runtime_error("authorization dialog failure changed the session");
                frame.GetEventHandler()->ProcessEvent(command);
                if (cancellations != 1 || frame.GetTitle() != restricted_title ||
                    !frame.GetStatusBar()->GetStatusText().Contains("Reader"))
                    throw std::runtime_error("authorization retry changed the restricted session");
            }
        }
    } catch (const std::exception &error) {
        injected = fault::none;
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
