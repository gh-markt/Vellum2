#include "vellum/atomic_file.hh"
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
#include <wx/statusbr.h>

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
    if (argc != 4)
        return 2;
    const auto index = std::stoll(argv[2]);
    const std::filesystem::path source(argv[1]);
    const bool external = std::string(argv[3]) == "external";
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit())
        return 2;
    int result = 0;
    try {
        vellum::main_frame frame;
        frame.Show();
        frame.open_path(source);
        wxYield();
        const auto original_title = frame.GetTitle();
        const auto original = vellum::read_file(source);
        const std::string expected = external ? "external replacement" : original;
        if (external)
            vellum::atomic_write(source, expected);
        error_hook messages;
        wxCommandEvent save(wxEVT_MENU, wxID_SAVE);
        fail_at =
            index < 0 ? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(index);
        allocations = 0;
        armed = true;
        try {
            frame.GetEventHandler()->ProcessEvent(save);
        } catch (const std::bad_alloc &) {
            armed = false;
        }
        armed = false;
        if (index >= 0 && !injected)
            throw std::runtime_error("requested allocation was not reached");
        if (vellum::read_file(source) != expected || frame.GetTitle() != original_title) {
            throw std::runtime_error("failed save changed the source or viewer identity");
        }
        if (index < 0 && messages.errors != (external ? 1 : 0)) {
            throw std::runtime_error("save did not report external replacement correctly");
        }
        frame.go_to_page(2);
        if (frame.current_page() != 2)
            throw std::runtime_error("failed save changed the active document");
        frame.go_to_page(1);
        const int prior_errors = messages.errors;
        frame.GetEventHandler()->ProcessEvent(save);
        if (vellum::read_file(source) != expected ||
            messages.errors != prior_errors + (external ? 1 : 0)) {
            throw std::runtime_error("save retry did not preserve external-change behavior");
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
