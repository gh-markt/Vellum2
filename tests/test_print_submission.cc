#include "vellum/atomic_file.hh"
#include "vellum/print_dialog.hh"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <unistd.h>
#include <vector>
#include <wx/app.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>

namespace {
std::vector<vellum::print_job> submitted;
std::vector<bool> outcomes;
int confirmation = wxYES;
int confirmations = 0;
int errors = 0;
wxString last_error_message;
int completed = 0;
bool throw_submission = false;
bool throw_confirmation = false;
bool throw_error_box = false;
bool watch_error_fallback = false, reject_error_fallback = false;
int error_fallback_calls = 0;
bool error_fallback_matches = false;
bool throw_preview = false;
bool throw_preview_dc = false;
bool throw_preview_brush = false;
bool throw_preview_log = false;
bool throw_preview_log_filter = false;
int preview_dc_failures = 0;
int preview_brush_failures = 0;
int preview_log_failures = 0;
int preview_log_filter_failures = 0;
int preview_failures = 0;
int preview_successes = 0;
int small_paint_events = 0;
bool printer_selected = true;
bool throw_selection = false;
bool throw_printer_defaults = false;
bool deny_next_export_permission = false;
int export_permission_denials = 0;
int selections = 0;
bool throw_file_dialog = false;
bool throw_file_modal = false;
bool accept_file_modal = false;
wxString accepted_file_path = "/proc/vellum-preview.png";
bool throw_printer_name = false;
bool throw_printer_measurement = false;
bool throw_status_concat = false;
int status_concat_failures = 0;
bool throw_status_wrap = false;
bool throw_memory_dc = false;
bool throw_bitmap_save = false;
bool reject_bitmap_save = false;
bool throw_bitmap_conversion = false, reject_bitmap_conversion = false;
bool throw_encoding_stream = false, throw_encoded_copy = false, reject_encoded_copy = false;
bool throw_preview_publish = false;
bool mark_stream_failed = false;
bool throw_bitmap_create = false;
bool throw_dc_deselect = false;
bool reject_bitmap_create = false;
bool throw_dialog_layout = false;
int preview_refresh_failures_remaining = 0, preview_refresh_failures = 0;
int side_format_failures_remaining = 0, side_format_failures = 0;
bool reject_timer_start = false;
bool throw_timer_start = false;
int rejected_timer_starts = 0;
int thrown_timer_starts = 0;
int numeric_read_count = 0;
int fail_numeric_read = 0;
bool track_numeric_reads = false;
int forced_double_index = 0, forced_double_reads = 0;
double forced_double_value = 0;
thread_local bool arm_manual_copy = false;
thread_local bool track_manual_copy = false;
thread_local std::size_t manual_allocations = 0;
thread_local std::size_t fail_manual_allocation = 0;
thread_local bool arm_export_path_failure = false, track_export_path_failure = false;
thread_local bool arm_encoded_allocation_failure = false;
thread_local int export_path_failures = 0;
wxWindow *manual_sentinel = nullptr;
bool sentinel_was_disabled = false;
bool throw_window_disabler = false;
bool fail_range_conversion = false;
int range_conversion_failures = 0;
enum class planning_fault { none, paper, selection, composition };
planning_fault injected_planning_fault = planning_fault::none;
int planning_failures = 0;
int document_page_calls = 0;
int fail_document_page_call = 0;

void planning_checkpoint(planning_fault expected) {
    if (injected_planning_fault == expected) {
        ++planning_failures;
        throw std::bad_alloc();
    }
}

void numeric_read_checkpoint() {
    if (track_numeric_reads && ++numeric_read_count == fail_numeric_read) {
        throw std::bad_alloc();
    }
}

void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void reset(std::vector<bool> result) {
    submitted.clear();
    outcomes = std::move(result);
    confirmation = wxYES;
    confirmations = errors = completed = 0;
    throw_submission = false;
}
void click_print(vellum::print_dialog &dialog) {
    wxCommandEvent event(wxEVT_BUTTON, wxID_PRINT);
    require(dialog.GetEventHandler()->ProcessEvent(event), "print event was not handled");
}
void select(vellum::print_dialog &dialog, const char *name, int value) {
    auto *choice = dynamic_cast<wxChoice *>(dialog.FindWindowByName(name));
    require(choice != nullptr, "print choice missing");
    choice->SetSelection(value);
    dialog.regenerate_preview();
    require(dialog.valid(), "test setup produced an invalid print job");
}
} // namespace

extern "C" int __wrap_fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    if (watch_error_fallback) {
        ++error_fallback_calls;
        char text[512];
        const int result = std::vsnprintf(text, sizeof(text), format, args);
        va_end(args);
        error_fallback_matches =
            stream == stderr && std::string(text).find("injected ") != std::string::npos;
        return reject_error_fallback ? -1 : result;
    }
    const int result = std::vfprintf(stream, format, args);
    va_end(args);
    return result;
}
extern "C" wxString __wrap__ZN8wxString13DoFormatWcharEPKwz(const wchar_t *format, ...) {
    if (side_format_failures_remaining > 0) {
        --side_format_failures_remaining;
        ++side_format_failures;
        throw std::runtime_error("injected side label formatting failure");
    }
    va_list args;
    va_start(args, format);
    try {
        const auto result = wxString::FormatV(wxString(format), args);
        va_end(args);
        return result;
    } catch (...) {
        va_end(args);
        throw;
    }
}

[[gnu::noinline]] void *operator new(std::size_t size) {
    if (track_export_path_failure) {
        track_export_path_failure = false;
        ++export_path_failures;
        throw std::bad_alloc();
    }
    if (track_manual_copy && manual_allocations++ == fail_manual_allocation) {
        track_manual_copy = false;
        throw std::bad_alloc();
    }
    if (void *value = std::malloc(size ? size : 1)) {
        return value;
    }
    throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](std::size_t size) {
    return ::operator new(size);
}
[[gnu::noinline]] void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
[[gnu::noinline]] void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
    return ::operator new(size, std::nothrow);
}
[[gnu::noinline]] void operator delete(void *value) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete(void *value, std::size_t) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value, std::size_t) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete(void *value, const std::nothrow_t &) noexcept {
    std::free(value);
}
[[gnu::noinline]] void operator delete[](void *value, const std::nothrow_t &) noexcept {
    std::free(value);
}

extern "C" void __real__ZN16wxWindowDisablerC1Eb(void *, bool);
extern "C" void __wrap__ZN16wxWindowDisablerC1Eb(void *disabler, bool disable) {
    if (throw_window_disabler) {
        throw std::bad_alloc();
    }
    __real__ZN16wxWindowDisablerC1Eb(disabler, disable);
    if (arm_manual_copy) {
        sentinel_was_disabled = manual_sentinel && !manual_sentinel->IsEnabled();
        track_manual_copy = true;
    }
}

