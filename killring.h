#ifndef KILLRING_HPP
#define KILLRING_HPP

#include <cstdint>

// Forward declarations
struct editor_t;

namespace ke {

/**
 * Killring class for cut/paste (kill/yank) operations.
 */
class Killring {
public:
    Killring() = default;
    ~Killring() = default;
    
    // Deleted copy constructor and assignment
    Killring(const Killring&) = delete;
    Killring& operator=(const Killring&) = delete;
    
    // Killring operations
    static void flush(editor_t* editor);
    static void yank(editor_t* editor);
    static void start_with_char(editor_t* editor, unsigned char ch);
    static void append_char(editor_t* editor, unsigned char ch);
    static void prepend_char(editor_t* editor, unsigned char ch);
    
    // Mark operations
    static void toggle_markset(editor_t* editor);
    static int cursor_after_mark(editor_t* editor);
    
    // Region operations
    static void kill_region(editor_t* editor);
    static void delete_region(editor_t* editor);
    
private:
    static int count_chars_from_cursor_to_mark(editor_t* editor);
};

} // namespace ke

#endif // KILLRING_HPP
