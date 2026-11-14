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
        const auto windows = wxTopLevelWindows.GetCount();
        fail_at =
            index < 0 ? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(index);
        allocations = 0;
        armed = true;
        try {
            vellum::main_frame frame;
            armed = false;
            if (index >= 0 && !injected) {
                throw std::runtime_error("requested allocation was not reached");
            }
            if (frame.GetMenuBar() == nullptr || frame.GetMenuBar()->IsEnabled(wxID_SAVE)) {
                throw std::runtime_error("viewer frame did not initialize safely");
            }
        } catch (const std::bad_alloc &) {
            armed = false;
            if (!injected)
                throw;
        }
        armed = false;
        if (wxTopLevelWindows.GetCount() != windows) {
            throw std::runtime_error("failed viewer construction retained a top-level window");
        }
        if (injected) {
            vellum::main_frame retry;
            if (retry.GetMenuBar() == nullptr || retry.GetMenuBar()->IsEnabled(wxID_SAVE)) {
                throw std::runtime_error("viewer retry did not initialize safely");
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