extern "C" bool __real__ZNK8wxString8ToDoubleEPd(const void *, double *);
extern "C" bool __wrap__ZNK8wxString8ToDoubleEPd(const void *text, double *value) {
    numeric_read_checkpoint();
    if (forced_double_index > 0 && ++forced_double_reads == forced_double_index) {
        *value = forced_double_value;
        return true;
    }
    return __real__ZNK8wxString8ToDoubleEPd(text, value);
}
extern "C" bool __real__ZNK8wxString6ToLongEPli(const void *, long *, int);
extern "C" bool __wrap__ZNK8wxString6ToLongEPli(const void *text, long *value, int base) {
    numeric_read_checkpoint();
    return __real__ZNK8wxString6ToLongEPli(text, value, base);
}
extern "C" double __real__ZNK17wxSpinCtrlGTKBase10DoGetValueEv(const void *);
extern "C" double __wrap__ZNK17wxSpinCtrlGTKBase10DoGetValueEv(const void *control) {
    numeric_read_checkpoint();
    return __real__ZNK17wxSpinCtrlGTKBase10DoGetValueEv(control);
}
extern "C" double __real__ZNK17wxSpinCtrlGTKBase8DoGetMinEv(const void *);
extern "C" double __wrap__ZNK17wxSpinCtrlGTKBase8DoGetMinEv(const void *control) {
    numeric_read_checkpoint();
    return __real__ZNK17wxSpinCtrlGTKBase8DoGetMinEv(control);
}
extern "C" double __real__ZNK17wxSpinCtrlGTKBase8DoGetMaxEv(const void *);
extern "C" double __wrap__ZNK17wxSpinCtrlGTKBase8DoGetMaxEv(const void *control) {
    numeric_read_checkpoint();
    return __real__ZNK17wxSpinCtrlGTKBase8DoGetMaxEv(control);
}
extern "C" const char *__real__ZNK8wxString6AsCharERK8wxMBConv(const void *, const wxMBConv &);
extern "C" const char *__wrap__ZNK8wxString6AsCharERK8wxMBConv(const void *text,
                                                               const wxMBConv &conversion) {
    if (fail_range_conversion) {
        ++range_conversion_failures;
        throw std::bad_alloc();
    }
    return __real__ZNK8wxString6AsCharERK8wxMBConv(text, conversion);
}
extern "C" void __real__ZN6vellum16printer_settings9set_paperEib(void *, int, bool);
extern "C" bool __real__ZNK6vellum12pdf_document9may_printEv(const void *);
extern "C" bool __wrap__ZNK6vellum12pdf_document9may_printEv(const void *document) {
    if (deny_next_export_permission) {
        deny_next_export_permission = false;
        ++export_permission_denials;
        return false;
    }
    return __real__ZNK6vellum12pdf_document9may_printEv(document);
}
extern "C" void __real__ZN6vellum16printer_settings16set_job_defaultsERKNS_13print_optionsE(
    void *, const vellum::print_options &);
extern "C" void __wrap__ZN6vellum16printer_settings16set_job_defaultsERKNS_13print_optionsE(
    void *printer, const vellum::print_options &options) {
    if (throw_printer_defaults) {
        throw std::runtime_error("injected printer defaults failure");
    }
    __real__ZN6vellum16printer_settings16set_job_defaultsERKNS_13print_optionsE(printer, options);
}
extern "C" int __real__ZNK6vellum12pdf_document5pagesEv(const void *);
extern "C" int __wrap__ZNK6vellum12pdf_document5pagesEv(const void *document) {
    if (fail_document_page_call > 0 && ++document_page_calls == fail_document_page_call) {
        throw std::runtime_error("injected document page-count failure");
    }
    return __real__ZNK6vellum12pdf_document5pagesEv(document);
}
extern "C" void __wrap__ZN6vellum16printer_settings9set_paperEib(void *printer, int format,
                                                                 bool landscape) {
    planning_checkpoint(planning_fault::paper);
    __real__ZN6vellum16printer_settings9set_paperEib(printer, format, landscape);
}
extern "C" std::vector<int>
__real__ZN6vellum18select_print_pagesESt17basic_string_viewIcSt11char_traitsIcEEiNS_11page_subsetE(
    std::string_view, int, vellum::page_subset);
extern "C" std::vector<int>
__wrap__ZN6vellum18select_print_pagesESt17basic_string_viewIcSt11char_traitsIcEEiNS_11page_subsetE(
    std::string_view ranges, int pages, vellum::page_subset subset) {
    planning_checkpoint(planning_fault::selection);
    return __real__ZN6vellum18select_print_pagesESt17basic_string_viewIcSt11char_traitsIcEEiNS_11page_subsetE(
        ranges, pages, subset);
}
extern "C" vellum::print_job
__real__ZN6vellum17compose_print_jobERKSt6vectorINS_11page_extentESaIS1_EERKS0_IiSaIiEERKNS_13print_optionsE(
    const std::vector<vellum::page_extent> &, const std::vector<int> &,
    const vellum::print_options &);
extern "C" vellum::print_job
__wrap__ZN6vellum17compose_print_jobERKSt6vectorINS_11page_extentESaIS1_EERKS0_IiSaIiEERKNS_13print_optionsE(
    const std::vector<vellum::page_extent> &document, const std::vector<int> &pages,
    const vellum::print_options &options) {
    planning_checkpoint(planning_fault::composition);
    return __real__ZN6vellum17compose_print_jobERKSt6vectorINS_11page_extentESaIS1_EERKS0_IiSaIiEERKNS_13print_optionsE(
        document, pages, options);
}

extern "C" bool interpose_timer_start(void *, int) __asm__("_ZN7wxTimer9StartOnceEi");
extern "C" bool interpose_timer_start(void *timer, int milliseconds) {
    if (throw_timer_start) {
        throw_timer_start = false;
        ++thrown_timer_starts;
        throw std::bad_alloc();
    }
    if (reject_timer_start) {
        ++rejected_timer_starts;
        return false;
    }
    return static_cast<wxTimer *>(timer)->Start(milliseconds, true);
}
extern "C" bool __real__ZN7wxTimer5StartEib(void *, int, bool);
extern "C" bool __wrap__ZN7wxTimer5StartEib(void *timer, int milliseconds, bool one_shot) {
    if (throw_timer_start) {
        throw_timer_start = false;
        ++thrown_timer_starts;
        throw std::bad_alloc();
    }
    if (reject_timer_start) {
        ++rejected_timer_starts;
        return false;
    }
    return __real__ZN7wxTimer5StartEib(timer, milliseconds, one_shot);
}

