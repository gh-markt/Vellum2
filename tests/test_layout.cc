#include "vellum/print_plan.hh"
#include "vellum/view_zoom.hh"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool value, const char *reason) {
    if (!value) {
        throw std::runtime_error(reason);
    }
}
void near(double actual, double expected) {
    require(std::abs(actual - expected) < 0.00001, "geometry mismatch");
}
template <class Operation> void reject(Operation operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "expected invalid geometry/settings to fail");
}
void zoom_tests() {
    using vellum::zoom_mode;
    near(vellum::calculate_view_scale(zoom_mode::fit_page, {600, 800}, {300, 600}), .5);
    near(vellum::calculate_view_scale(zoom_mode::fit_width, {600, 800}, {300, 600}), .5);
    near(vellum::calculate_view_scale(zoom_mode::fit_height, {600, 800}, {300, 600}), .75);
    near(vellum::calculate_view_scale(zoom_mode::percentage, {600, 800}, {300, 600}, 125), 1.25);
    near(vellum::calculate_view_scale(zoom_mode::percentage, {600, 800}, {300, 600}, 1), .01);
    near(vellum::calculate_view_scale(zoom_mode::percentage, {600, 800}, {300, 600}, 800), 8);
    near(vellum::calculate_view_scale(zoom_mode::fit_width, {1200, 200}, {300, 600}), .25);
    near(vellum::calculate_view_scale(zoom_mode::fit_page, {600, 800}, {600, 200}), .25);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    reject([nan] { vellum::calculate_view_scale(zoom_mode::fit_page, {nan, 800}, {300, 600}); });
    reject([infinity] {
        vellum::calculate_view_scale(zoom_mode::fit_page, {600, infinity}, {300, 600});
    });
    reject([infinity] {
        vellum::calculate_view_scale(zoom_mode::fit_page, {600, 800}, {infinity, 600});
    });
    reject([nan] { vellum::calculate_view_scale(zoom_mode::fit_page, {600, 800}, {300, nan}); });
    reject([nan] {
        vellum::calculate_view_scale(zoom_mode::percentage, {600, 800}, {300, 600}, nan);
    });
    reject([] { vellum::calculate_view_scale(zoom_mode::fit_page, {0, 800}, {300, 600}); });
    reject([] { vellum::calculate_view_scale(zoom_mode::fit_page, {600, -1}, {300, 600}); });
    reject([] { vellum::calculate_view_scale(zoom_mode::fit_page, {600, 800}, {-1, 600}); });
    reject([] { vellum::calculate_view_scale(zoom_mode::fit_page, {600, 800}, {300, 0}); });
    reject([] { vellum::calculate_view_scale(zoom_mode::percentage, {600, 800}, {300, 600}, 0); });
    reject(
        [] { vellum::calculate_view_scale(zoom_mode::percentage, {600, 800}, {300, 600}, 801); });
    reject(
        [] { vellum::calculate_view_scale(static_cast<zoom_mode>(99), {600, 800}, {300, 600}); });
}
vellum::print_options options() {
    vellum::print_options value;
    value.paper = {600, 800};
    value.printable = {0, 0, 600, 800};
    value.auto_rotate = false;
    value.tile_labels = false;
    value.overlap = 0;
    return value;
}
void regular_tests() {
    const std::vector<vellum::page_extent> document(8, {200, 400});
    auto settings = options();
    auto job = vellum::compose_print_job(document, {1, 2, 3}, settings);
    require(job.sides.size() == 3, "single sides");
    near(job.sides[0].placements[0].destination.width, 400);
    near(job.sides[0].placements[0].destination.x, 100);
    settings.scaling = vellum::print_scale::actual;
    job = vellum::compose_print_job(document, {1}, settings);
    near(job.sides[0].placements[0].destination.width, 200);
    settings.scaling = vellum::print_scale::custom;
    settings.percentage = 50;
    job = vellum::compose_print_job(document, {1}, settings);
    near(job.sides[0].placements[0].destination.width, 100);
    settings.scaling = vellum::print_scale::shrink;
    job = vellum::compose_print_job(document, {1}, settings);
    near(job.sides[0].placements[0].destination.width, 200);
    settings.layout = vellum::print_layout::multiple;
    settings.columns = 2;
    settings.rows = 2;
    job = vellum::compose_print_job(document, {1, 2, 3, 4, 5}, settings);
    require(job.sides.size() == 2 && job.sides[0].placements.size() == 4, "N-up grouping");
    near(job.sides[0].placements[2].clip.y, 400);
    settings.order = vellum::nup_order::down;
    job = vellum::compose_print_job(document, {1, 2, 3, 4}, settings);
    near(job.sides[0].placements[1].clip.x, 0);
    near(job.sides[0].placements[1].clip.y, 400);
    settings.order = vellum::nup_order::across_reverse;
    job = vellum::compose_print_job(document, {1}, settings);
    near(job.sides[0].placements[0].clip.x, 300);
    near(job.sides[0].placements[0].clip.y, 0);
}
void nup_combination_tests() {
    const std::vector<vellum::page_extent> document(8, {100, 200});
    const vellum::nup_order orders[] = {vellum::nup_order::across, vellum::nup_order::down,
                                        vellum::nup_order::across_reverse,
                                        vellum::nup_order::down_reverse};
    std::size_t cases = 0;
    for (const auto order : orders) {
        for (int rows = 1; rows <= 3; ++rows) {
            for (int columns = 1; columns <= 3; ++columns) {
                std::vector<std::pair<int, int>> traversal;
                if (order == vellum::nup_order::across ||
                    order == vellum::nup_order::across_reverse) {
                    for (int row = 0; row < rows; ++row) {
                        for (int column = 0; column < columns; ++column) {
                            traversal.emplace_back(row, order == vellum::nup_order::across
                                                            ? column
                                                            : columns - 1 - column);
                        }
                    }
                } else {
                    for (int column = 0; column < columns; ++column) {
                        for (int row = 0; row < rows; ++row) {
                            traversal.emplace_back(
                                order == vellum::nup_order::down ? row : rows - 1 - row, column);
                        }
                    }
                }
                for (int count = 1; count <= 8; ++count) {
                    std::vector<int> selected;
                    for (int page = 1; page <= count; ++page) {
                        selected.push_back(page);
                    }
                    for (const bool auto_rotate : {false, true}) {
                        auto settings = options();
                        settings.layout = vellum::print_layout::multiple;
                        settings.order = order;
                        settings.rows = rows;
                        settings.columns = columns;
                        settings.gutter = 6;
                        settings.auto_rotate = auto_rotate;
                        const auto job = vellum::compose_print_job(document, selected, settings);
                        ++cases;
                        const int slots = rows * columns;
                        const double width = (600.0 - (columns - 1) * 6) / columns;
                        const double height = (800.0 - (rows - 1) * 6) / rows;
                        require(job.sides.size() ==
                                    static_cast<std::size_t>((count + slots - 1) / slots),
                                "N-up sheet count differs from the selected pages");
                        for (int page = 1; page <= count; ++page) {
                            const auto &side = job.sides[(page - 1) / slots];
                            const auto &placement = side.placements[(page - 1) % slots];
                            const auto [row, column] = traversal[(page - 1) % slots];
                            require(placement.page == page, "N-up changed selected page order");
                            near(placement.clip.x, column * (width + 6));
                            near(placement.clip.y, row * (height + 6));
                            near(placement.clip.width, width);
                            near(placement.clip.height, height);
                            const bool should_rotate =
                                auto_rotate && std::min(width / 200.0, height / 100.0) >
                                                   std::min(width / 100.0, height / 200.0);
                            require(placement.clockwise == should_rotate,
                                    "N-up chose an incorrect page rotation");
                        }
                    }
                }
            }
        }
    }
    std::cout << "N-up ordering, dimensions and rotation: " << cases << " cases\n";
}
void booklet_tests() {
    const std::vector<vellum::page_extent> document(8, {200, 400});
    auto settings = options();
    settings.layout = vellum::print_layout::booklet;
    const auto job = vellum::compose_print_job(document, {1, 2, 3, 4, 5, 6, 7, 8}, settings);
    const int expected[][2] = {{8, 1}, {2, 7}, {6, 3}, {4, 5}};
    require(job.sides.size() == 4, "booklet side count");
    for (std::size_t side = 0; side < job.sides.size(); ++side) {
        require(job.sides[side].placements[0].page == expected[side][0], "left booklet page");
        require(job.sides[side].placements[1].page == expected[side][1], "right booklet page");
        require(job.sides[side].back == (side % 2 == 1), "booklet back side");
    }
    auto padded = vellum::compose_print_job(document, {1, 2, 3, 4, 5}, settings);
    require(padded.sides[0].placements[0].page == 0, "booklet blank padding");
    settings.right_binding = true;
    padded = vellum::compose_print_job(document, {1, 2, 3, 4}, settings);
    require(padded.sides[0].placements[0].page == 1 && padded.sides[0].placements[1].page == 4,
            "right binding");
}
void booklet_size_and_binding_tests() {
    const std::vector<vellum::page_extent> document(31, {200, 400});
    std::size_t cases = 0;
    for (int count = 1; count <= 31; ++count) {
        std::vector<int> selected;
        for (int page = 1; page <= count; ++page) {
            selected.push_back(page);
        }
        const int padded = ((count + 3) / 4) * 4;
        for (const bool right_binding : {false, true}) {
            auto settings = options();
            settings.layout = vellum::print_layout::booklet;
            settings.right_binding = right_binding;
            const auto job = vellum::compose_print_job(document, selected, settings);
            ++cases;
            require(job.sides.size() == static_cast<std::size_t>(padded / 2),
                    "booklet side count must include required padding");
            for (int sheet = 0; sheet < padded / 4; ++sheet) {
                const int expected[2][2] = {{padded - 2 * sheet, 1 + 2 * sheet},
                                            {2 + 2 * sheet, padded - 1 - 2 * sheet}};
                for (int back = 0; back < 2; ++back) {
                    const auto &side = job.sides[2 * sheet + back];
                    require(side.back == (back == 1) && side.placements.size() == 2,
                            "booklet must retain front/back sheet pairs");
                    for (int slot = 0; slot < 2; ++slot) {
                        const int page = expected[back][right_binding ? 1 - slot : slot];
                        require(side.placements[slot].page == (page <= count ? page : 0),
                                "booklet page or padding is in the wrong position");
                    }
                }
            }
        }
    }
    std::cout << "Booklet size and binding: " << cases << " cases\n";
}
void poster_tests() {
    auto settings = options();
    settings.layout = vellum::print_layout::poster;
    auto job = vellum::compose_print_job({{1200, 1600}}, {1}, settings);
    require(job.sides.size() == 4, "exact-multiple poster must not add extra tiles");
    near(job.sides[1].placements[0].destination.x, -600);
    near(job.sides[2].placements[0].destination.y, -800);
    settings.overlap = 20;
    job = vellum::compose_print_job({{1200, 1600}}, {1}, settings);
    require(job.sides.size() == 9, "overlap changes coverage");
    near(job.sides[1].placements[0].destination.x, -580);
    settings.percentage = 200;
    settings.overlap = 0;
    job = vellum::compose_print_job({{600, 800}}, {1}, settings);
    require(job.sides.size() == 4, "poster scale");
    settings.overlap = 800;
    const auto invalid_overlap = [&] { vellum::compose_print_job({{600, 800}}, {1}, settings); };
    reject(invalid_overlap);
}
void explicit_blank_tests() {
    const std::vector<vellum::page_extent> document(2, {200, 400});
    auto settings = options();
    auto job = vellum::compose_print_job(document, {2, 0, 1}, settings);
    require(job.sides.size() == 3 && job.sides[0].placements[0].page == 2 &&
                job.sides[1].placements[0].page == 0 && job.sides[2].placements[0].page == 1,
            "single-page output must retain the explicit blank position");
    settings.copies = 2;
    job = vellum::compose_print_job(document, {2, 0, 1}, settings);
    require(job.sides.size() == 6 && job.sides[1].placements[0].page == 0 &&
                job.sides[4].placements[0].page == 0 && job.sides[3].placements[0].page == 2 &&
                job.sides[5].placements[0].page == 1,
            "each collated copy must retain the blank in the selected sequence");
    settings.copies = 1;
    settings.duplex = vellum::print_duplex::long_edge;
    job = vellum::compose_print_job(document, {1, 0, 2}, settings);
    require(job.sides.size() == 4 && !job.sides[0].back && job.sides[1].back &&
                job.sides[1].placements[0].page == 0 && job.sides[2].placements[0].page == 2 &&
                job.sides[3].back && job.sides[3].placements.empty(),
            "duplex must preserve the explicit blank and add a separate final padding side");
    settings.duplex = vellum::print_duplex::simplex;
    settings.reverse = true;
    job = vellum::compose_print_job(document, {2, 0, 1}, settings);
    require(job.sides[0].placements[0].page == 1 && job.sides[1].placements[0].page == 0 &&
                job.sides[2].placements[0].page == 2,
            "reverse must reverse the complete sequence including blanks");
    settings = options();
    settings.layout = vellum::print_layout::multiple;
    settings.columns = 2;
    settings.rows = 1;
    job = vellum::compose_print_job(document, {1, 0, 2}, settings);
    require(job.sides.size() == 2 && job.sides[0].placements[0].page == 1 &&
                job.sides[0].placements[1].page == 0 && job.sides[1].placements[0].page == 2,
            "N-up must allocate a cell for an explicit blank");
    settings.layout = vellum::print_layout::booklet;
    job = vellum::compose_print_job(document, {1, 0, 2}, settings);
    require(job.sides.size() == 2 && job.sides[0].placements[0].page == 0 &&
                job.sides[0].placements[1].page == 1 && job.sides[1].placements[0].page == 0 &&
                job.sides[1].placements[1].page == 2,
            "booklet must retain explicit blanks as well as padding");
    settings = options();
    settings.layout = vellum::print_layout::poster;
    job = vellum::compose_print_job(document, {1, 0, 2}, settings);
    require(job.sides.size() == 3 && job.sides[1].placements.size() == 1 &&
                job.sides[1].placements[0].page == 0,
            "poster must emit one blank sheet without indexing a document page");
    reject([&] { vellum::compose_print_job(document, {-1}, settings); });
}
void ordering_tests() {
    const std::vector<vellum::page_extent> document(4, {200, 400});
    auto settings = options();
    settings.duplex = vellum::print_duplex::long_edge;
    settings.copies = 2;
    auto job = vellum::compose_print_job(document, {1, 2, 3}, settings);
    require(job.sides.size() == 8 && job.sides[3].placements.empty(),
            "duplex copy must be blank padded");
    settings.collate = false;
    job = vellum::compose_print_job(document, {1, 2, 3, 4}, settings);
    require(job.sides[0].placements[0].page == 1 && job.sides[2].placements[0].page == 1,
            "uncollated duplex must keep pairs together");
    settings.copies = 1;
    settings.reverse = true;
    job = vellum::compose_print_job(document, {1, 2, 3, 4}, settings);
    require(job.sides[0].placements[0].page == 4, "reverse selection order");
    settings.duplex = vellum::print_duplex::manual;
    settings.reverse = false;
    settings.reverse_backs = true;
    job = vellum::compose_print_job(document, {1, 2, 3, 4}, settings);
    require(!job.sides[0].back && !job.sides[1].back && job.sides[2].back,
            "manual duplex separates passes");
    require(job.sides[2].placements[0].page == 4, "manual reverse backs");
    settings.sides = vellum::sheet_subset::fronts;
    job = vellum::compose_print_job(document, {1, 2, 3, 4}, settings);
    require(job.sides.size() == 2 && !job.sides[1].back, "front-side subset");
}
void negative_tests() {
    auto settings = options();
    reject([&] { vellum::compose_print_job({{200, 400}}, {}, settings); });
    reject([&] { vellum::compose_print_job({{200, 400}}, {2}, settings); });
    reject([&] { vellum::compose_print_job({{0, 400}}, {1}, settings); });
    settings.percentage = std::numeric_limits<double>::infinity();
    reject([&] { vellum::compose_print_job({{200, 400}}, {1}, settings); });
    settings = options();
    settings.columns = 0;
    reject([&] { vellum::compose_print_job({{200, 400}}, {1}, settings); });
    settings = options();
    settings.printable.width = 601;
    reject([&] { vellum::compose_print_job({{200, 400}}, {1}, settings); });
    settings = options();
    settings.layout = vellum::print_layout::poster;
    settings.percentage = 1000;
    reject([&] { vellum::compose_print_job({{1000000, 1000000}}, {1}, settings); });
}
void print_option_boundaries() {
    const std::vector<std::function<void(vellum::print_options &)>> invalid = {
        [](auto &s) { s.paper.width = 0; },
        [](auto &s) { s.paper.height = 0; },
        [](auto &s) { s.paper.width = 14401; },
        [](auto &s) { s.paper.height = 14401; },
        [](auto &s) { s.printable.x = -1; },
        [](auto &s) { s.printable.y = -1; },
        [](auto &s) { s.printable.width = 0; },
        [](auto &s) { s.printable.height = 0; },
        [](auto &s) { s.printable.y = 1; },
        [](auto &s) { s.percentage = 0; },
        [](auto &s) { s.percentage = 1001; },
        [](auto &s) { s.overlap = -1; },
        [](auto &s) { s.gutter = -1; },
        [](auto &s) { s.order = static_cast<vellum::nup_order>(-1); },
        [](auto &s) { s.order = static_cast<vellum::nup_order>(100); },
        [](auto &s) { s.columns = 11; },
        [](auto &s) { s.rows = 0; },
        [](auto &s) { s.rows = 11; },
        [](auto &s) { s.copies = 0; },
        [](auto &s) { s.copies = 101; },
        [](auto &s) { s.layout = static_cast<vellum::print_layout>(-1); },
        [](auto &s) { s.layout = static_cast<vellum::print_layout>(100); },
        [](auto &s) { s.scaling = static_cast<vellum::print_scale>(-1); },
        [](auto &s) { s.scaling = static_cast<vellum::print_scale>(100); },
        [](auto &s) { s.duplex = static_cast<vellum::print_duplex>(-1); },
        [](auto &s) { s.duplex = static_cast<vellum::print_duplex>(100); },
        [](auto &s) { s.sides = static_cast<vellum::sheet_subset>(-1); },
        [](auto &s) { s.sides = static_cast<vellum::sheet_subset>(100); }};
    for (const auto &mutate : invalid) {
        auto settings = options();
        mutate(settings);
        reject([&] { vellum::compose_print_job({{200, 400}}, {1}, settings); });
    }
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<std::function<void(vellum::print_options &)>> nonfinite = {
        [=](auto &s) { s.paper.width = nan; },     [=](auto &s) { s.paper.height = nan; },
        [=](auto &s) { s.printable.x = nan; },     [=](auto &s) { s.printable.y = nan; },
        [=](auto &s) { s.printable.width = nan; }, [=](auto &s) { s.printable.height = nan; },
        [=](auto &s) { s.percentage = nan; },      [=](auto &s) { s.overlap = nan; },
        [=](auto &s) { s.gutter = nan; }};
    for (const auto &mutate : nonfinite) {
        auto settings = options();
        mutate(settings);
        reject([&] { vellum::compose_print_job({{200, 400}}, {1}, settings); });
    }
    auto limits = options();
    limits.paper = {14400, 14400};
    limits.percentage = 1000;
    limits.copies = 100;
    limits.rows = limits.columns = 10;
    limits.layout = vellum::print_layout::multiple;
    require(vellum::compose_print_job({{200, 400}}, {1}, limits).sides.size() == 100,
            "maximum valid paper, scaling, copies and grid settings must be accepted");
    for (const auto &size : {vellum::page_extent{-1, 10}, vellum::page_extent{10, -1},
                             vellum::page_extent{10000001, 10}, vellum::page_extent{10, 10000001},
                             vellum::page_extent{10, std::numeric_limits<double>::quiet_NaN()}}) {
        reject([&] { vellum::compose_print_job({size}, {1}, options()); });
    }
    auto grid = options();
    grid.layout = vellum::print_layout::multiple;
    grid.columns = 2;
    grid.gutter = 600;
    reject([&] { vellum::compose_print_job({{200, 400}}, {1}, grid); });
    grid = options();
    grid.sides = vellum::sheet_subset::backs;
    reject([&] { vellum::compose_print_job({{200, 400}}, {1}, grid); });
    const std::vector<int> maximum(10000, 1);
    require(vellum::compose_print_job({{200, 400}}, maximum, options()).sides.size() == 10000,
            "maximum sheet count must be accepted");
    reject([&] { vellum::compose_print_job({{200, 400}}, std::vector<int>(10001, 1), options()); });
    reject(
        [&] { vellum::compose_print_job({{200, 400}}, std::vector<int>(1000001, 1), options()); });
}
} // namespace
int main() {
    try {
        zoom_tests();
        regular_tests();
        nup_combination_tests();
        booklet_tests();
        booklet_size_and_binding_tests();
        poster_tests();
        explicit_blank_tests();
        ordering_tests();
        negative_tests();
        print_option_boundaries();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
