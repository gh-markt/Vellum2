#include "vellum/main_frame.hh"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <wx/app.h>

namespace {
bool armed = false;
bool injected = false;
std::size_t allocations = 0;
std::size_t fail_at = 0;
} // namespace

void *operator new(std::size_t size) {
    if (armed && allocations++ == fail_at) {
        armed = false;
        injected = true;
        throw std::bad_alloc();
    }
    if (void *result = std::malloc(size ? size : 1)) {
        return result;
    }
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void *value) noexcept {
    std::free(value);
}
void operator delete[](void *value) noexcept {
    ::operator delete(value);
}
void operator delete(void *value, std::size_t) noexcept {
    ::operator delete(value);
}
void operator delete[](void *value, std::size_t) noexcept {
    ::operator delete(value);
}

extern "C" void __real__ZN12wxEvtHandler6DoBindEiiiP14wxEventFunctorP8wxObject(void *, int, int,
                                                                               int,
                                                                               wxEventFunctor *,
                                                                               wxObject *);
extern "C" void __wrap__ZN12wxEvtHandler6DoBindEiiiP14wxEventFunctorP8wxObject(
    void *handler, int id, int last, int type, wxEventFunctor *functor, wxObject *user_data) {
    armed = true;
    try {
        __real__ZN12wxEvtHandler6DoBindEiiiP14wxEventFunctorP8wxObject(handler, id, last, type,
                                                                       functor, user_data);
    } catch (...) {
        armed = false;
        throw;
    }
    armed = false;
}

class binding_test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        this->SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(binding_test_app);

int main(int argc, char **argv) {
    if (argc != 2 || !wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    const auto selected = std::stoll(argv[1]);
    fail_at =
        selected < 0 ? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(selected);
    int result = 0;
    try {
        const auto windows = wxTopLevelWindows.GetCount();
        bool caught = false;
        try {
            vellum::main_frame frame;
        } catch (const std::bad_alloc &) {
            caught = true;
        }
        if ((selected >= 0 && (!injected || !caught)) || (selected < 0 && (injected || caught)) ||
            wxTopLevelWindows.GetCount() != windows) {
            throw std::runtime_error("event binding failure did not cleanly abort construction");
        }
        const auto count = allocations;
        fail_at = std::numeric_limits<std::size_t>::max();
        {
            vellum::main_frame retry;
            if (!retry.GetMenuBar() || retry.GetMenuBar()->IsEnabled(wxID_SAVE)) {
                throw std::runtime_error("viewer did not recover after event binding failure");
            }
        }
        if (wxTopLevelWindows.GetCount() != windows) {
            throw std::runtime_error("viewer retry retained a top-level window");
        }
        std::cout << "VELLUM_BIND_ALLOCATIONS=" << count << '\n';
    } catch (const std::exception &error) {
        armed = false;
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
