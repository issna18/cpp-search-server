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
constexpr double EPSILON = 1e-6;

string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber()
{
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text)
{
    istringstream in(text);
    string word;
    vector<string> words;
    while (in >> word) {
        words.push_back(word);
    }
    return words;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

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

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / static_cast<double>(words.size());
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template<typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      Predicate predicate) const
    {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < EPSILON) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query,
                                      DocumentStatus status) const {
        return FindTopDocuments(raw_query,
                                [status](int document_id,
                                         DocumentStatus s,
                                         int rating) {
                                            return s == status;
                                         });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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

    static int ComputeAverageRating(const vector<int>& ratings) {
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
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

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(static_cast<double>(documents_.size()) / static_cast<double>(word_to_document_freqs_.at(word).size()));
    }

    template<typename Predicate>
    vector<Document> FindAllDocuments(const Query& query,
                                      Predicate predicate) const
    {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document = documents_.at(document_id);
                if (predicate(document_id, document.status, document.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id,
                                         relevance,
                                         documents_.at(document_id).rating
                                        });
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

template <typename Container>
ostream& Print(ostream& out, const Container& container)
{
    bool is_first = true;
    for (const auto& element : container) {
        if (!is_first) {
            out << ", "s;
        }
        is_first = false;
        out << element;
    }
    return out;
}

template <typename ElementT>
ostream& operator<<(ostream& out, const vector<ElementT> container)
{
    out << '[';
    return Print(out, container) << ']';
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str,
                     const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint)
{
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file,
                const string& func, unsigned line,
                const string& hint)
{
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T func, const string& name) {
    func();
    cerr << name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), (#func))

void TestAddedDocumentContent()
{
    DocumentStatus status = DocumentStatus::ACTUAL;
    const vector<int> ratings {1, 2, 3};
    {
        SearchServer server;
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 0,
                          "Сервер не должен содержать документы"s);
        server.AddDocument(11, ""s, status, ratings);
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 1u,
                          "Сервер должен содержать 1 документ"s);
    }

    {
        SearchServer server;
        const int doc_id = 22;
        const string content = "dog"s;
        server.AddDocument(doc_id, content, status, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_HINT(found_docs.empty(), "Найденных документов не должно быть"s);
    }

    {
        SearchServer server;
        const int doc_id = 33;
        const string content = "cat in the city"s;
        server.AddDocument(doc_id, content, status, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Должен найтись ровно 1 документ"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Неправильный id документа"s);
    }

    {
        SearchServer server;
        server.AddDocument(33, "cat in the city"s, status, ratings);
        server.AddDocument(44, "cat in black"s, status, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 2u, "Должно найтись ровно 2 документа"s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL_HINT(doc0.id, 33u, "Неправильный id первого документа"s);
        ASSERT_EQUAL_HINT(doc1.id, 44u, "Неправильный id второго документа"s);
    }
}

void TestExcludeStopWordsFromAddedDocumentContent()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
                          "Неправильная обработка пустого списка стоп-слов"s);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Стоп-слова должны быть исключены из документов"s);
    }

    {
        SearchServer server;
        server.SetStopWords("dog"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1u,
                          "Cтоп-слово не входит в содержимое документа"s);
    }
}

void TestExcludeMinusQueryFromSearchServer()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments(""s);
        ASSERT_HINT(found_docs.empty(),
                    "Неправильная обработка пустого запроса"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat -city"s);
        ASSERT_HINT(found_docs.empty(),
                    "Неправильная обработка запроса с минус-словами"s);
    }
}

void TestMatchedDocument()
{
    const vector<int> ratings {1, 2, 3};
    const int document_id = 42;
    const string content = "cat in the city"s;
    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        const string raw_query = "cat city"s;
        const auto words = server.MatchDocument(raw_query, document_id);
        vector<string> query {"cat"s, "city"s};
        ASSERT_EQUAL_HINT(get<0>(words), query,
                          "Возвращены не все слова из поискового запроса, "
                          "присутствующие в документе"s);
    }

    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        const string& raw_query = "cat -city"s;
        const auto words = server.MatchDocument(raw_query, document_id);
        ASSERT_HINT(get<0>(words).empty(),
                    "Должен возвращаться пустой список слов, "
                    "при наличии минус-слова в документе"s);
    }
}

void TestSortedRelevance()
{
    const vector<int> ratings {1, 2, 3};
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(33, "cat black"s, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("cat in"s);
    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];
    ASSERT(doc0.relevance > doc1.relevance);
}

void TestCalcRating()
{
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto found_docs = server.FindTopDocuments("in"s);
    const Document& doc0 = found_docs[0];
    ASSERT_EQUAL(doc0.rating, 2u);
}

void TestFilteredPridicate()
{
    SearchServer server;
    server.AddDocument(33, "пушистый ухоженный кот"s, DocumentStatus::BANNED, {1, 2, 3});
    server.AddDocument(22, "пушистый ухоженный кот"s, DocumentStatus::ACTUAL, {1, 2, 3});
    {
        const auto found_docs = server.FindTopDocuments(
                    "пушистый ухоженный кот"s,
                    [](int document_id, DocumentStatus status, int rating)
                        { return document_id % 2 == 0; });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Должен найтись ровно 1 документ с четным id");
        ASSERT_EQUAL_HINT(found_docs[0].id, 22,
                "Неверная фильтрация результатов поиска с использованием предиката"s);
    }

    {
        const auto found_docs = server.FindTopDocuments(
                    "пушистый ухоженный кот"s,
                    [](int document_id, DocumentStatus status, int rating)
                        { return status == DocumentStatus::BANNED; });
        ASSERT_EQUAL_HINT(found_docs.size(), 1u, "Должен найтись ровно 1 документ со статусом BANNED");
        ASSERT_EQUAL_HINT(found_docs[0].id, 33u, "Документ не соответствует статусу BANNED"s);
    }
}

void TestSerchedStatus()
{
    SearchServer server;
    server.AddDocument(33, "пушистый ухоженный кот"s, DocumentStatus::BANNED, {1, 2, 3});
    server.AddDocument(22, "пушистый ухоженный кот"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs[0].id, 22u);
}

void TestCalcRelevance()
{
    SearchServer server;
    server.SetStopWords("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::BANNED, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
    constexpr double RELEVANCE = 0.866434;
    ASSERT(found_docs[0].relevance > RELEVANCE - EPSILON &&
           found_docs[0].relevance < RELEVANCE + EPSILON);
}

void TestSearchServer()
{
    RUN_TEST(TestAddedDocumentContent);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusQueryFromSearchServer);
    RUN_TEST(TestMatchedDocument);
    RUN_TEST(TestSortedRelevance);
    RUN_TEST(TestCalcRating);
    RUN_TEST(TestFilteredPridicate);
    RUN_TEST(TestSerchedStatus);
    RUN_TEST(TestCalcRelevance);
}

int main()
{
    TestSearchServer();

    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "ACTUAL:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; })) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}
