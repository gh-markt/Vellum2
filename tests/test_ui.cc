#include "vellum/main_frame.hh"
#include "vellum/print_dialog.hh"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <wx/app.h>
#include <wx/modalhook.h>
#include <wx/statbmp.h>
#include <wx/statusbr.h>
#include <wx/textdlg.h>
class ui_test_app : public wxApp {
  public:
    bool OnInit() override {
        this->SetAppName("vellum-ui-tests");
        wxInitAllImageHandlers();
        this->SetExitOnFrameDelete(false);
        return true;
    }
};
wxIMPLEMENT_APP_NO_MAIN(ui_test_app);
namespace {
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
void events() {
    for (int step = 0; step < 40; ++step) {
        wxYield();
        wxMilliSleep(5);
    }
}
void select_menu(vellum::main_frame &frame, std::size_t menu, const wxString &label) {
    const int id = frame.GetMenuBar()->GetMenu(menu)->FindItem(label);
    require(id != wxNOT_FOUND, "menu item missing");
    wxCommandEvent event(wxEVT_MENU, id);
    require(frame.GetEventHandler()->ProcessEvent(event), "menu action not handled");
}
class password_hook : public wxModalDialogHook {
  public:
    password_hook(wxString password, int response)
        : password(std::move(password)), response(response) {
        this->Register();
    }
    int prompts() const { return this->prompt_count; }
    int files() const { return this->file_count; }
    int prints() const { return this->print_count; }
    int alerts() const { return this->alert_count; }
    bool unexpected() const { return this->unexpected_dialog; }

  protected:
    int Enter(wxDialog *dialog) override {
        if (auto *input = dynamic_cast<wxPasswordEntryDialog *>(dialog)) {
            ++this->prompt_count;
            input->SetValue(this->password);
            return this->response;
        }
        if (dialog->GetTitle() == "Authorization failed") {
            ++this->alert_count;
            return wxID_OK;
        }
        if (dynamic_cast<wxFileDialog *>(dialog)) {
            ++this->file_count;
            return wxID_CANCEL;
        }
        if (dynamic_cast<vellum::print_dialog *>(dialog)) {
            ++this->print_count;
            return wxID_CANCEL;
        }
        this->unexpected_dialog = true;
        return wxID_CANCEL;
    }

