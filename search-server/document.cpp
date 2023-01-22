#include "document.h"

Document::Document(int _id, double _relevance, int _rating)
    : id{_id},
      relevance{_relevance},
      rating{_rating}
{

}

std::ostream& operator<<(std::ostream& output, const Document& document) {
    using namespace std::string_literals;
    output << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s;
    return output;
}
