#include "vellum/atomic_file.hh"
#include "vellum/print_dialog.hh"
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <system_error>
#include <unistd.h>
#include <wx/app.h>
#include <wx/log.h>

namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void exhaust(const std::filesystem::path &path) {
    const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
    require(fd >= 0, "cannot create filler");
    const std::string block(4096, 'x');
    for (;;) {
        const auto count = ::write(fd, block.data(), block.size());
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0 && errno == ENOSPC)
            break;
        if (count <= 0) {
            ::close(fd);
            throw std::runtime_error("unexpected filler write failure");
        }
    }
    ::close(fd);
}
class preview_disk_app : public wxApp {
  public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        SetExitOnFrameDelete(false);
        return true;
    }
};
} // namespace
wxIMPLEMENT_APP_NO_MAIN(preview_disk_app);
int main(int argc, char **argv) {
    const char *configured = std::getenv("VELLUM_DISK_FULL_TEST_DIR");
    const char *fixture = std::getenv("VELLUM_DISK_FULL_REFERENCE");
    if (!configured || !*configured || !fixture || !*fixture || !wxEntryStart(argc, argv) ||
        !wxTheApp->CallOnInit())
        return 2;
    int result = 0;
    const auto root = std::filesystem::canonical(configured);
    const auto image = root / "preview.png";
    const auto filler = root / "filler";
    try {
        struct statfs fs{};
        struct stat current{}, parent{};
        require(::statfs(root.c_str(), &fs) == 0 && ::stat(root.c_str(), &current) == 0 &&
                    ::stat(root.parent_path().c_str(), &parent) == 0 && fs.f_type == 0x01021994 &&
                    current.st_dev != parent.st_dev && fs.f_blocks > 0 && fs.f_bsize > 0 &&
                    static_cast<unsigned long long>(fs.f_blocks) * fs.f_bsize <=
                        16ULL * 1024 * 1024 &&
                    std::filesystem::is_empty(root),
                "test requires an empty dedicated tmpfs no larger than 16 MiB");
        vellum::pdf_document document(fixture);
        const auto original_document = document.snapshot();
        vellum::printer_settings printer;
        vellum::print_dialog dialog(nullptr, document, printer, 1);
        dialog.export_preview(image);
        const auto encoded = vellum::read_file(image);
        require(encoded.size() > static_cast<std::size_t>(fs.f_bsize),
                "preview fixture must span more than one filesystem block");
        wxCommandEvent next(wxEVT_BUTTON, wxID_FORWARD);
        require(dialog.GetEventHandler()->ProcessEvent(next), "cannot navigate to second side");
        dialog.export_preview(image);
        const auto old_image = vellum::read_file(image);
        wxImage previous(wxString::FromUTF8(image.string()), wxBITMAP_TYPE_PNG);
        require(previous.IsOk() &&
                    previous.GetGreen(previous.GetWidth() / 2, previous.GetHeight() / 2) > 240 &&
                    old_image != encoded,
                "previous preview must be a distinct valid image");
        wxCommandEvent back(wxEVT_BUTTON, wxID_BACKWARD);
        require(dialog.GetEventHandler()->ProcessEvent(back), "cannot navigate to first side");
        exhaust(filler);
        for (const bool partial : {false, true}) {
            if (partial) {
                const auto size = std::filesystem::file_size(filler);
                require(size >= static_cast<std::uintmax_t>(fs.f_bsize), "filler is too small");
                std::filesystem::resize_file(filler,
                                             size - static_cast<std::uintmax_t>(fs.f_bsize));
            }
            bool failed_with_enospc = false;
            {
                wxLogNull suppress_expected_write_error;
                try {
                    dialog.export_preview(image);
                } catch (const std::system_error &error) {
                    failed_with_enospc = error.code().value() == ENOSPC;
                }
            }
            const bool preserved = vellum::read_file(image) == old_image;
            if (!failed_with_enospc || !preserved || !dialog.valid() ||
                document.snapshot() != original_document)
                std::cerr << "enospc=" << failed_with_enospc << " preserved=" << preserved
                          << " valid=" << dialog.valid()
                          << " document=" << (document.snapshot() == original_document) << '\n';
            require(failed_with_enospc && preserved && dialog.valid() &&
                        document.snapshot() == original_document,
                    "full filesystem changed the previous preview or document");
            for (const auto &entry : std::filesystem::directory_iterator(root))
                require(entry.path() == image || entry.path() == filler,
                        "failed preview export left a temporary file");
        }
        require(std::filesystem::remove(filler), "cannot release filler");
        dialog.export_preview(image);
        wxImage reopened(wxString::FromUTF8(image.string()), wxBITMAP_TYPE_PNG);
        require(reopened.IsOk() &&
                    reopened.GetRed(reopened.GetWidth() / 2, reopened.GetHeight() / 2) > 240 &&
                    reopened.GetGreen(reopened.GetWidth() / 2, reopened.GetHeight() / 2) < 15 &&
                    document.snapshot() == original_document,
                "preview did not recover after filesystem space was freed");
        std::filesystem::remove(image);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::error_code ignored;
    std::filesystem::remove(filler, ignored);
    std::filesystem::remove(image, ignored);
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
