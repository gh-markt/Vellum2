#pragma once
#include "vellum/printing.hh"
#include "vellum/sheet_painter.hh"
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
namespace vellum {
class print_preview_panel;
class print_dialog : public wxDialog {
  public:
    print_dialog(wxWindow *parent, pdf_document &document, printer_settings &printer,
                 int current_page);
    ~print_dialog() override;
    void regenerate_preview();
    const print_job &job() const;
    bool valid() const;
    void export_preview(const std::filesystem::path &path);

  private:
    void build_controls(wxWindow *controls, wxFlexGridSizer *grid);
    void build_footer(wxBoxSizer *root);
    void connect_controls();
    void settings_changed();
    print_options read_options();
    void choose_printer();
    void submit();
    void update_side_label();
    void save_preview();
    void on_preview_timer(wxTimerEvent &event);
    pdf_document &document;
    printer_settings &printer;
    std::vector<page_extent> dimensions;
    int current_page;
    print_job current_job;
    sheet_painter painter;
    bool plan_valid = false;
    std::size_t preview_side = 0;
    wxTimer timer;
    wxChoice *selection = nullptr;
    wxTextCtrl *ranges = nullptr;
    wxChoice *parity = nullptr;
    wxChoice *layout = nullptr;
    wxChoice *scaling = nullptr;
    wxSpinCtrlDouble *percentage = nullptr;
    wxChoice *paper = nullptr;
    wxChoice *orientation = nullptr;
    wxSpinCtrlDouble *margin = nullptr;
    wxSpinCtrl *columns = nullptr;
    wxSpinCtrl *rows = nullptr;
    wxChoice *order = nullptr;
    wxSpinCtrlDouble *gutter = nullptr;
    wxSpinCtrlDouble *overlap = nullptr;
    wxChoice *duplex = nullptr;
    wxChoice *sides = nullptr;
    wxSpinCtrl *copies = nullptr;
    wxChoice *resolution = nullptr;
    wxCheckBox *collate = nullptr;
    wxCheckBox *grayscale = nullptr;
    wxCheckBox *reverse = nullptr;
    wxCheckBox *rotate = nullptr;
    wxCheckBox *binding = nullptr;
    wxCheckBox *borders = nullptr;
    wxCheckBox *cut_marks = nullptr;
    wxCheckBox *labels = nullptr;
    wxCheckBox *reverse_backs = nullptr;
    wxButton *print_button = nullptr;
    wxStaticText *status = nullptr;
    wxStaticText *side_label = nullptr;
    print_preview_panel *preview = nullptr;
};
} // namespace vellum
