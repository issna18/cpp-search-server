#include <algorithm>
#include <cmath>
#include <iostream>
#include <iterator>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std::string_literals;

using
    std::cin,
    std::cerr,
    std::cout,
    std::endl,
    std::set,
    std::string,
    std::vector;

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
    std::istringstream in(text);
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
    Document() = default;
    Document(int _id, double _relevance, int _rating)
        : id{_id}, relevance{_relevance}, rating{_rating} { };
    int id{};
    double relevance{};
    int rating{};
};

class SearchServer {
public:
    SearchServer() = default;

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words) {
        for (const string& word : stop_words) {
            if(!IsValidString(word)) {
                throw std::invalid_argument("В стоп-слове недопустимые символы");
            }
            stop_words_.insert(word);
        }
    }

    explicit SearchServer(const string& text)
        : SearchServer{SplitIntoWords(text)}
    {}

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        if(document_id < 0) {
            throw std::invalid_argument("Документ с отрицательным id");
        }
        if(documents_.count(document_id) > 0) {
            throw std::invalid_argument("Документ с id уже добавлен");
        }
        if(!IsValidString(document)) {
            throw std::invalid_argument("В тексте документы недопустимые символы");
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / static_cast<double>(words.size());
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template<typename Predicate>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
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
        const auto predicate = [status](int document_id, DocumentStatus s, int rating) {
            (void) document_id; (void) rating;
            return s == status;
        };
        return FindTopDocuments(raw_query, predicate);
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    int GetDocumentId(int index) const {
        if(index < 0 || static_cast<size_t>(index) > documents_.size()){
            throw std::out_of_range("Индекс документа за пределами диапазона от 0 до количества документов");
        }
        auto it = documents_.cbegin();
        std::advance(it, index);
        return it->first;
    }

    std::tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
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

    std::set<string> stop_words_{};
    std::map<string, std::map<int, double>> word_to_document_freqs_{};
    std::map<int, DocumentData> documents_{};

    static bool IsValidString(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

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
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        std::set<string> plus_words;
        std::set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        if(!IsValidString(text)) {
            throw std::invalid_argument("В поисковом запросе есть недопустимые символы");
        }
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            if(word[0] == '-' && word[1] == '-') {
                throw std::invalid_argument("В поисковом запросе более одного минуса перед словами");
            }
            if(word[word.size()-1] == '-' ) {
                throw std::invalid_argument("В поисковом запросе отсутствует текст после символа \"минус\"");
            }
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
        std::map<int, double> document_to_relevance;
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
            matched_documents.emplace_back(Document(document_id,
                                                    relevance,
                                                    documents_.at(document_id).rating));
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
std::ostream& Print(std::ostream& out, const Container& container)
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
std::ostream& operator<<(std::ostream& out, const vector<ElementT> container)
{
    out << '[';
    return Print(out, container) << ']';
}

template <typename T>
void AssertEqualImpl(const T& t, const T& u, const string& t_str,
                     const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint)
{
    if (t != u) {
        cerr << std::boolalpha;
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
        ASSERT_EQUAL_HINT(server.GetDocumentCount(), 1,
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
        ASSERT_EQUAL_HINT(found_docs.size(), 1ul, "Должен найтись ровно 1 документ"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Неправильный id документа"s);
    }

    {
        SearchServer server;
        server.AddDocument(33, "cat in the city"s, status, ratings);
        server.AddDocument(44, "cat in black"s, status, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 2ul, "Должно найтись ровно 2 документа"s);
        const Document& doc0 = found_docs[0];
        const Document& doc1 = found_docs[1];
        ASSERT_EQUAL_HINT(doc0.id, 33, "Неправильный id первого документа"s);
        ASSERT_EQUAL_HINT(doc1.id, 44, "Неправильный id второго документа"s);
    }
}

void TestExcludeStopWordsFromAddedDocumentContent()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings {1, 2, 3};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1ul,
                          "Неправильная обработка пустого списка стоп-слов"s);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Стоп-слова должны быть исключены из документов"s);
    }

    {
        SearchServer server("dog"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1ul,
                          "Cтоп-слово не входит в содержимое документа"s);
    }

    {
        const vector<string> stop_words{"in"s, "the"s};
        SearchServer server(stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Стоп-слова в vector должны быть исключены из документов"s);
    }

    {
        const std::set<string> stop_words{"in"s, "the"s};
        SearchServer server(stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Стоп-слова в set должны быть исключены из документов"s);
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
        ASSERT_EQUAL_HINT(std::get<0>(words), query,
                          "Возвращены не все слова из поискового запроса, "
                          "присутствующие в документе"s);
    }

    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        const string& raw_query = "cat -city"s;
        const auto words = server.MatchDocument(raw_query, document_id);
        ASSERT_HINT(std::get<0>(words).empty(),
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
    {
        SearchServer server;
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
        const auto found_docs = server.FindTopDocuments("in"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, 2);
    }

    {
        SearchServer server;
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {-1, -2, -5});
        const auto found_docs = server.FindTopDocuments("in"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, -2);
    }

    {
        SearchServer server;
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        const auto found_docs = server.FindTopDocuments("in"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.rating, -1);
    }
}

void TestFilteredPredicate()
{
    SearchServer server;
    server.AddDocument(33, "пушистый ухоженный кот"s, DocumentStatus::BANNED, {1, 2, 3});
    server.AddDocument(22, "пушистый ухоженный кот"s, DocumentStatus::ACTUAL, {1, 2, 3});
    {
        const auto predicate = [](int document_id, DocumentStatus status, int rating) {
            (void) status; (void) rating;
            return document_id % 2 == 0;
        };
        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, predicate);
        ASSERT_EQUAL_HINT(found_docs.size(), 1ul, "Должен найтись ровно 1 документ с четным id");
        ASSERT_EQUAL_HINT(found_docs[0].id, 22,
                "Неверная фильтрация результатов поиска с использованием предиката"s);
    }

    {
        const auto predicate = [](int document_id, DocumentStatus status, int rating) {
            (void) document_id; (void) rating;
            return status == DocumentStatus::BANNED;
        };
        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, predicate);
        ASSERT_EQUAL_HINT(found_docs.size(), 1ul, "Должен найтись ровно 1 документ со статусом BANNED");
        ASSERT_EQUAL_HINT(found_docs[0].id, 33, "Документ не соответствует статусу BANNED"s);
    }

    {
        const auto predicate = [](int document_id, DocumentStatus status, int rating) {
            (void) document_id; (void) rating;
            return status == DocumentStatus::ACTUAL;
        };
        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, predicate);
        ASSERT_EQUAL_HINT(found_docs.size(), 1ul, "Должен найтись ровно 1 документ со статусом ACTUAL");
        ASSERT_EQUAL_HINT(found_docs[0].id, 22, "Документ не соответствует статусу ACTUAL"s);
    }

    {
        const auto predicate = [](int document_id, DocumentStatus status, int rating) {
            (void) document_id; (void) rating;
            return status == DocumentStatus::IRRELEVANT;
        };
        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, predicate);
        ASSERT_HINT(found_docs.empty(), "Документов со статусом IRRELEVANT не должно быть");
    }

    {
        const auto predicate = [](int document_id, DocumentStatus status, int rating) {
            (void) document_id; (void) rating;
            return status == DocumentStatus::REMOVED;
        };
        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, predicate);
        ASSERT_HINT(found_docs.empty(), "Документов со статусом REMOVED не должно быть");
    }
}

