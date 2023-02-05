#pragma once

#include "document.h"
#include "log_duration.h"
#include "string_processing.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double EPSILON = 1e-6;

class SearchServer {
public:
    SearchServer() = default;

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words);

    explicit SearchServer(const std::string& text);

    void AddDocument(int document_id, const std::string& document, DocumentStatus status,
                     const std::vector<int>& ratings);

    template<typename Predicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query,
                                      Predicate predicate) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query,
                                      DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin();
    std::set<int>::const_iterator end();

    std::tuple<std::vector<std::string>, DocumentStatus>
    MatchDocument(const std::string& raw_query, int document_id) const;

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    std::set<std::string> stop_words_{};
    std::map<std::string, std::map<int, double>> word_to_document_freqs_{};
    std::map<int, std::map<std::string, double>> document_to_word_freqs_{};
    std::map<int, DocumentData> documents_{};
    std::set<int> documents_id_{};
    static const std::map<std::string, double> empty_;

    bool IsStopWord(const std::string& word) const;

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string word) const;
    Query ParseQuery(const std::string& text) const;

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template<typename Predicate>
    std::vector<Document> FindAllDocuments(const Query& query,
                                      Predicate predicate) const;
};

template <typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words) {
    for (const std::string& word : stop_words) {
        if (word.empty()) continue;
        if (!IsValidString(word)) {
            throw std::invalid_argument("В стоп-слове недопустимые символы");
        }
        stop_words_.insert(word);
    }
}

template<typename Predicate>
std::vector<Document>
SearchServer::FindTopDocuments(const std::string& raw_query,
                               Predicate predicate) const
{
    using namespace std::literals;
    LOG_DURATION_STREAM("Operation time", std::cerr);
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, predicate);

    std::sort(matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
             if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                 return lhs.rating > rhs.rating;
             } else {
                 return lhs.relevance > rhs.relevance;
             }
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template<typename Predicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
                                  Predicate predicate) const
{
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document = documents_.at(document_id);
            if (predicate(document_id, document.status, document.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.emplace_back(Document(document_id,
                                                relevance,
                                                documents_.at(document_id).rating));
    }
    return matched_documents;
}
