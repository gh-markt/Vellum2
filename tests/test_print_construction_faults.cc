#include "vellum/print_dialog.hh"
#include <array>
#include <iostream>
#include <stdexcept>
#include <wx/app.h>
#include <wx/frame.h>

namespace {
enum class control_kind {
    label,
    button,
    checkbox,
    choice,
    panel,
    spin,
    spin_digits,
    window_item,
    sizer_item,
    spacer_item,
    growable_column,
    set_sizer,
    text_control,
    ascii_string,
    scroll_rate,
    top_minimum,
    top_layout,
    child_minimum,
    sizer_minimum,
    preview_background,
    page_dimensions,
    page_count,
    count
};
constexpr auto kind_count = static_cast<std::size_t>(control_kind::count);
std::array<int, kind_count> observed{};
int failure_kind = -1;
int failure_index = -1;
int tracked_page_calls = 0;
void maybe_fail(control_kind kind) {
    const auto index = static_cast<std::size_t>(kind);
    if (observed[index]++ == failure_index && static_cast<int>(kind) == failure_kind) {
        throw std::bad_alloc();
    }
}
} // namespace

extern "C" void __real__ZN12wxStaticTextC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(
    void *, wxWindow *, int, const wxString &, const wxPoint &, const wxSize &, long,
    const wxString &);
extern "C" void __wrap__ZN12wxStaticTextC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(
    void *object, wxWindow *parent, int id, const wxString &label, const wxPoint &position,
    const wxSize &size, long style, const wxString &name) {
    maybe_fail(control_kind::label);
    __real__ZN12wxStaticTextC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(
        object, parent, id, label, position, size, style, name);
}
extern "C" bool
__real__ZN8wxButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(
    void *, wxWindow *, int, const wxString &, const wxPoint &, const wxSize &, long,
    const wxValidator &, const wxString &);
extern "C" bool
__wrap__ZN8wxButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(
    void *object, wxWindow *parent, int id, const wxString &label, const wxPoint &position,
    const wxSize &size, long style, const wxValidator &validator, const wxString &name) {
    maybe_fail(control_kind::button);
    return __real__ZN8wxButton6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(
        object, parent, id, label, position, size, style, validator, name);
}
extern "C" bool
__real__ZN10wxCheckBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(
    void *, wxWindow *, int, const wxString &, const wxPoint &, const wxSize &, long,
    const wxValidator &, const wxString &);
extern "C" bool
__wrap__ZN10wxCheckBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(
    void *object, wxWindow *parent, int id, const wxString &label, const wxPoint &position,
    const wxSize &size, long style, const wxValidator &validator, const wxString &name) {
    maybe_fail(control_kind::checkbox);
    return __real__ZN10wxCheckBox6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(
        object, parent, id, label, position, size, style, validator, name);
}
extern "C" bool
__real__ZN8wxChoice6CreateEP8wxWindowiRK7wxPointRK6wxSizeiPK8wxStringlRK11wxValidatorRS9_(
    void *, wxWindow *, int, const wxPoint &, const wxSize &, int, const wxString *, long,
    const wxValidator &, const wxString &);
extern "C" bool
__wrap__ZN8wxChoice6CreateEP8wxWindowiRK7wxPointRK6wxSizeiPK8wxStringlRK11wxValidatorRS9_(
    void *object, wxWindow *parent, int id, const wxPoint &position, const wxSize &size, int count,
    const wxString *choices, long style, const wxValidator &validator, const wxString &name) {
    maybe_fail(control_kind::choice);
    return __real__ZN8wxChoice6CreateEP8wxWindowiRK7wxPointRK6wxSizeiPK8wxStringlRK11wxValidatorRS9_(
        object, parent, id, position, size, count, choices, style, validator, name);
}
extern "C" bool __real__ZN11wxPanelBase6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(
    void *, wxWindow *, int, const wxPoint &, const wxSize &, long, const wxString &);
extern "C" bool __wrap__ZN11wxPanelBase6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(
    void *object, wxWindow *parent, int id, const wxPoint &position, const wxSize &size, long style,
    const wxString &name) {
    maybe_fail(control_kind::panel);
    return __real__ZN11wxPanelBase6CreateEP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(
        object, parent, id, position, size, style, name);
}
extern "C" bool
__real__ZN17wxSpinCtrlGTKBase6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelddddS4_(
    void *, wxWindow *, int, const wxString &, const wxPoint &, const wxSize &, long, double,
    double, double, double, const wxString &);
