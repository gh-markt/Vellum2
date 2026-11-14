#include "vellum/sheet_painter.hh"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <wx/app.h>
#include <wx/dcmemory.h>
#include <wx/dcps.h>
#include <wx/print.h>
#include <wx/printdlg.h>

namespace {
enum class fault {
    none,
    image,
    image_message_oom,
    image_throw,
    rotate,
    rotate_message_oom,
    rotate_throw,
    grayscale,
    grayscale_message_oom,
    grayscale_throw,
    bitmap,
    bitmap_message_oom,
    bitmap_throw,
    memory_dc_throw,
    blit,
    blit_message_oom,
    render,
    dimensions
};
fault injected = fault::none;
int triggered = 0;
int device_operation = 0;
int failing_operation = 0;
bool track_device_operations = false;
thread_local bool fail_next_allocation = false;
void device_checkpoint() {
    if (track_device_operations && ++device_operation == failing_operation) {
        throw std::runtime_error("injected printer device failure");
    }
}
bool hit(fault expected) {
    if (injected != expected) {
        return false;
    }
    ++triggered;
    return true;
}
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
const char *expected_message(fault current) {
    switch (current) {
    case fault::image:
        return "cannot allocate print tile";
    case fault::rotate:
    case fault::grayscale:
        return "cannot transform print tile";
    case fault::bitmap:
        return "cannot allocate print bitmap";
    case fault::blit:
        return "print device cannot draw bitmap";
    case fault::render:
        return "injected page-render failure";
    case fault::dimensions:
        return "injected page-dimensions failure";
    case fault::memory_dc_throw:
        return "injected tile memory-device failure";
    case fault::image_throw:
    case fault::rotate_throw:
    case fault::grayscale_throw:
    case fault::bitmap_throw:
        return "injected image operation failure";
    case fault::image_message_oom:
    case fault::rotate_message_oom:
    case fault::grayscale_message_oom:
    case fault::bitmap_message_oom:
    case fault::blit_message_oom:
        return "out of memory";
    case fault::none:
        break;
    }
    throw std::logic_error("fault has no expected message");
}
} // namespace

void *operator new(std::size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc();
    }
    if (void *value = std::malloc(size ? size : 1)) {
        return value;
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
    std::free(value);
}
void operator delete(void *value, std::size_t) noexcept {
    std::free(value);
}
void operator delete[](void *value, std::size_t) noexcept {
    std::free(value);
}

extern "C" bool __real__ZN7wxImage6CreateEiib(void *, int, int, bool);
extern "C" bool __wrap__ZN7wxImage6CreateEiib(void *image, int width, int height, bool clear) {
    if (hit(fault::image_message_oom)) {
        fail_next_allocation = true;
        return false;
    }
    if (hit(fault::image_throw)) {
        throw std::runtime_error("injected image operation failure");
    }
    return hit(fault::image) ? false : __real__ZN7wxImage6CreateEiib(image, width, height, clear);
}
extern "C" wxImage __real__ZNK7wxImage8Rotate90Eb(const void *, bool);
extern "C" wxImage __wrap__ZNK7wxImage8Rotate90Eb(const void *image, bool clockwise) {
    if (hit(fault::rotate_message_oom)) {
        fail_next_allocation = true;
        return wxImage();
    }
    if (hit(fault::rotate_throw)) {
        throw std::runtime_error("injected image operation failure");
    }
    return hit(fault::rotate) ? wxImage() : __real__ZNK7wxImage8Rotate90Eb(image, clockwise);
}
extern "C" wxImage __real__ZNK7wxImage18ConvertToGreyscaleEv(const void *);
extern "C" wxImage __wrap__ZNK7wxImage18ConvertToGreyscaleEv(const void *image) {
    if (hit(fault::grayscale_message_oom)) {
        fail_next_allocation = true;
        return wxImage();
    }
    if (hit(fault::grayscale_throw)) {
        throw std::runtime_error("injected image operation failure");
    }
    return hit(fault::grayscale) ? wxImage() : __real__ZNK7wxImage18ConvertToGreyscaleEv(image);
}
extern "C" void __real__ZN8wxBitmapC1ERK7wxImageid(void *, const wxImage &, int, double);
extern "C" void __wrap__ZN8wxBitmapC1ERK7wxImageid(void *bitmap, const wxImage &image, int depth,
                                                   double scale) {
    if (hit(fault::bitmap_throw)) {
        throw std::runtime_error("injected image operation failure");
    }
    __real__ZN8wxBitmapC1ERK7wxImageid(bitmap, image, depth, scale);
    if (hit(fault::bitmap_message_oom)) {
        *static_cast<wxBitmap *>(bitmap) = wxNullBitmap;
        fail_next_allocation = true;
    }
    if (hit(fault::bitmap)) {
        *static_cast<wxBitmap *>(bitmap) = wxNullBitmap;
    }
}
extern "C" vellum::page_extent __real__ZNK6vellum12pdf_document10dimensionsEi(const void *, int);
extern "C" void __real__ZN10wxMemoryDCC1ER8wxBitmap(void *, wxBitmap &);
extern "C" void __wrap__ZN10wxMemoryDCC1ER8wxBitmap(void *dc, wxBitmap &bitmap) {
    if (hit(fault::memory_dc_throw)) {
        throw std::runtime_error("injected tile memory-device failure");
    }
    __real__ZN10wxMemoryDCC1ER8wxBitmap(dc, bitmap);
}
extern "C" vellum::page_extent __wrap__ZNK6vellum12pdf_document10dimensionsEi(const void *document,
                                                                              int page) {
    if (hit(fault::dimensions)) {
        throw std::runtime_error("injected page-dimensions failure");
    }
    return __real__ZNK6vellum12pdf_document10dimensionsEi(document, page);
}
extern "C" vellum::page_image
__real__ZNK6vellum12pdf_document19render_print_regionEiNS_14page_rectangleEfi(
    const void *, int, vellum::page_rectangle, float, int);
