#pragma once
#include "vellum/print_plan.hh"
#include <memory>
#include <string>
class wxWindow;
namespace vellum {
class pdf_document;
void require_print_permission(const pdf_document &document, int dpi);
struct printer_preferences {
    bool landscape;
    print_duplex duplex;
    int copies;
    bool collate;
    bool grayscale;
};
class printer_settings {
  public:
    printer_settings();
    ~printer_settings();
    printer_settings(const printer_settings &) = delete;
    printer_settings &operator=(const printer_settings &) = delete;
    bool select_printer(wxWindow *parent);
    bool selected() const;
    printer_preferences preferences() const;
    void set_job_defaults(const print_options &options);
    std::string name() const;
    void set_paper(int format, bool landscape);
    print_options measure(const print_options &requested) const;
    bool submit(wxWindow *parent, pdf_document &document, const print_job &job, int dpi);

  private:
    class impl;
    std::unique_ptr<impl> implementation;
};
} // namespace vellum