  private:
    wxString password;
    int response;
    int prompt_count = 0;
    int alert_count = 0;
    int file_count = 0;
    int print_count = 0;
    bool unexpected_dialog = false;
};
void viewer_tests(vellum::main_frame &frame, const std::filesystem::path &fixture) {
    frame.open_path(fixture);
    events();
    require(frame.GetStatusBar()->GetStatusText().Contains("Page 1 / 2"), "first page");
    select_menu(frame, 1, "Previous page");
    require(frame.current_page() == 1, "previous boundary");
    frame.set_zoom(vellum::zoom_mode::percentage, 125);
    require(std::abs(frame.zoom_scale() - 1.25) < .0001, "manual zoom");
    frame.SetSize(900, 700);
    events();
    require(std::abs(frame.zoom_scale() - 1.25) < .0001, "manual zoom changed on resize");
    frame.set_zoom(vellum::zoom_mode::percentage, 800);
    events();
    auto *canvas = dynamic_cast<wxScrolledWindow *>(frame.FindWindowByName("viewer-canvas"));
    auto *bitmap = dynamic_cast<wxStaticBitmap *>(frame.FindWindowByName("viewer-bitmap"));
    require(canvas && bitmap, "viewport controls missing");
    require(bitmap->GetBitmap().GetWidth() <= canvas->GetClientSize().x + 2,
            "viewer allocated the entire zoomed page instead of visible region");
    canvas->Scroll(20, 15);
    wxScrollWinEvent scroll(wxEVT_SCROLLWIN_THUMBRELEASE, 20, wxHORIZONTAL);
    canvas->GetEventHandler()->ProcessEvent(scroll);
    events();
    require(bitmap->IsShown() && bitmap->GetPosition().x >= -2,
            "scroll did not refresh visible region position");
    canvas->Scroll(0, 0);
    frame.set_zoom(vellum::zoom_mode::fit_width);
    events();
    const auto old_width_scale = frame.zoom_scale();
    frame.SetSize(1150, 800);
    events();
    require(frame.zoom_scale() > old_width_scale, "fit width did not track viewport resize");
    const auto first_page_scale = frame.zoom_scale();
    select_menu(frame, 1, "Next page");
    events();
    require(frame.current_page() == 2 && frame.zoom_scale() < first_page_scale * .75,
            "fit width did not track mixed page width");
    frame.set_zoom(vellum::zoom_mode::fit_height);
    events();
    const auto short_page_scale = frame.zoom_scale();
    frame.go_to_page(1);
    events();
    require(frame.zoom_scale() < short_page_scale * .75, "fit height did not track page height");
    select_menu(frame, 2, "Fit page");
    events();
    require(frame.zoom_policy() == vellum::zoom_mode::fit_page, "fit-page menu");
    select_menu(frame, 0, "Enable editor mode for this document");
    require(frame.GetStatusBar()->GetStatusText().Contains("Editor"), "editor transition");
    const int editor =
        frame.GetMenuBar()->GetMenu(0)->FindItem("Enable editor mode for this document");
    require(!frame.GetMenuBar()->IsEnabled(editor),
            "editor must remain enabled until close/reopen");
    frame.open_path(fixture);
    events();
    require(frame.GetStatusBar()->GetStatusText().Contains("Reader"),
            "reopen resets document mode");
}
void permission_tests(vellum::main_frame &frame, const std::filesystem::path &root) {
    frame.open_path(root / "restricted.pdf");
    events();
    const int editor =
        frame.GetMenuBar()->GetMenu(0)->FindItem("Enable editor mode for this document");
    require(frame.GetMenuBar()->IsEnabled(editor),
            "restricted Editor action must offer owner authentication");
    require(frame.GetMenuBar()->IsEnabled(wxID_PRINT) &&
                frame.GetToolBar()->GetToolEnabled(wxID_PRINT),
            "Print action must offer owner authentication");
    require(frame.GetMenuBar()->IsEnabled(wxID_SAVE) && frame.GetMenuBar()->IsEnabled(wxID_SAVEAS),
            "save actions must offer owner authentication");
    {
        password_hook hook("", wxID_CANCEL);
        select_menu(frame, 0, "Save");
        require(hook.prompts() == 0 && !hook.unexpected() &&
                    frame.GetStatusBar()->GetStatusText().Contains("Reader"),
                "form-save permission must not require owner or enable Editor");
    }
    {
        password_hook hook("", wxID_CANCEL);
        select_menu(frame, 0, "Enable editor mode for this document");
        require(hook.prompts() == 1 && hook.alerts() == 0 && !hook.unexpected(),
                "cancel authentication path");
        require(frame.GetStatusBar()->GetStatusText().Contains("Reader"), "cancel changed mode");
    }
    {
        password_hook hook("incorrect", wxID_OK);
        select_menu(frame, 0, "Enable editor mode for this document");
        require(hook.prompts() == 1 && hook.alerts() == 1 && !hook.unexpected(),
                "wrong owner password must alert");
        require(frame.GetStatusBar()->GetStatusText().Contains("Reader"),
                "wrong password changed mode");
    }
    {
        password_hook hook("", wxID_CANCEL);
        select_menu(frame, 0, "Print...");
        require(hook.prompts() == 1 && hook.prints() == 0 && !hook.unexpected(),
                "prohibited Print cancellation");
    }
    frame.open_path(root / "read-only-copy.pdf");
    events();
    {
        password_hook hook("incorrect", wxID_OK);
        select_menu(frame, 0, "Save");
        require(hook.prompts() == 1 && hook.alerts() == 1 && hook.files() == 0 &&
                    !hook.unexpected(),
                "prohibited Save wrong password");
    }
    {
        password_hook hook("", wxID_CANCEL);
        select_menu(frame, 0, "Save");
        require(hook.prompts() == 1 && !hook.unexpected(), "prohibited Save cancellation");
    }
    frame.open_path(root / "restricted.pdf");
    events();
    {
        password_hook hook("test-owner", wxID_OK);
        select_menu(frame, 0, "Enable editor mode for this document");
        require(hook.prompts() == 1 && hook.alerts() == 0 && !hook.unexpected(),
                "valid owner authentication");
        require(frame.GetStatusBar()->GetStatusText().Contains("Editor"),
                "owner password did not enable Editor");
        require(!frame.GetMenuBar()->IsEnabled(editor),
                "Editor mode must persist after owner authentication");
    }
    {
        password_hook hook("", wxID_CANCEL);
        select_menu(frame, 0, "Print...");
        require(hook.prompts() == 0 && hook.prints() == 1 && !hook.unexpected(),
                "owner authorization must persist across commands");
    }
    frame.open_path(root / "restricted.pdf");
    events();
    {
        password_hook hook("test-owner", wxID_OK);
        select_menu(frame, 0, "Print...");
        require(hook.prompts() == 1 && hook.prints() == 1 && !hook.unexpected(),
                "reopen must discard authorization; Print can unlock");
        require(frame.GetStatusBar()->GetStatusText().Contains("Reader"),
                "print authentication must not switch mode");
    }
    {
        password_hook hook("", wxID_CANCEL);
        select_menu(frame, 0, "Save As...");
        require(hook.prompts() == 0 && hook.files() == 1 && !hook.unexpected(),
                "owner login should suppress repeated prompts");
    }
    frame.open_path(root / "read-only-copy.pdf");
    events();
    {
        password_hook hook("", wxID_CANCEL);
        select_menu(frame, 0, "Save As...");
        require(hook.prompts() == 0 && hook.files() == 1 && !hook.unexpected(),
                "permitted Save As must not ask owner password");
    }
    const auto title_before = frame.GetTitle();
    {
        password_hook hook("", wxID_CANCEL);
        frame.open_path(root / "password.pdf");
        require(hook.prompts() == 1 && frame.GetTitle() == title_before,
                "cancelled protected open replaced prior document");
    }
    {
        password_hook hook("wrong", wxID_OK);
        bool rejected = false;
        try {
            frame.open_path(root / "password.pdf");
        } catch (const vellum::pdf_password_error &) {
            rejected = true;
        }
        require(rejected && hook.prompts() == 1 && frame.GetTitle() == title_before,
                "wrong opening password replaced prior document");
    }
    {
        password_hook hook("test-user", wxID_OK);
        frame.open_path(root / "password.pdf");
        events();
        require(hook.prompts() == 1 && !hook.unexpected(), "password-protected open prompt");
    }
    {
        password_hook hook("", wxID_CANCEL);
        select_menu(frame, 0, "Print...");
        require(hook.prompts() == 0 && hook.prints() == 1 && !hook.unexpected(),
                "permitted Print must not ask owner password");
    }
}

} // namespace
int main(int argc, char **argv) {
    if (argc != 3) {
        return 2;
    }
    const std::filesystem::path fixture(argv[1]), security(argv[2]);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) {
        return 2;
    }
    int result = 0;
    try {
        auto frame = std::make_unique<vellum::main_frame>();
        frame->Show();
        events();
        viewer_tests(*frame, fixture);
        permission_tests(*frame, security);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
