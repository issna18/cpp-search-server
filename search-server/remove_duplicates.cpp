#include "remove_duplicates.h"

#include <algorithm>
#include <iterator>

void RemoveDuplicates(SearchServer& server)
{
    std::set<int> docs_to_delete;
    std::map<std::set<std::string>, int> words_to_doc;

    for (int document_id : server) {
        const auto& words = server.GetWordFrequencies(document_id);
        std::set<std::string> words_key;
        std::transform(words.begin(),
                       words.end(),
                       std::inserter(words_key, words_key.end()),
                       [](auto pair){ return pair.first; }
        );
        if (0 == words_to_doc.count(words_key)) {
            words_to_doc[words_key] = document_id;
        } else {
            docs_to_delete.emplace(document_id);
        }
    }

    for (int id : docs_to_delete) {
        std::cout << "Found duplicate document id " << id << '\n';
        server.RemoveDocument(id);
    }
}