extern "C" vellum::page_image
__wrap__ZNK6vellum12pdf_document19render_print_regionEiNS_14page_rectangleEfi(
    const void *document, int page, vellum::page_rectangle region, float scale, int dpi) {
    if (hit(fault::render)) {
        throw std::runtime_error("injected page-render failure");
    }
    return __real__ZNK6vellum12pdf_document19render_print_regionEiNS_14page_rectangleEfi(
        document, page, region, scale, dpi);
}

class fault_printer_dc : public wxPostScriptDCImpl {
  public:
    fault_printer_dc(wxPrinterDC *owner, const wxPrintData &data)
        : wxPostScriptDCImpl(owner, data) {}
    void SetBrush(const wxBrush &brush) override {
        device_checkpoint();
        wxPostScriptDCImpl::SetBrush(brush);
    }
    void SetPen(const wxPen &pen) override {
        device_checkpoint();
        wxPostScriptDCImpl::SetPen(pen);
    }
    void SetFont(const wxFont &font) override {
        device_checkpoint();
        wxPostScriptDCImpl::SetFont(font);
    }
    void SetTextForeground(const wxColour &colour) override {
        device_checkpoint();
        wxPostScriptDCImpl::SetTextForeground(colour);
    }
    void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height) override {
        device_checkpoint();
        wxPostScriptDCImpl::DoDrawRectangle(x, y, width, height);
    }
    void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2) override {
        device_checkpoint();
        wxPostScriptDCImpl::DoDrawLine(x1, y1, x2, y2);
    }
    void DoDrawText(const wxString &text, wxCoord x, wxCoord y) override {
        device_checkpoint();
        wxPostScriptDCImpl::DoDrawText(text, x, y);
    }
    void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height) override {
        device_checkpoint();
        wxPostScriptDCImpl::DoSetClippingRegion(x, y, width, height);
    }
    bool DoStretchBlit(int x, int y, int width, int height, wxDC *source, int source_x,
                       int source_y, int source_width, int source_height,
                       wxRasterOperationMode operation, bool use_mask, int mask_x,
                       int mask_y) override {
        device_checkpoint();
        if (hit(fault::blit_message_oom)) {
            fail_next_allocation = true;
            return false;
        }
        if (hit(fault::blit)) {
            return false;
        }
        return wxPostScriptDCImpl::DoStretchBlit(x, y, width, height, source, source_x, source_y,
                                                 source_width, source_height, operation, use_mask,
                                                 mask_x, mask_y);
    }
};
class fault_print_factory : public wxNativePrintFactory {
  public:
    wxDCImpl *CreatePrinterDCImpl(wxPrinterDC *owner, const wxPrintData &data) override {
        return new fault_printer_dc(owner, data);
    }
};
class painter_fault_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(painter_fault_app);

