#pragma once
#include "vellum/autosave.hh"
#include "vellum/pdf_document.hh"
#include "vellum/printing.hh"
#include "vellum/view_zoom.hh"
#include <memory>
#include <optional>
#include <wx/combobox.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>
namespace vellum {
class main_frame : public wxFrame {
  public:
    main_frame();
    ~main_frame() override;
    void open_path(const std::filesystem::path &path);
    void set_zoom(zoom_mode mode, double percentage = 100);
    void go_to_page(int page);
    double zoom_scale() const;
    zoom_mode zoom_policy() const;
    int current_page() const;

  private:
    enum { unlock_id = wxID_HIGHEST + 99 };
    enum {
        open_id = wxID_HIGHEST + 1,
        recover_id,
        editor_id,
        previous_id,
        next_id,
        fit_page_id,
        fit_width_id,
        fit_height_id,
        zoom_in_id,
        zoom_out_id,
        actual_id,
        zoom_control_id,
        page_control_id
    };
    enum class document_action { edit, print, save, save_as };
    struct authorization {
        bool owner_required = false;
        std::string password;
    };
    std::optional<authorization> request_authorization(document_action action);
    void commit_authorization(const authorization &authorization);
    void build_menus();
    void build_toolbar();
    void bind_actions();
    void bind_view_actions();
    void update_actions();
    void unlock_permissions();
    void apply_zoom_control();
    void print_document();
    void open_document();
    void recover_document();
    void load_document(const std::filesystem::path &path, bool recovered);
    void show_page();
    void save_document();
    void save_copy();
    void enable_editor();
    void on_timer(wxTimerEvent &event);
    void on_zoom_timer(wxTimerEvent &event);
    void on_resize(wxSizeEvent &event);
    void on_scroll(wxScrollWinEvent &event);
    void on_mouse_wheel(wxMouseEvent &event);
    void show_error(const std::exception &error);
    wxScrolledWindow *canvas = nullptr;
    wxStaticBitmap *bitmap = nullptr;
    wxComboBox *zoom_control = nullptr;
    wxSpinCtrl *page_control = nullptr;
    std::unique_ptr<pdf_document> document;
    document_state state;
    std::unique_ptr<autosave_controller> autosave;
    std::filesystem::path recovery_path;
    int page = 0;
    zoom_mode current_zoom_mode = zoom_mode::fit_page;
    double percentage = 100;
    double current_scale = 1;
    printer_settings printer;
    wxTimer timer;
    wxTimer zoom_timer;
};
} // namespace vellum
