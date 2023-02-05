#include "log_duration.h"
#include "search_server.h"

#include <algorithm>
#include <numeric>
#include <cmath>

SearchServer::SearchServer(const std::string& text)
    : SearchServer{SplitIntoWords(text)}
{

}

void SearchServer::AddDocument(int document_id,
                               const std::string& document,
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
    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / static_cast<double>(words.size());
    for (const std::string& word : words) {
        double freq = word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] = freq;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    documents_id_.emplace(document_id);
}

std::vector<Document>
SearchServer::FindTopDocuments(const std::string& raw_query,
                               DocumentStatus status) const
{
    const auto predicate = [status](int document_id, DocumentStatus s, int rating) {
        (void) document_id; (void) rating;
        return s == status;
    };
    return FindTopDocuments(raw_query, predicate);
}

std::vector<Document>
SearchServer::FindTopDocuments(const std::string& raw_query) const
{
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
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

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    try {
        return document_to_word_freqs_.at(document_id);
    } catch (...) {
        return empty_;
    }
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

std::tuple<std::vector<std::string>, DocumentStatus>
SearchServer::MatchDocument(const std::string& raw_query, int document_id) const
{
    LOG_DURATION_STREAM("Operation time", std::cerr);
    const Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}


bool SearchServer::IsStopWord(const std::string& word) const
{
    return stop_words_.count(word) > 0;
}

std::vector<std::string>
SearchServer::SplitIntoWordsNoStop(const std::string& text) const
{
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings)
{
    if(ratings.empty()) return 0;
    return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string word) const
{
    bool is_minus = false;

    if (word.front() == '-') {
        if (word.size() == 1) {
            throw std::invalid_argument("В поисковом запросе слово состоит из одного символа \"минус\"");
        }
        if (word[1] == '-') {
            throw std::invalid_argument("В поисковом запросе более одного минуса перед словами");
        }
        is_minus = true;
        word = word.substr(1);
    }
    if (word.back() == '-' ) {
        throw std::invalid_argument("В поисковом запросе отсутствует текст после символа \"минус\"");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const
{
    if (!IsValidString(text)) {
        throw std::invalid_argument("В поисковом запросе недопустимые символы");
    }
    Query query;
    for (const std::string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const
{
    return std::log(
                static_cast<double>(documents_.size()) /
                static_cast<double>(word_to_document_freqs_.at(word).size())
                );
}