int main(int argc, char **argv) {
    if (argc != 2 || !wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        wxPrintFactory::SetPrintFactory(new fault_print_factory);
        struct reset_factory {
            ~reset_factory() { wxPrintFactory::SetPrintFactory(new wxNativePrintFactory); }
        } reset;
        auto pattern =
            (std::filesystem::path(argv[1]).parent_path() / "painter-device-fault-XXXXXX").string();
        const int temporary = ::mkstemp(pattern.data());
        require(temporary >= 0, "cannot create temporary printer output");
        ::close(temporary);
        struct remove_output {
            std::filesystem::path path;
            ~remove_output() {
                std::error_code ignored;
                std::filesystem::remove(path, ignored);
            }
        } cleanup{pattern};
        vellum::pdf_document document(argv[1]);
        const auto original = document.snapshot();
        vellum::sheet_painter painter(document);
        vellum::print_options options;
        auto job = vellum::compose_print_job({document.dimensions(0)}, {1}, options);
        const wxRect paper(0, 0, 600, 850);
        for (const auto current :
             {fault::image, fault::image_message_oom, fault::image_throw, fault::rotate,
              fault::rotate_message_oom, fault::rotate_throw, fault::grayscale,
              fault::grayscale_message_oom, fault::grayscale_throw, fault::bitmap,
              fault::bitmap_message_oom, fault::bitmap_throw, fault::memory_dc_throw, fault::blit,
              fault::blit_message_oom, fault::render, fault::dimensions}) {
            auto attempt = job;
            attempt.options.grayscale = current == fault::grayscale ||
                                        current == fault::grayscale_throw ||
                                        current == fault::grayscale_message_oom;
            attempt.sides[0].placements[0].clockwise = current == fault::rotate ||
                                                       current == fault::rotate_throw ||
                                                       current == fault::rotate_message_oom;
            const bool message_oom =
                current == fault::image_message_oom || current == fault::rotate_message_oom ||
                current == fault::grayscale_message_oom || current == fault::bitmap_message_oom ||
                current == fault::blit_message_oom;
            wxBitmap target(600, 850, 24);
            wxMemoryDC dc(target);
            require(target.IsOk() && dc.IsOk(), "test drawing device is unavailable");
            injected = current;
            triggered = 0;
            bool rejected = false;
            if (current == fault::blit || current == fault::blit_message_oom) {
                wxPrintData data;
                data.SetPrintMode(wxPRINT_MODE_FILE);
                data.SetFilename(wxString::FromUTF8(pattern));
                wxPrinterDC printer_dc(data);
                require(printer_dc.IsOk() && printer_dc.StartDoc("Vellum painter fault"),
                        "virtual printer is unavailable");
                printer_dc.StartPage();
                try {
                    painter.paint(printer_dc, attempt, 0, paper, 72, false);
                } catch (const std::bad_alloc &) {
                    rejected = message_oom;
                } catch (const std::runtime_error &error) {
                    rejected =
                        !message_oom && std::string(error.what()) == expected_message(current);
                }
                printer_dc.EndPage();
                printer_dc.EndDoc();
            } else {
                try {
                    painter.paint(dc, attempt, 0, paper, 72, false);
                } catch (const std::bad_alloc &) {
                    rejected = message_oom;
                } catch (const std::runtime_error &error) {
                    rejected =
                        !message_oom && std::string(error.what()) == expected_message(current);
                }
            }
            injected = fault::none;
            require(rejected && triggered == 1, "expected painter failure was not propagated");
            require(document.snapshot() == original, "painter failure changed the document");
            painter.paint(dc, attempt, 0, paper, 72, false);
            dc.SelectObject(wxNullBitmap);
            const auto result_image = target.ConvertToImage();
            require(result_image.IsOk(), "retry produced no output image");
            const int x = 300, y = 425;
            if (attempt.options.grayscale) {
                require(result_image.GetRed(x, y) < 240 &&
                            result_image.GetRed(x, y) == result_image.GetGreen(x, y) &&
                            result_image.GetRed(x, y) == result_image.GetBlue(x, y),
                        "grayscale retry did not render the expected page");
            } else {
                require(result_image.GetRed(x, y) > 240 && result_image.GetGreen(x, y) < 15 &&
                            result_image.GetBlue(x, y) < 15,
                        "retry did not render the expected red page");
            }
        }
        auto decorated = job;
        decorated.options.layout = vellum::print_layout::poster;
        decorated.options.borders = true;
        decorated.options.cut_marks = true;
        decorated.options.tile_labels = true;
        const auto exercise_device = [&](int failure) {
            wxPrintData data;
            data.SetPrintMode(wxPRINT_MODE_FILE);
            data.SetFilename(wxString::FromUTF8(pattern));
            wxPrinterDC dc(data);
            require(dc.IsOk() && dc.StartDoc("Vellum printer operation fault"),
                    "virtual printer is unavailable");
            dc.StartPage();
            device_operation = 0;
            failing_operation = failure;
            track_device_operations = true;
            bool failed = false;
            try {
                painter.paint(dc, decorated, 0, paper, 72, true);
            } catch (const std::runtime_error &error) {
                failed = std::string(error.what()) == "injected printer device failure";
            }
            track_device_operations = false;
            const int operations = device_operation;
            dc.EndPage();
            dc.EndDoc();
            require(failed == (failure != 0), "printer operation failure was not propagated");
            require(document.snapshot() == original, "printer operation failure changed document");
            return operations;
        };
        const int operations = exercise_device(0);
        require(operations > 10, "decorated page did not exercise printer operations");
        for (int failure = 1; failure <= operations; ++failure) {
            require(exercise_device(failure) == failure, "wrong printer operation failed");
            wxBitmap target(600, 850, 24);
            wxMemoryDC dc(target);
            painter.paint(dc, job, 0, paper, 72, false);
            dc.SelectObject(wxNullBitmap);
            const auto image = target.ConvertToImage();
            require(image.IsOk() && image.GetRed(300, 425) > 240 && image.GetGreen(300, 425) < 15 &&
                        image.GetBlue(300, 425) < 15,
                    "retry after printer operation failure rendered incorrectly");
        }
    } catch (const std::exception &error) {
        injected = fault::none;
        fail_next_allocation = false;
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
