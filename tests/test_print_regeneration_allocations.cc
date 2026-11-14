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
    for (int index = 2; index < count; ++index) {
        Dl_info info{};
        if (!::dladdr(frames[index], &info))
            continue;
        if (info.dli_fname && std::strstr(info.dli_fname, "libwx"))
            break;
        if (info.dli_sname && std::strncmp(info.dli_sname, "_ZN6vellum", 10) == 0) {
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
    if (argc != 4)
        return 2;
    const auto index = std::stoll(argv[2]);
    const auto layout = std::stoi(argv[3]);
    const std::string fixture(argv[1]);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit())
        return 2;
    int result = 0;
    try {
        wxFrame parent(nullptr, wxID_ANY, "Vellum allocation test");
        vellum::pdf_document document(fixture);
        const auto original = document.snapshot();
        vellum::printer_settings printer;
        vellum::print_dialog dialog(&parent, document, printer, 1);
        auto *layout_control =
            dynamic_cast<wxChoice *>(wxWindow::FindWindowByName("print-layout", &dialog));
        auto *selection =
            dynamic_cast<wxChoice *>(wxWindow::FindWindowByName("print-selection", &dialog));
        auto *ranges =
            dynamic_cast<wxTextCtrl *>(wxWindow::FindWindowByName("print-ranges", &dialog));
        auto *scale = dynamic_cast<wxSpinCtrlDouble *>(
            wxWindow::FindWindowByName("print-percentage", &dialog));
        auto *submit = wxWindow::FindWindowByName("print-submit", &dialog);
        if (!layout_control || !selection || !ranges || !scale || !submit)
            throw std::runtime_error("print control missing");
        layout_control->SetSelection(layout);
        selection->SetSelection(2);
        if (std::getenv("VELLUM_PRINT_RANGE_STRESS")) {
            std::string expression;
            for (int page = 0; page < 32; ++page) {
                if (page != 0) {
                    expression += ',';
                }
                expression += page % 2 == 0 ? '2' : '1';
            }
            ranges->SetValue(wxString::FromUTF8(expression));
        } else {
            ranges->SetValue("1,,2");
        }
        if (layout == 3)
            scale->SetValue(300);
        dialog.regenerate_preview();
        if (!dialog.valid())
            throw std::runtime_error("initial plan is invalid");
        fail_at =
            index < 0 ? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(index);
        allocations = 0;
        armed = true;
        try {
            dialog.regenerate_preview();
        } catch (const std::bad_alloc &) {
            armed = false;
        }
        armed = false;
        if (index >= 0 && !injected)
            throw std::runtime_error("requested allocation was not reached");
        if (!dialog.valid() && submit->IsEnabled())
            throw std::runtime_error("invalid plan left Print enabled");
        if (document.snapshot() != original || printer.selected())
            throw std::runtime_error("regeneration changed the document or printer");
        dialog.regenerate_preview();
        if (!dialog.valid() || !submit->IsEnabled())
            throw std::runtime_error("plan regeneration retry failed");
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
