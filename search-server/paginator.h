#pragma once

#include <cstddef>
#include <ostream>
#include <vector>

template <typename Iterator>
struct Page {
    Iterator begin;
    Iterator end;
    size_t size() const {
        return std::distance(begin, end);
    }
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const Page<Iterator>& page){
    for (auto it = page.begin; it != page.end; it++) {
        out << *it;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(const Iterator begin, const Iterator end, int size)
    {
        Iterator it = begin;

        int documents_count = static_cast<int>(std::distance(begin, end));
        int full_pages_count = documents_count / size;
        int documents_ostatok = documents_count % size;

        for (int i{0}; i < full_pages_count; ++i){
            Iterator end_doc = std::next(it, size);
            pages_.emplace_back(Page<Iterator>{it, end_doc});
            it = end_doc;
        }

        if(0 != documents_ostatok){
            pages_.emplace_back(Page<Iterator>{it, end});
        }
    };

    auto begin() const {
        return pages_.begin();
    };

    auto end() const {
        return pages_.end();
    };

    int size() const {
        return pages_.size();
    };

private:
    std::vector<Page<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, int page_size) {
    return Paginator(begin(c), end(c), page_size);
}