extern "C" bool __wrap__ZNK6vellum16printer_settings8selectedEv(const void *) {
    return printer_selected;
}
extern "C" bool __wrap__ZN6vellum16printer_settings14select_printerEP8wxWindow(void *, wxWindow *) {
    ++selections;
    if (throw_selection) {
        throw std::runtime_error("injected printer selection failure");
    }
    return false;
}
extern "C" vellum::print_options __wrap__ZNK6vellum16printer_settings7measureERKNS_13print_optionsE(
    const void *, const vellum::print_options &requested) {
    if (throw_printer_measurement) {
        throw std::runtime_error("injected printer measurement failure");
    }
    return requested;
}
extern "C" std::string __real__ZNK6vellum16printer_settings4nameB5cxx11Ev(const void *);
extern "C" std::string __wrap__ZNK6vellum16printer_settings4nameB5cxx11Ev(const void *printer) {
    if (throw_printer_name) {
        throw std::runtime_error("injected printer-name failure");
    }
    return __real__ZNK6vellum16printer_settings4nameB5cxx11Ev(printer);
}
extern "C" wxString __real__ZplPKcRK8wxString(const char *, const wxString &);
extern "C" wxString __wrap__ZplPKcRK8wxString(const char *left, const wxString &right) {
    if (throw_status_concat) {
        ++status_concat_failures;
        throw std::bad_alloc();
    }
    return __real__ZplPKcRK8wxString(left, right);
}
extern "C" void __real__ZN16wxStaticTextBase4WrapEi(void *, int);
extern "C" void __wrap__ZN16wxStaticTextBase4WrapEi(void *label, int width) {
    if (throw_status_wrap) {
        throw std::bad_alloc();
    }
    __real__ZN16wxStaticTextBase4WrapEi(label, width);
}
extern "C" bool
__wrap__ZN6vellum16printer_settings6submitEP8wxWindowRNS_12pdf_documentERKNS_9print_jobEi(
    void *, wxWindow *, vellum::pdf_document &, const vellum::print_job &job, int dpi) {
    track_manual_copy = false;
    require(dpi == 150, "unexpected submission resolution");
    submitted.push_back(job);
    if (throw_submission) {
        throw std::runtime_error("injected printer failure");
    }
    const auto index = submitted.size() - 1;
    require(index < outcomes.size(), "unexpected extra printer submission");
    return outcomes[index];
}
extern "C" int __wrap__Z12wxMessageBoxRK8wxStringS1_lP8wxWindowii(const wxString &message,
                                                                  const wxString &caption, long,
                                                                  wxWindow *, int, int) {
    if (caption == "Manual duplex") {
        ++confirmations;
        if (throw_confirmation) {
            throw std::runtime_error("injected duplex confirmation failure");
        }
        return confirmation;
    }
    require(caption == "Printing failed" || caption == "Printer selection failed" ||
                caption == "Preview export failed" || caption == "Preview update failed",
            "unexpected message box");
    ++errors;
    if (throw_error_box) {
        throw_error_box = false;
        throw std::runtime_error("injected error display failure");
    }
    last_error_message = message;
    return wxOK;
}
extern "C" void __wrap__ZN8wxDialog8EndModalEi(void *, int result) {
    require(result == wxID_OK, "unexpected dialog result");
    ++completed;
}
extern "C" bool __real__ZN20wxTopLevelWindowBase6LayoutEv(void *);
extern "C" bool __wrap__ZN20wxTopLevelWindowBase6LayoutEv(void *object) {
    if (throw_dialog_layout) {
        throw std::runtime_error("injected preview layout failure");
    }
    return __real__ZN20wxTopLevelWindowBase6LayoutEv(object);
}
extern "C" void __real__ZN8wxWindow7RefreshEbPK6wxRect(void *, bool, const wxRect *);
extern "C" void __wrap__ZN8wxWindow7RefreshEbPK6wxRect(void *window, bool erase,
                                                       const wxRect *rect) {
    if (preview_refresh_failures_remaining > 0) {
        --preview_refresh_failures_remaining;
        ++preview_refresh_failures;
        throw std::runtime_error("injected preview refresh failure");
    }
    __real__ZN8wxWindow7RefreshEbPK6wxRect(window, erase, rect);
}
extern "C" void __real__ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_(
    void *, wxWindow *, const wxString &, const wxString &, const wxString &, const wxString &,
    long, const wxPoint &, const wxSize &, const wxString &);
extern "C" void __wrap__ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_(
    void *object, wxWindow *parent, const wxString &message, const wxString &directory,
    const wxString &filename, const wxString &wildcard, long style, const wxPoint &position,
    const wxSize &size, const wxString &name) {
    if (throw_file_dialog) {
        throw std::runtime_error("injected file dialog construction failure");
    }
    __real__ZN12wxFileDialogC1EP8wxWindowRK8wxStringS4_S4_S4_lRK7wxPointRK6wxSizeS4_(
        object, parent, message, directory, filename, wildcard, style, position, size, name);
}
extern "C" int __wrap__ZN12wxFileDialog9ShowModalEv(void *) {
    if (throw_file_modal) {
        throw std::runtime_error("injected file dialog display failure");
    }
    if (accept_file_modal) {
        return wxID_OK;
    }
    return wxID_CANCEL;
}
extern "C" wxString __real__ZNK12wxFileDialog7GetPathEv(const void *);
extern "C" wxString __wrap__ZNK12wxFileDialog7GetPathEv(const void *dialog) {
    return accept_file_modal ? accepted_file_path : __real__ZNK12wxFileDialog7GetPathEv(dialog);
}
extern "C" void __real__ZN10wxMemoryDCC1ER8wxBitmap(void *, wxBitmap &);
extern "C" void __real__ZN9wxPaintDCC2EP8wxWindow(void *, wxWindow *);
extern "C" void __real__ZN7wxBrushC1ERK8wxColour12wxBrushStyle(void *, const wxColour &,
                                                               wxBrushStyle);
extern "C" void __wrap__ZN7wxBrushC1ERK8wxColour12wxBrushStyle(void *brush, const wxColour &colour,
                                                               wxBrushStyle style) {
    if (throw_preview_brush) {
        throw_preview_brush = false;
        ++preview_brush_failures;
        throw std::bad_alloc();
    }
    __real__ZN7wxBrushC1ERK8wxColour12wxBrushStyle(brush, colour, style);
}
extern "C" void __real__ZN5wxLog5OnLogEmRK8wxStringRK15wxLogRecordInfo(unsigned long,
                                                                       const wxString &,
                                                                       const wxLogRecordInfo &);
extern "C" wxLogLevel __real__ZN5wxLog17GetComponentLevelERK8wxString(const wxString &);
extern "C" wxLogLevel __wrap__ZN5wxLog17GetComponentLevelERK8wxString(const wxString &component) {
    if (throw_preview_log_filter) {
        throw_preview_log_filter = false;
        ++preview_log_filter_failures;
        throw std::bad_alloc();
    }
    return __real__ZN5wxLog17GetComponentLevelERK8wxString(component);
}
extern "C" void
__wrap__ZN5wxLog5OnLogEmRK8wxStringRK15wxLogRecordInfo(unsigned long level, const wxString &message,
                                                       const wxLogRecordInfo &info) {
    if (throw_preview_log) {
        throw_preview_log = false;
        ++preview_log_failures;
        throw std::bad_alloc();
    }
    __real__ZN5wxLog5OnLogEmRK8wxStringRK15wxLogRecordInfo(level, message, info);
}
extern "C" void __wrap__ZN9wxPaintDCC2EP8wxWindow(void *dc, wxWindow *window) {
    if (throw_preview_dc) {
        throw_preview_dc = false;
        ++preview_dc_failures;
        throw std::bad_alloc();
    }
    __real__ZN9wxPaintDCC2EP8wxWindow(dc, window);
}
extern "C" void __wrap__ZN10wxMemoryDCC1ER8wxBitmap(void *object, wxBitmap &bitmap) {
    if (throw_memory_dc) {
        throw std::runtime_error("injected preview device failure");
    }
    __real__ZN10wxMemoryDCC1ER8wxBitmap(object, bitmap);
}
extern "C" bool __real__ZN8wxBitmap6CreateEiii(void *, int, int, int);
extern "C" bool __wrap__ZN8wxBitmap6CreateEiii(void *object, int width, int height, int depth) {
    if (throw_bitmap_create) {
        throw std::runtime_error("injected preview bitmap failure");
    }
    if (reject_bitmap_create) {
        return false;
    }
    return __real__ZN8wxBitmap6CreateEiii(object, width, height, depth);
}
extern "C" void __real__ZN10wxMemoryDC12SelectObjectER8wxBitmap(void *, wxBitmap &);
extern "C" void __wrap__ZN10wxMemoryDC12SelectObjectER8wxBitmap(void *object, wxBitmap &bitmap) {
    if (throw_dc_deselect) {
        throw std::runtime_error("injected preview device cleanup failure");
    }
    __real__ZN10wxMemoryDC12SelectObjectER8wxBitmap(object, bitmap);
}
extern "C" wxImage __real__ZNK8wxBitmap14ConvertToImageEv(const void *);
extern "C" wxImage __wrap__ZNK8wxBitmap14ConvertToImageEv(const void *object) {
    if (throw_bitmap_conversion)
        throw std::runtime_error("injected preview conversion failure");
    if (reject_bitmap_conversion)
        return {};
    return __real__ZNK8wxBitmap14ConvertToImageEv(object);
}
extern "C" void __real__ZN20wxMemoryOutputStreamC1EPvm(void *, void *, std::size_t);
extern "C" void __wrap__ZN20wxMemoryOutputStreamC1EPvm(void *stream, void *data,
                                                       std::size_t length) {
    if (throw_encoding_stream)
        throw std::runtime_error("injected preview stream failure");
    __real__ZN20wxMemoryOutputStreamC1EPvm(stream, data, length);
}
extern "C" bool __real__ZNK7wxImage8SaveFileER14wxOutputStream12wxBitmapType(const void *,
                                                                             wxOutputStream &,
                                                                             wxBitmapType);
