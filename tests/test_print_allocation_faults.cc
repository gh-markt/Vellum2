#include "vellum/print_plan.hh"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

#include "support/fail_new.hh"

namespace {
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
template <class Operation, class Verify>
std::size_t check_allocations(Operation operation, Verify verify) {
    allocations = 0;
    fail_at = static_cast<std::size_t>(-1);
    armed = true;
    operation();
    armed = false;
    const auto total = allocations;
    require(total > 0, "allocation test did not exercise an allocation");
    verify();
    for (std::size_t index = 0; index < total; ++index) {
        allocations = 0;
        fail_at = index;
        armed = true;
        bool failed = false;
        try {
            operation();
        } catch (const std::bad_alloc &) {
            failed = true;
        }
        armed = false;
        require(failed && allocations == index + 1, "allocation failure was not observed");
        verify();
        operation();
        verify();
    }
    return total;
}
template <class Operation> void check_rejection_allocation(Operation operation) {
    allocations = 0;
    fail_at = static_cast<std::size_t>(-1);
    armed = true;
    bool rejected = false;
    try {
        operation();
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    armed = false;
    const auto total = allocations;
    require(rejected && total > 0, "invalid request did not allocate its rejection");
    allocations = 0;
    fail_at = total - 1;
    armed = true;
    bool failed = false;
    try {
        operation();
    } catch (const std::bad_alloc &) {
        failed = true;
    }
    armed = false;
    require(failed && allocations == total, "rejection allocation failure was not observed");
    rejected = false;
    try {
        operation();
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected, "invalid request was accepted after allocation failure");
}
} // namespace
int main() {
    try {
        const std::string expression = "1-8,,3-7,1";
        const std::vector<int> expected{1, 2, 3, 4, 5, 6, 7, 8, 0, 3, 4, 5, 6, 7, 1};
        const auto select = [&] {
            const auto result = vellum::select_print_pages(expression, 8);
            if (result != expected) {
                throw std::runtime_error("selection changed after allocation failure");
            }
        };
        const auto verify_expression = [&] {
            require(expression == "1-8,,3-7,1", "selection input was modified");
        };
        const auto selected = expected;
        const std::vector<vellum::page_extent> document(8, {200, 400});
        vellum::print_options options;
        options.copies = 3;
        const auto verify_inputs = [&] {
            require(selected == expected && document.size() == 8 && options.copies == 3,
                    "failed allocation modified the print inputs");
        };
        const auto selection_cases = check_allocations(select, verify_expression);
        std::cout << "Selection allocation failures: " << selection_cases << " cases\n";
        const std::string oversized(65537, '1');
        for (const auto &invalid :
             {std::string("0"), std::string("3-1"), std::string(), std::string(",1"),
              std::string("1,"), std::string("1x"), oversized, std::string("1-1000000,1")}) {
            check_rejection_allocation([&] { (void)vellum::select_print_pages(invalid, 1000000); });
        }
        check_rejection_allocation([] { (void)vellum::select_print_pages("1", 0); });
        const std::vector<int> one_page{1};
        const std::vector<int> missing_page{9};
        const std::vector<int> no_pages;
        const std::vector<vellum::page_extent> invalid_document{{0, 400}};
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, no_pages, {}); });
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, missing_page, {}); });
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(invalid_document, one_page, {}); });
        auto bad_options = vellum::print_options{};
        bad_options.paper.width = std::numeric_limits<double>::quiet_NaN();
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, one_page, bad_options); });
        bad_options = {};
        bad_options.paper.width = 0;
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, one_page, bad_options); });
        bad_options = {};
        bad_options.layout = vellum::print_layout::poster;
        bad_options.duplex = vellum::print_duplex::long_edge;
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, one_page, bad_options); });
        bad_options = {};
        bad_options.layout = vellum::print_layout::multiple;
        bad_options.columns = 2;
        bad_options.gutter = bad_options.printable.width;
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, one_page, bad_options); });
        bad_options = {};
        bad_options.layout = vellum::print_layout::poster;
        bad_options.overlap = bad_options.printable.width;
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, one_page, bad_options); });
        bad_options.overlap = 0;
        bad_options.percentage = 1000;
        const std::vector<vellum::page_extent> huge_page{{1000000, 1000000}};
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(huge_page, one_page, bad_options); });
        bad_options = {};
        bad_options.sides = vellum::sheet_subset::backs;
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, one_page, bad_options); });
        const std::vector<int> too_many_sides(10001, 1);
        const std::vector<int> too_many_pages(1000001, 1);
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, too_many_sides, {}); });
        check_rejection_allocation(
            [&] { (void)vellum::compose_print_job(document, too_many_pages, {}); });
        for (const auto &[layout, expected_sides] :
             {std::pair{vellum::print_layout::single, std::size_t{45}},
              std::pair{vellum::print_layout::multiple, std::size_t{12}},
              std::pair{vellum::print_layout::booklet, std::size_t{24}},
              std::pair{vellum::print_layout::poster, std::size_t{45}}}) {
            options.layout = layout;
            options.columns = options.rows = 2;
            const auto compose = [&] {
                const auto result = vellum::compose_print_job(document, selected, options);
                if (result.sides.size() != expected_sides ||
                    result.sides.front().placements.empty()) {
                    throw std::runtime_error("print plan changed after allocation failure");
                }
            };
            std::cout << "Layout " << static_cast<int>(layout)
                      << " allocation failures: " << check_allocations(compose, verify_inputs)
                      << " cases\n";
        }
    } catch (const std::exception &error) {
        armed = false;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
