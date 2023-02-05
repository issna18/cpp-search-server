#pragma once

#include <iostream>
#include <vector>
#include <map>

void TestSearchServer();

template <typename F, typename S>
std::ostream& operator<<(std::ostream& out, const std::pair<F, S> p) {
    using namespace std::string_literals;
    out << '(' << p.first << ", "s << p.second << ')';
    return out;
}

template <typename Container>
std::ostream& Print(std::ostream& out, const Container& container)
{
    using namespace std::string_literals;

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
std::ostream& operator<<(std::ostream& out, const std::vector<ElementT> container)
{
    out << '[';
    return Print(out, container) << ']';
}

template <typename K, typename V>
std::ostream& operator<<(std::ostream& out, const std::map<K, V> container) {
    out << '<';
    return Print(out, container) << '>';
}

template <typename T>
void AssertEqualImpl(const T& t, const T& u, const std::string& t_str,
                     const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint)
{
    using namespace std::string_literals;
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const std::string& expr_str, const std::string& file,
                const std::string& func, unsigned line,
                const std::string& hint);

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T func, const std::string& name)
{
    using namespace std::string_literals;
    func();
    std::cerr << name << " OK"s << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), (#func))
