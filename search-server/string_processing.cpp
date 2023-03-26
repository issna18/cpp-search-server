#include "string_processing.h"

#include <algorithm>
#include <iterator>
#include <vector>

std::vector<std::string_view> SplitIntoWords(std::string_view text)
{
    text.remove_prefix(std::min(text.find_first_not_of(' '), text.size()));
    if (text.empty()) return {};

    size_t pos{0};
    std::vector<std::string_view> result;
    while (!text.empty()) {
        pos = text.find(' ');
        result.emplace_back(pos == text.npos ? text.substr() : text.substr(0, pos));
        pos = text.find_first_not_of(' ', pos);
        text.remove_prefix(std::min(pos, text.size()));
    }

    return result;
}