void TestSearchedStatus()
{
    SearchServer server;
    server.AddDocument(33, "пушистый ухоженный кот"s, DocumentStatus::BANNED, {1, 2, 3});
    server.AddDocument(22, "пушистый ухоженный кот"s, DocumentStatus::ACTUAL, {1, 2, 3});
    const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs[0].id, 22);
}

void TestCalcRelevance()
{
    SearchServer server("и в на"s);
    server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::BANNED, {8, -3});
    server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
    constexpr double RELEVANCE = 0.866434;
    ASSERT(std::abs(found_docs[0].relevance - RELEVANCE) < EPSILON);
}

void TestGetDocumentId()
{
    SearchServer server("и в на"s);
    server.AddDocument(100, "белый кот и модный ошейник"s,      DocumentStatus::BANNED, {8, -3});
    server.AddDocument(50, "пушистый кот пушистый хвост"s,      DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    int id{server.GetDocumentId(2)};
    ASSERT_EQUAL(id, 100);
}

void TestSearchServer()
{
    RUN_TEST(TestAddedDocumentContent);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusQueryFromSearchServer);
    RUN_TEST(TestMatchedDocument);
    RUN_TEST(TestSortedRelevance);
    RUN_TEST(TestCalcRating);
    RUN_TEST(TestFilteredPredicate);
    RUN_TEST(TestSearchedStatus);
    RUN_TEST(TestCalcRelevance);
    RUN_TEST(TestGetDocumentId);
}

int main() {
    TestSearchServer();

    SearchServer search_server("и в на"s);
    try {
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    } catch (const std::invalid_argument& e) {
        cout << "Ошибка: "s << e.what() << endl;
    }

    try {
        search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    } catch (const std::invalid_argument& e) {
        cout << "Ошибка: "s << e.what() << endl;
    }

    try {
        search_server.AddDocument(3, "большой пёс скво\x12рец"s, DocumentStatus::ACTUAL, {1, 3, 2});
    } catch (const std::invalid_argument& e) {
        cout << "Ошибка: "s << e.what() << endl;
    }

    try {
        const auto documents = search_server.FindTopDocuments("--пушистый"s);
    } catch (const std::invalid_argument& e) {
        cout << "Ошибка: "s << e.what() << endl;
    }

    try {
        search_server.GetDocumentId(5);
    } catch (const std::out_of_range& e) {
        cout << "Ошибка: "s << e.what() << endl;
    }

    return 0;
}
