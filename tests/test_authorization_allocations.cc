#include "vellum/atomic_file.hh"
#include "vellum/main_frame.hh"
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
#include <wx/modalhook.h>
#include <wx/statbmp.h>
#include <wx/statusbr.h>
#include <wx/stdpaths.h>
#include <wx/textdlg.h>

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
class print_hook : public wxModalDialogHook {
  public:
    print_hook() { Register(); }
    int dialogs = 0;
    int prompts = 0;
    int errors = 0;
    bool unsafe = false;

  protected:
    int Enter(wxDialog *dialog) override {
        if (auto *password = dynamic_cast<wxPasswordEntryDialog *>(dialog)) {
            ++prompts;
            password->SetValue("vellum-long-owner-password-test");
            return wxID_OK;
        }
        if (auto *print = dynamic_cast<vellum::print_dialog *>(dialog)) {
            ++dialogs;
            auto *submit = wxWindow::FindWindowByName("print-submit", print);
            if (!submit || (!print->valid() && submit->IsEnabled()))
                unsafe = true;
            return wxID_CANCEL;
        }
        if (dialog->GetTitle() == "Vellum") {
            ++errors;
            return wxID_OK;
        }
        unsafe = true;
        return wxID_CANCEL;
    }
};
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    const auto index = std::stoll(argv[2]);
    const std::string fixture(argv[1]);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit())
        return 2;
    int result = 0;
    try {
        vellum::main_frame frame;
        frame.Show();
        frame.open_path(fixture);
        wxYield();
        const auto original_title = frame.GetTitle();
        const auto original_bytes = vellum::read_file(fixture);
        const auto windows = wxTopLevelWindows.GetCount();
        print_hook messages;
        wxCommandEvent event(wxEVT_MENU, wxID_PRINT);
        fail_at =
            index < 0 ? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(index);
        allocations = 0;
        armed = true;
        try {
            frame.GetEventHandler()->ProcessEvent(event);
        } catch (const std::bad_alloc &) {
            armed = false;
        }
        armed = false;
        if (index >= 0 && !injected)
            throw std::runtime_error("requested allocation was not reached");
        if (index < 0 && (messages.prompts != 1 || messages.dialogs != 1 || messages.errors != 0)) {
            throw std::runtime_error("restricted printing did not require owner authentication");
        }
        if (vellum::read_file(fixture) != original_bytes)
            throw std::runtime_error("authorization changed PDF bytes");
        if (messages.unsafe || frame.GetTitle() != original_title ||
            wxTopLevelWindows.GetCount() != windows) {
            throw std::runtime_error(
                "failed print action changed viewer state or retained a window");
        }
        frame.go_to_page(2);
        if (frame.current_page() != 2)
            throw std::runtime_error("failed print action changed the active document");
        frame.go_to_page(1);
        const int prior_prompts = messages.prompts;
        const int prior_dialogs = messages.dialogs;
        const int prior_errors = messages.errors;
        frame.GetEventHandler()->ProcessEvent(event);
        if (messages.unsafe || messages.dialogs != prior_dialogs + 1 ||
            messages.errors != prior_errors || wxTopLevelWindows.GetCount() != windows ||
            (index < 0 && messages.prompts != prior_prompts)) {
            throw std::runtime_error("print action did not recover after allocation failure");
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
