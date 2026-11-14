#include "vellum/atomic_file.hh"
#include "vellum/pdf_document.hh"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
template <class Operation> void rejects(Operation operation) {
    try {
        operation();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("invalid PDF rendering request was accepted");
}
void render_boundaries(const vellum::pdf_document &document) {
    const auto area = document.render_region(0, {10, 20, 30, 40}, 1.0F);
    if (area.width != 30 || area.height != 40 || area.rgb.size() != 30 * 40 * 3 ||
        area.rgb[0] != 255 || area.rgb[1] != 0 || area.rgb[2] != 0) {
        throw std::runtime_error("valid region differs from the independent red-page fixture");
    }
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto infinity = std::numeric_limits<float>::infinity();
    rejects([&] { document.render(2, 1.0F); });
    rejects([&] { document.render(0, 0.0F); });
    rejects([&] { document.render(0, -1.0F); });
    rejects([&] { document.render(0, 0.0000001F); });
    rejects([&] { document.render(0, nan); });
    rejects([&] { document.render(0, infinity); });
    rejects([&] { document.render(0, 41.0F); });
    rejects([&] { document.render(0, 10001.0F); });
    rejects([&] { document.dimensions(2); });
    rejects([&] { document.render_region(0, {-1, 0, 10, 10}, 1.0F); });
    rejects([&] { document.render_region(0, {0, -1, 10, 10}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 0, 0, 10}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 0, 10, 0}, 1.0F); });
    rejects([&] { document.render_region(0, {190, 0, 20, 10}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 190, 10, 20}, 1.0F); });
    rejects([&] { document.render_region(0, {nan, 0, 10, 10}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 0, infinity, 10}, 1.0F); });
    rejects([&] { document.render_region(-1, {0, 0, 10, 10}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 0, 10, 10}, 0.0F); });
    rejects([&] { document.render_region(0, {0, 0, 10, 10}, nan); });
    rejects([&] { document.render_region(0, {0, 0, 10, 10}, infinity); });
    rejects([&] { document.render_region(0, {0, 0, 200, 200}, 41.0F); });
    rejects([&] { document.render_region(0, {0, nan, 10, 10}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 0, nan, 10}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 0, 10, nan}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 0, 200.002, 10}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 0, 10, 200.002}, 1.0F); });
    rejects([&] { document.render_region(0, {0, 0, 1, .001}, 8191.0F); });
    rejects([&] { document.render_region(0, {0, 0, .001, 1}, 8191.0F); });
    rejects([&] { document.render_region(0, {0, 0, .001, .001}, 10001.0F); });
    const auto before = document.snapshot();
    for (const vellum::page_rectangle tiny : std::initializer_list<vellum::page_rectangle>{
             {0, 0, 1e-50, 1}, {0, 0, 1, 1e-50}, {100, 0, 1e-8, 1}, {0, 100, 1, 1e-8}}) {
        bool rejected = false;
        try {
            (void)document.render_region(0, tiny, 1.0F);
        } catch (const std::runtime_error &error) {
            rejected = std::string(error.what()) == "PDF region rendering failed";
        }
        if (!rejected || document.snapshot() != before) {
            throw std::runtime_error("subpixel region was not rejected safely");
        }
    }
    const auto corner = document.render_region(0, {199, 199, 1, 1}, 1.0F);
    if (corner.width != 1 || corner.height != 1 ||
        corner.rgb != std::vector<unsigned char>{255, 0, 0}) {
        throw std::runtime_error("valid page-edge region differs from red-page fixture");
    }
    for (const int dpi : {36, 600}) {
        const auto printed = document.render_print_region(0, {199, 199, 1, 1}, 1.0F, dpi);
        if (printed.rgb != corner.rgb) {
            throw std::runtime_error("print resolution boundary changed the expected page region");
        }
    }
    for (const int dpi : {35, 601}) {
        rejects([&] { document.render_print_region(0, {0, 0, 1, 1}, 1.0F, dpi); });
    }
}
void save_identity_boundaries(const std::filesystem::path &fixture) {
    auto pattern =
        (std::filesystem::temp_directory_path() / "vellum-save-identity-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr) {
        throw std::runtime_error("cannot create temporary PDF test directory");
    }
    const std::filesystem::path directory(pattern);
    try {
        const auto original = vellum::read_file(fixture);
        const auto source = directory / "source.pdf";
        const auto alias = directory / "alias.pdf";
        const auto destination = directory / "saved.pdf";
        vellum::atomic_write(source, original);
        std::filesystem::create_symlink(source, alias);
        vellum::pdf_document document(source);
        document.save_copy(alias);
        if (vellum::read_file(source) != original || !std::filesystem::is_symlink(alias)) {
            throw std::runtime_error("saving through an alias replaced the source or symlink");
        }
        const auto loop = directory / "loop.pdf";
        std::filesystem::create_symlink("loop.pdf", loop);
        for (const auto &invalid :
             {directory, source / "child.pdf", loop, directory / std::string(300, 'x')}) {
            rejects([&] { document.save_as(invalid); });
            if (document.path() != source || document.snapshot() != original ||
                vellum::read_file(source) != original || !std::filesystem::is_symlink(loop)) {
                throw std::runtime_error("invalid Save As target changed the source or session");
            }
            for (const auto &entry : std::filesystem::directory_iterator(directory)) {
                if (entry.path() != source && entry.path() != alias && entry.path() != loop) {
                    throw std::runtime_error("invalid Save As target left an unexpected file");
                }
            }
        }
        vellum::atomic_write(source, "external change");
        rejects([&] { document.save(); });
        rejects([&] { document.save_copy(alias); });
        if (vellum::read_file(source) != "external change" || document.snapshot() != original) {
            throw std::runtime_error("failed save did not preserve external and in-memory state");
        }
        rejects([&] { document.save_as(directory / "missing" / "failed.pdf"); });
        if (document.path() != source) {
            throw std::runtime_error("failed Save As changed the current document path");
        }
        document.save_as(destination);
        if (document.path() != destination || vellum::read_file(destination) != original ||
            vellum::read_file(source) != "external change") {
            throw std::runtime_error("Save As did not preserve document identity and bytes");
        }
        document.save();
        vellum::pdf_document reopened(destination);
        if (reopened.pages() != 2 || reopened.snapshot() != original) {
            throw std::runtime_error("saved document could not be reopened intact");
        }
        std::filesystem::remove_all(directory);
    } catch (...) {
        std::filesystem::remove_all(directory);
        throw;
    }
}
void strict_recovery_corpus(const vellum::pdf_document &document, const std::string &original) {
    const auto xref = original.find("\nxref\n");
    const auto marker = original.rfind("startxref\n");
    if (xref == std::string::npos || marker == std::string::npos || xref <= 128) {
        throw std::runtime_error("recovery corpus fixture has an unexpected cross-reference");
    }
    std::size_t cases = 0;
    for (std::size_t length = 0; length <= xref; ++length) {
        rejects([&] { document.validate_recovery_snapshot(original.substr(0, length)); });
        ++cases;
    }
    const auto offset = marker + std::string("startxref\n").size();
    const auto end = original.find('\n', offset);
    if (end == std::string::npos || end == offset) {
        throw std::runtime_error("recovery corpus fixture has no cross-reference offset");
    }
    for (int wrong_offset = 0; wrong_offset < 128; ++wrong_offset) {
        auto corrupted = original;
        corrupted.replace(offset, end - offset, std::to_string(wrong_offset));
        rejects([&] { document.validate_recovery_snapshot(corrupted); });
        ++cases;
    }
    if (document.snapshot() != original || document.render(0, 1.0F).rgb[1] != 0) {
        throw std::runtime_error("malformed recovery corpus changed the live document");
    }
    std::cout << "Strict recovery corruption: " << cases << " cases\n";
}
} // namespace

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
        render_boundaries(document);
        save_identity_boundaries(argv[1]);
        if (!document.may_copy()) {
            return 1;
        }
        document.validate_snapshot(document.snapshot());
        const auto original = vellum::read_file(argv[1]);
        strict_recovery_corpus(document, original);
        rejects(
            [&] { document.validate_recovery_snapshot(original.substr(0, original.size() / 2)); });
        auto bad_cross_reference = original;
        const auto marker = bad_cross_reference.rfind("startxref\n");
        if (marker == std::string::npos) {
            throw std::runtime_error("test fixture has no cross-reference marker");
        }
        const auto offset = marker + std::string("startxref\n").size();
        const auto end = bad_cross_reference.find('\n', offset);
        if (end == std::string::npos || end == offset) {
            throw std::runtime_error("test fixture has no cross-reference offset");
        }
        bad_cross_reference.replace(offset, end - offset, "0");
        rejects([&] { document.validate_recovery_snapshot(bad_cross_reference); });
        if (document.snapshot() != original || document.render(0, 1.0F).rgb[0] != 255) {
            throw std::runtime_error("rejected recovery data changed the live document");
        }
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
