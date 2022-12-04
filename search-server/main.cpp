#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber()
{
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text)
{
    istringstream in(text);
    string word;
    vector<string> words;
    while(in >> word){
        words.push_back(word);
    }
    return words;
}

struct Document {
    int id;
    double relevance;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double tf_unit {1.0 / static_cast<double>(words.size())};

        for(const auto& word : words){
            word_to_document_freqs_[word][document_id] += tf_unit;
        }
        ++document_count_;
    }

    vector<Document> FindTopDocuments(const string& query) const {
        auto matched_documents = FindAllDocuments(ParseQuery(query));

        sort(matched_documents.begin(), matched_documents.end(),
             [](const auto& lhs, const auto& rhs) {
                 return lhs.relevance > rhs.relevance;
             });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct Query {
        set<string> plus;
        set<string> minus;
    };

    int document_count_{0};  // all documents number
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string& line) const {
        vector<string> words = SplitIntoWordsNoStop(line);
        Query q;
        for (const auto& word : words){
            if(word.at(0) == '-'){
                q.minus.insert(word.substr(1));
            } else {
                q.plus.insert(word);
            }
        }
        return q;
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_to_relevance;

        for (const auto& wordp : query.plus){
            if (word_to_document_freqs_.count(wordp) > 0) {
                const auto& doc_tf = word_to_document_freqs_.at(wordp);
                double idf{log(static_cast<double>(document_count_) / static_cast<double>(doc_tf.size()))};
                for(const auto& [id, tf]: doc_tf){
                    document_to_relevance[id] += tf * idf;
                }
            }
        }

        vector<Document> all_documents;
        for (const auto& [k, v]: document_to_relevance){
            all_documents.push_back({k, v});
        }
        return all_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }
    return search_server;
}

int main()
{
    const SearchServer search_server = CreateSearchServer();
    const string query = ReadLine();

    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
        }

    return 0;
}
