#include "vellum/atomic_file.hh"
#include "vellum/printing.hh"
#include "vellum/sheet_painter.hh"
#include <iostream>
#include <stdexcept>
#include <wx/app.h>
#include <wx/dcps.h>
class device_test_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(device_test_app);
int main(int argc, char **argv) {
#if wxUSE_POSTSCRIPT
    if (argc != 3) {
        return 2;
    }
    const std::filesystem::path fixture(argv[1]), output(argv[2]);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        vellum::pdf_document document(fixture);
        vellum::require_print_permission(document, 72);
        std::vector<vellum::page_extent> dimensions;
        for (int page = 0; page < document.pages(); ++page) {
            dimensions.push_back(document.dimensions(page));
        }
        vellum::print_options options;
        options.layout = vellum::print_layout::multiple;
        options.copies = 2;
        const auto job = vellum::compose_print_job(dimensions, {1, 2, 1}, options);
        if (job.sides.size() != 4) {
            throw std::runtime_error("unexpected virtual printer page count");
        }
        wxPrintData settings;
        settings.SetPrintMode(wxPRINT_MODE_FILE);
        settings.SetFilename(wxString::FromUTF8(output.string()));
        settings.SetPaperId(wxPAPER_A4);
        settings.SetColour(true);
        settings.SetNoCopies(1);
        wxPostScriptDC dc(settings);
        if (!dc.IsOk() || !dc.StartDoc("Vellum virtual print test")) {
            throw std::runtime_error("virtual print device unavailable");
        }
        vellum::sheet_painter painter(document);
        const auto size = dc.GetSize();
        for (std::size_t side = 0; side < job.sides.size(); ++side) {
            dc.StartPage();
            painter.paint(dc, job, side, wxRect(0, 0, size.x, size.y), 72);
            dc.EndPage();
        }
        dc.EndDoc();
        const auto postscript = vellum::read_file(output);
        std::size_t count = 0, position = 0;
        while ((position = postscript.find("%%Page:", position)) != std::string::npos) {
            ++count;
            position += 7;
        }
        if (!postscript.starts_with("%!PS") || count != 4 || postscript.size() < 10000 ||
            postscript.size() > 20000000 ||
            (postscript.find("ff0000") == std::string::npos &&
             postscript.find("FF0000") == std::string::npos)) {
            throw std::runtime_error(
                "virtual print output missing pages or rendered red image data");
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
