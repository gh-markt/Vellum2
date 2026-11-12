#pragma once
#include "vellum/pdf_document.hh"
#include "vellum/print_plan.hh"
#include <wx/wx.h>
namespace vellum {
class sheet_painter {
  public:
    explicit sheet_painter(pdf_document &document);
    void paint(wxDC &dc, const print_job &job, std::size_t side, const wxRect &paper, int dpi = 72,
               bool guides = false);

  private:
    struct drawing_context {
        wxDC &dc;
        const print_options &options;
        const wxRect &paper;
        int dpi;
    };
    void paint_placement(drawing_context &context, const print_placement &placement);
    void paint_tile(drawing_context &context, const print_placement &placement, page_rectangle tile,
                    page_extent source);
    pdf_document &document;
};
} // namespace vellum
