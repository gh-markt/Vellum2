#include "vellum/main_frame.hh"
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>
#include <wx/app.h>
#include <wx/statbmp.h>
#include <wx/statusbr.h>

namespace {
std::string_view failure;
bool injected = false;
int timer_initializations = 0;
int menu_items = 0;
int menu_initializations = 0;
int toolbar_icons = 0;
int toolbar_strings = 0;
int menu_strings = 0;
bool tracking_toolbar_strings = false;
bool tracking_menu_strings = false;
bool track_menus = false;
void *live_menus[3]{};
void fail_if(std::string_view point) {
    if (failure == point) {
        failure = {};
        injected = true;
        throw std::bad_alloc();
    }
}
} // namespace

void *operator new(std::size_t size) {
    if (void *value = std::malloc(size ? size : 1))
        return value;
    throw std::bad_alloc();
}
void operator delete(void *value) noexcept {
    for (auto &menu : live_menus)
        if (menu == value)
            menu = nullptr;
    std::free(value);
}
void operator delete(void *value, std::size_t) noexcept {
    ::operator delete(value);
}
extern "C" void __real__ZN8wxStringC1EPKc(void *, const char *);
extern "C" void __wrap__ZN8wxStringC1EPKc(void *value, const char *text) {
    if (tracking_menu_strings && failure.starts_with("menu-text-")) {
        const int requested = std::stoi(std::string(failure.substr(10)));
        if (menu_strings++ == requested) {
            failure = {};
            injected = true;
            throw std::bad_alloc();
        }
    }
    __real__ZN8wxStringC1EPKc(value, text);
}

extern "C" void __real__ZN6wxMenu4InitEv(void *);
extern "C" void __wrap__ZN6wxMenu4InitEv(void *menu) {
    if (failure.starts_with("menu-init-")) {
        const int requested = std::stoi(std::string(failure.substr(10)));
        if (menu_initializations++ == requested) {
            failure = {};
            injected = true;
            throw std::bad_alloc();
        }
    }
    __real__ZN6wxMenu4InitEv(menu);
    if (track_menus) {
        for (auto &slot : live_menus) {
            if (!slot) {
                slot = menu;
                return;
            }
        }
        throw std::runtime_error("unexpected number of menus");
    }
}
extern "C" void __real__ZN9wxMenuBarC1Ev(void *);
extern "C" void __wrap__ZN9wxMenuBarC1Ev(void *bar) {
    fail_if("menu-bar");
    __real__ZN9wxMenuBarC1Ev(bar);
}
extern "C" wxToolBar *__real__ZN11wxFrameBase13CreateToolBarEliRK8wxString(void *, long, int,
                                                                           const wxString &);
extern "C" wxToolBar *__wrap__ZN11wxFrameBase13CreateToolBarEliRK8wxString(void *frame, long style,
                                                                           int id,
                                                                           const wxString &name) {
    if (failure == "toolbar-unavailable") {
        failure = {};
        injected = true;
        return nullptr;
    }
    return __real__ZN11wxFrameBase13CreateToolBarEliRK8wxString(frame, style, id, name);
}
extern "C" wxBitmap __real__ZN13wxArtProvider9GetBitmapERK8wxStringS2_RK6wxSize(const wxString &,
                                                                                const wxString &,
                                                                                const wxSize &);
extern "C" wxBitmap __wrap__ZN13wxArtProvider9GetBitmapERK8wxStringS2_RK6wxSize(
    const wxString &id, const wxString &client, const wxSize &size) {
    if (failure.starts_with("toolbar-icon-")) {
        const int requested = std::stoi(std::string(failure.substr(13)));
        if (toolbar_icons++ == requested) {
            failure = {};
            injected = true;
            throw std::bad_alloc();
        }
    }
    return __real__ZN13wxArtProvider9GetBitmapERK8wxStringS2_RK6wxSize(id, client, size);
}
extern "C" wxString __real__ZN8wxString9FromAsciiEPKc(const char *);
extern "C" wxString __wrap__ZN8wxString9FromAsciiEPKc(const char *text) {
    if (tracking_toolbar_strings && failure.starts_with("toolbar-text-")) {
        const int requested = std::stoi(std::string(failure.substr(13)));
        if (toolbar_strings++ == requested) {
            failure = {};
            injected = true;
            throw std::bad_alloc();
        }
    }
    return __real__ZN8wxString9FromAsciiEPKc(text);
}
extern "C" bool
__real__ZN10wxComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeiPS3_lRK11wxValidatorS4_(
    void *, wxWindow *, int, const wxString &, const wxPoint &, const wxSize &, int,
    const wxString *, long, const wxValidator &, const wxString &);
