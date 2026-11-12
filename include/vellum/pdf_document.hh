#pragma once
#include "vellum/geometry.hh"
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace vellum {
struct page_image {
    int width;
    int height;
    std::vector<unsigned char> rgb;
    page_rectangle raster_area{};
};
class pdf_password_error : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class pdf_document {
  public:
    explicit pdf_document(const std::filesystem::path &path, std::string_view password = {});
    ~pdf_document();
    pdf_document(const pdf_document &) = delete;
    pdf_document &operator=(const pdf_document &) = delete;
    int pages() const;
    page_extent dimensions(int page) const;
    bool may_print() const;
    bool may_print_high_quality() const;
    page_image render(int page, float scale) const;
    page_image render_for_print(int page, float scale) const;
    page_image render_region(int page, page_rectangle region, float scale) const;
    page_image render_print_region(int page, page_rectangle region, float scale,
                                   int output_dpi) const;
    bool may_copy() const;
    bool may_edit() const;
    bool may_fill_forms() const;
    bool may_annotate() const;
    bool owner_password_valid(std::string_view password) const;
    bool authenticate_owner(std::string_view password);
    void validate_recovery_snapshot(const std::string &bytes) const;
    bool may_save() const;
    bool may_save_as() const;
    void save() const;
    void save_as(const std::filesystem::path &target);
    const std::filesystem::path &path() const;
    std::string snapshot() const;
    void save_copy(const std::filesystem::path &target) const;
    static void validate_snapshot(const std::string &bytes);

  private:
    page_image render_page(int page, float scale, bool print) const;
    page_image render_region_impl(int page, page_rectangle region, float scale, bool print) const;
    class impl;
    std::filesystem::path source;
    std::unique_ptr<impl> implementation;
};
} // namespace vellum
