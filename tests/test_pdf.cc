#include "vellum/atomic_file.hh"
#include "vellum/pdf_document.hh"
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        const vellum::pdf_document document(argv[1]);
        if (document.pages() != 2) {
            return 1;
        }
        const auto image = document.render(0, 1.0F);
        if (image.width != 200 || image.height != 200 || image.rgb.size() != 120000) {
            return 1;
        }
        if (image.rgb[0] != 255 || image.rgb[1] != 0 || image.rgb[2] != 0) {
            return 1;
        }
        const auto second = document.render(1, 1.0F);
        if (second.rgb[0] != 255 || second.rgb[1] != 255 || second.rgb[2] != 255) {
            return 1;
        }
        if (!document.may_copy()) {
            return 1;
        }
        document.validate_snapshot(document.snapshot());
        const auto original = vellum::read_file(argv[1]);
        const auto copy = std::filesystem::path(argv[1]).parent_path() / "test-copy.pdf";
        document.save_copy(copy);
        if (vellum::read_file(copy) != original || vellum::read_file(argv[1]) != original) {
            return 1;
        }
        std::filesystem::remove(copy);
        bool rejected = false;
        try {
            document.render(-1, 1.0F);
        } catch (const std::exception &) {
            rejected = true;
        }
        if (!rejected) {
            return 1;
        }
        rejected = false;
        try {
            document.validate_snapshot("not a PDF");
        } catch (const std::exception &) {
            rejected = true;
        }
        if (!rejected) {
            return 1;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
