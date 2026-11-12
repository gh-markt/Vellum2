#include "vellum/print_plan.hh"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace vellum {
namespace {
constexpr std::size_t maximum_sides = 10000;
void validate_options(const print_options &options) {
    if (options.layout == print_layout::poster && options.duplex != print_duplex::simplex) {
        throw std::invalid_argument("poster tiles require one-sided printing");
    }
    const auto &box = options.printable;
    for (const auto value : {options.paper.width, options.paper.height, box.x, box.y, box.width,
                             box.height, options.percentage, options.overlap, options.gutter}) {
        if (!std::isfinite(value)) {
            throw std::invalid_argument("non-finite print setting");
        }
    }
    if (options.paper.width <= 0 || options.paper.height <= 0 || options.paper.width > 14400 ||
        options.paper.height > 14400 || box.x < 0 || box.y < 0 || box.width <= 0 ||
        box.height <= 0 || box.x + box.width > options.paper.width + 0.01 ||
        box.y + box.height > options.paper.height + 0.01 || options.percentage < 1 ||
        options.percentage > 1000 || options.overlap < 0 || options.gutter < 0 ||
        options.order < nup_order::across || options.order > nup_order::down_reverse ||
        options.columns < 1 || options.columns > 10 || options.rows < 1 || options.rows > 10 ||
        options.copies < 1 || options.copies > 100 || options.layout < print_layout::single ||
        options.layout > print_layout::poster || options.scaling < print_scale::fit ||
        options.scaling > print_scale::custom || options.duplex < print_duplex::simplex ||
        options.duplex > print_duplex::manual || options.sides < sheet_subset::all ||
        options.sides > sheet_subset::backs) {
        throw std::invalid_argument("invalid paper, printable bounds or print options");
    }
}
void add_side(std::vector<print_side> &output, print_side side) {
    if (output.size() >= maximum_sides) {
        throw std::invalid_argument("print job exceeds 10000 sides");
    }
    output.push_back(std::move(side));
}
print_placement place_page(int page, const std::vector<page_extent> &document, page_rectangle cell,
                           const print_options &options) {
    if (page == 0) {
        return {0, cell, cell, false};
    }
    const auto size = document[static_cast<std::size_t>(page - 1)];
    const auto normal = std::min(cell.width / size.width, cell.height / size.height);
    const auto rotated = std::min(cell.width / size.height, cell.height / size.width);
    const bool turn = options.auto_rotate && rotated > normal;
    const double width = turn ? size.height : size.width;
    const double height = turn ? size.width : size.height;
    double scale = turn ? rotated : normal;
    if (options.scaling == print_scale::actual) {
        scale = 1;
    }
    if (options.scaling == print_scale::shrink) {
        scale = std::min(1.0, scale);
    }
    if (options.scaling == print_scale::custom) {
        scale = options.percentage / 100;
    }
    const page_rectangle destination{cell.x + (cell.width - width * scale) / 2,
                                     cell.y + (cell.height - height * scale) / 2, width * scale,
                                     height * scale};
    return {page, destination, cell, turn};
}
page_rectangle grid_cell(const print_options &options, int slot, int columns, int rows) {
    int row = slot / columns;
    int column = slot % columns;
    if (options.layout == print_layout::multiple) {
        switch (options.order) {
        case nup_order::across:
            break;
        case nup_order::down:
            row = slot % rows;
            column = slot / rows;
            break;
        case nup_order::across_reverse:
            column = columns - 1 - column;
            break;
        case nup_order::down_reverse:
            row = rows - 1 - slot % rows;
            column = slot / rows;
            break;
        }
    }
    const auto area = options.printable;
    const double width = (area.width - (columns - 1) * options.gutter) / columns;
    const double height = (area.height - (rows - 1) * options.gutter) / rows;
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("gutter leaves no printable area");
    }
    return {area.x + column * (width + options.gutter), area.y + row * (height + options.gutter),
            width, height};
}
std::vector<print_side> regular_sides(const std::vector<page_extent> &document,
                                      const std::vector<int> &pages, const print_options &options) {
    const int columns = options.layout == print_layout::multiple ? options.columns : 1;
    const int rows = options.layout == print_layout::multiple ? options.rows : 1;
    const int slots = columns * rows;
    const bool duplex = options.duplex != print_duplex::simplex;
    std::vector<print_side> output;
    for (std::size_t start = 0; start < pages.size(); start += slots) {
        const int index = static_cast<int>(output.size());
        print_side side{duplex ? index / 2 : index, 0, duplex && index % 2 == 1, 0, 0, {}};
        for (int slot = 0; slot < slots && start + slot < pages.size(); ++slot) {
            side.placements.push_back(place_page(pages[start + slot], document,
                                                 grid_cell(options, slot, columns, rows), options));
        }
        add_side(output, std::move(side));
    }
    if (duplex && output.size() % 2 == 1) {
        add_side(output, {output.back().sheet, 0, true, 0, 0, {}});
    }
    return output;
}
std::vector<print_side> booklet_sides(const std::vector<page_extent> &document,
                                      const std::vector<int> &pages, const print_options &options) {
    const std::size_t padded = ((pages.size() + 3) / 4) * 4;
    std::vector<print_side> output;
    const auto page_at = [&](std::size_t index) { return index < pages.size() ? pages[index] : 0; };
    for (std::size_t sheet = 0; sheet < padded / 4; ++sheet) {
        for (int back = 0; back < 2; ++back) {
            const std::size_t left = back == 0 ? padded - 1 - 2 * sheet : 2 * sheet + 1;
            const std::size_t right = back == 0 ? 2 * sheet : padded - 2 - 2 * sheet;
            print_side side{static_cast<int>(sheet), 0, back == 1, 0, 0, {}};
            for (int slot = 0; slot < 2; ++slot) {
                const bool use_left = (slot == 0) != options.right_binding;
                side.placements.push_back(place_page(page_at(use_left ? left : right), document,
                                                     grid_cell(options, slot, 2, 1), options));
            }
            add_side(output, std::move(side));
        }
    }
    return output;
}
std::vector<print_side> poster_sides(const std::vector<page_extent> &document,
                                     const std::vector<int> &pages, const print_options &options) {
    std::vector<print_side> output;
    auto clip = options.printable;
    if (options.tile_labels) {
        clip.height -= 14;
    }
    if (clip.width <= options.overlap || clip.height <= options.overlap) {
        throw std::invalid_argument("tile overlap/labels leave no printable area");
    }
    for (const int page : pages) {
        const auto size = document[static_cast<std::size_t>(page - 1)];
        const double width = size.width * options.percentage / 100;
        const double height = size.height * options.percentage / 100;
        const double columns =
            std::max(1.0, std::ceil((width - options.overlap) / (clip.width - options.overlap)));
        const double rows =
            std::max(1.0, std::ceil((height - options.overlap) / (clip.height - options.overlap)));
        if (columns * rows > maximum_sides) {
            throw std::invalid_argument("poster exceeds 10000 tiles");
        }
        for (int row = 0; row < static_cast<int>(rows); ++row) {
            for (int column = 0; column < static_cast<int>(columns); ++column) {
                const page_rectangle destination{clip.x - column * (clip.width - options.overlap),
                                                 clip.y - row * (clip.height - options.overlap),
                                                 width, height};
                add_side(output, {static_cast<int>(output.size()),
                                  0,
                                  false,
                                  column,
                                  row,
                                  {{page, destination, clip, false}}});
            }
        }
    }
    return output;
}
std::vector<print_side> copy_sides(const std::vector<print_side> &source,
                                   const print_options &options) {
    std::vector<print_side> output;
    const auto append = [&](const print_side &side, int copy) {
        if ((options.sides == sheet_subset::fronts && side.back) ||
            (options.sides == sheet_subset::backs && !side.back)) {
            return;
        }
        auto result = side;
        result.copy = copy;
        add_side(output, std::move(result));
    };
    if (options.collate) {
        for (int copy = 0; copy < options.copies; ++copy) {
            for (const auto &side : source) {
                append(side, copy);
            }
        }
    } else {
        for (std::size_t first = 0; first < source.size();) {
            const std::size_t end =
                first + (first + 1 < source.size() && source[first + 1].back ? 2 : 1);
            for (int copy = 0; copy < options.copies; ++copy) {
                for (std::size_t index = first; index < end; ++index) {
                    append(source[index], copy);
                }
            }
            first = end;
        }
    }
    return output;
}
} // namespace
print_job compose_print_job(const std::vector<page_extent> &document,
                            const std::vector<int> &selected, const print_options &options) {
    validate_options(options);
    if (selected.empty() || selected.size() > maximum_sides * 100) {
        throw std::invalid_argument("print selection is empty or too large");
    }
    for (const int page : selected) {
        if (page < 1 || static_cast<std::size_t>(page) > document.size()) {
            throw std::invalid_argument("print page outside document");
        }
        const auto size = document[static_cast<std::size_t>(page - 1)];
        if (!std::isfinite(size.width) || !std::isfinite(size.height) || size.width <= 0 ||
            size.height <= 0 || size.width > 10000000 || size.height > 10000000) {
            throw std::invalid_argument("invalid source page size");
        }
    }
    auto pages = selected;
    if (options.reverse) {
        std::reverse(pages.begin(), pages.end());
    }
    std::vector<print_side> sides;
    switch (options.layout) {
    case print_layout::single:
    case print_layout::multiple:
        sides = regular_sides(document, pages, options);
        break;
    case print_layout::booklet:
        sides = booklet_sides(document, pages, options);
        break;
    case print_layout::poster:
        sides = poster_sides(document, pages, options);
        break;
    }
    auto output = copy_sides(sides, options);
    if (options.duplex == print_duplex::manual) {
        std::stable_partition(output.begin(), output.end(),
                              [](const print_side &side) { return !side.back; });
        if (options.reverse_backs) {
            const auto first_back = std::find_if(output.begin(), output.end(),
                                                 [](const print_side &side) { return side.back; });
            std::reverse(first_back, output.end());
        }
    }
    if (output.empty()) {
        throw std::invalid_argument("no sheet sides match selection");
    }
    return {options, std::move(output)};
}
} // namespace vellum
