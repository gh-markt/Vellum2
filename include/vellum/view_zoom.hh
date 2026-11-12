#pragma once
#include "vellum/geometry.hh"
namespace vellum {
enum class zoom_mode { percentage, fit_page, fit_width, fit_height };
double calculate_view_scale(zoom_mode mode, page_extent page, page_extent viewport,
                            double percentage = 100.0);
} // namespace vellum