extern "C" bool
__wrap__ZN17wxSpinCtrlGTKBase6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelddddS4_(
    void *object, wxWindow *parent, int id, const wxString &value, const wxPoint &position,
    const wxSize &size, long style, double minimum, double maximum, double initial,
    double increment, const wxString &name) {
    maybe_fail(control_kind::spin);
    return __real__ZN17wxSpinCtrlGTKBase6CreateEP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelddddS4_(
        object, parent, id, value, position, size, style, minimum, maximum, initial, increment,
        name);
}
extern "C" void __real__ZN16wxSpinCtrlDouble9SetDigitsEj(void *, unsigned int);
extern "C" void __wrap__ZN16wxSpinCtrlDouble9SetDigitsEj(void *control, unsigned int digits) {
    __real__ZN16wxSpinCtrlDouble9SetDigitsEj(control, digits);
    maybe_fail(control_kind::spin_digits);
}
extern "C" void __real__ZN11wxSizerItemC1EP8wxWindowiiiP8wxObject(void *, wxWindow *, int, int, int,
                                                                  wxObject *);
extern "C" void __wrap__ZN11wxSizerItemC1EP8wxWindowiiiP8wxObject(void *object, wxWindow *window,
                                                                  int proportion, int flags,
                                                                  int border, wxObject *user_data) {
    maybe_fail(control_kind::window_item);
    __real__ZN11wxSizerItemC1EP8wxWindowiiiP8wxObject(object, window, proportion, flags, border,
                                                      user_data);
}
extern "C" void __real__ZN11wxSizerItemC1EP7wxSizeriiiP8wxObject(void *, wxSizer *, int, int, int,
                                                                 wxObject *);
extern "C" void __wrap__ZN11wxSizerItemC1EP7wxSizeriiiP8wxObject(void *object, wxSizer *sizer,
                                                                 int proportion, int flags,
                                                                 int border, wxObject *user_data) {
    maybe_fail(control_kind::sizer_item);
    __real__ZN11wxSizerItemC1EP7wxSizeriiiP8wxObject(object, sizer, proportion, flags, border,
                                                     user_data);
}
extern "C" void __real__ZN11wxSizerItemC1EiiiiiP8wxObject(void *, int, int, int, int, int,
                                                          wxObject *);
extern "C" void __wrap__ZN11wxSizerItemC1EiiiiiP8wxObject(void *object, int width, int height,
                                                          int proportion, int flags, int border,
                                                          wxObject *user_data) {
    maybe_fail(control_kind::spacer_item);
    __real__ZN11wxSizerItemC1EiiiiiP8wxObject(object, width, height, proportion, flags, border,
                                              user_data);
}
extern "C" void __real__ZN15wxFlexGridSizer14AddGrowableColEmi(void *, unsigned long, int);
extern "C" void __wrap__ZN15wxFlexGridSizer14AddGrowableColEmi(void *object, unsigned long column,
                                                               int proportion) {
    maybe_fail(control_kind::growable_column);
    __real__ZN15wxFlexGridSizer14AddGrowableColEmi(object, column, proportion);
}
extern "C" void __real__ZN12wxWindowBase8SetSizerEP7wxSizerb(void *, wxSizer *, bool);
extern "C" void __wrap__ZN12wxWindowBase8SetSizerEP7wxSizerb(void *object, wxSizer *sizer,
                                                             bool delete_old) {
    maybe_fail(control_kind::set_sizer);
    __real__ZN12wxWindowBase8SetSizerEP7wxSizerb(object, sizer, delete_old);
}
extern "C" void
__real__ZN10wxTextCtrlC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(
    void *, wxWindow *, int, const wxString &, const wxPoint &, const wxSize &, long,
    const wxValidator &, const wxString &);