extern "C" bool
__wrap__ZN10wxComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeiPS3_lRK11wxValidatorS4_(
    void *control, wxWindow *parent, int id, const wxString &value, const wxPoint &position,
    const wxSize &size, int count, const wxString *choices, long style,
    const wxValidator &validator, const wxString &name) {
    const bool created =
        __real__ZN10wxComboBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizeiPS3_lRK11wxValidatorS4_(
            control, parent, id, value, position, size, count, choices, style, validator, name);
    fail_if("toolbar-zoom");
    return created;
}
extern "C" void __real__ZN11wxFrameBase10SetMenuBarEP9wxMenuBar(void *, wxMenuBar *);
extern "C" void __wrap__ZN11wxFrameBase10SetMenuBarEP9wxMenuBar(void *frame, wxMenuBar *bar) {
    fail_if("menu-attach");
    __real__ZN11wxFrameBase10SetMenuBarEP9wxMenuBar(frame, bar);
}

extern "C" wxMenuItem *__real__ZN14wxMenuItemBase3NewEP6wxMenuiRK8wxStringS4_10wxItemKindS1_(
    wxMenu *, int, const wxString &, const wxString &, wxItemKind, wxMenu *);
extern "C" wxMenuItem *__wrap__ZN14wxMenuItemBase3NewEP6wxMenuiRK8wxStringS4_10wxItemKindS1_(
    wxMenu *menu, int id, const wxString &text, const wxString &help, wxItemKind kind,
    wxMenu *submenu) {
    if (failure.starts_with("menu-item-")) {
        const int requested = std::stoi(std::string(failure.substr(10)));
        if (menu_items++ == requested) {
            failure = {};
            injected = true;
            throw std::bad_alloc();
        }
    }
    return __real__ZN14wxMenuItemBase3NewEP6wxMenuiRK8wxStringS4_10wxItemKindS1_(
        menu, id, text, help, kind, submenu);
}

extern "C" void __real__ZN11wxFrameBaseC2Ev(void *);
extern "C" void __wrap__ZN11wxFrameBaseC2Ev(void *frame) {
    fail_if("frame-base");
    __real__ZN11wxFrameBaseC2Ev(frame);
}
extern "C" bool __real__ZN7wxFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(
    void *, wxWindow *, int, const wxString &, const wxPoint &, const wxSize &, long,
    const wxString &);
extern "C" bool __wrap__ZN7wxFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(
    void *frame, wxWindow *parent, int id, const wxString &title, const wxPoint &position,
    const wxSize &size, long style, const wxString &name) {
    fail_if("frame-create");
    return __real__ZN7wxFrame6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(
        frame, parent, id, title, position, size, style, name);
}
extern "C" void __real__ZN7wxTimer4InitEv(void *);
extern "C" void __wrap__ZN7wxTimer4InitEv(void *timer) {
    if ((failure == "timer-init-0" && timer_initializations == 0) ||
        (failure == "timer-init-1" && timer_initializations == 1)) {
        failure = {};
        injected = true;
        throw std::bad_alloc();
    }
    ++timer_initializations;
    __real__ZN7wxTimer4InitEv(timer);
}
extern "C" bool __real__ZN11wxPanelBase6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(
    void *, wxWindow *, int, const wxPoint &, const wxSize &, long, const wxString &);
extern "C" bool __wrap__ZN11wxPanelBase6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(
    void *panel, wxWindow *parent, int id, const wxPoint &position, const wxSize &size, long style,
    const wxString &name) {
    fail_if("canvas-create");
    return __real__ZN11wxPanelBase6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(
        panel, parent, id, position, size, style, name);
}
extern "C" void __real__ZN18wxScrollHelperBaseC2EP8wxWindow(void *, wxWindow *);
extern "C" void __wrap__ZN18wxScrollHelperBaseC2EP8wxWindow(void *scroll, wxWindow *window) {
    fail_if("scroll-helper");
    __real__ZN18wxScrollHelperBaseC2EP8wxWindow(scroll, window);
}

extern "C" void __real__ZN6vellum10main_frame11build_menusEv(void *);
extern "C" void __wrap__ZN6vellum10main_frame11build_menusEv(void *frame) {
    fail_if("menus");
    tracking_menu_strings = failure.starts_with("menu-text-");
    try {
        __real__ZN6vellum10main_frame11build_menusEv(frame);
    } catch (...) {
        tracking_menu_strings = false;
        throw;
    }
    tracking_menu_strings = false;
}
extern "C" void __real__ZN6vellum10main_frame13build_toolbarEv(void *);
extern "C" void __wrap__ZN6vellum10main_frame13build_toolbarEv(void *frame) {
    fail_if("toolbar");
    tracking_toolbar_strings = failure.starts_with("toolbar-text-");
    try {
        __real__ZN6vellum10main_frame13build_toolbarEv(frame);
    } catch (...) {
        tracking_toolbar_strings = false;
        throw;
    }
    tracking_toolbar_strings = false;
}
extern "C" void __real__ZN6vellum10main_frame14update_actionsEv(void *);
extern "C" void __wrap__ZN6vellum10main_frame14update_actionsEv(void *frame) {
    fail_if("update");
    __real__ZN6vellum10main_frame14update_actionsEv(frame);
}

