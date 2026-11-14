#include "vellum/print_plan.hh"
#include <charconv>
#include <stdexcept>
namespace vellum {
namespace {
constexpr std::size_t maximum_selected_pages = 1000000;
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
void append_page(std::vector<int> &pages, int page) {
    if (pages.size() >= maximum_selected_pages) {
        throw std::invalid_argument("print selection exceeds 1000000 pages");
    }
    pages.push_back(page);
}
void add_range(std::string_view &text, std::vector<int> &pages, int page_count) {
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
    for (int page = first; page <= last; ++page) {
        append_page(pages, page);
    }
}
std::size_t consume_commas(std::string_view &text) {
    std::size_t count = 0;
    while (!text.empty() && text.front() == ',') {
        ++count;
        text.remove_prefix(1);
        skip_space(text);
    }
    return count;
}
void add_blanks(std::vector<int> &pages, std::size_t commas) {
    for (std::size_t index = 1; index < commas; ++index) {
        append_page(pages, 0);
    }
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
        throw std::invalid_argument("empty print range expression");
    }
    std::vector<int> pages;
    if (ranges.front() == ',') {
        const auto commas = consume_commas(ranges);
        if (commas < 2) {
            throw std::invalid_argument("a leading comma must insert a blank page");
        }
        add_blanks(pages, commas);
    }
    while (!ranges.empty()) {
        add_range(ranges, pages, page_count);
        const bool separated_by_space = !ranges.empty() && space(ranges.front());
        skip_space(ranges);
        if (ranges.empty()) {
            break;
        }
        if (ranges.front() == ',') {
            const auto commas = consume_commas(ranges);
            add_blanks(pages, commas);
            if (ranges.empty() && commas == 1) {
                throw std::invalid_argument("empty print range after comma");
            }
        } else if (!separated_by_space) {
            throw std::invalid_argument("print ranges require a comma or whitespace separator");
        }
    }
    if (subset != page_subset::all) {
        std::vector<int> filtered;
        for (std::size_t index = subset == page_subset::odd ? 0 : 1; index < pages.size();
             index += 2) {
            filtered.push_back(pages[index]);
        }
        return filtered;
    }
    return pages;
}
} // namespace vellum