extern "C" void
__wrap__ZN10wxTextCtrlC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(
    void *object, wxWindow *parent, int id, const wxString &value, const wxPoint &position,
    const wxSize &size, long style, const wxValidator &validator, const wxString &name) {
    maybe_fail(control_kind::text_control);
    __real__ZN10wxTextCtrlC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelRK11wxValidatorS4_(
        object, parent, id, value, position, size, style, validator, name);
}
extern "C" wxString __real__ZN8wxString9FromAsciiEPKc(const char *);
extern "C" wxString __wrap__ZN8wxString9FromAsciiEPKc(const char *text) {
    maybe_fail(control_kind::ascii_string);
    return __real__ZN8wxString9FromAsciiEPKc(text);
}
extern "C" void __real__ZN18wxScrollHelperBase13SetScrollRateEii(void *, int, int);
extern "C" void __wrap__ZN18wxScrollHelperBase13SetScrollRateEii(void *object, int x, int y) {
    maybe_fail(control_kind::scroll_rate);
    __real__ZN18wxScrollHelperBase13SetScrollRateEii(object, x, y);
}
extern "C" void __real__ZN19wxTopLevelWindowGTK10SetMinSizeERK6wxSize(void *, const wxSize &);
extern "C" void __wrap__ZN19wxTopLevelWindowGTK10SetMinSizeERK6wxSize(void *object,
                                                                      const wxSize &size) {
    maybe_fail(control_kind::top_minimum);
    __real__ZN19wxTopLevelWindowGTK10SetMinSizeERK6wxSize(object, size);
}
extern "C" bool __real__ZN20wxTopLevelWindowBase6LayoutEv(void *);
extern "C" bool __wrap__ZN20wxTopLevelWindowBase6LayoutEv(void *object) {
    maybe_fail(control_kind::top_layout);
    return __real__ZN20wxTopLevelWindowBase6LayoutEv(object);
}
extern "C" void __real__ZN12wxWindowBase10SetMinSizeERK6wxSize(void *, const wxSize &);
extern "C" void __wrap__ZN12wxWindowBase10SetMinSizeERK6wxSize(void *object, const wxSize &size) {
    maybe_fail(control_kind::child_minimum);
    __real__ZN12wxWindowBase10SetMinSizeERK6wxSize(object, size);
}
extern "C" vellum::page_extent __real__ZNK6vellum12pdf_document10dimensionsEi(const void *, int);
extern "C" int __real__ZNK6vellum12pdf_document5pagesEv(const void *);
extern "C" int __wrap__ZNK6vellum12pdf_document5pagesEv(const void *document) {
    if (observed[static_cast<std::size_t>(control_kind::page_count)] < tracked_page_calls)
        maybe_fail(control_kind::page_count);
    return __real__ZNK6vellum12pdf_document5pagesEv(document);
}
extern "C" wxSize __real__ZN7wxSizer10GetMinSizeEv(void *);
extern "C" wxSize __wrap__ZN7wxSizer10GetMinSizeEv(void *sizer) {
    maybe_fail(control_kind::sizer_minimum);
    return __real__ZN7wxSizer10GetMinSizeEv(sizer);
}
extern "C" bool __real__ZN8wxWindow18SetBackgroundStyleE17wxBackgroundStyle(void *,
                                                                            wxBackgroundStyle);
extern "C" bool
__wrap__ZN8wxWindow18SetBackgroundStyleE17wxBackgroundStyle(void *window, wxBackgroundStyle style) {
    maybe_fail(control_kind::preview_background);
    return __real__ZN8wxWindow18SetBackgroundStyleE17wxBackgroundStyle(window, style);
}
extern "C" vellum::page_extent __wrap__ZNK6vellum12pdf_document10dimensionsEi(const void *document,
                                                                              int page) {
    maybe_fail(control_kind::page_dimensions);
    return __real__ZNK6vellum12pdf_document10dimensionsEi(document, page);
}

class construction_test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        SetExitOnFrameDelete(false);
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
        wxFrame parent(nullptr, wxID_ANY, "Print construction failure test");
        vellum::pdf_document document(argv[1]);
        tracked_page_calls = document.pages() + 1;
        const auto original = document.snapshot();
        vellum::printer_settings printer;
        observed.fill(0);
        {
            vellum::print_dialog baseline(&parent, document, printer, 1);
            if (!baseline.valid()) {
                throw std::runtime_error("baseline print dialog is invalid");
            }
        }
        const auto counts = observed;
        int total = 0;
        for (std::size_t kind = 0; kind < counts.size(); ++kind) {
            if (counts[kind] == 0 || counts[kind] > 250) {
                throw std::runtime_error("unexpected print control count for kind " +
                                         std::to_string(kind) + ": " +
                                         std::to_string(counts[kind]));
            }
            total += counts[kind];
            for (int index = 0; index < counts[kind]; ++index) {
                observed.fill(0);
                failure_kind = static_cast<int>(kind);
                failure_index = index;
                bool rejected = false;
                try {
                    vellum::print_dialog dialog(&parent, document, printer, 1);
                } catch (const std::bad_alloc &) {
                    rejected = true;
                }
                failure_kind = failure_index = -1;
                if (!rejected || !parent.GetChildren().empty() || document.snapshot() != original ||
                    printer.selected()) {
                    throw std::runtime_error("failed print construction did not clean up safely");
                }
            }
        }
        {
            vellum::print_dialog retry(&parent, document, printer, 1);
            if (!retry.valid()) {
                throw std::runtime_error(
                    "print dialog failed to recover after construction failures");
            }
        }
        wxDialog *owned = new vellum::print_dialog(&parent, document, printer, 1);
        if (!static_cast<vellum::print_dialog *>(owned)->valid()) {
            delete owned;
            throw std::runtime_error("heap-owned print dialog was invalid");
        }
        delete owned;
        if (!parent.GetChildren().empty() || document.snapshot() != original ||
            printer.selected()) {
            throw std::runtime_error(
                "polymorphic print dialog deletion did not release its children");
        }
        std::cout << "construction failure positions: " << total << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
