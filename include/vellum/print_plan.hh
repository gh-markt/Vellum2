#pragma once
#include <string_view>
#include <vector>
namespace vellum {
enum class page_subset { all, odd, even };
// Document pages are one-based; zero represents an inserted blank. Order and
// repeated pages follow the expression. Odd/even filters use positions in the
// complete selection, including inserted blanks.
std::vector<int> select_print_pages(std::string_view ranges, int page_count,
                                    page_subset subset = page_subset::all);
} // namespace vellum

#include "vellum/geometry.hh"
namespace vellum {
enum class print_layout { single, multiple, booklet, poster };
enum class print_scale { fit, shrink, actual, custom };
enum class print_duplex { simplex, long_edge, short_edge, manual };
enum class nup_order { across, down, across_reverse, down_reverse };
enum class sheet_subset { all, fronts, backs };
struct print_options {
    page_extent paper{595.2756, 841.8898};
    page_rectangle printable{28.3465, 28.3465, 538.5826, 785.1968};
    print_layout layout = print_layout::single;
    print_scale scaling = print_scale::fit;
    print_duplex duplex = print_duplex::simplex;
    sheet_subset sides = sheet_subset::all;
    double percentage = 100;
    double overlap = 14.1732;
    double gutter = 0;
    nup_order order = nup_order::across;
    bool borders = false;
    bool grayscale = false;
    bool reverse_backs = false;
    int columns = 2;
    int rows = 1;
    int copies = 1;
    bool collate = true;
    bool reverse = false;
    bool auto_rotate = true;
    bool right_binding = false;
    bool cut_marks = false;
    bool tile_labels = true;
};
struct print_placement {
    int page;
    page_rectangle destination;
    page_rectangle clip;
    bool clockwise;
};
struct print_side {
    int sheet;
    int copy;
    bool back;
    int tile_column = 0;
    int tile_row = 0;
    std::vector<print_placement> placements;
};
struct print_job {
    print_options options;
    std::vector<print_side> sides;
};
print_job compose_print_job(const std::vector<page_extent> &document, const std::vector<int> &pages,
                            const print_options &options);
} // namespace vellum
