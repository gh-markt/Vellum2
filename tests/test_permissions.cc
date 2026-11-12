#include "vellum/autosave.hh"
#include "vellum/pdf_document.hh"
#include "vellum/printing.hh"
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value, const char *reason) {
    if (!value) {
        throw std::runtime_error(reason);
    }
}
template <class Operation> void rejects(Operation operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "permission boundary did not reject operation");
}
void restricted_tests(const std::filesystem::path &root, const std::filesystem::path &temporary) {
    vellum::pdf_document document(root / "restricted.pdf");
    require(!document.may_edit() && !document.may_print(), "restricted rights");
    require(document.may_save() && document.may_save_as() && document.may_fill_forms(),
            "form rights must permit saving without general editing");
    document.save_copy(temporary / "forms-save.pdf");
    vellum::pdf_document form_copy(temporary / "forms-save.pdf");
    form_copy.save();
    document.save_copy(temporary / "encrypted-copy.pdf");
    require(vellum::read_file(temporary / "encrypted-copy.pdf") == document.snapshot(),
            "copy must preserve encryption/permissions bytes");
    rejects([&] { document.render_for_print(0, 1); });
    rejects([&] { vellum::require_print_permission(document, 72); });
    vellum::pdf_document copied(temporary / "encrypted-copy.pdf");
    require(!copied.may_edit() && !copied.may_print(), "saved copy must retain restrictions");
    require(!document.authenticate_owner("incorrect"), "wrong owner password");
    require(!document.may_edit() && !document.may_print(), "failed authentication granted rights");
    require(document.render(0, .5F).width == 100, "failed authentication corrupted viewing");
    require(document.authenticate_owner("test-owner"), "owner password not accepted");
    require(document.may_edit() && document.may_print() && document.may_save_as(),
            "owner rights missing");
    document.save_copy(temporary / "owner-copy.pdf");
    require(std::filesystem::exists(temporary / "owner-copy.pdf"), "authorized export missing");
}
void password_tests(const std::filesystem::path &root) {
    rejects([&] { vellum::pdf_document locked(root / "password.pdf"); });
    rejects([&] { vellum::pdf_document locked(root / "password.pdf", "wrong"); });
    vellum::pdf_document user(root / "password.pdf", "test-user");
    require(user.may_print() && !user.may_edit(), "user password incorrectly grants owner rights");
    require(!user.authenticate_owner("wrong-owner"), "wrong owner must leave user session intact");
    require(!user.authenticate_owner("test-user"), "user password is not owner password");
    require(user.render(0, .5F).width == 100, "user authentication damaged document");
    require(user.authenticate_owner("test-owner") && user.may_edit(), "owner escalation failed");
    user.validate_recovery_snapshot(user.snapshot());
    vellum::pdf_document owner(root / "password.pdf", "test-owner");
    require(owner.may_edit(), "open directly with owner password");
}
void annotation_print_flags(const std::filesystem::path &root) {
    const vellum::pdf_document document(root / "print-flags.pdf");
    const auto viewed = document.render(0, 1);
    const auto printed = document.render_for_print(0, 1);
    const auto offset = [](int x, int y) { return static_cast<std::size_t>((y * 200 + x) * 3); };
    require(viewed.rgb[offset(50, 150) + 1] > 240 && viewed.rgb[offset(50, 150)] < 15,
            "view annotation appearance");
    require(printed.rgb[offset(50, 150)] > 240 && printed.rgb[offset(50, 150) + 1] < 15,
            "nonprinting annotation leaked to output");
    require(viewed.rgb[offset(140, 150)] > 240 && viewed.rgb[offset(140, 150) + 2] < 15,
            "NoView annotation leaked to viewer");
    require(printed.rgb[offset(140, 150) + 2] > 240 && printed.rgb[offset(140, 150)] < 15,
            "print-only annotation missing");
}
void region_tests(const std::filesystem::path &root) {
    const vellum::pdf_document document(root / "print-flags.pdf");
    const auto view = document.render_region(0, {30, 130, 40, 40}, 2);
    const auto print = document.render_print_region(0, {30, 130, 40, 40}, 2, 144);
    const std::size_t middle = (40 * 80 + 40) * 3;
    require(view.width == 80 && view.height == 80 && view.rgb[middle] < 15 &&
                view.rgb[middle + 1] > 240,
            "view region must use View annotation flags");
    require(print.rgb[middle] > 240 && print.rgb[middle + 1] < 15,
            "print region must use Print annotation flags");
    rejects([&] { document.render_region(0, {-1, 0, 10, 10}, 1); });
    rejects([&] { document.render_region(0, {0, 0, 201, 10}, 1); });
    rejects([&] { document.render_region(0, {0, 0, 200, 200}, 100); });
    rejects([&] { document.render_print_region(0, {0, 0, 10, 10}, 1, 601); });
    const vellum::pdf_document restricted(root / "restricted.pdf");
    require(restricted.render_region(0, {0, 0, 100, 100}, 1).width == 100,
            "print prohibition must not block viewing regions");
    rejects([&] { restricted.render_print_region(0, {0, 0, 100, 100}, 1, 72); });
    const vellum::pdf_document low(root / "low-quality-print.pdf");
    rejects([&] { low.render_print_region(0, {0, 0, 10, 10}, 1, 300); });
}

void independent_save_and_autosave(const std::filesystem::path &root,
                                   const std::filesystem::path &temporary) {
    vellum::pdf_document document(root / "edit-no-extraction.pdf");
    require(document.may_edit() && document.may_save_as() && !document.may_copy(),
            "extraction must not prohibit file saving");
    vellum::document_state state;
    state.changed();
    const auto serialize = [&] { return document.snapshot(); };
    const auto validate = [&](const std::string &bytes) {
        document.validate_recovery_snapshot(bytes);
    };
    const auto now = vellum::autosave_controller::clock::now();
    vellum::autosave_controller autosave(state, vellum::snapshot_store(temporary / "recovery.pdf"),
                                         serialize, validate, now);
    require(!autosave.trigger_now(now), "Reader must never autosave");
    state.enable_editor();
    require(autosave.trigger_now(now), "autosave must not depend on content extraction permission");
    require(vellum::read_file(temporary / "recovery.pdf") == document.snapshot(),
            "snapshot content mismatch");
    vellum::pdf_document read_only(root / "read-only-copy.pdf");
    require(!read_only.may_edit() && !read_only.may_save() && read_only.may_save_as(),
            "read-only copy policy");
    read_only.save_copy(temporary / "allowed-copy.pdf");
    rejects([&] { read_only.save_copy(root / "read-only-copy.pdf"); });
    vellum::pdf_document low(root / "low-quality-print.pdf");
    require(low.may_print() && !low.may_print_high_quality(), "low-quality print policy");
    vellum::require_print_permission(low, 150);
    rejects([&] { vellum::require_print_permission(low, 300); });
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    auto pattern = (std::filesystem::temp_directory_path() / "vellum-permissions-XXXXXX").string();
    if (::mkdtemp(pattern.data()) == nullptr) {
        return 2;
    }
    const std::filesystem::path temporary(pattern);
    try {
        annotation_print_flags(argv[1]);
        region_tests(argv[1]);
        restricted_tests(argv[1], temporary);
        password_tests(argv[1]);
        independent_save_and_autosave(argv[1], temporary);
        std::filesystem::remove_all(temporary);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(temporary);
        return 1;
    }
}
