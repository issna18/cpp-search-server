#include "process_queries.h"

#include <execution>
#include <iostream>
#include <iterator>
#include <numeric>

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(),
                   [&search_server](const std::string& query) {
                   return search_server.FindTopDocuments(query);
                   });
    return result;
}
std::vector<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> responses {ProcessQueries(search_server, queries)};
    std::vector<Document> result = std::reduce(std::execution::par,
                                               responses.begin(), responses.end(),
                                               std::vector<Document>{},
                                               [](std::vector<Document> a, const std::vector<Document> b) {
                                               a.insert(a.end(), b.begin(), b.end());
                                               return a;
                                               });

    return result;
}
