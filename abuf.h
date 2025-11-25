#ifndef ABUF_HPP
#define ABUF_HPP

#include <string>
#include <string_view>
#include <cstddef>

namespace ke {

/**
 * Append buffer class for efficient string building.
 * C++17 implementation with RAII memory management.
 */
class abuf {
public:
    // Constructors
    abuf() noexcept = default;
    
    // Deleted copy constructor and assignment (use move semantics)
    abuf(const abuf&) = delete;
    abuf& operator=(const abuf&) = delete;
    
    // Move constructor and assignment
    abuf(abuf&&) noexcept = default;
    abuf& operator=(abuf&&) noexcept = default;
    
    // Destructor (automatic cleanup via std::string)
    ~abuf() = default;
    
    // Append methods
    void append(std::string_view s);
    void append(const char* s, std::size_t len);
    void append(char c);
    
    // Accessors
    [[nodiscard]] const char* data() const noexcept { return buffer_.data(); }
    [[nodiscard]] std::size_t size() const noexcept { return buffer_.size(); }
    [[nodiscard]] std::size_t length() const noexcept { return buffer_.length(); }
    [[nodiscard]] bool empty() const noexcept { return buffer_.empty(); }
    [[nodiscard]] std::size_t capacity() const noexcept { return buffer_.capacity(); }
    
    // Get the underlying string
    [[nodiscard]] const std::string& str() const noexcept { return buffer_; }
    
    // Clear the buffer
    void clear() noexcept { buffer_.clear(); }
    
    // Reserve capacity
    void reserve(std::size_t new_cap) { buffer_.reserve(new_cap); }

private:
    std::string buffer_;
};

} // namespace ke

#endif // ABUF_HPP
