#include "search_server.h"
#include "string_processing.h"

#include <algorithm>
#include <numeric>
#include <cmath>

SearchServer::SearchServer(const std::string& stop_words)
    : SearchServer {SplitIntoWords(stop_words)}
{
}
SearchServer::SearchServer(const std::string_view stop_words)
    : SearchServer {SplitIntoWords(stop_words)}
{
}

void SearchServer::AddDocument(int document_id,
                               const std::string_view document,
                               DocumentStatus status,
                               const std::vector<int>& ratings)
{
    if (document_id < 0) {
        throw std::invalid_argument("Документ с отрицательным id");
    }
    if (documents_.count(document_id) > 0) {
        throw std::invalid_argument("Документ с id уже добавлен");
    }
    if (!IsValidString(document)) {
        throw std::invalid_argument("В тексте документа недопустимые символы");
    }
    const std::vector<std::string_view> words {SplitIntoWordsNoStop(document)};
    const double inv_word_count = 1.0 / static_cast<double>(words.size());
    for (const std::string_view word : words) {
        auto it_src {all_words_.emplace(word)};
        double freq = word_to_document_freqs_[*it_src.first][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][*it_src.first] = freq;
    }
    documents_.emplace(document_id, DocumentData {ComputeAverageRating(ratings), status});
    documents_id_.emplace(document_id);
}

std::vector<Document>
SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const
{
    return FindTopDocuments(std::execution::seq, raw_query, status);
}

int SearchServer::GetDocumentCount() const
{
    return static_cast<int>(documents_.size());
}

std::set<int>::const_iterator SearchServer::begin()
{
    return documents_id_.cbegin();
}

std::set<int>::const_iterator SearchServer::end()
{
    return documents_id_.cend();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    static const std::map<std::string_view, double> empty_{};
    if (0 == document_to_word_freqs_.count(document_id)) return empty_;
    return document_to_word_freqs_.at(document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const
{
    const Query query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;

    for (const std::string_view word : query.minus_words) {
        if (0 == word_to_document_freqs_.count(word)) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {matched_words, documents_.at(document_id).status};
        }
    }

    for (const std::string_view word : query.plus_words) {
        if (0 == word_to_document_freqs_.count(word)) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.emplace_back(word);
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(const std::execution::sequenced_policy&,
                            const std::string_view raw_query,
                            int document_id) const
{
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(const std::execution::parallel_policy&,
                            const std::string_view raw_query,
                            int document_id) const
{
    if (documents_id_.count(document_id) == 0) throw std::out_of_range("No document");

    const Query query {ParseQuery(raw_query, false)};
    std::vector<std::string_view> matched_words;

    auto predicate = [this, &document_id](const std::string_view word) {
        return word_to_document_freqs_.at(word).count(document_id);
    };

    if (std::any_of(std::execution::par,
                    query.minus_words.begin(),
                    query.minus_words.end(),
                    predicate))
    {
        return {matched_words, documents_.at(document_id).status};
    }

    matched_words.resize(query.plus_words.size());
    auto it = std::copy_if(std::execution::par,
                           query.plus_words.begin(),
                           query.plus_words.end(),
                           matched_words.begin(),
                           predicate);

    matched_words.resize(static_cast<size_t>(std::distance(matched_words.begin(), it)));
    std::sort(std::execution::par, matched_words.begin(), matched_words.end());
    auto last = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
    matched_words.erase(last, matched_words.end());

    return {matched_words, documents_.at(document_id).status};
}

void SearchServer::RemoveDocument(int document_id)
{
    for(const auto& [word, _]: document_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    documents_id_.erase(document_id);
}

bool SearchServer::IsStopWord(const std::string_view word) const
{
    return stop_words_.count(word) > 0;
}

std::vector<std::string_view>
SearchServer::SplitIntoWordsNoStop(const std::string_view text) const
{
    std::vector<std::string_view> words;
    for (const std::string_view wv : SplitIntoWords(text)) {
        if (!IsStopWord(wv)) {
            words.emplace_back(wv);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
    if(ratings.empty()) return 0;
    return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view w) const
{
    bool is_minus = false;

    if (w.front() == '-') {
        if (w.size() == 1) {
            throw std::invalid_argument("В поисковом запросе слово состоит из одного символа \"минус\"");
        }
        if (w[1] == '-') {
            throw std::invalid_argument("В поисковом запросе более одного минуса перед словами");
        }
        is_minus = true;
    }
    if (w.back() == '-' ) {
        throw std::invalid_argument("В поисковом запросе отсутствует текст после символа \"минус\"");
    }

    return {is_minus ? w.substr(1) : w,
            is_minus,
            IsStopWord(w)};
}

SearchServer::Query
SearchServer::ParseQuery(const std::string_view text, bool cleanup) const
{
    if (!IsValidString(text)) {
        throw std::invalid_argument("В поисковом запросе недопустимые символы");
    }
    Query query;
    for (const std::string_view word : SplitIntoWords(text)) {
        const QueryWord query_word {ParseQueryWord(word)};
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.emplace_back(query_word.data);
            } else {
                query.plus_words.emplace_back(query_word.data);
            }
        }
    }

    if (cleanup) {
        std::sort(query.minus_words.begin(), query.minus_words.end());
        auto last_m = std::unique(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(last_m, query.minus_words.end());

        std::sort(query.plus_words.begin(), query.plus_words.end());
        auto last_p = std::unique(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(last_p, query.plus_words.end());
    }

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view word) const
{
    return std::log(
                static_cast<double>(documents_.size()) /
                static_cast<double>(word_to_document_freqs_.at(word).size())
                );
}
