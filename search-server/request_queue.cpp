#include "request_queue.h"

#include <execution>
#include <string>
#include <vector>

RequestQueue::RequestQueue(const SearchServer& search_server)
        : server_(search_server)
{

}

std::vector<Document>
RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status)
{
    AddQueryResult({raw_query, server_.FindTopDocuments(raw_query, status)});
    return requests_.back().result;
}

std::vector<Document>
RequestQueue::AddFindRequest(const std::string& raw_query)
{
    AddQueryResult({raw_query, server_.FindTopDocuments(raw_query)});
    return requests_.back().result;
}

int RequestQueue::GetNoResultRequests() const
{
    return bad_requests_count;
}

void RequestQueue::AddQueryResult(const RequestQueue::QueryResult& qr)
{
    requests_.emplace_back(qr);
    if(qr.result.empty()) ++bad_requests_count;
    if (requests_.size() > min_in_day_){
        requests_.pop_front();
        --bad_requests_count;
    }
}
