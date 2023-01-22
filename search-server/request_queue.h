#pragma once

#include "document.h"
#include "search_server.h"

#include <string>
#include <vector>
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        std::string query;
        std::vector<Document> result;
    };

    void AddQueryResult(const QueryResult& qr);

    const SearchServer& server_;
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int bad_requests_count{};
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate)
{
    AddQueryResult({raw_query, server_.FindTopDocuments(raw_query, document_predicate)});
    return requests_.back().result;
}
