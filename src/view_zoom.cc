#include "vellum/view_zoom.hh"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace vellum {
double calculate_view_scale(zoom_mode mode, page_extent page, page_extent viewport,
                            double percentage) {
    if (!std::isfinite(page.width) || !std::isfinite(page.height) || page.width <= 0 ||
        page.height <= 0 || !std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
        viewport.width <= 0 || viewport.height <= 0 || !std::isfinite(percentage) ||
        percentage < 1 || percentage > 800) {
        throw std::invalid_argument("invalid zoom dimensions or percentage (1–800%)");
    }
    switch (mode) {
    case zoom_mode::percentage:
        return percentage / 100.0;
    case zoom_mode::fit_page:
        return std::min(viewport.width / page.width, viewport.height / page.height);
    case zoom_mode::fit_width:
        return viewport.width / page.width;
    case zoom_mode::fit_height:
        return viewport.height / page.height;
    }
    throw std::invalid_argument("invalid zoom mode");
}
} // namespace vellum
