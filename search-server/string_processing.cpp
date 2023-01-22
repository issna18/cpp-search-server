#include "string_processing.h"

#include <algorithm>
#include <sstream>
#include <vector>

std::vector<std::string> SplitIntoWords(const std::string& text)
{
    std::istringstream in(text);
    std::string word;
    std::vector<std::string> words;
    while (in >> word) {
        words.push_back(word);
    }
    return words;
}

bool IsValidString(const std::string& word)
{
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}
