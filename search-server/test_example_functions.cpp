#include "test_example_functions.h"
#include "document.h"
#include "search_server.h"

#include <string>
#include <string_view>
#include <set>
#include <map>

using namespace std::string_literals;
using namespace std::string_view_literals;

void AssertImpl(bool value, const std::string& expr_str, const std::string& file,
                const std::string& func, unsigned line,
                const std::string& hint)
{
    if (!value) {
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

void TestAddedDocumentContent()
{
    DocumentStatus status = DocumentStatus::ACTUAL;
    const std::vector<int> ratings {1, 2, 3};
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
        const std::string content = "dog"s;
        server.AddDocument(doc_id, content, status, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_HINT(found_docs.empty(), "Найденных документов не должно быть"s);
    }

    {
        SearchServer server;
        const int doc_id = 33;
        const std::string content = "cat in the city"s;
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
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings {1, 2, 3};
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
        const std::vector<std::string> stop_words{"in"s, "the"s};
        SearchServer server(stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Стоп-слова в vector должны быть исключены из документов"s);
    }

    {
        const std::set<std::string> stop_words{"in"s, "the"s};
        SearchServer server(stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Стоп-слова в set должны быть исключены из документов"s);
    }
}

void TestExcludeMinusQueryFromSearchServer()
{
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings {1, 2, 3};
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
    const std::vector<int> ratings {1, 2, 3};
    const int document_id = 42;
    const std::string content = "cat in the city"s;
    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        const std::string raw_query = "cat city"s;
        const auto words = server.MatchDocument(raw_query, document_id);
        std::vector<std::string_view> query {"cat"sv, "city"sv};
        ASSERT_EQUAL_HINT(std::get<0>(words), query,
                          "Возвращены не все слова из поискового запроса, "
                          "присутствующие в документе"s);
    }

    {
        SearchServer server;
        server.AddDocument(document_id, content, DocumentStatus::ACTUAL, ratings);
        const std::string& raw_query = "cat -city"s;
        const auto words = server.MatchDocument(raw_query, document_id);
        ASSERT_HINT(std::get<0>(words).empty(),
                    "Должен возвращаться пустой список слов, "
                    "при наличии минус-слова в документе"s);
    }
}

void TestSortedRelevance()
{
    const std::vector<int> ratings {1, 2, 3};
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

void TestServerIterator()
{
    SearchServer server("и в на"s);
    server.AddDocument(100, "белый кот и модный ошейник"s,      DocumentStatus::BANNED, {8, -3});
    server.AddDocument(50, "пушистый кот пушистый хвост"s,      DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    std::vector<int> expected {1, 50, 100};
    auto it = expected.begin();
    for (const auto& document_id : server) {
         ASSERT_EQUAL(document_id, *it++);
    }
}

void TestGetWordFrequencies()
{
    SearchServer server("и в на"s);
    server.AddDocument(100, "белый кот и модный ошейник"s,      DocumentStatus::BANNED, {8, -3});
    server.AddDocument(50, "пушистый кот пушистый хвост"s,      DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    ASSERT_EQUAL(server.GetWordFrequencies(50).at("пушистый"), 0.5);
    const std::map<std::string_view, double>& expected = {};
    ASSERT_EQUAL(server.GetWordFrequencies(2), expected);
}

void TestRemoveDocument()
{
    SearchServer server("и в на"s);
    server.AddDocument(100, "белый кот и модный ошейник"s,      DocumentStatus::BANNED, {8, -3});
    server.AddDocument(50, "пушистый кот пушистый хвост"s,      DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(1, "ухоженный пёс выразительные глаза"s, DocumentStatus::BANNED, {5, -12, 2, 1});
    server.RemoveDocument(50);
    ASSERT_EQUAL(server.GetDocumentCount(), 2);
    const std::map<std::string_view, double>& expected = {};
    ASSERT_EQUAL(server.GetWordFrequencies(50), expected);
}

void TestStringViewConstructor()
{
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings {1, 2, 3};
    {
        SearchServer server(""sv);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1ul,
                          "Неправильная обработка пустого списка стоп-слов"s);
    }

    {
        SearchServer server("in the"sv);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Стоп-слова должны быть исключены из документов"s);
    }

    {
        SearchServer server("dog"sv);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1ul,
                          "Cтоп-слово не входит в содержимое документа"s);
    }

    {
        const std::vector<std::string_view> stop_words{"in"sv, "the"sv};
        SearchServer server(stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Стоп-слова в vector должны быть исключены из документов"s);
    }

    {
        const std::set<std::string_view> stop_words{"in"sv, "the"sv};
        SearchServer server(stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Стоп-слова в set должны быть исключены из документов"s);
    }
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
    RUN_TEST(TestServerIterator);
    RUN_TEST(TestGetWordFrequencies);
    RUN_TEST(TestRemoveDocument);
    RUN_TEST(TestStringViewConstructor);
}
