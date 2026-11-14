#include "vellum/pdf_document.hh"
#include "vellum/atomic_file.hh"
#include <cmath>
#include <cstring>
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <stdexcept>
namespace vellum {
namespace {
void consume_pdf_error(fz_context *context) {
    int code = 0;
    fz_convert_error(context, &code);
}
} // namespace
class pdf_document::impl {
  public:
    explicit impl(std::string bytes, bool strict = false, std::string_view password = {})
        : bytes(std::move(bytes)), open_password(password) {
        if (password.find('\0') != std::string_view::npos) {
            throw std::invalid_argument("password contains a null byte");
        }
        this->context = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
        if (this->context == nullptr) {
            throw std::runtime_error("MuPDF context allocation failed");
        }
        bool password_failure = false;
        fz_var(password_failure);
        fz_try(this->context) {
            fz_register_document_handlers(this->context);
            this->stream = fz_open_memory(
                this->context, reinterpret_cast<const unsigned char *>(this->bytes.data()),
                this->bytes.size());
            this->document =
                fz_open_document_with_stream(this->context, "application/pdf", this->stream);
            auto *pdf = pdf_specifics(this->context, this->document);
            if (pdf == nullptr) {
                fz_throw(this->context, FZ_ERROR_GENERIC, "opened document is not a PDF");
            }
            const int authentication =
                pdf_authenticate_password(this->context, pdf, this->open_password.c_str());
            if (authentication == 0) {
                password_failure = true;
                fz_throw(this->context, FZ_ERROR_GENERIC, "PDF requires a valid password");
            }
            this->owner_authorized = (authentication & 4) != 0;
            if (strict) {
                auto *pdf = pdf_specifics(this->context, this->document);
                if (pdf == nullptr || pdf_was_repaired(this->context, pdf)) {
                    fz_throw(this->context, FZ_ERROR_GENERIC, "recovery PDF required repair");
                }
            }
            this->page_count = fz_count_pages(this->context, this->document);
            if (this->page_count <= 0) {
                fz_throw(this->context, FZ_ERROR_GENERIC, "PDF has no pages");
            }
        }
        fz_catch(this->context) {
            consume_pdf_error(this->context);
            this->release();
            if (password_failure) {
                throw pdf_password_error("PDF requires a valid password");
            }
            throw std::runtime_error("cannot open PDF (invalid or empty)");
        }
    }
    ~impl() { this->release(); }
    impl(const impl &) = delete;
    impl &operator=(const impl &) = delete;
    void release() {
        fz_drop_document(this->context, this->document);
        fz_drop_stream(this->context, this->stream);
        fz_drop_context(this->context);
        this->document = nullptr;
        this->stream = nullptr;
        this->context = nullptr;
    }
    bool permits(fz_permission permission) const {
        if (this->owner_authorized) {
            return true;
        }
        int allowed = 0;
        fz_var(allowed);
        fz_try(this->context) {
            allowed = fz_has_permission(this->context, this->document, permission);
        }
        fz_catch(this->context) {
            consume_pdf_error(this->context);
            throw std::runtime_error("cannot inspect PDF permissions");
        }
        return allowed != 0;
    }
    std::string bytes;
    std::string open_password;
    bool owner_authorized = false;
    fz_context *context = nullptr;
    fz_stream *stream = nullptr;
    fz_document *document = nullptr;
    int page_count = 0;
};
pdf_document::pdf_document(const std::filesystem::path &path, std::string_view password)
    : source(std::filesystem::absolute(path)),
      implementation(std::make_unique<impl>(read_file(path), false, password)) {}
pdf_document::~pdf_document() = default;
int pdf_document::pages() const {
    return this->implementation->page_count;
}
page_extent pdf_document::dimensions(int page) const {
    if (page < 0 || page >= this->pages()) {
        throw std::invalid_argument("invalid page number");
    }
    auto *context = this->implementation->context;
    fz_page *loaded = nullptr;
    fz_rect bounds{};
    fz_var(loaded);
    fz_var(bounds);
    fz_try(context) {
        loaded = fz_load_page(context, this->implementation->document, page);
        bounds = fz_bound_page(context, loaded);
    }
    fz_always(context) {
        fz_drop_page(context, loaded);
    }
    fz_catch(context) {
        consume_pdf_error(context);
        throw std::runtime_error("cannot inspect page dimensions");
    }
    const page_extent result{bounds.x1 - bounds.x0, bounds.y1 - bounds.y0};
    if (!std::isfinite(result.width) || !std::isfinite(result.height) || result.width <= 0 ||
        result.height <= 0) {
        throw std::runtime_error("invalid PDF page dimensions");
    }
    return result;
}
bool pdf_document::may_print() const {
    return this->implementation->permits(FZ_PERMISSION_PRINT);
}
bool pdf_document::may_print_high_quality() const {
    return this->implementation->permits(FZ_PERMISSION_PRINT_HQ);
}
bool pdf_document::may_copy() const {
    return this->implementation->permits(FZ_PERMISSION_COPY);
}
page_image pdf_document::render(int page, float scale) const {
    return this->render_page(page, scale, false);
}
page_image pdf_document::render_for_print(int page, float scale) const {
    if (!this->may_print()) {
        throw std::runtime_error("PDF permissions prohibit printing");
    }
    if (!this->may_print_high_quality() && scale > 150.0F / 72.0F + 0.000001F) {
        throw std::runtime_error("PDF permissions prohibit high-resolution printing");
    }
    return this->render_page(page, scale, true);
}
page_image pdf_document::render_page(int page, float scale, bool print) const {
    if (page < 0 || page >= this->pages() || !std::isfinite(scale) || scale < 0.000001F ||
        scale > 10000.0F) {
        throw std::invalid_argument("invalid page or render scale");
    }
    auto *context = this->implementation->context;
    fz_pixmap *pixmap = nullptr;
    fz_page *loaded = nullptr;
    fz_var(pixmap);
    fz_var(loaded);
    const auto transform = fz_scale(scale, scale);
    fz_try(context) {
        loaded = fz_load_page(context, this->implementation->document, page);
        const auto bounds = fz_transform_rect(fz_bound_page(context, loaded), transform);
        if (!std::isfinite(bounds.x1 - bounds.x0) || !std::isfinite(bounds.y1 - bounds.y0) ||
            bounds.x1 - bounds.x0 <= 0 || bounds.y1 - bounds.y0 <= 0 ||
            bounds.x1 - bounds.x0 > 8192 || bounds.y1 - bounds.y0 > 8192) {
            fz_throw(context, FZ_ERROR_GENERIC, "page exceeds render bounds");
        }
        if (print) {
            pixmap = pdf_new_pixmap_from_page_with_usage(
                context, pdf_page_from_fz_page(context, loaded), transform, fz_device_rgb(context),
                0, "Print", FZ_CROP_BOX);
        } else {
            pixmap = fz_new_pixmap_from_page(context, loaded, transform, fz_device_rgb(context), 0);
        }
    }
    fz_always(context) {
        fz_drop_page(context, loaded);
    }
    fz_catch(context) {
        consume_pdf_error(context);
        fz_drop_pixmap(context, pixmap);
        throw std::runtime_error("PDF rendering failed");
    }
    try {
        const int width = fz_pixmap_width(context, pixmap);
        const int height = fz_pixmap_height(context, pixmap);
        page_image result{width, height,
                          std::vector<unsigned char>(static_cast<std::size_t>(width) * height * 3)};
        const auto *samples = fz_pixmap_samples(context, pixmap);
        const int stride = fz_pixmap_stride(context, pixmap);
        for (int row = 0; row < height; ++row) {
            std::memcpy(result.rgb.data() + static_cast<std::size_t>(row) * width * 3,
                        samples + static_cast<std::size_t>(row) * stride,
                        static_cast<std::size_t>(width) * 3);
        }
        fz_drop_pixmap(context, pixmap);
        return result;
    } catch (...) {
        fz_drop_pixmap(context, pixmap);
        throw;
    }
}
bool pdf_document::may_fill_forms() const {
    return this->implementation->permits(FZ_PERMISSION_FORM);
}
bool pdf_document::may_annotate() const {
    return this->implementation->permits(FZ_PERMISSION_ANNOTATE);
}
page_image pdf_document::render_print_region(int page, page_rectangle region, float scale,
                                             int output_dpi) const {
    if (output_dpi < 36 || output_dpi > 600) {
        throw std::invalid_argument("invalid print resolution");
    }
    if (!this->may_print() || (output_dpi > 150 && !this->may_print_high_quality())) {
        throw std::runtime_error("PDF permissions prohibit this printing operation");
    }
    return this->render_region_impl(page, region, scale, true);
}
page_image pdf_document::render_region(int page, page_rectangle region, float scale) const {
    return this->render_region_impl(page, region, scale, false);
}
page_image pdf_document::render_region_impl(int page, page_rectangle region, float scale,
                                            bool print) const {
    const auto size = this->dimensions(page);
    for (const double value :
         {region.x, region.y, region.width, region.height, static_cast<double>(scale)}) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("non-finite raster region");
        }
    }
    if (scale <= 0 || scale > 10000 || region.x < 0 || region.y < 0 || region.width <= 0 ||
        region.height <= 0 || region.x + region.width > size.width + 0.001 ||
        region.y + region.height > size.height + 0.001 || region.width * scale > 8190 ||
        region.height * scale > 8190) {
        throw std::invalid_argument("invalid or excessive print raster region");
    }
    auto *context = this->implementation->context;
    fz_page *loaded = nullptr;
    fz_pixmap *pixmap = nullptr;
    fz_device *device = nullptr;
    fz_rect bounds{};
    fz_irect pixels{};
    fz_var(loaded);
    fz_var(pixmap);
    fz_var(device);
    fz_var(bounds);
    fz_var(pixels);
    const auto transform = fz_scale(scale, scale);
    fz_try(context) {
        loaded = fz_load_page(context, this->implementation->document, page);
        bounds = fz_bound_page(context, loaded);
        const fz_rect requested{static_cast<float>(bounds.x0 + region.x),
                                static_cast<float>(bounds.y0 + region.y),
                                static_cast<float>(bounds.x0 + region.x + region.width),
                                static_cast<float>(bounds.y0 + region.y + region.height)};
        pixels = fz_round_rect(fz_transform_rect(requested, transform));
        if (pixels.x1 <= pixels.x0 || pixels.y1 <= pixels.y0 || pixels.x1 - pixels.x0 > 8192 ||
            pixels.y1 - pixels.y0 > 8192) {
            fz_throw(context, FZ_ERROR_GENERIC, "raster region is empty or too large");
        }
        pixmap = fz_new_pixmap_with_bbox(context, fz_device_rgb(context), pixels, nullptr, 0);
        fz_clear_pixmap_with_value(context, pixmap, 255);
        device = fz_new_draw_device(context, fz_identity, pixmap);
        pdf_run_page_with_usage(context, pdf_page_from_fz_page(context, loaded), device, transform,
                                print ? "Print" : "View", nullptr);
        fz_close_device(context, device);
    }
    fz_always(context) {
        fz_drop_device(context, device);
        fz_drop_page(context, loaded);
    }
    fz_catch(context) {
        consume_pdf_error(context);
        fz_drop_pixmap(context, pixmap);
        throw std::runtime_error("PDF region rendering failed");
    }
    try {
        const int width = fz_pixmap_width(context, pixmap),
                  height = fz_pixmap_height(context, pixmap);
        page_image result{width, height,
                          std::vector<unsigned char>(static_cast<std::size_t>(width) * height * 3)};
        const auto *source = fz_pixmap_samples(context, pixmap);
        const int stride = fz_pixmap_stride(context, pixmap);
        for (int row = 0; row < height; ++row) {
            std::memcpy(result.rgb.data() + static_cast<std::size_t>(row) * width * 3,
                        source + static_cast<std::size_t>(row) * stride,
                        static_cast<std::size_t>(width) * 3);
        }
        result.raster_area = {pixels.x0 / static_cast<double>(scale) - bounds.x0,
                              pixels.y0 / static_cast<double>(scale) - bounds.y0,
                              width / static_cast<double>(scale),
                              height / static_cast<double>(scale)};
        fz_drop_pixmap(context, pixmap);
        return result;
    } catch (...) {
        fz_drop_pixmap(context, pixmap);
        throw;
    }
}
bool pdf_document::owner_password_valid(std::string_view password) const {
    try {
        const impl candidate(this->implementation->bytes, false, password);
        return candidate.owner_authorized;
    } catch (const pdf_password_error &) {
        return false;
    }
}
bool pdf_document::authenticate_owner(std::string_view password) {
    if (!this->owner_password_valid(password)) {
        return false;
    }
    std::string text(password);
    auto *context = this->implementation->context;
    int authenticated = 0;
    fz_var(authenticated);
    fz_try(context) {
        authenticated = pdf_authenticate_password(
            context, pdf_specifics(context, this->implementation->document), text.c_str());
    }
    fz_catch(context) {
        consume_pdf_error(context);
        throw std::runtime_error("owner authentication failed");
    }
    if ((authenticated & 4) == 0) {
        return false;
    }
    this->implementation->open_password.swap(text);
    this->implementation->owner_authorized = true;
    return true;
}
void pdf_document::validate_recovery_snapshot(const std::string &bytes) const {
    const impl candidate(bytes, true, this->implementation->open_password);
}
bool pdf_document::may_save_as() const {
    // The standard extraction permission is not a prohibition on copying the
    // encrypted file. The bootstrap preserves its exact security envelope.
    return true;
}
bool pdf_document::may_edit() const {
    return this->implementation->permits(FZ_PERMISSION_EDIT);
}
bool pdf_document::may_save() const {
    return this->may_edit() || this->may_fill_forms() || this->may_annotate();
}
void pdf_document::save() const {
    if (!this->may_save()) {
        throw std::runtime_error("PDF permissions prohibit saving");
    }
    if (read_file(this->source) != this->implementation->bytes) {
        throw std::runtime_error("original file changed externally; use Save As to another file");
    }
    const auto bytes = this->snapshot();
    if (bytes == this->implementation->bytes) {
        return;
    }
    atomic_write(this->source, bytes);
}
const std::filesystem::path &pdf_document::path() const {
    return this->source;
}
void pdf_document::save_as(const std::filesystem::path &target) {
    auto destination = std::filesystem::absolute(target);
    this->save_copy(destination);
    this->source.swap(destination);
}
std::string pdf_document::snapshot() const {
    // The bootstrap has no PDF mutations: preserve the exact loaded bytes, including signatures.
    // Stage 2 must serialize the live PDF model before adding any mutation operation.
    return this->implementation->bytes;
}
void pdf_document::save_copy(const std::filesystem::path &target) const {
    std::error_code error;
    if (std::filesystem::equivalent(target, this->source, error) ||
        std::filesystem::weakly_canonical(target) ==
            std::filesystem::weakly_canonical(this->source)) {
        this->save();
        return;
    }
    atomic_write(target, this->snapshot());
}
void pdf_document::validate_snapshot(const std::string &bytes) {
    const impl candidate(bytes, true);
}
} // namespace vellum