extern "C" wxStatusBar *
__real__ZN11wxFrameBase15CreateStatusBarEiliRK8wxString(void *, int, long, int, const wxString &);
extern "C" wxStatusBar *
__wrap__ZN11wxFrameBase15CreateStatusBarEiliRK8wxString(void *frame, int fields, long style, int id,
                                                        const wxString &name) {
    fail_if("status-bar");
    if (failure == "status-unavailable") {
        failure = {};
        injected = true;
        return nullptr;
    }
    return __real__ZN11wxFrameBase15CreateStatusBarEiliRK8wxString(frame, fields, style, id, name);
}

extern "C" void __real__ZN18wxScrollHelperBase13SetScrollRateEii(void *, int, int);
extern "C" void __wrap__ZN18wxScrollHelperBase13SetScrollRateEii(void *scroll, int x, int y) {
    fail_if("scroll-rate");
    __real__ZN18wxScrollHelperBase13SetScrollRateEii(scroll, x, y);
}

extern "C" void
__real__ZN14wxStaticBitmapC1EP8wxWindowiRK14wxBitmapBundleRK7wxPointRK6wxSizelRK8wxString(
    void *, wxWindow *, int, const wxBitmapBundle &, const wxPoint &, const wxSize &, long,
    const wxString &);
extern "C" void
__wrap__ZN14wxStaticBitmapC1EP8wxWindowiRK14wxBitmapBundleRK7wxPointRK6wxSizelRK8wxString(
    void *bitmap, wxWindow *parent, int id, const wxBitmapBundle &bundle, const wxPoint &position,
    const wxSize &size, long style, const wxString &name) {
    fail_if("bitmap");
    __real__ZN14wxStaticBitmapC1EP8wxWindowiRK14wxBitmapBundleRK7wxPointRK6wxSizelRK8wxString(
        bitmap, parent, id, bundle, position, size, style, name);
}

extern "C" bool __real__ZN7wxTimer5StartEib(void *, int, bool);
extern "C" bool __wrap__ZN7wxTimer5StartEib(void *timer, int milliseconds, bool once) {
    fail_if("timer");
    return __real__ZN7wxTimer5StartEib(timer, milliseconds, once);
}

extern "C" void __real__ZN11wxFrameBase13SetStatusTextERK8wxStringi(void *, const wxString &, int);
extern "C" void
__wrap__ZN11wxFrameBase13SetStatusTextERK8wxStringi(void *frame, const wxString &text, int field) {
    fail_if("status-text");
    __real__ZN11wxFrameBase13SetStatusTextERK8wxStringi(frame, text, field);
}

class construction_test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        this->SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(construction_test_app);

int main(int argc, char **argv) {
    if (argc != 2 || !wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        const auto windows = wxTopLevelWindows.GetCount();
        failure = argv[1];
        track_menus = failure.starts_with("menu-item-") || failure.starts_with("menu-init-") ||
                      failure.starts_with("menu-text-") || failure == "menu-bar" ||
                      failure == "menu-attach";
        bool caught = false;
        try {
            vellum::main_frame frame;
        } catch (const std::bad_alloc &) {
            caught = true;
        } catch (const std::runtime_error &error) {
            caught = (std::string(argv[1]) == "toolbar-unavailable" &&
                      std::string(error.what()) == "cannot create toolbar") ||
                     (std::string(argv[1]) == "status-unavailable" &&
                      std::string(error.what()) == "cannot create status bar");
        }
        failure = {};
        track_menus = false;
        for (const auto *menu : live_menus)
            if (menu)
                throw std::runtime_error("failed menu construction retained an unattached menu");
        if (!caught || !injected || wxTopLevelWindows.GetCount() != windows) {
            throw std::runtime_error("failed construction did not release its window or propagate");
        }
        {
            vellum::main_frame retry;
            if (!retry.GetMenuBar() || retry.GetMenuBar()->IsEnabled(wxID_SAVE)) {
                throw std::runtime_error("viewer construction did not recover");
            }
        }
        if (wxTopLevelWindows.GetCount() != windows) {
            throw std::runtime_error("viewer retry retained a top-level window");
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
