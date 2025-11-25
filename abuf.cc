#include "abuf.h"

namespace ke {

void abuf::append(std::string_view s) {
    buffer_.append(s);
}

void abuf::append(const char* s, std::size_t len) {
    buffer_.append(s, len);
}

void abuf::append(char c) {
    buffer_.push_back(c);
}

} // namespace ke
