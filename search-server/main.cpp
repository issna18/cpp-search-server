#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
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
    int rating;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, const vector<int>& rating) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double tf_unit {1.0 / static_cast<double>(words.size())};

        for(const auto& word : words){
            word_to_document_freqs_[word][document_id] += tf_unit;
        }
        ++document_count_;
        document_ratings_[document_id] = ComputeAverageRating(rating);
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

    size_t document_count_ {0};
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, int> document_ratings_;


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

    static double calculateIDF(size_t document_count, size_t word_freqs){
      return log(static_cast<double>(document_count) / static_cast<double>(word_freqs));
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_to_relevance;

        for (const auto& wordp : query.plus){
            if (word_to_document_freqs_.count(wordp) > 0) {
                const auto& doc_tf = word_to_document_freqs_.at(wordp);
                double idf = calculateIDF(document_count_, doc_tf.size());
                for(const auto& [id, tf]: doc_tf){
                    document_to_relevance[id] += tf * idf;
                }
            }
        }
        for (const auto& word : query.minus){
            if (word_to_document_freqs_.count(word) > 0) {
                for (const auto& [id, tf] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.erase(id);
                }
            }
        }

        vector<Document> all_documents;
        for (const auto& [k, v] : document_to_relevance){
            all_documents.push_back({k, v, document_ratings_.at(k)});
        }
        return all_documents;
    }

    static int ComputeAverageRating(const vector<int>& ratings)  {
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }
};

SearchServer CreateSearchServer()
{
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        auto doc = ReadLine();
        size_t count;
        int number;
        vector<int> numbers;
        cin >> count;
        for (size_t i = 0; i < count; ++i){
            cin >> number;
            numbers.push_back(number);
        }
        search_server.AddDocument(document_id, ReadLine(), numbers);
    }
    return search_server;
}

int main()
{
    const SearchServer search_server = CreateSearchServer();
    const string query = ReadLine();
    for (const auto& [document_id, relevance, ratings] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << ", "
             << "rating = "s <<  ratings << "}"s << endl;
    }
    return 0;
}
