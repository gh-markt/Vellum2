#include "vellum/print_plan.hh"
#include "vellum/view_zoom.hh"
#include <cmath>
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
    near(vellum::calculate_view_scale(zoom_mode::fit_width, {1200, 200}, {300, 600}), .25);
    reject([] { vellum::calculate_view_scale(zoom_mode::fit_page, {0, 800}, {300, 600}); });
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
} // namespace
int main() {
    try {
        zoom_tests();
        regular_tests();
        booklet_tests();
        poster_tests();
        ordering_tests();
        negative_tests();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
