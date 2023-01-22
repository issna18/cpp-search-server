#pragma once

#include <iostream>

struct Document {
    Document() = default;
    Document(int _id, double _relevance, int _rating);

    int id{};
    double relevance{};
    int rating{};
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

std::ostream& operator<<(std::ostream& output, const Document& document);
