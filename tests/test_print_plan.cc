#include "vellum/print_plan.hh"
#include <iostream>
#include <stdexcept>
namespace {
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("print selection assertion failed");
    }
}
void reject(std::string_view input, int count = 20) {
    bool rejected = false;
    try {
        vellum::select_print_pages(input, count);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected);
}
} // namespace
void compare_interval_oracle() {
    for (int first = 1; first <= 5; ++first) {
        for (int last = first; last <= 5; ++last) {
            for (int other_first = 1; other_first <= 5; ++other_first) {
                for (int other_last = other_first; other_last <= 5; ++other_last) {
                    const auto expression = std::to_string(first) + "-" + std::to_string(last) +
                                            "," + std::to_string(other_first) + "-" +
                                            std::to_string(other_last);
                    std::vector<int> expected;
                    for (int page = 1; page <= 5; ++page) {
                        if ((page >= first && page <= last) ||
                            (page >= other_first && page <= other_last)) {
                            expected.push_back(page);
                        }
                    }
                    require(vellum::select_print_pages(expression, 5) == expected);
                }
            }
        }
    }
}

int main() {
    try {
        compare_interval_oracle();
        using vellum::page_subset;
        require(vellum::select_print_pages("1-3, 7-10 12-19", 20) ==
                std::vector<int>({1, 2, 3, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19}));
        require(vellum::select_print_pages(" 3, 1-3, 2 ", 3) == std::vector<int>({1, 2, 3}));
        require(vellum::select_print_pages("2-6", 6, page_subset::odd) == std::vector<int>({3, 5}));
        require(vellum::select_print_pages("2-6", 6, page_subset::even) ==
                std::vector<int>({2, 4, 6}));
        require(vellum::select_print_pages("1", 1, page_subset::even).empty());
        require(vellum::select_print_pages("1 - 3\t5\n7", 7) == std::vector<int>({1, 2, 3, 5, 7}));
        for (const auto *bad : {"", " ", ",1", "1,", "1,,2", "0", "-1", "+1", "3-2", "21", "1-21",
                                "1-", "1-2-3", "1x2", "1;2", "1.5", "21474836499999", "１"}) {
            reject(bad);
        }
        reject(std::string(65537, '1'));
        bool invalid_subset_rejected = false;
        try {
            vellum::select_print_pages("1", 1, static_cast<page_subset>(99));
        } catch (const std::invalid_argument &) {
            invalid_subset_rejected = true;
        }
        require(invalid_subset_rejected);
        reject("1", 0);
        reject("1", 1000001);
        std::string repeated;
        for (int count = 0; count < 10000; ++count) {
            repeated += "1-1000000,";
        }
        // Oversized repeated ranges must fail before doing expensive interval expansion.
        reject(repeated, 1000000);
        const auto all = vellum::select_print_pages("1-10000", 10000);
        const auto odd = vellum::select_print_pages("1-10000", 10000, page_subset::odd);
        const auto even = vellum::select_print_pages("1-10000", 10000, page_subset::even);
        require(all.size() == 10000 && odd.size() == 5000 && even.size() == 5000);
        for (std::size_t index = 0; index < odd.size(); ++index) {
            require(odd[index] == static_cast<int>(index) * 2 + 1);
            require(even[index] == static_cast<int>(index) * 2 + 2);
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
