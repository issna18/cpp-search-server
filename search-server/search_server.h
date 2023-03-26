#pragma once

#include "concurrent_map.h"
#include "document.h"

#include <algorithm>
#include <execution>
#include <map>
#include <mutex>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

constexpr size_t MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double EPSILON = 1e-6;

class SearchServer {
public:
    SearchServer() = default;

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words);
    explicit SearchServer(const std::string& stop_words);
    explicit SearchServer(const std::string_view stop_words);

    void AddDocument(int document_id, const std::string_view document,
                     DocumentStatus status, const std::vector<int>& ratings);

    std::vector<Document>
    FindTopDocuments(const std::string_view raw_query,
                     DocumentStatus status=DocumentStatus::ACTUAL) const;

    template<typename ExecutionPolicy>
    std::vector<Document>
    FindTopDocuments(ExecutionPolicy&& policy,
                     const std::string_view raw_query,
                     DocumentStatus status=DocumentStatus::ACTUAL) const;

    template<typename Predicate>
    std::vector<Document>
    FindTopDocuments(const std::string_view raw_query, Predicate predicate) const;

    template<typename Predicate, typename ExecutionPolicy>
    std::vector<Document>
    FindTopDocuments(ExecutionPolicy&& policy, const std::string_view raw_query,
                     Predicate predicate) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin();
    std::set<int>::const_iterator end();

    using MatchedDocument = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    MatchedDocument MatchDocument(const std::string_view raw_query, int document_id) const;
    MatchedDocument MatchDocument(const std::execution::sequenced_policy&,
                                  const std::string_view raw_query, int document_id) const;
    MatchedDocument MatchDocument(const std::execution::parallel_policy&,
                                  const std::string_view raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    std::set<std::string, std::less<>> all_words_{};
    std::set<std::string, std::less<>> stop_words_{};
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_{};
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_{};
    std::map<int, DocumentData> documents_{};
    std::set<int> documents_id_{};

    static bool IsValidString(const std::string_view word);
    bool IsStopWord(const std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view) const;
    Query ParseQuery(const std::string_view text, bool cleanup = true) const;

    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template<typename ExecutionPolicy, typename Predicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&& policy,
                                           const Query& query,
                                           Predicate predicate) const;
};

template <typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words) {
    for (const std::string_view word : stop_words) {
        if (word.empty()) continue;
        if (!IsValidString(word)) {
            throw std::invalid_argument("В стоп-слове недопустимые символы");
        }
        stop_words_.emplace(std::string(word));
    }
}

template<typename Predicate, typename ExecutionPolicy>
std::vector<Document>
SearchServer::FindTopDocuments(ExecutionPolicy&& policy,
                               const std::string_view raw_query,
                               Predicate predicate) const
{
    using namespace std::literals;
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(),
         [](const Document& lhs, const Document& rhs) {
             if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                 return lhs.rating > rhs.rating;
             } else {
                 return lhs.relevance > rhs.relevance;
             }
    });

    matched_documents.resize(std::min(matched_documents.size(),
                                      MAX_RESULT_DOCUMENT_COUNT));

    return matched_documents;
}

template<typename Predicate>
std::vector<Document>
SearchServer::FindTopDocuments(const std::string_view raw_query,
                               Predicate predicate) const
{
    return FindTopDocuments(std::execution::seq, raw_query, predicate);
}

template<typename ExecutionPolicy>
std::vector<Document>
SearchServer::FindTopDocuments(ExecutionPolicy&& policy,
                               const std::string_view raw_query,
                               DocumentStatus status) const
{
    const auto predicate = [status]([[maybe_unused]] int document_id,
                                    DocumentStatus s,
                                    [[maybe_unused]] int rating)
                                    {
                                        return s == status;
                                    };
    return FindTopDocuments(policy, raw_query, predicate);
}

template<typename ExecutionPolicy, typename Predicate>
std::vector<Document>
SearchServer::FindAllDocuments(ExecutionPolicy&& policy,
                               const Query& query,
                               Predicate predicate) const
{
    ConcurrentMap<int, double> document_to_relevance(64);
    ConcurrentSet<int> useless_documents(64);

    std::for_each(policy,
                  query.minus_words.begin(),
                  query.minus_words.end(),
                  [this, &useless_documents](std::string_view word)
    {
        if (word_to_document_freqs_.count(word) != 0) {
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                useless_documents.Insert(document_id);
            }
        }
    });

    auto plus_predicate = [this, &document_to_relevance,
                          &useless_documents, &predicate] (std::string_view word)
    {
        if (word_to_document_freqs_.count(word) != 0) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            const auto& docs_freqs = word_to_document_freqs_.at(word);
            for (const auto& [document_id, term_freq] : docs_freqs) {
                if (useless_documents.Count(document_id) > 0) continue;

                const auto& document = documents_.at(document_id);
                if (predicate(document_id, document.status, document.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    };

    std::for_each(policy,
                  query.plus_words.begin(),
                  query.plus_words.end(),
                  plus_predicate);

    std::map<int,double> om = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents(om.size());
    std::transform(policy,
                   om.begin(),
                   om.end(),
                   matched_documents.begin(),
                   [this](const auto& pair){
                        return Document(pair.first,
                                        pair.second,
                                        documents_.at(pair.first).rating);
                    }
    );

    return matched_documents;
}

template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id)
{
    const auto& words {document_to_word_freqs_.at(document_id)};

    std::vector<const std::string*> words_key(words.size());
    std::transform(policy,
                   words.begin(),
                   words.end(),
                   words_key.begin(),
                   [](const auto& pair){return &pair.first; }
    );

    std::for_each(policy, words_key.begin(), words_key.end(),
                  [&document_id, this](const std::string* s) {
                   word_to_document_freqs_.at(*s).erase(document_id);
    });

    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    documents_id_.erase(document_id);
}
