#include "vellum/main_frame.hh"
#include <wx/wx.h>
class vellum_app : public wxApp {
  public:
    bool OnInit() override {
        this->SetAppName("vellum");
        wxInitAllImageHandlers();
        auto *frame = new vellum::main_frame;
        frame->Show();
        if (this->argc > 1) {
            try {
                frame->open_path(std::filesystem::path(this->argv[1].ToStdString(wxConvUTF8)));
            } catch (const std::exception &error) {
                wxMessageBox(wxString::FromUTF8(error.what()), "Cannot open PDF",
                             wxOK | wxICON_ERROR, frame);
            }
        }
        return true;
    }
};
wxIMPLEMENT_APP(vellum_app);
