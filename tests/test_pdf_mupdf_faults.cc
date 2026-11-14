#include "vellum/pdf_document.hh"
#include <iostream>
#include <limits>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <new>
#include <stdexcept>
#include <string>

namespace {
enum class fault {
    none,
    context,
    register_handlers,
    open_stream,
    open_document,
    not_pdf,
    not_pdf_strict,
    count_pages,
    zero_pages,
    negative_pages,
    repaired,
    load,
    bounds,
    region_load,
    region_bounds,
    page_conversion,
    bounds_nan_width,
    bounds_nan_height,
    bounds_infinite_width,
    bounds_infinite_height,
    bounds_zero_width,
    bounds_zero_height,
    bounds_negative_width,
    bounds_negative_height,
    bounds_large_width,
    bounds_large_height,
    bounds_overflow_width,
    bounds_overflow_height,
    permission,
    authentication,
    authentication_reject,
    live_authentication,
    live_authentication_reject,
    full_pixmap,
    print_pixmap,
    region_pixmap,
    draw_device,
    run_page,
    run_page_after,
    close_device,
    close_device_after
};
fault injected = fault::none;
int triggered = 0;
int authentication_skip = 0;
fz_context *permission_context = nullptr;
int boundary_calls = 0;
int specifics_calls = 0;
bool null_pdf_passed = false;
bool fail_error_construction = false;
bool fail_invalid_construction = false;
bool fail_password_error_construction = false;
void checkpoint(fz_context *context, fault expected) {
    if (injected == expected) {
        injected = fault::none;
        ++triggered;
        fz_throw(context, FZ_ERROR_GENERIC, "injected MuPDF boundary failure");
    }
}
} // namespace
extern "C" void __real__ZNSt13runtime_errorC1EPKc(std::runtime_error *, const char *);
extern "C" void __wrap__ZNSt13runtime_errorC1EPKc(std::runtime_error *error, const char *message) {
    if (fail_error_construction) {
        fail_error_construction = false;
        throw std::bad_alloc();
    }
    __real__ZNSt13runtime_errorC1EPKc(error, message);
}
extern "C" void __real__ZNSt13runtime_errorC2EPKc(std::runtime_error *, const char *);
extern "C" void __wrap__ZNSt13runtime_errorC2EPKc(std::runtime_error *error, const char *message) {
    if (fail_password_error_construction) {
        fail_password_error_construction = false;
        throw std::bad_alloc();
    }
    __real__ZNSt13runtime_errorC2EPKc(error, message);
}
extern "C" void __real__ZNSt16invalid_argumentC1EPKc(std::invalid_argument *, const char *);
extern "C" void __wrap__ZNSt16invalid_argumentC1EPKc(std::invalid_argument *error,
                                                     const char *message) {
    if (fail_invalid_construction) {
        fail_invalid_construction = false;
        throw std::bad_alloc();
    }
    __real__ZNSt16invalid_argumentC1EPKc(error, message);
}
extern "C" {
void __real_fz_register_document_handlers(fz_context *);
void __wrap_fz_register_document_handlers(fz_context *context) {
    checkpoint(context, fault::register_handlers);
    __real_fz_register_document_handlers(context);
}
fz_stream *__real_fz_open_memory(fz_context *, const unsigned char *, size_t);
fz_stream *__wrap_fz_open_memory(fz_context *context, const unsigned char *bytes, size_t size) {
    checkpoint(context, fault::open_stream);
    return __real_fz_open_memory(context, bytes, size);
}
fz_document *__real_fz_open_document_with_stream(fz_context *, const char *, fz_stream *);
fz_document *__wrap_fz_open_document_with_stream(fz_context *context, const char *format,
                                                 fz_stream *stream) {
    checkpoint(context, fault::open_document);
    return __real_fz_open_document_with_stream(context, format, stream);
}
pdf_document *__real_pdf_specifics(fz_context *, fz_document *);
pdf_document *__wrap_pdf_specifics(fz_context *context, fz_document *document) {
    if ((injected == fault::not_pdf && specifics_calls++ == 0) ||
        (injected == fault::not_pdf_strict && specifics_calls++ == 1)) {
        injected = fault::none;
        ++triggered;
        return nullptr;
    }
    return __real_pdf_specifics(context, document);
}
int __real_fz_count_pages(fz_context *, fz_document *);
int __wrap_fz_count_pages(fz_context *context, fz_document *document) {
    checkpoint(context, fault::count_pages);
    if (injected == fault::zero_pages || injected == fault::negative_pages) {
        const int count = injected == fault::zero_pages ? 0 : -1;
        injected = fault::none;
        ++triggered;
        return count;
    }
    return __real_fz_count_pages(context, document);
}
int __real_pdf_was_repaired(fz_context *, pdf_document *);
int __wrap_pdf_was_repaired(fz_context *context, pdf_document *document) {
    if (injected == fault::repaired) {
        injected = fault::none;
        ++triggered;
        return 1;
    }
    return __real_pdf_was_repaired(context, document);
}
fz_context *__real_fz_new_context_imp(const fz_alloc_context *, const fz_locks_context *, size_t,
                                      const char *);
fz_context *__wrap_fz_new_context_imp(const fz_alloc_context *a, const fz_locks_context *l,
                                      size_t n, const char *v) {
    if (injected == fault::context) {
        injected = fault::none;
        ++triggered;
        return nullptr;
    }
    return __real_fz_new_context_imp(a, l, n, v);
}
fz_page *__real_fz_load_page(fz_context *, fz_document *, int);
fz_page *__wrap_fz_load_page(fz_context *c, fz_document *d, int p) {
    checkpoint(c, fault::load);
    if (injected == fault::region_load && boundary_calls++ > 0) {
        checkpoint(c, fault::region_load);
    }
    return __real_fz_load_page(c, d, p);
}
fz_rect __real_fz_bound_page(fz_context *, fz_page *);
fz_rect __wrap_fz_bound_page(fz_context *c, fz_page *p) {
    checkpoint(c, fault::bounds);
    if (injected == fault::region_bounds && boundary_calls++ > 0) {
        checkpoint(c, fault::region_bounds);
    }
    const auto selected = injected;
    fz_rect result{0, 0, 200, 200};
    switch (selected) {
    case fault::bounds_nan_width:
        result.x1 = std::numeric_limits<float>::quiet_NaN();
        break;
    case fault::bounds_nan_height:
        result.y1 = std::numeric_limits<float>::quiet_NaN();
        break;
    case fault::bounds_infinite_width:
        result.x1 = std::numeric_limits<float>::infinity();
        break;
    case fault::bounds_infinite_height:
        result.y1 = std::numeric_limits<float>::infinity();
        break;
    case fault::bounds_zero_width:
        result.x1 = 0;
        break;
    case fault::bounds_zero_height:
        result.y1 = 0;
        break;
    case fault::bounds_negative_width:
        result.x1 = -1;
        break;
    case fault::bounds_negative_height:
        result.y1 = -1;
        break;
    case fault::bounds_large_width:
        result.x1 = 8193;
        break;
    case fault::bounds_large_height:
        result.y1 = 8193;
        break;
    case fault::bounds_overflow_width:
        result.x0 = -std::numeric_limits<float>::max();
        result.x1 = std::numeric_limits<float>::max();
        break;
    case fault::bounds_overflow_height:
        result.y0 = -std::numeric_limits<float>::max();
        result.y1 = std::numeric_limits<float>::max();
        break;
    default:
        return __real_fz_bound_page(c, p);
    }
    injected = fault::none;
    ++triggered;
    return result;
}
pdf_page *__real_pdf_page_from_fz_page(fz_context *, fz_page *);
pdf_page *__wrap_pdf_page_from_fz_page(fz_context *context, fz_page *page) {
    checkpoint(context, fault::page_conversion);
    return __real_pdf_page_from_fz_page(context, page);
}
int __real_fz_has_permission(fz_context *, fz_document *, fz_permission);
int __wrap_fz_has_permission(fz_context *c, fz_document *d, fz_permission p) {
    checkpoint(c, fault::permission);
    permission_context = c;
    return __real_fz_has_permission(c, d, p);
}
int __real_pdf_authenticate_password(fz_context *, pdf_document *, const char *);
int __wrap_pdf_authenticate_password(fz_context *c, pdf_document *d, const char *p) {
    if (!d) {
        null_pdf_passed = true;
        fz_throw(c, FZ_ERROR_GENERIC, "null PDF passed to authentication");
    }
    if (c == permission_context &&
        (injected == fault::live_authentication || injected == fault::live_authentication_reject)) {
        const bool reject = injected == fault::live_authentication_reject;
        injected = fault::none;
        ++triggered;
        if (reject) {
            return 0;
        }
        fz_throw(c, FZ_ERROR_GENERIC, "injected live authentication failure");
    }
    if (injected == fault::authentication || injected == fault::authentication_reject) {
        if (authentication_skip > 0) {
            --authentication_skip;
        } else if (injected == fault::authentication_reject) {
            injected = fault::none;
            ++triggered;
            return 0;
        } else {
            checkpoint(c, fault::authentication);
        }
    }
    return __real_pdf_authenticate_password(c, d, p);
}
fz_pixmap *__real_fz_new_pixmap_from_page(fz_context *, fz_page *, fz_matrix, fz_colorspace *, int);
fz_pixmap *__wrap_fz_new_pixmap_from_page(fz_context *c, fz_page *p, fz_matrix m, fz_colorspace *s,
                                          int a) {
    checkpoint(c, fault::full_pixmap);
    return __real_fz_new_pixmap_from_page(c, p, m, s, a);
}
fz_pixmap *__real_pdf_new_pixmap_from_page_with_usage(fz_context *, pdf_page *, fz_matrix,
                                                      fz_colorspace *, int, const char *,
                                                      fz_box_type);
fz_pixmap *__wrap_pdf_new_pixmap_from_page_with_usage(fz_context *c, pdf_page *p, fz_matrix m,
                                                      fz_colorspace *s, int a, const char *u,
                                                      fz_box_type b) {
    checkpoint(c, fault::print_pixmap);
    return __real_pdf_new_pixmap_from_page_with_usage(c, p, m, s, a, u, b);
}
fz_pixmap *__real_fz_new_pixmap_with_bbox(fz_context *, fz_colorspace *, fz_irect, fz_separations *,
                                          int);
fz_pixmap *__wrap_fz_new_pixmap_with_bbox(fz_context *c, fz_colorspace *s, fz_irect b,
                                          fz_separations *p, int a) {
    checkpoint(c, fault::region_pixmap);
    return __real_fz_new_pixmap_with_bbox(c, s, b, p, a);
}
fz_device *__real_fz_new_draw_device(fz_context *, fz_matrix, fz_pixmap *);
fz_device *__wrap_fz_new_draw_device(fz_context *c, fz_matrix m, fz_pixmap *p) {
    checkpoint(c, fault::draw_device);
    return __real_fz_new_draw_device(c, m, p);
}
void __real_pdf_run_page_with_usage(fz_context *, pdf_page *, fz_device *, fz_matrix, const char *,
                                    fz_cookie *);
void __wrap_pdf_run_page_with_usage(fz_context *c, pdf_page *p, fz_device *d, fz_matrix m,
                                    const char *u, fz_cookie *k) {
    checkpoint(c, fault::run_page);
    __real_pdf_run_page_with_usage(c, p, d, m, u, k);
    checkpoint(c, fault::run_page_after);
}
void __real_fz_close_device(fz_context *, fz_device *);
void __wrap_fz_close_device(fz_context *c, fz_device *d) {
    checkpoint(c, fault::close_device);
    __real_fz_close_device(c, d);
    checkpoint(c, fault::close_device_after);
}
}
namespace {
template <class Op>
void verify(fault which, Op op, const vellum::pdf_document &document, const std::string &original) {
    triggered = 0;
    boundary_calls = 0;
    specifics_calls = 0;
    injected = which;
    bool failed = false;
    try {
        op();
    } catch (const std::runtime_error &) {
        failed = true;
    }
    injected = fault::none;
    if (!failed || triggered != 1 || document.snapshot() != original || document.pages() != 2)
        throw std::runtime_error("MuPDF failure not propagated or changed document: " +
                                 std::to_string(static_cast<int>(which)));
    op();
    if (document.snapshot() != original)
        throw std::runtime_error("retry changed document");
}
template <class Op>
void verify_error_allocation(fault which, Op op, const vellum::pdf_document &document,
                             const std::string &original) {
    triggered = 0;
    boundary_calls = 0;
    injected = which;
    fail_error_construction = true;
    bool failed = false;
    try {
        op();
    } catch (const std::bad_alloc &) {
        failed = true;
    }
    injected = fault::none;
    const bool reached = !fail_error_construction;
    fail_error_construction = false;
    if (!failed || !reached || triggered != 1 || document.snapshot() != original ||
        document.pages() != 2)
        throw std::runtime_error("PDF error-construction failure changed the document");
    op();
    if (document.snapshot() != original)
        throw std::runtime_error("PDF error-construction failure prevented safe retry");
}
template <class Op>
void verify_invalid_error_allocation(Op op, const vellum::pdf_document &document,
                                     const std::string &original) {
    fail_invalid_construction = true;
    bool allocation_failed = false;
    try {
        op();
    } catch (const std::bad_alloc &) {
        allocation_failed = true;
    }
    const bool reached = !fail_invalid_construction;
    fail_invalid_construction = false;
    bool rejected = false;
    try {
        op();
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    if (!allocation_failed || !reached || !rejected || document.snapshot() != original ||
        document.pages() != 2)
        throw std::runtime_error("PDF invalid-input error allocation changed state or retry");
}
template <class Op>
void verify_denial_error_allocation(Op op, const vellum::pdf_document &document,
                                    const std::string &original) {
    const bool allowed_print = document.may_print();
    const bool allowed_edit = document.may_edit();
    fail_error_construction = true;
    bool allocation_failed = false;
    try {
        op();
    } catch (const std::bad_alloc &) {
        allocation_failed = true;
    }
    const bool reached = !fail_error_construction;
    fail_error_construction = false;
    bool rejected = false;
    try {
        op();
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    if (!allocation_failed || !reached || !rejected || document.snapshot() != original ||
        document.may_print() != allowed_print || document.may_edit() != allowed_edit)
        throw std::runtime_error("PDF permission-denial allocation changed state or retry");
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    try {
        vellum::pdf_document document(argv[1]);
        const auto original = document.snapshot();
        {
            const auto protected_path =
                std::filesystem::path(argv[2]).parent_path() / "password.pdf";
            fail_password_error_construction = true;
            bool failed = false;
            try {
                vellum::pdf_document protected_document(protected_path, "wrong");
            } catch (const std::bad_alloc &) {
                failed = true;
            }
            const bool reached = !fail_password_error_construction;
            fail_password_error_construction = false;
            bool rejected = false;
            try {
                vellum::pdf_document protected_document(protected_path, "wrong");
            } catch (const vellum::pdf_password_error &) {
                rejected = true;
            }
            vellum::pdf_document recovered(protected_path, "test-user");
            if (!failed || !reached || !rejected || recovered.pages() != 2 ||
                document.snapshot() != original)
                throw std::runtime_error("password error allocation prevented safe recovery");
        }
        verify_invalid_error_allocation(
            [&] { vellum::pdf_document candidate(argv[1], std::string_view("x\0y", 3)); }, document,
            original);
        verify_invalid_error_allocation([&] { (void)document.dimensions(-1); }, document, original);
        verify_invalid_error_allocation([&] { (void)document.render(-1, 1); }, document, original);
        verify_invalid_error_allocation(
            [&] { (void)document.render_print_region(0, {0, 0, 1, 1}, 1, 35); }, document,
            original);
        verify_invalid_error_allocation(
            [&] {
                (void)document.render_region(0, {0, 0, std::numeric_limits<double>::quiet_NaN(), 1},
                                             1);
            },
            document, original);
        verify_invalid_error_allocation([&] { (void)document.render_region(0, {-1, 0, 1, 1}, 1); },
                                        document, original);
        verify(
            fault::context, [&] { vellum::pdf_document candidate(argv[1]); }, document, original);
        verify(
            fault::authentication, [&] { vellum::pdf_document candidate(argv[1]); }, document,
            original);
        for (const auto failure :
             {fault::register_handlers, fault::open_stream, fault::open_document,
              fault::count_pages, fault::zero_pages, fault::negative_pages}) {
            verify(failure, [&] { vellum::pdf_document candidate(argv[1]); }, document, original);
        }
        null_pdf_passed = false;
        verify(
            fault::not_pdf, [&] { vellum::pdf_document candidate(argv[1]); }, document, original);
        if (null_pdf_passed)
            throw std::runtime_error("non-PDF document was passed to password authentication");
        verify(
            fault::not_pdf_strict, [&] { document.validate_recovery_snapshot(original); }, document,
            original);
        for (const auto failure : {fault::bounds_large_width, fault::bounds_large_height,
                                   fault::bounds_overflow_width, fault::bounds_overflow_height}) {
            verify(failure, [&] { (void)document.render(0, 1); }, document, original);
        }
        verify(
            fault::repaired, [&] { document.validate_recovery_snapshot(original); }, document,
            original);
        for (const auto failure : {fault::context, fault::register_handlers, fault::open_stream,
                                   fault::open_document, fault::count_pages}) {
            verify_error_allocation(
                failure, [&] { vellum::pdf_document candidate(argv[1]); }, document, original);
        }
        verify_error_allocation(
            fault::permission, [&] { (void)document.may_copy(); }, document, original);
        for (const auto failure : {fault::load, fault::bounds, fault::full_pixmap}) {
            verify_error_allocation(
                failure, [&] { (void)document.render(0, 1); }, document, original);
        }
        for (const auto failure :
             {fault::region_pixmap, fault::draw_device, fault::run_page, fault::run_page_after,
              fault::close_device, fault::close_device_after}) {
            verify_error_allocation(
                failure, [&] { (void)document.render_region(0, {10, 20, 30, 40}, 1); }, document,
                original);
        }
        verify(fault::permission, [&] { (void)document.may_copy(); }, document, original);
        for (auto f : {fault::load, fault::bounds}) {
            verify(f, [&] { (void)document.dimensions(0); }, document, original);
            verify(f, [&] { (void)document.render(0, 1); }, document, original);
        }
        for (const auto failure :
             {fault::bounds_nan_width, fault::bounds_nan_height, fault::bounds_infinite_width,
              fault::bounds_infinite_height, fault::bounds_zero_width, fault::bounds_zero_height,
              fault::bounds_negative_width, fault::bounds_negative_height}) {
            verify(failure, [&] { (void)document.dimensions(0); }, document, original);
            verify(failure, [&] { (void)document.render(0, 1); }, document, original);
        }
        verify(fault::full_pixmap, [&] { (void)document.render(0, 1); }, document, original);
        verify(
            fault::print_pixmap, [&] { (void)document.render_for_print(0, 1); }, document,
            original);
        for (auto f : {fault::region_pixmap, fault::draw_device, fault::run_page,
                       fault::run_page_after, fault::close_device, fault::close_device_after}) {
            verify(
                f, [&] { (void)document.render_region(0, {10, 20, 30, 40}, 1); }, document,
                original);
            verify(
                f, [&] { (void)document.render_print_region(0, {10, 20, 30, 40}, 1, 72); },
                document, original);
        }
        for (const auto failure :
             {fault::region_load, fault::region_bounds, fault::page_conversion}) {
            verify(
                failure, [&] { (void)document.render_region(0, {10, 20, 30, 40}, 1); }, document,
                original);
            verify(
                failure, [&] { (void)document.render_print_region(0, {10, 20, 30, 40}, 1, 72); },
                document, original);
        }
        verify(
            fault::page_conversion, [&] { (void)document.render_for_print(0, 1); }, document,
            original);
        for (const auto failure : {fault::authentication, fault::authentication_reject,
                                   fault::live_authentication, fault::live_authentication_reject}) {
            vellum::pdf_document restricted(argv[2]);
            const auto bytes = restricted.snapshot();
            if (restricted.may_edit() || restricted.may_print())
                throw std::runtime_error("restricted fixture unexpectedly grants owner rights");
            triggered = 0;
            authentication_skip = 1;
            injected = failure;
            bool threw = false;
            bool accepted = false;
            try {
                accepted = restricted.authenticate_owner("test-owner");
            } catch (const std::runtime_error &) {
                threw = true;
            }
            injected = fault::none;
            const bool expect_throw =
                failure == fault::authentication || failure == fault::live_authentication;
            if (triggered != 1 || accepted || threw != expect_throw ||
                restricted.snapshot() != bytes || restricted.may_edit() || restricted.may_print())
                throw std::runtime_error("failed owner authentication changed authorization");
            if (!restricted.authenticate_owner("test-owner") || !restricted.may_edit() ||
                !restricted.may_print() || restricted.snapshot() != bytes)
                throw std::runtime_error("owner authentication failed to recover");
        }
        {
            vellum::pdf_document restricted(argv[2]);
            const auto bytes = restricted.snapshot();
            if (restricted.may_edit())
                throw std::runtime_error("restricted fixture unexpectedly permits editing");
            triggered = 0;
            injected = fault::live_authentication;
            fail_error_construction = true;
            bool failed = false;
            try {
                (void)restricted.authenticate_owner("test-owner");
            } catch (const std::bad_alloc &) {
                failed = true;
            }
            injected = fault::none;
            const bool reached = !fail_error_construction;
            fail_error_construction = false;
            if (!failed || !reached || triggered != 1 || restricted.snapshot() != bytes ||
                restricted.may_edit() || restricted.may_print())
                throw std::runtime_error("live authentication error allocation changed rights");
            if (!restricted.authenticate_owner("test-owner") || !restricted.may_edit() ||
                restricted.snapshot() != bytes)
                throw std::runtime_error("live authentication error allocation prevented retry");
        }
        {
            vellum::pdf_document restricted(argv[2]);
            const auto bytes = restricted.snapshot();
            verify_denial_error_allocation([&] { (void)restricted.render_for_print(0, 1); },
                                           restricted, bytes);
            verify_denial_error_allocation(
                [&] { (void)restricted.render_print_region(0, {0, 0, 1, 1}, 1, 72); }, restricted,
                bytes);
            vellum::pdf_document read_only(std::filesystem::path(argv[2]).parent_path() /
                                           "read-only-copy.pdf");
            const auto read_only_bytes = read_only.snapshot();
            verify_denial_error_allocation([&] { read_only.save(); }, read_only, read_only_bytes);
            vellum::pdf_document low_quality(std::filesystem::path(argv[2]).parent_path() /
                                             "low-quality-print.pdf");
            const auto low_quality_bytes = low_quality.snapshot();
            if (!low_quality.may_print() || low_quality.may_print_high_quality())
                throw std::runtime_error("low-quality print fixture permissions differ");
            verify_denial_error_allocation([&] { (void)low_quality.render_for_print(0, 3); },
                                           low_quality, low_quality_bytes);
            verify_denial_error_allocation(
                [&] { (void)low_quality.render_print_region(0, {0, 0, 1, 1}, 1, 151); },
                low_quality, low_quality_bytes);
        }
        const auto image = document.render(0, 1);
        if (image.width != 200 || image.height != 200 || image.rgb.at(0) != 255 ||
            image.rgb.at(1) != 0)
            throw std::runtime_error("retry output differs from reference fixture");
        std::cout << "MuPDF boundary failure and recovery cases passed\n";
    } catch (const std::exception &e) {
        injected = fault::none;
        std::cerr << e.what() << '\n';
        return 1;
    }
}
