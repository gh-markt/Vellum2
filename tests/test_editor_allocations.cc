#include "vellum/main_frame.hh"
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
class error_hook : public wxModalDialogHook {
  public:
    error_hook() { Register(); }
    int errors = 0;

  protected:
    int Enter(wxDialog *dialog) override {
        if (dialog->GetTitle() == "Vellum") {
            ++errors;
            return wxID_OK;
        }
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
        const int editor =
            frame.GetMenuBar()->GetMenu(0)->FindItem("Enable editor mode for this document");
        if (editor == wxNOT_FOUND || !frame.GetMenuBar()->IsEnabled(editor))
            throw std::runtime_error("Editor action is not available");
        const auto base =
            std::filesystem::path(wxStandardPaths::Get().GetUserLocalDataDir().ToStdString());
        error_hook messages;
        wxCommandEvent event(wxEVT_MENU, editor);
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
        const bool editor_enabled = !frame.GetMenuBar()->IsEnabled(editor);
        if (editor_enabled && messages.errors > 0) {
            auto *bitmap = dynamic_cast<wxStaticBitmap *>(frame.FindWindowByName("viewer-bitmap"));
            if (bitmap == nullptr || bitmap->IsShown()) {
                throw std::runtime_error("failed rendering left a stale page visible");
            }
        }
        if (!editor_enabled && std::filesystem::exists(base)) {
            for (const auto &entry : std::filesystem::directory_iterator(base)) {
                if (entry.path().filename().string().starts_with("recovery-")) {
                    throw std::runtime_error(
                        "failed Editor transition left an unused recovery directory");
                }
            }
        }
        if (!editor_enabled)
            frame.GetEventHandler()->ProcessEvent(event);
        if (frame.GetMenuBar()->IsEnabled(editor) ||
            !frame.GetStatusBar()->GetStatusText().Contains("Editor")) {
            throw std::runtime_error(
                "Editor transition did not recover: menu=" +
                std::to_string(frame.GetMenuBar()->IsEnabled(editor)) +
                " status=" + frame.GetStatusBar()->GetStatusText().ToStdString() +
                " errors=" + std::to_string(messages.errors));
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
