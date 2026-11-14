#include "vellum/printing.hh"
#include "vellum/pdf_document.hh"
#include "vellum/sheet_painter.hh"
#include <cmath>
#include <stdexcept>
#include <wx/dcprint.h>
#include <wx/print.h>
#include <wx/printdlg.h>
namespace vellum {
namespace {
class document_printout : public wxPrintout {
  public:
    document_printout(pdf_document &document, const print_job &job, int dpi)
        : wxPrintout("Vellum"), document(document), job(job), painter(document), dpi(dpi) {}
    bool HasPage(int page) override {
        return page >= 1 && static_cast<std::size_t>(page) <= this->job.sides.size();
    }
    void GetPageInfo(int *minimum, int *maximum, int *from, int *to) override {
        *minimum = *from = 1;
        *maximum = *to = static_cast<int>(this->job.sides.size());
    }
    bool OnPrintPage(int page) override {
        try {
            require_print_permission(this->document, this->dpi);
            if (!this->HasPage(page) || this->GetDC() == nullptr) {
                return false;
            }
            this->painter.paint(*this->GetDC(), this->job, static_cast<std::size_t>(page - 1),
                                this->GetPaperRectPixels(), this->dpi);
            return true;
        } catch (const std::exception &error) {
            wxLogError("Printing failed: %s", wxString::FromUTF8(error.what()));
            return false;
        }
    }

  private:
    pdf_document &document;
    const print_job &job;
    sheet_painter painter;
    int dpi;
};
} // namespace
class printer_settings::impl {
  public:
    wxPrintDialogData data;
    bool chosen = false;
    printer_preferences preferences{false, print_duplex::simplex, 1, true, false};
};
void require_print_permission(const pdf_document &document, int dpi) {
    if (!document.may_print()) {
        throw std::runtime_error("PDF permissions prohibit printing");
    }
    if (dpi < 36 || dpi > 600 || (dpi > 150 && !document.may_print_high_quality())) {
        throw std::runtime_error(
            "PDF permissions or renderer limits prohibit this print resolution");
    }
}
printer_settings::printer_settings() : implementation(std::make_unique<impl>()) {
    this->implementation->data.EnablePageNumbers(false);
    this->implementation->data.EnableSelection(false);
}
printer_settings::~printer_settings() = default;
bool printer_settings::selected() const {
    return this->implementation->chosen;
}
std::string printer_settings::name() const {
    return this->implementation->data.GetPrintData().GetPrinterName().ToStdString(wxConvUTF8);
}
bool printer_settings::select_printer(wxWindow *parent) {
    auto candidate = this->implementation->data;
    candidate.SetAllPages(true);
    wxPrintDialog dialog(parent, &candidate);
    if (dialog.ShowModal() != wxID_OK) {
        return false;
    }
    auto accepted = dialog.GetPrintDialogData();
    const auto &settings = accepted.GetPrintData();
    const int copies = accepted.GetNoCopies();
    if (copies < 1 || copies > 100) {
        throw std::runtime_error("copies must be between 1 and 100");
    }
    auto replacement = std::make_unique<impl>(*this->implementation);
    replacement->preferences = {settings.GetOrientation() == wxLANDSCAPE,
                                settings.GetDuplex() == wxDUPLEX_VERTICAL ? print_duplex::long_edge
                                : settings.GetDuplex() == wxDUPLEX_HORIZONTAL
                                    ? print_duplex::short_edge
                                    : print_duplex::simplex,
                                copies, accepted.GetCollate(), !settings.GetColour()};
    replacement->data = accepted;
    replacement->data.SetAllPages(true);
    replacement->data.SetNoCopies(1);
    replacement->chosen = true;
    this->implementation.swap(replacement);
    return true;
}
printer_preferences printer_settings::preferences() const {
    return this->implementation->preferences;
}
void printer_settings::set_job_defaults(const print_options &options) {
    auto &data = this->implementation->data;
    data.SetNoCopies(options.copies);
    data.SetCollate(options.collate);
    data.GetPrintData().SetColour(!options.grayscale);
    data.GetPrintData().SetDuplex(options.duplex == print_duplex::long_edge    ? wxDUPLEX_VERTICAL
                                  : options.duplex == print_duplex::short_edge ? wxDUPLEX_HORIZONTAL
                                                                               : wxDUPLEX_SIMPLEX);
}
void printer_settings::set_paper(int format, bool landscape) {
    const wxPaperSize papers[] = {wxPAPER_A4, wxPAPER_LETTER, wxPAPER_A3, wxPAPER_LEGAL};
    if (format < 0 || format > 4) {
        throw std::invalid_argument("invalid paper format");
    }
    auto &data = this->implementation->data.GetPrintData();
    if (format < 4) {
        data.SetPaperSize(wxDefaultSize);
        data.SetPaperId(papers[format]);
    }
    data.SetOrientation(landscape ? wxLANDSCAPE : wxPORTRAIT);
}
print_options printer_settings::measure(const print_options &requested) const {
    if (!this->selected()) {
        return requested;
    }
    wxPrinterDC dc(this->implementation->data.GetPrintData());
    if (!dc.IsOk()) {
        throw std::runtime_error("selected printer is unavailable");
    }
    const auto paper = dc.GetPaperRect();
    const auto ppi = dc.GetPPI();
    const auto printable = dc.GetSize();
    if (ppi.x <= 0 || ppi.y <= 0 || paper.width <= 0 || paper.height <= 0 || printable.x <= 0 ||
        printable.y <= 0) {
        throw std::runtime_error("printer did not provide usable paper dimensions");
    }
    auto options = requested;
    options.paper = {paper.width * 72.0 / ppi.x, paper.height * 72.0 / ppi.y};
    options.printable = {-paper.x * 72.0 / ppi.x, -paper.y * 72.0 / ppi.y,
                         printable.x * 72.0 / ppi.x, printable.y * 72.0 / ppi.y};
    return options;
}
bool printer_settings::submit(wxWindow *parent, pdf_document &document, const print_job &job,
                              int dpi) {
    require_print_permission(document, dpi);
    if (!this->selected() || job.sides.empty()) {
        throw std::runtime_error("select a printer before submission");
    }
    auto data = this->implementation->data;
    auto &native = data.GetPrintData();
    native.SetNoCopies(1);
    native.SetCollate(false);
    native.SetColour(!job.options.grayscale);
    const bool two_sided = job.options.sides == sheet_subset::all;
    native.SetDuplex(two_sided && job.options.duplex == print_duplex::long_edge ? wxDUPLEX_VERTICAL
                     : two_sided && job.options.duplex == print_duplex::short_edge
                         ? wxDUPLEX_HORIZONTAL
                         : wxDUPLEX_SIMPLEX);
    data.SetAllPages(true);
    data.SetNoCopies(1);
    const print_job frozen = job;
    document_printout output(document, frozen, dpi);
    wxPrinter printer(&data);
    const bool success = printer.Print(parent, &output, false);
    if (!success && wxPrinter::GetLastError() != wxPRINTER_CANCELLED) {
        throw std::runtime_error("printer rejected or failed the job");
    }
    return success;
}
} // namespace vellum
