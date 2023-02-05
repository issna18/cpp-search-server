#include "remove_duplicates.h"

struct IsFirstEqual {
    template <typename Pair>
    bool operator() (Pair const &lhs, Pair const &rhs) const {
        return lhs.first == rhs.first;
    }
};

template <typename Map>
bool keys_equal(Map const &lhs, Map const &rhs)
{
    return lhs.size() == rhs.size()
        && std::equal(lhs.begin(), lhs.end(),
                      rhs.begin(),
                      IsFirstEqual()); // predicate instance
}


void RemoveDuplicates(SearchServer& server)
{
    std::set<int> docs_to_delete;

    for (const auto& [document_id, words] : server.document_to_word_freqs_) {
        for(const auto& [word, _] : words) {
            const auto& ids = server.word_to_document_freqs_.at(word);
            for (const auto& [id_other, __] : ids) {
                if (id_other > document_id){
                    const auto& words_other = server.GetWordFrequencies(id_other);
                    if (keys_equal(words, words_other)) docs_to_delete.emplace(id_other);
                }
            }
        }
    }

    for (int id: docs_to_delete){
        std::cout << "Found duplicate document id " << id << '\n';
        server.RemoveDocument(id);
    }
}
