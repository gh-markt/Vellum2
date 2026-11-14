#include "vellum/print_dialog.hh"
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <execinfo.h>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <wx/app.h>

namespace {
thread_local bool armed = false;
thread_local bool injected = false;
thread_local std::size_t allocations = 0;
thread_local std::size_t fail_at = 0;
thread_local bool checking = false;
bool owned_allocation() {
    if (checking)
        return false;
    checking = true;
    void *frames[40];
    const int count = ::backtrace(frames, 40);
    bool owned = false;
    const char *scope = std::getenv("VELLUM_PRINT_ALLOC_SCOPE");
    const bool wx_string_scope = scope && std::strcmp(scope, "wx-string") == 0;
    for (int index = 2; index < count; ++index) {
        Dl_info info{};
        if (!::dladdr(frames[index], &info))
            continue;
        if (info.dli_fname && std::strstr(info.dli_fname, "libwx")) {
            if (wx_string_scope && info.dli_sname &&
                std::strncmp(info.dli_sname, "_ZN8wxString10ConvertStr", 24) == 0) {
                owned = true;
            }
            break;
        }
        if (!wx_string_scope && info.dli_sname &&
            std::strncmp(info.dli_sname, "_ZN6vellum", 10) == 0) {
            owned = true;
            break;
        }
    }
    checking = false;
    return owned;
}
void *allocate(std::size_t size) {
    if (armed && owned_allocation() && allocations++ == fail_at) {
        armed = false;
        injected = true;
        throw std::bad_alloc();
    }
    if (void *result = std::malloc(size ? size : 1)) {
        return result;
    }
    throw std::bad_alloc();
}
} // namespace
void *operator new(std::size_t size) {
    return allocate(size);
}
void *operator new[](std::size_t size) {
    return allocate(size);
}
void operator delete(void *value) noexcept {
    std::free(value);
}
void operator delete[](void *value) noexcept {
    std::free(value);
}
void operator delete(void *value, std::size_t) noexcept {
    std::free(value);
}
void operator delete[](void *value, std::size_t) noexcept {
    std::free(value);
}
class allocation_test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(allocation_test_app);
int main(int argc, char **argv) {
    if (argc != 3) {
        return 2;
    }
    const auto index = std::stoll(argv[2]);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        wxFrame parent(nullptr, wxID_ANY, "Vellum allocation test");
        vellum::pdf_document document(argv[1]);
        const auto original = document.snapshot();
        vellum::printer_settings printer;
        const char *scope = std::getenv("VELLUM_PRINT_ALLOC_SCOPE");
        const bool wx_string_scope = scope && std::strcmp(scope, "wx-string") == 0;
        fail_at =
            index < 0 ? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(index);
        allocations = 0;
        armed = true;
        try {
            vellum::print_dialog dialog(&parent, document, printer, 1);
            if (wx_string_scope) {
                auto *selection =
                    dynamic_cast<wxChoice *>(dialog.FindWindowByName("print-selection"));
                auto *ranges = dynamic_cast<wxTextCtrl *>(dialog.FindWindowByName("print-ranges"));
                if (!selection || !ranges) {
                    throw std::runtime_error("print selection controls are missing");
                }
                selection->SetSelection(2);
                ranges->ChangeValue("invalid");
                dialog.regenerate_preview();
            }
            armed = false;
            if (index >= 0 && !injected) {
                throw std::runtime_error("requested allocation was not reached");
            }
            if (!dialog.valid()) {
                auto *submit = wxWindow::FindWindowByName("print-submit", &dialog);
                if ((!injected && !wx_string_scope) || submit == nullptr || submit->IsEnabled()) {
                    throw std::runtime_error("invalid print dialog did not disable submission");
                }
            }
        } catch (const std::bad_alloc &) {
            armed = false;
            if (!injected) {
                throw;
            }
        }
        armed = false;
        if (injected) {
            if (!parent.GetChildren().empty() || document.snapshot() != original ||
                printer.selected()) {
                throw std::runtime_error(
                    "allocation failure changed document, printer or child ownership");
            }
            vellum::print_dialog retry(&parent, document, printer, 1);
            if (!retry.valid()) {
                throw std::runtime_error("print dialog retry is invalid");
            }
        }
        std::cout << "VELLUM_ALLOCATIONS=" << allocations << '\n';
    } catch (const std::exception &error) {
        armed = false;
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
