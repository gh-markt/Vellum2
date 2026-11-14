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
    std::size_t cases = 0;
    for (int first = 1; first <= 5; ++first) {
        for (int last = first; last <= 5; ++last) {
            for (int other_first = 1; other_first <= 5; ++other_first) {
                for (int other_last = other_first; other_last <= 5; ++other_last) {
                    const auto expression = std::to_string(first) + "-" + std::to_string(last) +
                                            "," + std::to_string(other_first) + "-" +
                                            std::to_string(other_last);
                    std::vector<int> expected;
                    for (int page = first; page <= last; ++page) {
                        expected.push_back(page);
                    }
                    for (int page = other_first; page <= other_last; ++page) {
                        expected.push_back(page);
                    }
                    require(vellum::select_print_pages(expression, 5) == expected);
                    ++cases;
                }
            }
        }
    }
    std::cout << "Ordered interval oracle: " << cases << " cases\n";
}
void compare_blank_and_parity_oracle() {
    using vellum::page_subset;
    std::size_t cases = 0;
    for (int first = 1; first <= 3; ++first) {
        for (int second = 1; second <= 3; ++second) {
            for (int third = 1; third <= 3; ++third) {
                for (int left = 1; left <= 4; ++left) {
                    for (int right = 1; right <= 4; ++right) {
                        const auto expression = std::to_string(first) + std::string(left, ',') +
                                                std::to_string(second) + std::string(right, ',') +
                                                std::to_string(third);
                        std::vector<int> all{first};
                        all.insert(all.end(), left - 1, 0);
                        all.push_back(second);
                        all.insert(all.end(), right - 1, 0);
                        all.push_back(third);
                        require(vellum::select_print_pages(expression, 3) == all);
                        ++cases;
                        for (const auto subset : {page_subset::odd, page_subset::even}) {
                            std::vector<int> filtered;
                            for (std::size_t position = 0; position < all.size(); ++position) {
                                if (position % 2 == (subset == page_subset::odd ? 0U : 1U)) {
                                    filtered.push_back(all[position]);
                                }
                            }
                            require(vellum::select_print_pages(expression, 3, subset) == filtered);
                            ++cases;
                        }
                    }
                }
            }
        }
    }
    std::cout << "Blank and parity oracle: " << cases << " cases\n";
}

int main() {
    try {
        compare_interval_oracle();
        compare_blank_and_parity_oracle();
        using vellum::page_subset;
        require(vellum::select_print_pages("1-3, 7-10 12-19", 20) ==
                std::vector<int>({1, 2, 3, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19}));
        require(vellum::select_print_pages(" 3, 1-3, 2 ", 3) == std::vector<int>({3, 1, 2, 3, 2}));
        require(vellum::select_print_pages("3,,1", 3) == std::vector<int>({3, 0, 1}));
        require(vellum::select_print_pages("1,,,2", 3) == std::vector<int>({1, 0, 0, 2}));
        require(vellum::select_print_pages("1,,,,2", 3) == std::vector<int>({1, 0, 0, 0, 2}));
        require(vellum::select_print_pages("1,,1", 3) == std::vector<int>({1, 0, 1}));
        require(vellum::select_print_pages(",,1,,", 3) == std::vector<int>({0, 1, 0}));
        require(vellum::select_print_pages(",,,", 3) == std::vector<int>({0, 0}));
        require(vellum::select_print_pages("1, , 2", 3) == std::vector<int>({1, 0, 2}));
        require(vellum::select_print_pages("3,,1,2,3", 3, page_subset::odd) ==
                std::vector<int>({3, 1, 3}));
        require(vellum::select_print_pages("3,,1,2,3", 3, page_subset::even) ==
                std::vector<int>({0, 2}));
        require(vellum::select_print_pages(",,", 3, page_subset::even).empty());
        require(vellum::select_print_pages("2-6", 6, page_subset::odd) ==
                std::vector<int>({2, 4, 6}));
        require(vellum::select_print_pages("2-6", 6, page_subset::even) ==
                std::vector<int>({3, 5}));
        require(vellum::select_print_pages("1", 1, page_subset::even).empty());
        require(vellum::select_print_pages("1 - 3\t5\n7", 7) == std::vector<int>({1, 2, 3, 5, 7}));
        require(vellum::select_print_pages("\r1\r-\r3\r,\r,\r2\r", 3) ==
                std::vector<int>({1, 2, 3, 0, 2}));
        for (const auto *bad : {"", " ", ",1", "1,", "0", "-1", "+1", "3-2", "21", "1-21", "1-",
                                "1-2-3", "1x2", "1;2", "1.5", "21474836499999", "１"}) {
            reject(bad);
        }
        reject(std::string(65537, '1'));
        reject("1-1000000,1", 1000000);
        const auto maximum_selection = vellum::select_print_pages("1-1000000", 1000000);
        require(maximum_selection.size() == 1000000 && maximum_selection.front() == 1 &&
                maximum_selection.back() == 1000000);
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
