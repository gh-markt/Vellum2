#include "vellum/sheet_painter.hh"
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <execinfo.h>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <wx/app.h>
#include <wx/dcmemory.h>

namespace {
thread_local bool armed = false;
thread_local bool injected = false;
thread_local std::size_t allocations = 0;
thread_local std::size_t fail_at = 0;
thread_local bool checking = false;
thread_local bool force_allocation = false;
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
    if (armed && (force_allocation || owned_allocation()) && allocations++ == fail_at) {
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
    if (argc != 4)
        return 2;
    const auto index = std::stoll(argv[2]);
    const int layout = std::stoi(argv[3]);
    const std::string fixture(argv[1]);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit())
        return 2;
    int result = 0;
    try {
        wxFrame parent(nullptr, wxID_ANY, "Vellum allocation test");
        vellum::pdf_document document(fixture);
        const auto original = document.snapshot();
        vellum::print_options options;
        options.layout = static_cast<vellum::print_layout>(layout);
        options.columns = options.rows = 2;
        options.grayscale = layout == 1;
        options.auto_rotate = true;
        if (layout == 3) {
            options.percentage = 300;
            options.cut_marks = true;
            options.tile_labels = true;
        }
        const auto job = vellum::compose_print_job({document.dimensions(0), document.dimensions(1)},
                                                   {1, 0, 2}, options);
        wxBitmap bitmap(600, 850, 24);
        wxMemoryDC dc(bitmap);
        vellum::sheet_painter painter(document);
        const wxRect paper(0, 0, 600, 850);
        const auto has_ink = [&] {
            dc.SelectObject(wxNullBitmap);
            const auto image = bitmap.ConvertToImage();
            bool ink = false;
            for (int y = 0; y < image.GetHeight() && !ink; ++y) {
                for (int x = 0; x < image.GetWidth(); ++x) {
                    if (image.GetRed(x, y) < 240 || image.GetGreen(x, y) < 240 ||
                        image.GetBlue(x, y) < 240) {
                        ink = true;
                        break;
                    }
                }
            }
            dc.SelectObject(bitmap);
            return ink;
        };
        fail_at =
            index < 0 ? std::numeric_limits<std::size_t>::max() : static_cast<std::size_t>(index);
        allocations = 0;
        armed = true;
        bool threw = false;
        try {
            painter.paint(dc, job, 0, paper, 72, false);
        } catch (const std::bad_alloc &) {
            armed = false;
            threw = true;
        }
        armed = false;
        if (index >= 0 && !injected)
            throw std::runtime_error("requested allocation was not reached");
        if (!threw && !has_ink())
            throw std::runtime_error("successful paint did not contain expected content");
        if (document.snapshot() != original)
            throw std::runtime_error("failed paint changed the document");
        painter.paint(dc, job, 0, paper, 72, false);
        if (!has_ink())
            throw std::runtime_error("painting did not recover after allocation failure");
        const auto painting_allocations = allocations;
        for (const bool empty_poster : {false, true}) {
            auto invalid = job;
            if (empty_poster) {
                invalid.options.layout = vellum::print_layout::poster;
                invalid.sides[0].placements.clear();
            }
            fail_at = 0;
            allocations = 0;
            injected = false;
            force_allocation = true;
            armed = true;
            bool allocation_failed = false;
            try {
                painter.paint(dc, invalid, 0, paper, empty_poster ? 72 : 35, false);
            } catch (const std::bad_alloc &) {
                allocation_failed = true;
            }
            armed = false;
            force_allocation = false;
            if (!allocation_failed || !injected || document.snapshot() != original)
                throw std::runtime_error("painter validation allocation failure was not safe");
            bool rejected = false;
            try {
                painter.paint(dc, invalid, 0, paper, empty_poster ? 72 : 35, false);
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error(
                    "painter validation did not recover after allocation failure");
        }
        dc.SelectObject(wxNullBitmap);
        std::cout << "VELLUM_ALLOCATIONS=" << painting_allocations << '\n';
    } catch (const std::exception &error) {
        armed = false;
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
