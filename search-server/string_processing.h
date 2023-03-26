#pragma once

#include <string_view>
#include <vector>

std::vector<std::string_view> SplitIntoWords(const std::string_view text);

bool IsValidString(const std::string_view word);
