#include "vellum/print_plan.hh"
#include <charconv>
#include <stdexcept>
namespace vellum {
namespace {
bool space(char character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}
void skip_space(std::string_view &text) {
    while (!text.empty() && space(text.front())) {
        text.remove_prefix(1);
    }
}
int page_number(std::string_view &text, int page_count) {
    int page = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), page);
    if (result.ec != std::errc{} || page < 1 || page > page_count) {
        throw std::invalid_argument("print page number is invalid or outside document");
    }
    text.remove_prefix(static_cast<std::size_t>(result.ptr - text.data()));
    return page;
}
void add_range(std::string_view &text, std::vector<int> &boundaries, int page_count) {
    const int first = page_number(text, page_count);
    auto after_number = text;
    skip_space(after_number);
    int last = first;
    if (!after_number.empty() && after_number.front() == '-') {
        text = after_number;
        text.remove_prefix(1);
        skip_space(text);
        last = page_number(text, page_count);
    }
    if (last < first) {
        throw std::invalid_argument("descending print ranges are not supported");
    }
    ++boundaries[static_cast<std::size_t>(first)];
    --boundaries[static_cast<std::size_t>(last) + 1];
}
bool next_range(std::string_view &text) {
    const bool separated_by_space = !text.empty() && space(text.front());
    skip_space(text);
    if (text.empty()) {
        return false;
    }
    if (text.front() == ',') {
        text.remove_prefix(1);
        skip_space(text);
        if (text.empty()) {
            throw std::invalid_argument("empty print range after comma");
        }
    } else if (!separated_by_space) {
        throw std::invalid_argument("print ranges require a comma or whitespace separator");
    }
    return true;
}
} // namespace
std::vector<int> select_print_pages(std::string_view ranges, int page_count, page_subset subset) {
    if (page_count < 1 || page_count > 1000000 ||
        (subset != page_subset::all && subset != page_subset::odd && subset != page_subset::even)) {
        throw std::invalid_argument("invalid print page count or subset");
    }
    if (ranges.size() > 65536) {
        throw std::invalid_argument("oversized print range expression");
    }
    skip_space(ranges);
    if (ranges.empty()) {
        throw std::invalid_argument("empty or oversized print range expression");
    }
    std::vector<int> boundaries(static_cast<std::size_t>(page_count) + 2, 0);
    do {
        add_range(ranges, boundaries, page_count);
    } while (next_range(ranges));
    std::vector<int> pages;
    int active_ranges = 0;
    for (int page = 1; page <= page_count; ++page) {
        active_ranges += boundaries[static_cast<std::size_t>(page)];
        const bool parity_matches = subset == page_subset::all ||
                                    (subset == page_subset::odd && page % 2 == 1) ||
                                    (subset == page_subset::even && page % 2 == 0);
        if (active_ranges > 0 && parity_matches) {
            pages.push_back(page);
        }
    }
    return pages;
}
} // namespace vellum
