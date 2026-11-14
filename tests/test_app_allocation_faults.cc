#include <cstdlib>
#include <cstring>
#include <new>
#include <wx/modalhook.h>
#include <wx/wx.h>

#undef wxIMPLEMENT_APP
#define wxIMPLEMENT_APP(app) wxIMPLEMENT_APP_NO_MAIN(app)
#include "../src/app.cc"
#undef wxIMPLEMENT_APP

namespace {
long early_allocation = -1;
long open_allocation = -1;
long report_allocation = -1;
long label_allocation = -1;
bool foreign_exception = false;
bool fail_app_allocation = false;
bool fail_app_constructor = false;
bool fail_frame_allocation = false;
bool fail_frame_constructor = false;
bool injected = false;
bool released = false;
void *frame_storage = nullptr;
} // namespace

void *operator new(std::size_t size) {
    if (early_allocation >= 0 && early_allocation-- == 0) {
        injected = true;
        throw std::bad_alloc();
    }
    if (fail_app_allocation && size == sizeof(vellum_app)) {
        fail_app_allocation = false;
        injected = true;
        throw std::bad_alloc();
    }
    if (fail_frame_allocation && size == sizeof(vellum::main_frame)) {
        fail_frame_allocation = false;
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
    if (value && value == frame_storage) {
        released = true;
    }
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

extern "C" void __real__ZN5wxAppC2Ev(void *);
extern "C" void __wrap__ZN5wxAppC2Ev(void *storage) {
    if (fail_app_constructor) {
        frame_storage = storage;
        injected = true;
        throw std::bad_alloc();
    }
    __real__ZN5wxAppC2Ev(storage);
}

extern "C" void __real__ZN6vellum10main_frameC1Ev(void *);
extern "C" void __wrap__ZN6vellum10main_frameC1Ev(void *storage) {
    if (fail_frame_constructor) {
        frame_storage = storage;
        injected = true;
        throw std::bad_alloc();
    }
    __real__ZN6vellum10main_frameC1Ev(storage);
    if (open_allocation >= 0) {
        frame_storage = storage;
    }
}

extern "C" bool __real__ZN19wxTopLevelWindowGTK4ShowEb(void *, bool);
extern "C" bool __wrap__ZN19wxTopLevelWindowGTK4ShowEb(void *window, bool shown) {
    const bool result = __real__ZN19wxTopLevelWindowGTK4ShowEb(window, shown);
    if (window == frame_storage && shown && open_allocation >= 0) {
        early_allocation = open_allocation;
        open_allocation = -1;
    }
    return result;
}

class report_failure : public std::exception {
  public:
    const char *what() const noexcept override {
        if (report_allocation >= 0) {
            early_allocation = report_allocation;
        }
        return "injected open failure";
    }
};

extern "C" void __real__ZN6vellum10main_frame9open_pathERKNSt10filesystem7__cxx114pathE(
    void *, const std::filesystem::path &);
extern "C" void __wrap__ZN6vellum10main_frame9open_pathERKNSt10filesystem7__cxx114pathE(
    void *frame, const std::filesystem::path &path) {
    if (foreign_exception) {
        throw 42;
    }
    if (report_allocation >= 0 || label_allocation >= 0) {
        if (label_allocation >= 0) {
            early_allocation = label_allocation;
        }
        throw report_failure();
    }
    __real__ZN6vellum10main_frame9open_pathERKNSt10filesystem7__cxx114pathE(frame, path);
}

class error_hook : public wxModalDialogHook {
  public:
    error_hook() { this->Register(); }
    int errors = 0;

  protected:
    int Enter(wxDialog *dialog) override {
        if (dialog->GetTitle() == "Cannot open PDF") {
            ++this->errors;
            return wxID_OK;
        }
        return wxID_CANCEL;
    }
};

int main(int argc, char **argv) {
    const char *mode = std::getenv("VELLUM_TEST_APP_FAILURE");
    if (!mode || (std::strcmp(mode, "allocation") && std::strcmp(mode, "constructor") &&
                  std::strcmp(mode, "application") && std::strcmp(mode, "app-constructor") &&
                  std::strncmp(mode, "early", 5) && std::strncmp(mode, "open", 4) &&
                  std::strncmp(mode, "report", 6) && std::strncmp(mode, "label", 5) &&
                  std::strcmp(mode, "foreign"))) {
        return 2;
    }
    if (std::strcmp(mode, "application") == 0 || std::strcmp(mode, "app-constructor") == 0) {
        bool started = false;
        const bool constructor = std::strcmp(mode, "app-constructor") == 0;
        fail_app_allocation = !constructor;
        fail_app_constructor = constructor;
        try {
            started = wxEntryStart(argc, argv);
        } catch (const std::bad_alloc &) {
        }
        fail_app_allocation = false;
        fail_app_constructor = false;
        if (started) {
            wxEntryCleanup();
        }
        return injected && !started && (!constructor || released) ? 0 : 1;
    }
    if (!wxEntryStart(argc, argv)) {
        return 2;
    }
    if (&wxGetApp() != wxTheApp) {
        wxEntryCleanup();
        return 1;
    }
    const bool early = std::strncmp(mode, "early", 5) == 0;
    const bool open = std::strncmp(mode, "open", 4) == 0;
    const bool report = std::strncmp(mode, "report", 6) == 0;
    const bool label = std::strncmp(mode, "label", 5) == 0;
    const bool foreign = std::strcmp(mode, "foreign") == 0;
    error_hook errors;
    const bool constructor = std::strcmp(mode, "constructor") == 0;
    bool caught = false;
    bool caught_foreign = false;
    fail_frame_allocation = !constructor && !early && !open && !report && !label && !foreign;
    if (label || foreign) {
        open_allocation = 100000;
        foreign_exception = foreign;
        if (label) {
            label_allocation = std::strtol(mode + 5, nullptr, 10);
        }
    }
    if (report) {
        report_allocation = std::strtol(mode + 6, nullptr, 10);
        open_allocation = 100000;
    }
    if (open) {
        open_allocation = std::strtol(mode + 4, nullptr, 10);
    }
    if (early) {
        early_allocation = std::strtol(mode + 5, nullptr, 10);
    }
    fail_frame_constructor = constructor;
    try {
        (void)wxTheApp->CallOnInit();
    } catch (const std::bad_alloc &) {
        caught = true;
    } catch (int value) {
        caught_foreign = value == 42;
    }
    early_allocation = -1;
    fail_frame_allocation = false;
    fail_frame_constructor = false;
    if ((open || report || label || foreign) && frame_storage) {
        static_cast<vellum::main_frame *>(frame_storage)->Destroy();
        wxYield();
    }
    wxEntryCleanup();
    if (open) {
        return injected && errors.errors == 1 && !caught ? 0 : 1;
    }
    if (report || label) {
        return injected && caught && errors.errors == 0 ? 0 : 1;
    }
    if (foreign) {
        return caught_foreign && errors.errors == 0 ? 0 : 1;
    }
    return caught && injected && (!constructor || released) ? 0 : 1;
}