extern "C" bool __wrap__ZNK7wxImage8SaveFileER14wxOutputStream12wxBitmapType(const void *object,
                                                                             wxOutputStream &stream,
                                                                             wxBitmapType type) {
    if (throw_bitmap_save) {
        throw std::runtime_error("injected preview encoding failure");
    }
    if (reject_bitmap_save) {
        return false;
    }
    const bool saved =
        __real__ZNK7wxImage8SaveFileER14wxOutputStream12wxBitmapType(object, stream, type);
    if (mark_stream_failed)
        stream.Reset(wxSTREAM_WRITE_ERROR);
    if (arm_encoded_allocation_failure) {
        arm_encoded_allocation_failure = false;
        track_export_path_failure = true;
    }
    return saved;
}
extern "C" std::size_t __real__ZNK20wxMemoryOutputStream6CopyToEPvm(const void *, void *,
                                                                    std::size_t);
extern "C" std::size_t __wrap__ZNK20wxMemoryOutputStream6CopyToEPvm(const void *stream, void *data,
                                                                    std::size_t length) {
    if (throw_encoded_copy)
        throw std::runtime_error("injected encoded preview copy failure");
    if (reject_encoded_copy)
        return length ? length - 1 : 0;
    const auto copied = __real__ZNK20wxMemoryOutputStream6CopyToEPvm(stream, data, length);
    if (arm_export_path_failure) {
        arm_export_path_failure = false;
        track_export_path_failure = true;
    }
    return copied;
}
extern "C" void
__real__ZN6vellum12atomic_writeERKNSt10filesystem7__cxx114pathESt17basic_string_viewIcSt11char_traitsIcEERKSt8functionIFvNS_12commit_pointEEE(
    const std::filesystem::path &, std::string_view, const vellum::commit_observer &);
extern "C" void
__wrap__ZN6vellum12atomic_writeERKNSt10filesystem7__cxx114pathESt17basic_string_viewIcSt11char_traitsIcEERKSt8functionIFvNS_12commit_pointEEE(
    const std::filesystem::path &path, std::string_view bytes,
    const vellum::commit_observer &observer) {
    if (throw_preview_publish)
        throw std::runtime_error("injected preview publication failure");
    __real__ZN6vellum12atomic_writeERKNSt10filesystem7__cxx114pathESt17basic_string_viewIcSt11char_traitsIcEERKSt8functionIFvNS_12commit_pointEEE(
        path, bytes, observer);
}
extern "C" void __real__ZN6vellum13sheet_painter5paintER4wxDCRKNS_9print_jobEmRK6wxRectib(
    void *, wxDC &, const vellum::print_job &, std::size_t, const wxRect &, int, bool);
extern "C" void __wrap__ZN6vellum13sheet_painter5paintER4wxDCRKNS_9print_jobEmRK6wxRectib(
    void *painter, wxDC &dc, const vellum::print_job &job, std::size_t side, const wxRect &area,
    int dpi, bool preview) {
    if (preview && throw_preview) {
        ++preview_failures;
        throw std::runtime_error("injected preview renderer failure");
    }
    if (preview) {
        ++preview_successes;
    }
    __real__ZN6vellum13sheet_painter5paintER4wxDCRKNS_9print_jobEmRK6wxRectib(
        painter, dc, job, side, area, dpi, preview);
}

class submission_test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(submission_test_app);

