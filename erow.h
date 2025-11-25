#ifndef EROW_HPP
#define EROW_HPP

#include <string>
#include <memory>
#include <cstddef>
#include <cstdint>

namespace ke {

/**
 * Editor row class representing a line of text with rendering information.
 * C++17 implementation with RAII memory management.
 */
class erow {
public:
    // Constructors
    erow() noexcept = default;
    explicit erow(std::size_t initial_capacity);
    erow(const char* text, std::size_t len);
    
    // Deleted copy constructor and assignment (use move semantics)
    erow(const erow&) = delete;
    erow& operator=(const erow&) = delete;
    
    // Move constructor and assignment
    erow(erow&&) noexcept = default;
    erow& operator=(erow&&) noexcept = default;
    
    // Destructor (automatic cleanup via std::string)
    ~erow() = default;
    
    // Core operations
    void update();
    void insert_char(std::size_t at, char c);
    void delete_char(std::size_t at);
    void append_string(const char* s, std::size_t len);
    
    // Cursor/render position conversions
    [[nodiscard]] int render_to_cursor(int cx) const;
    [[nodiscard]] int cursor_to_render(int rx) const;
    
    // Accessors for line data
    [[nodiscard]] const char* line_data() const noexcept { return line_.data(); }
    [[nodiscard]] char* line_data() noexcept { return line_.data(); }
    [[nodiscard]] std::size_t line_size() const noexcept { return line_.size(); }
    [[nodiscard]] std::size_t line_capacity() const noexcept { return line_.capacity(); }
    
    // Accessors for render data
    [[nodiscard]] const char* render_data() const noexcept { return render_.data(); }
    [[nodiscard]] std::size_t render_size() const noexcept { return render_.size(); }
    
    // Direct access to strings (for compatibility)
    [[nodiscard]] std::string& line() noexcept { return line_; }
    [[nodiscard]] const std::string& line() const noexcept { return line_; }
    [[nodiscard]] const std::string& render() const noexcept { return render_; }
    
    // Resize operations
    void resize_line(std::size_t new_size) { line_.resize(new_size); }
    void reserve_line(std::size_t new_cap) { line_.reserve(new_cap); }
    
    // Set line content
    void set_line(const char* data, std::size_t len);
    void set_line(std::string&& data) noexcept { line_ = std::move(data); }

private:
    std::string line_;    // The actual line content
    std::string render_;  // The rendered version (with tabs expanded, etc.)
    
    // Helper for nibble to hex conversion
    static char nibble_to_hex(char c);
};

} // namespace ke

#endif // EROW_HPP