int main(int argc, char **argv) {
    if (argc != 2 || !wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        vellum::pdf_document document(argv[1]);
        vellum::printer_settings printer;
        vellum::print_dialog dialog(nullptr, document, printer, 1);
        select(dialog, "print-duplex", 3);
        auto *side_label = wxWindow::FindWindowByLabel(
            wxString::FromUTF8("Side 1/2 — sheet 1, front, copy 1"), &dialog);
        require(side_label != nullptr, "preview side label missing");
        for (const int id : {wxID_FORWARD, wxID_BACKWARD}) {
            const auto before = side_label->GetLabel();
            reset({});
            preview_refresh_failures = 0;
            preview_refresh_failures_remaining = 1;
            wxCommandEvent event(wxEVT_BUTTON, id);
            bool escaped = false;
            try {
                dialog.GetEventHandler()->ProcessEvent(event);
            } catch (const std::exception &) {
                escaped = true;
            }
            preview_refresh_failures_remaining = 0;
            require(!escaped && preview_refresh_failures == 1 && errors == 1 && dialog.valid() &&
                        side_label->GetLabel() == before,
                    "failed side navigation must preserve the previous preview");
            reset({});
            dialog.GetEventHandler()->ProcessEvent(event);
            require(errors == 0 && dialog.valid() && side_label->GetLabel() != before,
                    "side navigation must recover after a refresh failure");
        }
        reset({});
        preview_refresh_failures = 0;
        preview_refresh_failures_remaining = 2;
        wxCommandEvent failed_forward(wxEVT_BUTTON, wxID_FORWARD);
        bool navigation_escaped = false;
        try {
            dialog.GetEventHandler()->ProcessEvent(failed_forward);
        } catch (const std::exception &) {
            navigation_escaped = true;
        }
        preview_refresh_failures_remaining = 0;
        auto *navigation_print = dynamic_cast<wxButton *>(dialog.FindWindowByName("print-submit"));
        require(!navigation_escaped && preview_refresh_failures == 2 && errors == 1 &&
                    !dialog.valid() && navigation_print && !navigation_print->IsEnabled(),
                "failed navigation rollback must invalidate the plan and disable printing");
        dialog.regenerate_preview();
        require(dialog.valid() && navigation_print->IsEnabled() &&
                    side_label->GetLabel().StartsWith("Side 1/2"),
                "preview did not recover after failed navigation rollback");
        for (const int failures_to_inject : {1, 2}) {
            reset({});
            side_format_failures = 0;
            side_format_failures_remaining = failures_to_inject;
            navigation_escaped = false;
            try {
                dialog.GetEventHandler()->ProcessEvent(failed_forward);
            } catch (const std::exception &) {
                navigation_escaped = true;
            }
            side_format_failures_remaining = 0;
            require(!navigation_escaped && side_format_failures == failures_to_inject &&
                        errors == 1 && dialog.valid() == (failures_to_inject == 1) &&
                        navigation_print->IsEnabled() == (failures_to_inject == 1) &&
                        side_label->GetLabel().StartsWith("Side 1/2"),
                    "side-label failure did not preserve navigation safety");
            dialog.regenerate_preview();
            require(dialog.valid() && navigation_print->IsEnabled(),
                    "preview did not recover after side-label failure");
        }

        for (int action = 0; action < 3; ++action) {
            for (const bool reject_fallback : {false, true}) {
                reset({});
                throw_submission = action == 0;
                throw_selection = action == 1;
                throw_file_dialog = action == 2;
                throw_error_box = true;
                watch_error_fallback = true;
                reject_error_fallback = reject_fallback;
                error_fallback_calls = 0;
                error_fallback_matches = false;
                wxCommandEvent event(wxEVT_BUTTON, action == 0   ? wxID_PRINT
                                                   : action == 1 ? wxID_SETUP
                                                                 : wxID_SAVEAS);
                bool escaped = false;
                try {
                    dialog.GetEventHandler()->ProcessEvent(event);
                } catch (const std::exception &) {
                    escaped = true;
                }
                throw_submission = throw_selection = throw_file_dialog = false;
                throw_error_box = watch_error_fallback = reject_error_fallback = false;
                require(!escaped && errors == 1 && error_fallback_calls == 1 &&
                            error_fallback_matches && completed == 0 && dialog.valid(),
                        "print error display failure escaped or invalidated the job");
                dialog.regenerate_preview();
                require(dialog.valid(), "print dialog did not recover after reporting failure");
            }
        }

        reset({false});
        click_print(dialog);
        require(submitted.size() == 1 && confirmations == 0 && completed == 0,
                "front-side failure must stop the job before confirmation");
        require(submitted[0].options.sides == vellum::sheet_subset::fronts,
                "first manual submission must contain fronts");
        for (const auto &side : submitted[0].sides) {
            require(!side.back, "front submission contained a back side");
        }

        reset({true});
        confirmation = wxNO;
        click_print(dialog);
        require(submitted.size() == 1 && confirmations == 1 && completed == 0,
                "declining reinsertion must not submit backs or complete");

        reset({true, false});
        click_print(dialog);
        require(submitted.size() == 2 && confirmations == 1 && completed == 0,
                "back-side failure must leave the dialog open");
        require(submitted[1].options.sides == vellum::sheet_subset::backs,
                "second manual submission must contain backs");
        for (const auto &side : submitted[1].sides) {
            require(side.back, "back submission contained a front side");
        }

        reset({true, true});
        click_print(dialog);
        require(submitted.size() == 2 && confirmations == 1 && completed == 1,
                "successful manual duplex must submit both halves and complete");
        {
            wxFrame sentinel(nullptr, wxID_ANY, "Manual duplex recovery sentinel");
            sentinel.Show();
            wxYield();
            manual_sentinel = &sentinel;
            reset({});
            throw_window_disabler = true;
            click_print(dialog);
            throw_window_disabler = false;
            require(submitted.empty() && errors == 1 && completed == 0 && sentinel.IsEnabled() &&
                        dialog.valid(),
                    "window-disabler failure must leave the job unsubmitted and windows enabled");
            reset({true});
            throw_confirmation = true;
            click_print(dialog);
            throw_confirmation = false;
            require(submitted.size() == 1 && confirmations == 1 && errors == 1 && completed == 0 &&
                        sentinel.IsEnabled() && dialog.valid(),
                    "duplex confirmation failure must stop before backs and restore windows");
            reset({true, true});
            manual_allocations = 0;
            fail_manual_allocation = std::numeric_limits<std::size_t>::max();
            arm_manual_copy = true;
            click_print(dialog);
            arm_manual_copy = track_manual_copy = false;
            const auto copy_allocations = manual_allocations;
            require(copy_allocations > 0 && submitted.size() == 2 && completed == 1 &&
                        sentinel_was_disabled && sentinel.IsEnabled(),
                    "manual duplex allocation baseline was not exercised");
            std::cout << "Manual-duplex copy allocation failures: " << copy_allocations
                      << " cases\n";
            for (std::size_t failure = 0; failure < copy_allocations; ++failure) {
                reset({});
                manual_allocations = 0;
                fail_manual_allocation = failure;
                sentinel_was_disabled = false;
                arm_manual_copy = true;
                click_print(dialog);
                arm_manual_copy = track_manual_copy = false;
                require(manual_allocations == failure + 1 && submitted.empty() && errors == 1 &&
                            completed == 0 && sentinel_was_disabled && sentinel.IsEnabled() &&
                            dialog.valid(),
                        "manual duplex copy failure did not restore the dialog and windows");
                reset({true, true});
                click_print(dialog);
                require(submitted.size() == 2 && completed == 1 && sentinel.IsEnabled(),
                        "manual duplex did not recover after copy failure");
            }
            manual_sentinel = nullptr;
        }

        select(dialog, "print-selection", 1);
        reset({true, true});
        click_print(dialog);
        require(submitted.size() == 2 && submitted[1].sides.size() == 1 &&
                    submitted[1].sides[0].back && submitted[1].sides[0].placements.empty() &&
                    completed == 1,
                "one-page manual duplex must still submit its blank back side");
        select(dialog, "print-selection", 0);

        select(dialog, "print-sides", 1);
        reset({true});
        click_print(dialog);
        require(submitted.size() == 1 && confirmations == 0 && completed == 1 &&
                    submitted[0].options.sides == vellum::sheet_subset::fronts,
                "fronts-only manual duplex must submit once without reinsertion");
        select(dialog, "print-sides", 2);
        reset({true});
        click_print(dialog);
        require(submitted.size() == 1 && confirmations == 0 && completed == 1 &&
                    submitted[0].options.sides == vellum::sheet_subset::backs,
                "backs-only manual duplex must submit once without reinsertion");
        select(dialog, "print-sides", 0);
        select(dialog, "print-duplex", 0);
        reset({false});
        click_print(dialog);
        require(submitted.size() == 1 && confirmations == 0 && completed == 0,
                "ordinary printer failure must leave the dialog open");
        reset({true});
        click_print(dialog);
        require(submitted.size() == 1 && confirmations == 0 && completed == 1,
                "ordinary printer success must complete once");
        reset({});
        throw_submission = true;
        click_print(dialog);
        require(submitted.size() == 1 && errors == 1 && completed == 0,
                "printer exception must be reported without completing");

        numeric_read_count = 0;
        fail_numeric_read = 0;
        track_numeric_reads = true;
        dialog.regenerate_preview();
        track_numeric_reads = false;
        const int preview_reads = numeric_read_count;
        require(dialog.valid() && preview_reads > 0, "preview numeric baseline was not observed");
        reset({});
        numeric_read_count = 0;
        fail_numeric_read = preview_reads + 1;
        track_numeric_reads = true;
        click_print(dialog);
        track_numeric_reads = false;
        require(numeric_read_count == fail_numeric_read && submitted.empty() && errors == 1 &&
                    completed == 0 && dialog.valid(),
                "resolution conversion failure must not submit a job");

        auto *resolution = dynamic_cast<wxChoice *>(dialog.FindWindowByName("print-resolution"));
        require(resolution != nullptr, "resolution choice missing");
        resolution->SetSelection(static_cast<int>(resolution->Append("not a number")));
        reset({});
        click_print(dialog);
        require(submitted.empty() && errors == 1 && completed == 0,
                "invalid resolution must be reported before submitting");
        resolution->SetSelection(1);
        printer_selected = false;
        selections = 0;
        reset({});
        click_print(dialog);
        require(selections == 1 && submitted.empty() && errors == 0 && completed == 0 &&
                    dialog.valid(),
                "canceling printer selection must leave the valid job open");
        selections = 0;
        throw_selection = true;
        reset({});
        click_print(dialog);
        require(selections == 1 && submitted.empty() && errors == 1 && completed == 0,
                "printer selection exception must be reported without submitting");
        selections = 0;
        reset({});
        wxCommandEvent properties(wxEVT_BUTTON, wxID_SETUP);
        require(dialog.GetEventHandler()->ProcessEvent(properties),
                "printer properties event was not handled");
        require(selections == 1 && errors == 1 && dialog.valid(),
                "printer properties failure must be reported and leave the dialog usable");
        throw_selection = false;
        throw_printer_defaults = true;
        selections = 0;
        reset({});
        require(dialog.GetEventHandler()->ProcessEvent(properties),
                "printer properties event was not handled");
        throw_printer_defaults = false;
        require(selections == 0 && errors == 1 && dialog.valid(),
                "printer defaults failure must be reported before opening selection");
        printer_selected = true;
        auto *submit_button = dynamic_cast<wxButton *>(dialog.FindWindowByName("print-submit"));
        require(submit_button != nullptr, "print submit button missing");
        for (int failure : {1, 2}) {
            document_page_calls = 0;
            fail_document_page_call = failure;
            dialog.regenerate_preview();
            fail_document_page_call = 0;
            require(document_page_calls == failure && !dialog.valid() &&
                        !submit_button->IsEnabled(),
                    "document page-count failure must invalidate the preview");
            dialog.regenerate_preview();
            require(dialog.valid() && submit_button->IsEnabled(),
                    "preview must recover after a document page-count failure");
        }
        for (const auto failure :
             {planning_fault::paper, planning_fault::selection, planning_fault::composition}) {
            planning_failures = 0;
            injected_planning_fault = failure;
            dialog.regenerate_preview();
            injected_planning_fault = planning_fault::none;
            require(planning_failures == 1 && !dialog.valid() && !submit_button->IsEnabled(),
                    "print planning failure must invalidate the preview");
            dialog.regenerate_preview();
            require(dialog.valid() && submit_button->IsEnabled(),
                    "preview must recover after print planning failure");
        }
        for (const int layout : {0, 1, 3}) {
            select(dialog, "print-layout", layout);
            numeric_read_count = 0;
            fail_numeric_read = 0;
            track_numeric_reads = true;
            dialog.regenerate_preview();
            track_numeric_reads = false;
            const int reads = numeric_read_count;
            require(dialog.valid() && reads >= 3, "numeric reads were not observed");
            for (int failure = 1; failure <= reads; ++failure) {
                numeric_read_count = 0;
                fail_numeric_read = failure;
                track_numeric_reads = true;
                dialog.regenerate_preview();
                track_numeric_reads = false;
                require(numeric_read_count == failure && !dialog.valid() &&
                            !submit_button->IsEnabled(),
                        "numeric read failure must invalidate the preview");
                dialog.regenerate_preview();
                require(dialog.valid() && submit_button->IsEnabled(),
                        "preview must recover after numeric read failure");
            }
        }
        select(dialog, "print-layout", 0);
        select(dialog, "print-scaling", 3);
        for (int index = 1; index <= 4; ++index) {
            select(dialog, "print-layout", index == 4 ? 3 : 0);
            for (const double value :
                 {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                  -std::numeric_limits<double>::infinity(), -1.0, 10001.0}) {
                forced_double_reads = 0;
                forced_double_index = index;
                forced_double_value = value;
                dialog.regenerate_preview();
                forced_double_index = 0;
                require(forced_double_reads == index && !dialog.valid() &&
                            !submit_button->IsEnabled(),
                        "non-finite or out-of-range decimal must invalidate the print plan");
                dialog.regenerate_preview();
                require(dialog.valid() && submit_button->IsEnabled(),
                        "print plan did not recover after an invalid decimal");
            }
        }
        select(dialog, "print-layout", 0);
        select(dialog, "print-scaling", 0);
        auto *ranges =
            dynamic_cast<wxTextCtrl *>(wxWindow::FindWindowByName("print-ranges", &dialog));
        require(ranges != nullptr, "custom range control missing");
        ranges->ChangeValue("1-2");
        select(dialog, "print-selection", 2);
        fail_range_conversion = true;
        dialog.regenerate_preview();
        fail_range_conversion = false;
        require(range_conversion_failures == 1 && !dialog.valid() && !submit_button->IsEnabled(),
                "page-range conversion failure must invalidate the preview");
        dialog.regenerate_preview();
        require(dialog.valid() && submit_button->IsEnabled(),
                "preview must recover after page-range conversion failure");
        select(dialog, "print-selection", 0);
        for (int failure = 0; failure < 4; ++failure) {
            throw_printer_name = failure == 0;
            throw_status_concat = failure == 1;
            throw_status_wrap = failure == 2;
            throw_printer_measurement = failure == 3;
            dialog.regenerate_preview();
            require(!submit_button->IsEnabled() && !dialog.valid(),
                    "late preview failure must invalidate the job and disable submission");
            if (throw_printer_measurement) {
                reset({});
                click_print(dialog);
                require(submitted.empty() && completed == 0 && !dialog.valid(),
                        "failed printer measurement must not submit a stale job");
            }
            bool export_rejected = false;
            try {
                dialog.export_preview("/proc/vellum-invalid-preview.png");
            } catch (const std::runtime_error &error) {
                export_rejected = std::string(error.what()) == "print settings are not valid";
            }
            require(export_rejected, "invalid preview must reject image export");
            throw_printer_name = throw_status_concat = throw_status_wrap = false;
            throw_printer_measurement = false;
            dialog.regenerate_preview();
            require(dialog.valid() && submit_button->IsEnabled(),
                    "preview must recover after a transient printer-status failure");
        }
        wxTimer preview_timer(&dialog);
        wxTimerEvent timer_event(preview_timer);
        throw_dialog_layout = true;
        const bool timer_handled = dialog.GetEventHandler()->ProcessEvent(timer_event);
        throw_dialog_layout = false;
        require(timer_handled && !dialog.valid() && !submit_button->IsEnabled(),
                "timer layout failure must invalidate the preview without escaping");
        dialog.regenerate_preview();
        require(dialog.valid() && submit_button->IsEnabled(),
                "preview must recover after a transient timer layout failure");
        reset({});
        preview_refresh_failures = 0;
        preview_refresh_failures_remaining = 2;
        bool refresh_escaped = false;
        try {
            dialog.GetEventHandler()->ProcessEvent(timer_event);
        } catch (const std::exception &) {
            refresh_escaped = true;
        }
        preview_refresh_failures_remaining = 0;
        if (refresh_escaped || preview_refresh_failures != 2 || errors != 1 || dialog.valid() ||
            submit_button->IsEnabled())
            std::cerr << "refresh escaped=" << refresh_escaped
                      << " failures=" << preview_refresh_failures << " errors=" << errors
                      << " valid=" << dialog.valid() << " enabled=" << submit_button->IsEnabled()
                      << '\n';
        require(!refresh_escaped && preview_refresh_failures == 2 && errors == 1 &&
                    !dialog.valid() && !submit_button->IsEnabled(),
                "preview and recovery refresh failures must not escape or allow stale printing");
        dialog.regenerate_preview();
        require(dialog.valid() && submit_button->IsEnabled(),
                "preview did not recover after repeated refresh failures");
        auto *reverse = dynamic_cast<wxCheckBox *>(dialog.FindWindowByName("print-reverse"));
        require(reverse != nullptr, "reverse-pages control missing");
        reverse->SetValue(true);
        wxCommandEvent reverse_change(wxEVT_CHECKBOX, reverse->GetId());
        reverse_change.SetEventObject(reverse);
        reject_timer_start = true;
        const bool change_handled = dialog.GetEventHandler()->ProcessEvent(reverse_change);
        reject_timer_start = false;
        require(rejected_timer_starts == 1, "timer-start failure was not injected");
        require(change_handled && dialog.valid() && submit_button->IsEnabled() &&
                    dialog.job().options.reverse,
                "timer start failure must still update the preview and enable printing");
        reverse->SetValue(false);
        reject_timer_start = true;
        throw_dialog_layout = true;
        const bool fallback_handled = dialog.GetEventHandler()->ProcessEvent(reverse_change);
        reject_timer_start = false;
        throw_dialog_layout = false;
        require(fallback_handled && rejected_timer_starts == 2 && !dialog.valid() &&
                    !submit_button->IsEnabled(),
                "failed timer and failed fallback must leave printing disabled");
        dialog.regenerate_preview();
        require(dialog.valid() && submit_button->IsEnabled() && !dialog.job().options.reverse,
                "preview must recover after timer and fallback failures");
        reverse->SetValue(true);
        throw_timer_start = true;
        bool timer_exception_escaped = false;
        try {
            dialog.GetEventHandler()->ProcessEvent(reverse_change);
        } catch (const std::bad_alloc &) {
            timer_exception_escaped = true;
        }
        throw_timer_start = false;
        if (thrown_timer_starts != 1 || timer_exception_escaped || !dialog.valid() ||
            !submit_button->IsEnabled() || !dialog.job().options.reverse) {
            std::cerr << "timer failures=" << thrown_timer_starts
                      << " escaped=" << timer_exception_escaped << " valid=" << dialog.valid()
                      << " enabled=" << submit_button->IsEnabled()
                      << " reverse=" << dialog.job().options.reverse << '\n';
        }
        require(thrown_timer_starts == 1 && !timer_exception_escaped && dialog.valid() &&
                    submit_button->IsEnabled() && dialog.job().options.reverse,
                "timer start exception must recover without escaping or leaving stale settings");
        reverse->SetValue(false);
        throw_timer_start = true;
        throw_dialog_layout = true;
        timer_exception_escaped = false;
        try {
            dialog.GetEventHandler()->ProcessEvent(reverse_change);
        } catch (const std::exception &) {
            timer_exception_escaped = true;
        }
        throw_timer_start = throw_dialog_layout = false;
        require(thrown_timer_starts == 2 && !timer_exception_escaped && !dialog.valid() &&
                    !submit_button->IsEnabled(),
                "failed timer and fallback must leave printing disabled without escaping");
        dialog.regenerate_preview();
        require(dialog.valid() && submit_button->IsEnabled() && !dialog.job().options.reverse,
                "preview must recover after timer and fallback exceptions");
        for (int failure = 0; failure < 2; ++failure) {
            reset({});
            throw_file_dialog = failure == 0;
            throw_file_modal = failure == 1;
            wxCommandEvent save(wxEVT_BUTTON, wxID_SAVEAS);
            require(dialog.GetEventHandler()->ProcessEvent(save), "save-preview event not handled");
            require(errors == 1 && dialog.valid(),
                    "file-dialog failure must be reported and leave the job usable");
        }
        throw_file_dialog = throw_file_modal = false;
        reset({});
        accept_file_modal = true;
        fail_range_conversion = true;
        range_conversion_failures = 0;
        wxCommandEvent save_with_invalid_path(wxEVT_BUTTON, wxID_SAVEAS);
        const bool save_handled = dialog.GetEventHandler()->ProcessEvent(save_with_invalid_path);
        fail_range_conversion = false;
        accept_file_modal = false;
        require(save_handled && range_conversion_failures == 1 && errors == 1 && dialog.valid(),
                "preview path conversion failure must be reported without invalidating the job");
        auto preview_path =
            (std::filesystem::path(argv[1]).parent_path() / "preview-event-XXXXXX.png").string();
        const int preview_fd = ::mkstemps(preview_path.data(), 4);
        require(preview_fd >= 0, "cannot create preview test file");
        ::close(preview_fd);
        struct remove_preview {
            std::filesystem::path path;
            ~remove_preview() {
                std::error_code ignored;
                std::filesystem::remove(path, ignored);
            }
        } preview_cleanup{preview_path};
        accepted_file_path = wxString::FromUTF8(preview_path);
        accept_file_modal = true;
        reset({});
        wxCommandEvent save_preview(wxEVT_BUTTON, wxID_SAVEAS);
        const bool saved = dialog.GetEventHandler()->ProcessEvent(save_preview);
        accept_file_modal = false;
        if (!saved || errors != 0 || !dialog.valid()) {
            std::cerr << "Preview save error: " << last_error_message.ToStdString() << '\n';
            throw std::runtime_error("preview save event failed");
        }
        wxImage saved_image;
        {
            wxLogNull silence_expected_error;
            saved_image.LoadFile(wxString::FromUTF8(preview_path), wxBITMAP_TYPE_PNG);
        }
        require(saved_image.IsOk(), "preview save did not produce a PNG");
        require(saved_image.GetRed(saved_image.GetWidth() / 2, saved_image.GetHeight() / 2) > 240 &&
                    saved_image.GetGreen(saved_image.GetWidth() / 2, saved_image.GetHeight() / 2) <
                        15,
                "saved preview does not contain the expected page image");
        const auto previous_preview = vellum::read_file(preview_path);
        const std::filesystem::path preview_destination(preview_path);
        for (const bool encoded_buffer : {false, true}) {
            arm_export_path_failure = !encoded_buffer;
            arm_encoded_allocation_failure = encoded_buffer;
            export_path_failures = 0;
            bool allocation_rejected = false;
            try {
                dialog.export_preview(preview_destination);
            } catch (const std::bad_alloc &) {
                allocation_rejected = true;
            }
            arm_export_path_failure = arm_encoded_allocation_failure = track_export_path_failure =
                false;
            require(allocation_rejected && export_path_failures == 1 && dialog.valid() &&
                        vellum::read_file(preview_path) == previous_preview,
                    "preview allocation failure must preserve the existing image");
            dialog.export_preview(preview_destination);
            require(vellum::read_file(preview_path) == previous_preview,
                    "preview export did not recover after allocation failure");
        }
        for (int failure = 0; failure < 13; ++failure) {
            throw_bitmap_create = failure == 0;
            reject_bitmap_create = failure == 1;
            throw_memory_dc = failure == 2;
            throw_dc_deselect = failure == 3;
            throw_bitmap_save = failure == 4;
            reject_bitmap_save = failure == 5;
            throw_bitmap_conversion = failure == 6;
            reject_bitmap_conversion = failure == 7;
            throw_encoding_stream = failure == 8;
            throw_encoded_copy = failure == 9;
            reject_encoded_copy = failure == 10;
            throw_preview_publish = failure == 11;
            mark_stream_failed = failure == 12;
            accept_file_modal = true;
            reset({});
            wxCommandEvent failed_save(wxEVT_BUTTON, wxID_SAVEAS);
            const bool handled = dialog.GetEventHandler()->ProcessEvent(failed_save);
            accept_file_modal = false;
            throw_bitmap_create = reject_bitmap_create = throw_memory_dc = false;
            throw_dc_deselect = throw_bitmap_save = reject_bitmap_save = false;
            throw_bitmap_conversion = reject_bitmap_conversion = throw_encoding_stream = false;
            throw_encoded_copy = reject_encoded_copy = throw_preview_publish = mark_stream_failed =
                false;
            require(handled && errors == 1 && dialog.valid() &&
                        vellum::read_file(preview_path) == previous_preview,
                    "preview export failure must be reported without invalidating the job");
        }
        const auto document_before_denial = document.snapshot();
        reset({});
        accept_file_modal = true;
        deny_next_export_permission = true;
        wxCommandEvent denied_save(wxEVT_BUTTON, wxID_SAVEAS);
        const bool denial_handled = dialog.GetEventHandler()->ProcessEvent(denied_save);
        accept_file_modal = false;
        require(denial_handled && export_permission_denials == 1 && errors == 1 && dialog.valid() &&
                    last_error_message.Contains("PDF permissions prohibit printing") &&
                    document.snapshot() == document_before_denial &&
                    vellum::read_file(preview_path) == previous_preview,
                "denied preview export must preserve the document and existing output");
        reset({});
        accept_file_modal = true;
        wxCommandEvent retry_save(wxEVT_BUTTON, wxID_SAVEAS);
        const bool retried = dialog.GetEventHandler()->ProcessEvent(retry_save);
        accept_file_modal = false;
        require(retried && errors == 0 && dialog.valid() &&
                    vellum::read_file(preview_path) == previous_preview,
                "preview export did not recover after a permission denial");
        for (int failure = 0; failure < 4; ++failure) {
            throw_memory_dc = failure == 0;
            throw_bitmap_save = failure == 1;
            throw_bitmap_create = failure == 2;
            throw_dc_deselect = failure == 3;
            bool rejected = false;
            try {
                dialog.export_preview("/proc/vellum-failed-preview.png");
            } catch (const std::runtime_error &error) {
                const char *messages[] = {
                    "injected preview device failure", "injected preview encoding failure",
                    "injected preview bitmap failure", "injected preview device cleanup failure"};
                rejected = std::string(error.what()) == messages[failure];
            }
            throw_memory_dc = throw_bitmap_save = throw_bitmap_create = throw_dc_deselect = false;
            require(rejected && dialog.valid(),
                    "preview export failure must propagate without invalidating the job");
        }
        reject_bitmap_create = true;
        bool bitmap_rejected = false;
        try {
            dialog.export_preview("/proc/vellum-failed-preview.png");
        } catch (const std::runtime_error &error) {
            bitmap_rejected = std::string(error.what()) == "cannot allocate preview image";
        }
        reject_bitmap_create = false;
        require(bitmap_rejected && dialog.valid(),
                "bitmap allocation failure must be reported without invalidating the job");

        wxWindow *preview = nullptr;
        for (auto *child : dialog.GetChildren()) {
            if (dynamic_cast<wxPanel *>(child) && !dynamic_cast<wxScrolledWindow *>(child)) {
                require(preview == nullptr, "multiple preview panels found");
                preview = child;
            }
        }
        require(preview != nullptr, "preview panel missing");
        dialog.Show();
        wxYield();
        throw_preview = true;
        preview->Refresh();
        preview->Update();
        wxYield();
        throw_preview = false;
        require(preview_failures > 0 && dialog.valid(),
                "preview renderer failure must be caught without invalidating the job");
        const int successful_before_retry = preview_successes;
        preview->Refresh();
        preview->Update();
        wxYield();
        require(preview_successes > successful_before_retry,
                "preview rendering must recover after a transient failure");

        bool paint_exception_escaped = false;
        bool preview_failure_logged = false;
        wxLogBuffer preview_log;
        {
            struct restore_log {
                wxLog *previous;
                ~restore_log() { wxLog::SetActiveTarget(previous); }
            } restore{wxLog::SetActiveTarget(&preview_log)};
            throw_preview_dc = true;
            preview->Refresh();
            try {
                preview->Update();
                wxYield();
            } catch (const std::bad_alloc &) {
                paint_exception_escaped = true;
            }
            throw_preview_dc = false;
            preview_failure_logged = preview_log.GetBuffer().Contains("Preview failed");
        }
        if (preview_dc_failures != 1 || paint_exception_escaped || !dialog.valid()) {
            std::cerr << "preview device failures=" << preview_dc_failures
                      << " escaped=" << paint_exception_escaped << " valid=" << dialog.valid()
                      << '\n';
        }
        require(preview_dc_failures == 1 && !paint_exception_escaped && dialog.valid() &&
                    preview_failure_logged,
                "preview device construction failure must not escape the paint event");
        {
            wxLogBuffer ignored_log;
            struct restore_log {
                wxLog *previous;
                ~restore_log() { wxLog::SetActiveTarget(previous); }
            } restore{wxLog::SetActiveTarget(&ignored_log)};
            throw_preview_dc = true;
            throw_preview_log = true;
            preview->Refresh();
            try {
                preview->Update();
                wxYield();
            } catch (const std::exception &) {
                paint_exception_escaped = true;
            }
            throw_preview_dc = throw_preview_log = false;
        }
        require(preview_dc_failures == 2 && preview_log_failures == 1 && !paint_exception_escaped &&
                    dialog.valid(),
                "preview reporting failure must not escape the paint event");
        {
            wxLogNull logging_disabled;
            throw_preview_dc = true;
            preview->Refresh();
            try {
                preview->Update();
                wxYield();
            } catch (const std::exception &) {
                paint_exception_escaped = true;
            }
            throw_preview_dc = false;
        }
        require(preview_dc_failures == 3 && !paint_exception_escaped && dialog.valid(),
                "preview failure must not escape when logging is disabled");
        bool filtered_log_empty = false;
        {
            wxLogBuffer filtered_log;
            struct restore_logging {
                wxLog *previous;
                wxLogLevel level;
                ~restore_logging() {
                    wxLog::SetLogLevel(level);
                    wxLog::SetActiveTarget(previous);
                }
            } restore{wxLog::SetActiveTarget(&filtered_log), wxLog::GetLogLevel()};
            wxLog::SetLogLevel(wxLOG_FatalError);
            throw_preview_dc = true;
            preview->Refresh();
            try {
                preview->Update();
                wxYield();
            } catch (const std::exception &) {
                paint_exception_escaped = true;
            }
            throw_preview_dc = false;
            filtered_log_empty = filtered_log.GetBuffer().empty();
        }
        require(preview_dc_failures == 4 && !paint_exception_escaped && dialog.valid() &&
                    filtered_log_empty,
                "preview failure must respect the configured log level");
        {
            wxLogBuffer ignored_log;
            struct restore_log {
                wxLog *previous;
                ~restore_log() { wxLog::SetActiveTarget(previous); }
            } restore{wxLog::SetActiveTarget(&ignored_log)};
            throw_preview_dc = true;
            throw_preview_log_filter = true;
            preview->Refresh();
            try {
                preview->Update();
                wxYield();
            } catch (const std::exception &) {
                paint_exception_escaped = true;
            }
            throw_preview_dc = throw_preview_log_filter = false;
        }
        require(preview_dc_failures == 5 && preview_log_filter_failures == 1 &&
                    !paint_exception_escaped && dialog.valid(),
                "preview log-filter failure must not escape the paint event");
        {
            wxLogNull logging_disabled;
            throw_preview_brush = true;
            preview->Refresh();
            try {
                preview->Update();
                wxYield();
            } catch (const std::exception &) {
                paint_exception_escaped = true;
            }
            throw_preview_brush = false;
        }
        require(preview_brush_failures == 1 && !paint_exception_escaped && dialog.valid(),
                "preview background allocation failure must not escape the paint event");
        wxLogBuffer fallback_log;
        bool fallback_logged = false;
        const int failures_before_fallback = preview_failures;
        status_concat_failures = 0;
        {
            struct restore_log {
                wxLog *previous;
                ~restore_log() { wxLog::SetActiveTarget(previous); }
            } restore{wxLog::SetActiveTarget(&fallback_log)};
            throw_preview = true;
            throw_status_concat = true;
            preview->Refresh();
            try {
                preview->Update();
                wxYield();
            } catch (const std::exception &) {
                paint_exception_escaped = true;
            }
            throw_preview = throw_status_concat = false;
            fallback_logged = fallback_log.GetBuffer().Contains("Preview failed");
        }
        require(preview_failures == failures_before_fallback + 1 && status_concat_failures == 1 &&
                    !paint_exception_escaped && fallback_logged && dialog.valid(),
                "preview error-display failure must fall back to logging");
        const int successful_after_device_failure = preview_successes;
        preview->Refresh();
        preview->Update();
        wxYield();
        require(preview_successes > successful_after_device_failure,
                "preview must recover after a device construction failure");

        preview->Bind(wxEVT_PAINT, [](wxPaintEvent &event) {
            ++small_paint_events;
            event.Skip();
        });
        preview->SetMinSize(wxSize(1, 1));
        preview->SetSize(20, 20);
        require(preview->GetClientSize().x <= 24 && preview->GetClientSize().y <= 24,
                "could not create a small preview for the drawing-boundary test");
        const int paints_before_small_refresh = small_paint_events;
        const int renders_before_small_refresh = preview_successes;
        preview->Refresh();
        preview->Update();
        wxYield();
        require(small_paint_events > paints_before_small_refresh &&
                    preview_successes == renders_before_small_refresh,
                "small preview must receive paint events without invoking the renderer");
    } catch (const std::exception &error) {
        arm_manual_copy = track_manual_copy = false;
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
